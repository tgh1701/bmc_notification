// bmc_notification.c
// Windows native notification + sound plugin for Flutter FFI
//
// Features:
//  - Windows Toast Notification via PowerShell (works Win8+, no WinRT COM needed)
//  - WAV playback via PlaySound (winmm)
//  - MP3/other via PowerShell Windows Media Player
//  - Taskbar flash via FlashWindowEx
//  - Ringtone loop in background thread (WAV or MP3)

#include "bmc_notification.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>

#pragma comment(lib, "winmm.lib")

// ============================================================================
// Internal state
// ============================================================================

static char  s_appId[256]            = {0};
static BOOL  s_initialized           = FALSE;
static BOOL  s_ringtoneRunning       = FALSE;
static HANDLE s_ringtoneThread       = NULL;
static char  s_ringtonePath[MAX_PATH]= {0};
static CRITICAL_SECTION s_lock;
static BOOL  s_lockInit              = FALSE;

// ============================================================================
// Utility helpers
// ============================================================================

static void utf8ToWide(const char* utf8, WCHAR* out, int outSize) {
    if (!utf8 || !out || outSize <= 0) {
        if (out && outSize > 0) out[0] = L'\0';
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, outSize);
}

// Check if path ends with .wav (case-insensitive)
static BOOL isWav(const char* path) {
    if (!path) return FALSE;
    size_t n = strlen(path);
    if (n < 4) return FALSE;
    const char* ext = path + n - 4;
    return (_stricmp(ext, ".wav") == 0);
}

// Run a PowerShell command hidden, wait up to timeoutMs (0=don't wait)
static HANDLE runPowerShell(const WCHAR* cmd, BOOL waitForExit, DWORD timeoutMs) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Need a mutable copy for CreateProcessW
    WCHAR cmdBuf[8192] = {0};
    wcsncpy_s(cmdBuf, 8192, cmd, _TRUNCATE);

    if (!CreateProcessW(NULL, cmdBuf, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return NULL;
    }
    CloseHandle(pi.hThread);
    if (waitForExit) {
        WaitForSingleObject(pi.hProcess, timeoutMs > 0 ? timeoutMs : INFINITE);
        CloseHandle(pi.hProcess);
        return NULL;
    }
    return pi.hProcess; // caller must CloseHandle
}

// ============================================================================
// INIT / DISPOSE
// ============================================================================

FFI_PLUGIN_EXPORT int initNotification(const char* appId) {
    if (!appId) return 0;
    if (!s_lockInit) {
        InitializeCriticalSection(&s_lock);
        s_lockInit = TRUE;
    }
    strncpy_s(s_appId, sizeof(s_appId), appId, _TRUNCATE);
    s_initialized = TRUE;
    return 1;
}

FFI_PLUGIN_EXPORT void disposeNotification(void) {
    stopRingtone();
    if (s_ringtoneThread) {
        WaitForSingleObject(s_ringtoneThread, 3000);
        CloseHandle(s_ringtoneThread);
        s_ringtoneThread = NULL;
    }
    if (s_lockInit) {
        DeleteCriticalSection(&s_lock);
        s_lockInit = FALSE;
    }
    s_initialized = FALSE;
}

// ============================================================================
// TOAST NOTIFICATION — PowerShell bridge (Win8+, no WinRT COM required)
// ============================================================================

static int showToastPS(const char* title, const char* message,
                        const char* btn1,  const char* btn2) {
    WCHAR wTitle[512]   = {0};
    WCHAR wMsg[1024]    = {0};
    WCHAR wAppId[256]   = {0};
    WCHAR wBtn1[128]    = {0};
    WCHAR wBtn2[128]    = {0};

    utf8ToWide(title,   wTitle,  512);
    utf8ToWide(message, wMsg,   1024);
    utf8ToWide(s_appId[0] ? s_appId : "BMC.Viva", wAppId, 256);
    if (btn1) utf8ToWide(btn1, wBtn1, 128);
    if (btn2) utf8ToWide(btn2, wBtn2, 128);

    // Build XML toast string
    WCHAR xmlToast[4096] = {0};
    if (btn1 && wBtn1[0] && btn2 && wBtn2[0]) {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template=\"ToastGeneric\">"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual>"
            L"<actions>"
            L"<action content=\"%s\" arguments=\"btn1\"/>"
            L"<action content=\"%s\" arguments=\"btn2\"/>"
            L"</actions></toast>",
            wTitle, wMsg, wBtn1, wBtn2);
    } else if (btn1 && wBtn1[0]) {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template=\"ToastGeneric\">"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual>"
            L"<actions><action content=\"%s\" arguments=\"btn1\"/></actions>"
            L"</toast>",
            wTitle, wMsg, wBtn1);
    } else {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template=\"ToastGeneric\">"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual></toast>",
            wTitle, wMsg);
    }

    // Escape single quotes in XML (PowerShell string delimited by "...")
    // XML already uses double-quote for attributes; title/message may have '
    // Simple approach: pass via environment variable to avoid escaping hell
    // We'll use a here-string approach in PowerShell

    WCHAR psCmd[8192] = {0};
    _snwprintf_s(psCmd, 8191, _TRUNCATE,
        L"powershell -WindowStyle Hidden -NonInteractive -Command \""
        L"[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType=WindowsRuntime] | Out-Null; "
        L"[Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom, ContentType=WindowsRuntime] | Out-Null; "
        L"$xml = New-Object Windows.Data.Xml.Dom.XmlDocument; "
        L"$xml.LoadXml('%s'); "
        L"$toast = [Windows.UI.Notifications.ToastNotification]::new($xml); "
        L"[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('%s').Show($toast)"
        L"\"",
        xmlToast, wAppId
    );

    runPowerShell(psCmd, TRUE, 5000);
    return 1;
}

FFI_PLUGIN_EXPORT int showToastNotification(
    const char* appName, const char* title,
    const char* message, const char* iconPath
) {
    (void)appName; (void)iconPath;
    return showToastPS(title, message, NULL, NULL);
}

FFI_PLUGIN_EXPORT int showToastNotificationWithButtons(
    const char* appName, const char* title,
    const char* message, const char* button1, const char* button2
) {
    (void)appName;
    return showToastPS(title, message, button1, button2);
}

// ============================================================================
// SOUND: WAV (PlaySound) + MP3 (PowerShell WMP)
// ============================================================================

static int playMp3Async(const char* mp3Path) {
    WCHAR wPath[MAX_PATH] = {0};
    utf8ToWide(mp3Path, wPath, MAX_PATH);

    WCHAR cmd[2048] = {0};
    _snwprintf_s(cmd, 2047, _TRUNCATE,
        L"powershell -WindowStyle Hidden -NonInteractive -Command \""
        L"$wmp=New-Object -ComObject WMPlayer.OCX; "
        L"$wmp.URL='%s'; "
        L"$wmp.controls.play(); "
        L"Start-Sleep -s 60; "
        L"$wmp.close()\"",
        wPath
    );

    HANDLE h = runPowerShell(cmd, FALSE, 0);
    if (h) { CloseHandle(h); return 1; }
    return 0;
}

FFI_PLUGIN_EXPORT int playNotificationSound(const char* filePath) {
    if (!filePath) return 0;
    if (isWav(filePath)) {
        WCHAR wPath[MAX_PATH] = {0};
        utf8ToWide(filePath, wPath, MAX_PATH);
        return PlaySoundW(wPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) ? 1 : 0;
    }
    return playMp3Async(filePath);
}

// ============================================================================
// RINGTONE LOOP (background thread, supports WAV + MP3)
// ============================================================================

static unsigned int __stdcall _ringtoneThread(void* arg) {
    char path[MAX_PATH] = {0};
    EnterCriticalSection(&s_lock);
    strncpy_s(path, MAX_PATH, s_ringtonePath, _TRUNCATE);
    LeaveCriticalSection(&s_lock);

    BOOL wav = isWav(path);
    WCHAR wPath[MAX_PATH] = {0};
    utf8ToWide(path, wPath, MAX_PATH);

    while (1) {
        EnterCriticalSection(&s_lock);
        BOOL run = s_ringtoneRunning;
        LeaveCriticalSection(&s_lock);
        if (!run) break;

        if (wav) {
            // SND_SYNC blocks until sound finishes
            PlaySoundW(wPath, NULL, SND_FILENAME | SND_SYNC | SND_NODEFAULT);
        } else {
            // MP3: launch PowerShell, wait until done or stopped
            WCHAR cmd[2048] = {0};
            _snwprintf_s(cmd, 2047, _TRUNCATE,
                L"powershell -WindowStyle Hidden -NonInteractive -Command \""
                L"$wmp=New-Object -ComObject WMPlayer.OCX; "
                L"$wmp.URL='%s'; "
                L"$wmp.controls.play(); "
                L"do { Start-Sleep -m 200 } while ($wmp.playState -ne 1); "
                L"$wmp.close()\"",
                wPath
            );

            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            ZeroMemory(&pi, sizeof(pi));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            HANDLE hProc = NULL;
            if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                               CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                hProc = pi.hProcess;
            }

            if (hProc) {
                while (WaitForSingleObject(hProc, 200) == WAIT_TIMEOUT) {
                    EnterCriticalSection(&s_lock);
                    BOOL still = s_ringtoneRunning;
                    LeaveCriticalSection(&s_lock);
                    if (!still) { TerminateProcess(hProc, 0); break; }
                }
                CloseHandle(hProc);
            }
        }

        // Check again before sleeping
        EnterCriticalSection(&s_lock);
        BOOL run2 = s_ringtoneRunning;
        LeaveCriticalSection(&s_lock);
        if (!run2) break;

        Sleep(300);
    }
    return 0;
}

FFI_PLUGIN_EXPORT int playRingtoneLoop(const char* filePath) {
    if (!filePath) return 0;
    stopRingtone(); // stop previous

    EnterCriticalSection(&s_lock);
    strncpy_s(s_ringtonePath, MAX_PATH, filePath, _TRUNCATE);
    s_ringtoneRunning = TRUE;
    LeaveCriticalSection(&s_lock);

    s_ringtoneThread = (HANDLE)_beginthreadex(NULL, 0, _ringtoneThread, NULL, 0, NULL);
    return s_ringtoneThread != NULL ? 1 : 0;
}

FFI_PLUGIN_EXPORT void stopRingtone(void) {
    if (!s_lockInit) return;
    EnterCriticalSection(&s_lock);
    s_ringtoneRunning = FALSE;
    LeaveCriticalSection(&s_lock);

    // Immediately stop WAV
    PlaySoundW(NULL, NULL, SND_PURGE);

    if (s_ringtoneThread) {
        WaitForSingleObject(s_ringtoneThread, 4000);
        CloseHandle(s_ringtoneThread);
        s_ringtoneThread = NULL;
    }
}

FFI_PLUGIN_EXPORT int isRingtonePlaying(void) {
    if (!s_lockInit) return 0;
    EnterCriticalSection(&s_lock);
    int r = s_ringtoneRunning ? 1 : 0;
    LeaveCriticalSection(&s_lock);
    return r;
}

// ============================================================================
// TASKBAR FLASH (FlashWindowEx)
// ============================================================================

FFI_PLUGIN_EXPORT int flashTaskbarIcon(int64_t hwnd, int count) {
    HWND hWnd = hwnd != 0 ? (HWND)(intptr_t)hwnd : GetForegroundWindow();
    if (!hWnd) return 0;

    FLASHWINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.cbSize   = sizeof(FLASHWINFO);
    fi.hwnd     = hWnd;
    fi.dwFlags  = (count == 0) ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_ALL;
    fi.uCount   = (count == 0) ? 0 : (UINT)count;
    fi.dwTimeout= 0;
    FlashWindowEx(&fi);
    return 1;
}

FFI_PLUGIN_EXPORT void stopFlashTaskbarIcon(int64_t hwnd) {
    HWND hWnd = hwnd != 0 ? (HWND)(intptr_t)hwnd : GetForegroundWindow();
    if (!hWnd) return;

    FLASHWINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.cbSize  = sizeof(FLASHWINFO);
    fi.hwnd    = hWnd;
    fi.dwFlags = FLASHW_STOP;
    FlashWindowEx(&fi);
}

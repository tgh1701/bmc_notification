// bmc_notification.c
// Windows native notification + sound plugin for Flutter FFI
//
// Features:
//  - Windows Toast Notification via PowerShell (Win8+)
//  - WAV playback via PlaySound (winmm)
//  - MP3 playback via MCI (Windows Media Control Interface) - NO PowerShell overhead
//  - Taskbar flash via FlashWindowEx
//  - Ringtone loop in background thread (WAV or MP3 via MCI)

#include "bmc_notification.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <mmsystem.h>   // PlaySound, mciSendString
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>

#pragma comment(lib, "winmm.lib")

// ============================================================================
// Internal state
// ============================================================================

static char   s_appId[256]             = {0};
static BOOL   s_initialized            = FALSE;
static BOOL   s_ringtoneRunning        = FALSE;
static HANDLE s_ringtoneThread         = NULL;
static char   s_ringtonePath[MAX_PATH] = {0};
static CRITICAL_SECTION s_lock;
static BOOL   s_lockInit               = FALSE;

// MCI alias dùng cho notification sound (one-shot)
#define MCI_ALIAS_NOTIF  L"bmc_notif"
// MCI alias dùng cho ringtone (sẽ mở/đóng từng lần)
#define MCI_ALIAS_RING   L"bmc_ring"

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

static BOOL isWav(const char* path) {
    if (!path) return FALSE;
    size_t n = strlen(path);
    if (n < 4) return FALSE;
    return (_stricmp(path + n - 4, ".wav") == 0);
}

// ============================================================================
// MCI helpers (hỗ trợ cả WAV + MP3 trực tiếp, không cần PowerShell)
// ============================================================================

// Phát file âm thanh một lần qua MCI (async, không block)
static BOOL mciPlayOnce(const char* filePath, const WCHAR* alias) {
    WCHAR wPath[MAX_PATH] = {0};
    utf8ToWide(filePath, wPath, MAX_PATH);

    WCHAR cmd[MAX_PATH + 64] = {0};

    // Đóng alias cũ nếu đang mở
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"close %s", alias);
    mciSendStringW(cmd, NULL, 0, NULL);

    // Mở file — không chỉ định type, Windows tự detect (WAV, MP3, ...)
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE,
        L"open \"%s\" alias %s", wPath, alias);
    MCIERROR err = mciSendStringW(cmd, NULL, 0, NULL);
    if (err != 0) return FALSE;

    // Phát
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"play %s", alias);
    err = mciSendStringW(cmd, NULL, 0, NULL);
    return (err == 0);
}

// Dừng và đóng MCI alias
static void mciStop(const WCHAR* alias) {
    WCHAR cmd[64] = {0};
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"stop %s", alias);
    mciSendStringW(cmd, NULL, 0, NULL);
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"close %s", alias);
    mciSendStringW(cmd, NULL, 0, NULL);
}

// Kiểm tra MCI alias đã phát xong chưa (mode == "stopped")
static BOOL mciIsPlaying(const WCHAR* alias) {
    WCHAR cmd[64]    = {0};
    WCHAR status[32] = {0};
    _snwprintf_s(cmd, _countof(cmd), _TRUNCATE, L"status %s mode", alias);
    MCIERROR err = mciSendStringW(cmd, status, _countof(status), NULL);
    if (err != 0) return FALSE;
    return (wcscmp(status, L"playing") == 0);
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
    mciStop(MCI_ALIAS_NOTIF);
    mciStop(MCI_ALIAS_RING);
    if (s_lockInit) {
        DeleteCriticalSection(&s_lock);
        s_lockInit = FALSE;
    }
    s_initialized = FALSE;
}

// ============================================================================
// TOAST NOTIFICATION — PowerShell bridge (Win8+, no COM/WinRT needed)
// ============================================================================

static int showToastPS(const char* title, const char* message,
                        const char* btn1,  const char* btn2) {
    WCHAR wTitle[512]  = {0};
    WCHAR wMsg[1024]   = {0};
    WCHAR wAppId[256]  = {0};
    WCHAR wBtn1[128]   = {0};
    WCHAR wBtn2[128]   = {0};

    utf8ToWide(title,   wTitle,  512);
    utf8ToWide(message, wMsg,   1024);
    utf8ToWide(s_appId[0] ? s_appId : "BMC.Viva", wAppId, 256);
    if (btn1) utf8ToWide(btn1, wBtn1, 128);
    if (btn2) utf8ToWide(btn2, wBtn2, 128);

    // Escape single quotes in strings (replace ' with `')
    // Build XML
    WCHAR xmlToast[4096] = {0};
    if (btn1 && wBtn1[0] && btn2 && wBtn2[0]) {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template='ToastGeneric'>"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual>"
            L"<actions>"
            L"<action content='%s' arguments='btn1'/>"
            L"<action content='%s' arguments='btn2'/>"
            L"</actions></toast>",
            wTitle, wMsg, wBtn1, wBtn2);
    } else if (btn1 && wBtn1[0]) {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template='ToastGeneric'>"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual>"
            L"<actions><action content='%s' arguments='btn1'/></actions>"
            L"</toast>",
            wTitle, wMsg, wBtn1);
    } else {
        _snwprintf_s(xmlToast, 4096, _TRUNCATE,
            L"<toast><visual><binding template='ToastGeneric'>"
            L"<text>%s</text><text>%s</text>"
            L"</binding></visual></toast>",
            wTitle, wMsg);
    }

    WCHAR psCmd[8192] = {0};
    _snwprintf_s(psCmd, 8191, _TRUNCATE,
        L"powershell -WindowStyle Hidden -NonInteractive -Command \""
        L"[Windows.UI.Notifications.ToastNotificationManager,"
        L" Windows.UI.Notifications, ContentType=WindowsRuntime] | Out-Null; "
        L"[Windows.Data.Xml.Dom.XmlDocument,"
        L" Windows.Data.Xml.Dom, ContentType=WindowsRuntime] | Out-Null; "
        L"$x = New-Object Windows.Data.Xml.Dom.XmlDocument; "
        L"$x.LoadXml('%s'); "
        L"$t = [Windows.UI.Notifications.ToastNotification]::new($x); "
        L"[Windows.UI.Notifications.ToastNotificationManager]::"
        L"CreateToastNotifier('%s').Show($t)\"",
        xmlToast, wAppId
    );

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessW(NULL, psCmd, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        // Không wait — fire and forget
        CloseHandle(pi.hProcess);
        return 1;
    }
    return 0;
}

FFI_PLUGIN_EXPORT int showToastNotification(
    const char* appName, const char* title,
    const char* message, const char* iconPath)
{
    (void)appName; (void)iconPath;
    return showToastPS(title, message, NULL, NULL);
}

FFI_PLUGIN_EXPORT int showToastNotificationWithButtons(
    const char* appName, const char* title,
    const char* message, const char* button1, const char* button2)
{
    (void)appName;
    return showToastPS(title, message, button1, button2);
}

// ============================================================================
// SOUND: one-shot playback via MCI (WAV + MP3)
// ============================================================================

FFI_PLUGIN_EXPORT int playNotificationSound(const char* filePath) {
    if (!filePath) return 0;
    // WAV: dùng PlaySound cho nhanh (không cần MCI)
    if (isWav(filePath)) {
        WCHAR wPath[MAX_PATH] = {0};
        utf8ToWide(filePath, wPath, MAX_PATH);
        return PlaySoundW(wPath, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) ? 1 : 0;
    }
    // MP3 (và các định dạng khác): dùng MCI
    return mciPlayOnce(filePath, MCI_ALIAS_NOTIF) ? 1 : 0;
}

// ============================================================================
// RINGTONE LOOP (background thread)
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
        // Kiểm tra dừng
        EnterCriticalSection(&s_lock);
        BOOL run = s_ringtoneRunning;
        LeaveCriticalSection(&s_lock);
        if (!run) break;

        if (wav) {
            // WAV: SND_SYNC block thread cho đến khi xong
            PlaySoundW(wPath, NULL, SND_FILENAME | SND_SYNC | SND_NODEFAULT);
        } else {
            // MP3: mở MCI (không specify type - Windows tự detect)
            WCHAR cmdOpen[MAX_PATH + 64] = {0};
            _snwprintf_s(cmdOpen, _countof(cmdOpen), _TRUNCATE,
                L"open \"%s\" alias %s", wPath, MCI_ALIAS_RING);
            MCIERROR err = mciSendStringW(cmdOpen, NULL, 0, NULL);
            if (err == 0) {
                WCHAR cmdPlay[64] = {0};
                _snwprintf_s(cmdPlay, _countof(cmdPlay), _TRUNCATE,
                    L"play %s", MCI_ALIAS_RING);
                mciSendStringW(cmdPlay, NULL, 0, NULL);

                // Poll cho đến khi xong hoặc dừng
                while (1) {
                    Sleep(200);
                    EnterCriticalSection(&s_lock);
                    BOOL stillRun = s_ringtoneRunning;
                    LeaveCriticalSection(&s_lock);
                    if (!stillRun) {
                        mciStop(MCI_ALIAS_RING);
                        goto done;
                    }
                    if (!mciIsPlaying(MCI_ALIAS_RING)) {
                        mciStop(MCI_ALIAS_RING);
                        break; // xong 1 lần → loop lại
                    }
                }
            } else {
                Sleep(500); // open lỗi → đợi rồi thử lại
            }
        }

        // Kiểm tra lại trước khi loop
        EnterCriticalSection(&s_lock);
        BOOL run2 = s_ringtoneRunning;
        LeaveCriticalSection(&s_lock);
        if (!run2) break;

        Sleep(100); // nghỉ ngắn giữa 2 lần phát
    }
done:
    return 0;
}

FFI_PLUGIN_EXPORT int playRingtoneLoop(const char* filePath) {
    if (!filePath) return 0;
    stopRingtone(); // dừng chuông cũ nếu đang phát

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

    // Dừng ngay WAV đang phát
    PlaySoundW(NULL, NULL, SND_PURGE);
    // Dừng MCI ring
    mciStop(MCI_ALIAS_RING);

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
// TASKBAR FLASH
// ============================================================================

FFI_PLUGIN_EXPORT int flashTaskbarIcon(int64_t hwnd, int count) {
    HWND hWnd = hwnd != 0 ? (HWND)(intptr_t)hwnd : GetForegroundWindow();
    if (!hWnd) return 0;

    FLASHWINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.cbSize    = sizeof(FLASHWINFO);
    fi.hwnd      = hWnd;
    fi.dwFlags   = (count == 0) ? (FLASHW_ALL | FLASHW_TIMERNOFG) : FLASHW_ALL;
    fi.uCount    = (count == 0) ? 0 : (UINT)count;
    fi.dwTimeout = 0;
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

import 'dart:async';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

// ============================================================================
// FFI typedefs
// ============================================================================

typedef _InitNotificationNative  = Int32 Function(Pointer<Utf8> appId);
typedef _DisposeNotificationNative = Void Function();

typedef _ShowToastNative = Int32 Function(
  Pointer<Utf8> appName,
  Pointer<Utf8> title,
  Pointer<Utf8> message,
  Pointer<Utf8> iconPath,
);

typedef _ShowToastButtonsNative = Int32 Function(
  Pointer<Utf8> appName,
  Pointer<Utf8> title,
  Pointer<Utf8> message,
  Pointer<Utf8> button1,
  Pointer<Utf8> button2,
);

typedef _PlaySoundNative   = Int32 Function(Pointer<Utf8> wavPath);
typedef _PlayRingtoneNative = Int32 Function(Pointer<Utf8> wavPath);
typedef _StopRingtoneNative = Void Function();
typedef _IsRingtonePlaying  = Int32 Function();

typedef _FlashTaskbarNative     = Int32 Function(Int64 hwnd, Int32 count);
typedef _StopFlashTaskbarNative = Void Function(Int64 hwnd);

// ============================================================================
// Dart function types
// ============================================================================

typedef _InitNotification   = int Function(Pointer<Utf8> appId);
typedef _DisposeNotification = void Function();

typedef _ShowToast = int Function(
  Pointer<Utf8> appName,
  Pointer<Utf8> title,
  Pointer<Utf8> message,
  Pointer<Utf8> iconPath,
);

typedef _ShowToastButtons = int Function(
  Pointer<Utf8> appName,
  Pointer<Utf8> title,
  Pointer<Utf8> message,
  Pointer<Utf8> button1,
  Pointer<Utf8> button2,
);

typedef _PlaySound    = int Function(Pointer<Utf8> wavPath);
typedef _PlayRingtone = int Function(Pointer<Utf8> wavPath);
typedef _StopRingtone = void Function();
typedef _IsPlaying    = int Function();

typedef _FlashTaskbar     = int Function(int hwnd, int count);
typedef _StopFlashTaskbar = void Function(int hwnd);

// ============================================================================
// BmcNotification — main Dart API class
// ============================================================================

/// Native Windows notification plugin (Toast + Sound + Flash).
///
/// Usage:
/// ```dart
/// await BmcNotification.init(appId: 'com.bmctech.viva');
///
/// // Hiện toast khi nhận tin nhắn
/// await BmcNotification.showMessage(
///   title: 'VIVA',
///   message: 'Bạn có tin nhắn mới từ Nguyễn Văn A',
/// );
///
/// // Phát nhạc chuông khi có cuộc gọi đến
/// await BmcNotification.startRingtone(wavPath: 'assets/sounds/ringtone.wav');
///
/// // Dừng nhạc chuông khi nghe hoặc bỏ cuộc gọi
/// BmcNotification.stopRingtone();
/// ```
class BmcNotification {
  static DynamicLibrary? _lib;
  static bool _initialized = false;

  static _InitNotification?   _fnInit;
  static _DisposeNotification? _fnDispose;
  static _ShowToast?          _fnShowToast;
  static _ShowToastButtons?   _fnShowToastButtons;
  static _PlaySound?          _fnPlaySound;
  static _PlayRingtone?       _fnPlayRingtone;
  static _StopRingtone?       _fnStopRingtone;
  static _IsPlaying?          _fnIsPlaying;
  static _FlashTaskbar?       _fnFlashTaskbar;
  static _StopFlashTaskbar?   _fnStopFlashTaskbar;

  // ──────────────────────────────────────────────────────────────────────────
  // Init
  // ──────────────────────────────────────────────────────────────────────────

  /// Khởi tạo plugin. Phải gọi một lần khi app khởi động.
  ///
  /// [appId] dùng để định danh app với hệ điều hành,
  /// ảnh hưởng đến icon & grouping của toast notification.
  static Future<bool> init({String appId = 'com.bmctech.viva'}) async {
    if (!Platform.isWindows) return false;
    if (_initialized) return true;

    try {
      _lib = DynamicLibrary.open('bmc_notification.dll');

      _fnInit = _lib!
          .lookup<NativeFunction<_InitNotificationNative>>('initNotification')
          .asFunction();
      _fnDispose = _lib!
          .lookup<NativeFunction<_DisposeNotificationNative>>('disposeNotification')
          .asFunction();
      _fnShowToast = _lib!
          .lookup<NativeFunction<_ShowToastNative>>('showToastNotification')
          .asFunction();
      _fnShowToastButtons = _lib!
          .lookup<NativeFunction<_ShowToastButtonsNative>>('showToastNotificationWithButtons')
          .asFunction();
      _fnPlaySound = _lib!
          .lookup<NativeFunction<_PlaySoundNative>>('playNotificationSound')
          .asFunction();
      _fnPlayRingtone = _lib!
          .lookup<NativeFunction<_PlayRingtoneNative>>('playRingtoneLoop')
          .asFunction();
      _fnStopRingtone = _lib!
          .lookup<NativeFunction<_StopRingtoneNative>>('stopRingtone')
          .asFunction();
      _fnIsPlaying = _lib!
          .lookup<NativeFunction<_IsRingtonePlaying>>('isRingtonePlaying')
          .asFunction();
      _fnFlashTaskbar = _lib!
          .lookup<NativeFunction<_FlashTaskbarNative>>('flashTaskbarIcon')
          .asFunction<_FlashTaskbar>();
      _fnStopFlashTaskbar = _lib!
          .lookup<NativeFunction<_StopFlashTaskbarNative>>('stopFlashTaskbarIcon')
          .asFunction<_StopFlashTaskbar>();

      final pAppId = appId.toNativeUtf8();
      try {
        _fnInit!(pAppId);
      } finally {
        calloc.free(pAppId);
      }

      _initialized = true;
      return true;
    } catch (e) {
      return false;
    }
  }

  /// Giải phóng tài nguyên khi app thoát.
  static void dispose() {
    if (!_initialized) return;
    _fnDispose?.call();
    _initialized = false;
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Toast helpers
  // ──────────────────────────────────────────────────────────────────────────

  /// Hiện toast thông báo tin nhắn mới.
  ///
  /// [soundPath] — đường dẫn file .wav để phát kèm thông báo (tuyệt đối).
  /// Nếu để null, không phát âm thanh.
  static Future<bool> showMessage({
    required String title,
    required String message,
    String? soundPath,
    bool flashTaskbar = true,
  }) async {
    if (!_initialized) return false;
    final ok = _showToast(title: title, message: message);
    if (ok && soundPath != null) {
      playMessageSound(wavPath: soundPath);
    }
    if (flashTaskbar) {
      BmcNotification.flashTaskbar(count: 5);
    }
    return ok;
  }

  /// Hiện toast thông báo cuộc gọi đến với 2 nút "Nghe máy" / "Từ chối".
  ///
  /// [ringtonePath] — đường dẫn file .wav. Phát lặp cho đến khi
  /// gọi [stopRingtone].
  static Future<bool> showIncomingCall({
    required String callerName,
    String? ringtonePath,
    String acceptLabel = 'Nghe máy',
    String rejectLabel = 'Từ chối',
    bool flashTaskbar = true,
  }) async {
    if (!_initialized) return false;
    final ok = _showToastWithButtons(
      title: 'Cuộc gọi đến',
      message: callerName,
      button1: acceptLabel,
      button2: rejectLabel,
    );
    if (ringtonePath != null) {
      startRingtone(wavPath: ringtonePath);
    }
    if (flashTaskbar) {
      BmcNotification.flashTaskbar(count: 0); // 0 = liên tục
    }
    return ok;
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Sound
  // ──────────────────────────────────────────────────────────────────────────

  /// Phát âm thanh thông báo tin nhắn một lần.
  static bool playMessageSound({required String wavPath}) {
    if (!_initialized || _fnPlaySound == null) return false;
    final p = wavPath.toNativeUtf8();
    try {
      return _fnPlaySound!(p) == 1;
    } finally {
      calloc.free(p);
    }
  }

  /// Bắt đầu phát nhạc chuông cuộc gọi (lặp liên tục).
  static bool startRingtone({required String wavPath}) {
    if (!_initialized || _fnPlayRingtone == null) return false;
    final p = wavPath.toNativeUtf8();
    try {
      return _fnPlayRingtone!(p) == 1;
    } finally {
      calloc.free(p);
    }
  }

  /// Dừng nhạc chuông.
  static void stopRingtone() {
    if (!_initialized) return;
    _fnStopRingtone?.call();
  }

  /// Kiểm tra nhạc chuông đang phát không.
  static bool get isRingtonePlaying {
    if (!_initialized || _fnIsPlaying == null) return false;
    return _fnIsPlaying!() == 1;
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Taskbar flash
  // ──────────────────────────────────────────────────────────────────────────

  /// Nhấp nháy icon taskbar.
  /// [count] = 0 nghĩa là nhấp nháy liên tục cho đến khi app được focus.
  static bool flashTaskbar({int hwnd = 0, int count = 5}) {
    if (!_initialized || _fnFlashTaskbar == null) return false;
    return _fnFlashTaskbar!(hwnd, count) == 1;
  }

  /// Dừng nhấp nháy taskbar.
  static void stopFlashTaskbar({int hwnd = 0}) {
    if (!_initialized) return;
    _fnStopFlashTaskbar?.call(hwnd);
  }

  // ──────────────────────────────────────────────────────────────────────────
  // Internal
  // ──────────────────────────────────────────────────────────────────────────

  static bool _showToast({required String title, required String message}) {
    if (_fnShowToast == null) return false;
    final pApp  = 'VIVA'.toNativeUtf8();
    final pTitle = title.toNativeUtf8();
    final pMsg  = message.toNativeUtf8();
    final pIcon = ''.toNativeUtf8();
    try {
      return _fnShowToast!(pApp, pTitle, pMsg, pIcon) == 1;
    } finally {
      calloc.free(pApp);
      calloc.free(pTitle);
      calloc.free(pMsg);
      calloc.free(pIcon);
    }
  }

  static bool _showToastWithButtons({
    required String title,
    required String message,
    required String button1,
    required String button2,
  }) {
    if (_fnShowToastButtons == null) return false;
    final pApp  = 'VIVA'.toNativeUtf8();
    final pTitle = title.toNativeUtf8();
    final pMsg  = message.toNativeUtf8();
    final pBtn1 = button1.toNativeUtf8();
    final pBtn2 = button2.toNativeUtf8();
    try {
      return _fnShowToastButtons!(pApp, pTitle, pMsg, pBtn1, pBtn2) == 1;
    } finally {
      calloc.free(pApp);
      calloc.free(pTitle);
      calloc.free(pMsg);
      calloc.free(pBtn1);
      calloc.free(pBtn2);
    }
  }
}

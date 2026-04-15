# bmc_notification

Flutter plugin cung cấp **Windows native notifications** bao gồm:

- 🔔 **Toast Notification** — Thông báo góc màn hình (WinRT via PowerShell)
- 🎵 **Sound Playback** — Phát âm thanh `.wav` (PlaySound) và `.mp3` (WMP)
- 📞 **Ringtone Loop** — Nhạc chuông lặp vô hạn, dừng tức thì
- 💡 **Taskbar Flash** — Nhấp nháy icon trên taskbar (FlashWindowEx)

---

## 📦 Cài đặt

```yaml
dependencies:
  bmc_notification:
    git:
      url: https://github.com/tgh1701/bmc_notification.git
      ref: main
```

---

## 🚀 Sử dụng nhanh

```dart
import 'package:bmc_notification/bmc_notification.dart';

// 1. Khởi tạo (gọi 1 lần trong main())
await BmcNotification.init(appId: 'com.myapp.viva');

// 2. Hiện toast tin nhắn
await BmcNotification.showMessage(
  title: 'Nguyen Van A',
  message: 'Bạn có tin nhắn mới',
  soundPath: '/path/to/notification.wav', // optional
  flashTaskbar: true,
);

// 3. Hiện toast cuộc gọi + phát nhạc chuông
await BmcNotification.showIncomingCall(
  callerName: 'Tran Thi B',
  ringtonePath: '/path/to/ringtone.wav', // optional
  acceptLabel: 'Nghe máy',
  rejectLabel: 'Từ chối',
  flashTaskbar: true,
);

// 4. Phát âm thanh 1 lần
BmcNotification.playMessageSound(wavPath: '/path/to/sound.wav');

// 5. Nhạc chuông lặp
BmcNotification.startRingtone(wavPath: '/path/to/ring.wav');

// 6. Dừng nhạc chuông
BmcNotification.stopRingtone();

// 7. Flash taskbar
BmcNotification.flashTaskbar(count: 5);   // 5 lần
BmcNotification.flashTaskbar(count: 0);   // Vô hạn (đến khi focus)
BmcNotification.stopFlashTaskbar();

// 8. Kiểm tra nhạc chuông
bool playing = BmcNotification.isRingtonePlaying;

// 9. Giải phóng khi app thoát
BmcNotification.dispose();
```

---

## 📁 Sử dụng với Flutter Assets

> Vì Windows FFI cần đường dẫn tuyệt đối, bạn cần giải nén assets ra thư mục tạm trước.

```dart
import 'dart:io';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';

Future<String> extractAsset(String assetPath) async {
  final data = await rootBundle.load(assetPath);
  final bytes = data.buffer.asUint8List();
  final dir = await getTemporaryDirectory();
  final file = File('${dir.path}/${assetPath.split('/').last}');
  await file.writeAsBytes(bytes, flush: true);
  return file.path;
}

// Sử dụng
final soundPath = await extractAsset('assets/sounds/notification.wav');
BmcNotification.playMessageSound(wavPath: soundPath);
```

---

## 🛠️ Build DLL (lần đầu tiên)

```bash
# Trong thư mục project Flutter dùng plugin này
flutter build windows
```

DLL `bmc_notification.dll` sẽ tự động được build và copy vào `build\windows\x64\runner\Release\`.

---

## 🖥️ Requirements

| | |
|---|---|
| **OS** | Windows 10+ (64-bit) |
| **Flutter** | >= 3.7.0 |
| **Dart** | >= 3.0.0 |

---

## 📂 Cấu trúc Plugin

```
bmc_notification/
├── src/
│   ├── bmc_notification.h     ← C API header
│   └── bmc_notification.c     ← Native Windows implementation
├── windows/
│   └── CMakeLists.txt         ← Windows build config
├── lib/
│   └── bmc_notification.dart  ← Dart FFI wrapper
└── example/                   ← Demo app đầy đủ
    └── lib/main.dart
```

---

## 📖 API Reference

| Method | Mô tả |
|--------|-------|
| `init({appId})` | Khởi tạo plugin. Gọi trước các method khác |
| `showMessage({title, message, soundPath?, flashTaskbar})` | Hiện toast tin nhắn |
| `showIncomingCall({callerName, ringtonePath?, acceptLabel, rejectLabel, flashTaskbar})` | Hiện toast cuộc gọi |
| `playMessageSound({wavPath})` | Phát âm thanh 1 lần (WAV/MP3) |
| `startRingtone({wavPath})` | Nhạc chuông lặp (WAV/MP3) |
| `stopRingtone()` | Dừng nhạc chuông |
| `isRingtonePlaying` | `true` nếu đang phát nhạc chuông |
| `flashTaskbar({count})` | Flash taskbar (0 = vô hạn) |
| `stopFlashTaskbar()` | Dừng flash |
| `dispose()` | Giải phóng tài nguyên native |

---

## 📄 License

MIT

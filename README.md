# bmc_notification

Plugin Flutter FFI cho Windows cung cấp:

- ✅ **Toast Notification** — hiển thị thông báo góc phải màn hình (Windows Action Center)
- ✅ **Notification Sound** — phát âm thanh `.wav` khi có tin nhắn
- ✅ **Ringtone Loop** — phát nhạc chuông lặp liên tục khi có cuộc gọi đến
- ✅ **Taskbar Flash** — nhấp nháy icon taskbar để thu hút sự chú ý

## Cài đặt

### pubspec.yaml

```yaml
dependencies:
  bmc_notification:
    git:
      url: https://github.com/tgh1701/bmc_notification.git
      ref: main
```

## Sử dụng

### Khởi tạo (1 lần khi app start)

```dart
import 'package:bmc_notification/bmc_notification.dart';

// Trong main() hoặc initState() của widget root
await BmcNotification.init(appId: 'com.bmctech.viva');
```

### Thông báo tin nhắn mới

```dart
await BmcNotification.showMessage(
  title: 'VIVA',
  message: 'Bạn có tin nhắn mới từ Nguyễn Văn A',
  soundPath: r'C:\path\to\message.wav',  // hoặc absolute path từ assets
  flashTaskbar: true,
);
```

### Cuộc gọi đến

```dart
// Hiện toast + phát nhạc chuông
await BmcNotification.showIncomingCall(
  callerName: 'Nguyễn Văn A đang gọi...',
  ringtonePath: r'C:\path\to\ringtone.wav',
  acceptLabel: 'Nghe máy',
  rejectLabel: 'Từ chối',
);

// Khi nghe máy / kết thúc cuộc gọi
BmcNotification.stopRingtone();
BmcNotification.stopFlashTaskbar();
```

### API đầy đủ

| Hàm | Mô tả |
|-----|-------|
| `BmcNotification.init(appId)` | Khởi tạo plugin |
| `BmcNotification.showMessage(...)` | Toast + Sound tin nhắn |
| `BmcNotification.showIncomingCall(...)` | Toast + Ringtone cuộc gọi |
| `BmcNotification.playMessageSound(wavPath)` | Phát âm thanh 1 lần |
| `BmcNotification.startRingtone(wavPath)` | Bắt đầu nhạc chuông lặp |
| `BmcNotification.stopRingtone()` | Dừng nhạc chuông |
| `BmcNotification.isRingtonePlaying` | Kiểm tra ringtone |
| `BmcNotification.flashTaskbar(count)` | Nhấp nháy taskbar |
| `BmcNotification.stopFlashTaskbar()` | Dừng nhấp nháy |
| `BmcNotification.dispose()` | Giải phóng khi thoát app |

## Yêu cầu

- Windows 10 trở lên
- Flutter 3.7+
- Đặt file `.wav` vào assets và lấy đường dẫn tuyệt đối khi gọi

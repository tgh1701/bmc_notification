#ifndef BMC_NOTIFICATION_H
#define BMC_NOTIFICATION_H

#include <stdint.h>

#ifdef _WIN32
  #define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define FFI_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TOAST NOTIFICATION API
// Hiển thị toast notification góc phải màn hình Windows (WinRT)
// ============================================================================

/**
 * Hiển thị toast notification.
 * @param appName    Tên ứng dụng (VD: "VIVA")
 * @param title      Tiêu đề thông báo
 * @param message    Nội dung thông báo
 * @param iconPath   Đường dẫn đến file icon .ico (có thể NULL)
 * @return 1 nếu thành công, 0 nếu lỗi
 */
FFI_PLUGIN_EXPORT int showToastNotification(
    const char* appName,
    const char* title,
    const char* message,
    const char* iconPath
);

/**
 * Hiển thị toast notification với nút bấm.
 * @param appName    Tên ứng dụng
 * @param title      Tiêu đề
 * @param message    Nội dung
 * @param button1    Nhãn nút 1 (VD: "Trả lời") - có thể NULL
 * @param button2    Nhãn nút 2 (VD: "Bỏ qua") - có thể NULL
 * @return 1 nếu thành công, 0 nếu lỗi
 */
FFI_PLUGIN_EXPORT int showToastNotificationWithButtons(
    const char* appName,
    const char* title,
    const char* message,
    const char* button1,
    const char* button2
);

// ============================================================================
// SOUND API
// Phát âm thanh thông báo
// ============================================================================

/**
 * Phát file âm thanh (.wav) một lần.
 * @param wavFilePath  Đường dẫn đến file .wav
 * @return 1 nếu thành công, 0 nếu lỗi
 */
FFI_PLUGIN_EXPORT int playNotificationSound(const char* wavFilePath);

/**
 * Phát âm thanh lặp lại (dùng cho ringtone cuộc gọi đến).
 * @param wavFilePath  Đường dẫn đến file .wav
 * @return 1 nếu thành công, 0 nếu lỗi
 */
FFI_PLUGIN_EXPORT int playRingtoneLoop(const char* wavFilePath);

/**
 * Dừng phát âm thanh ringtone (dừng vòng lặp).
 */
FFI_PLUGIN_EXPORT void stopRingtone(void);

/**
 * Kiểm tra xem ringtone có đang phát không.
 * @return 1 nếu đang phát, 0 nếu không
 */
FFI_PLUGIN_EXPORT int isRingtonePlaying(void);

// ============================================================================
// FLASH TASKBAR API
// Nhấp nháy icon taskbar khi có thông báo
// ============================================================================

/**
 * Nhấp nháy icon taskbar của app để thu hút sự chú ý.
 * @param hwnd     Handle cửa sổ (dùng 0 để dùng cửa sổ hiện tại)
 * @param count    Số lần nhấp nháy (0 = liên tục cho đến khi focus)
 * @return 1 nếu thành công
 */
FFI_PLUGIN_EXPORT int flashTaskbarIcon(int64_t hwnd, int count);

/**
 * Dừng nhấp nháy taskbar.
 * @param hwnd  Handle cửa sổ
 */
FFI_PLUGIN_EXPORT void stopFlashTaskbarIcon(int64_t hwnd);

// ============================================================================
// INIT / CLEANUP
// ============================================================================

/**
 * Khởi tạo notification module.
 * Phải gọi trước khi sử dụng bất kỳ hàm nào.
 * @param appId  App ID duy nhất (VD: "com.bmctech.viva")
 * @return 1 nếu thành công
 */
FFI_PLUGIN_EXPORT int initNotification(const char* appId);

/**
 * Dọn dẹp tài nguyên.
 */
FFI_PLUGIN_EXPORT void disposeNotification(void);

#ifdef __cplusplus
}
#endif

#endif // BMC_NOTIFICATION_H

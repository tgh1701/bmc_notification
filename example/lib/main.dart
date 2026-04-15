import 'dart:io';
import 'package:flutter/material.dart';
import 'package:bmc_notification/bmc_notification.dart';
import 'package:path_provider/path_provider.dart';
import 'package:flutter/services.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Khởi tạo plugin một lần khi app khởi động
  if (Platform.isWindows) {
    await BmcNotification.init(appId: 'com.bmctech.bmc_notification_example');
  }

  runApp(const BmcNotificationDemoApp());
}

// ============================================================================
// APP ROOT
// ============================================================================

class BmcNotificationDemoApp extends StatelessWidget {
  const BmcNotificationDemoApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BMC Notification Demo',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true).copyWith(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF0066FF),
          brightness: Brightness.dark,
        ),
        scaffoldBackgroundColor: const Color(0xFF0A0E1A),
        cardTheme: CardTheme(
          color: const Color(0xFF141928),
          elevation: 0,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
            side: BorderSide(color: Colors.white.withOpacity(0.08)),
          ),
        ),
      ),
      home: const DemoHomePage(),
    );
  }
}

// ============================================================================
// DEMO HOME PAGE
// ============================================================================

class DemoHomePage extends StatefulWidget {
  const DemoHomePage({super.key});

  @override
  State<DemoHomePage> createState() => _DemoHomePageState();
}

class _DemoHomePageState extends State<DemoHomePage> {
  // Sound paths (giải nén từ assets sang thư mục tạm)
  String? _notificationSoundPath;
  String? _ringtonePath;

  bool _pluginReady = false;
  bool _ringtoneActive = false;
  String _status = 'Khởi tạo...';

  @override
  void initState() {
    super.initState();
    _initPlugin();
  }

  // ──────────────────────────────────────────────────────────────────────────

  Future<void> _initPlugin() async {
    if (!Platform.isWindows) {
      setState(() => _status = 'Chỉ hỗ trợ Windows');
      return;
    }

    try {
      // Giải nén WAV từ assets
      _notificationSoundPath = await _extractAsset('assets/sounds/notification.wav');
      _ringtonePath = await _extractAsset('assets/sounds/ringtone.wav');

      setState(() {
        _pluginReady = true;
        _status = '✅ Plugin đã sẵn sàng';
      });
    } catch (e) {
      setState(() => _status = '❌ Lỗi: $e');
    }
  }

  Future<String?> _extractAsset(String assetPath) async {
    try {
      final data = await rootBundle.load(assetPath);
      final bytes = data.buffer.asUint8List();
      final dir = await getTemporaryDirectory();
      final fileName = assetPath.split('/').last;
      final file = File('${dir.path}/bmc_demo_$fileName');
      await file.writeAsBytes(bytes, flush: true);
      return file.path;
    } catch (e) {
      return null;
    }
  }

  // ──────────────────────────────────────────────────────────────────────────
  // ACTION HANDLERS
  // ──────────────────────────────────────────────────────────────────────────

  Future<void> _showMessageToast() async {
    final ok = await BmcNotification.showMessage(
      title: 'VIVA',
      message: 'Nguyễn Văn A: Bạn có thời gian họp lúc 3h không? 📅',
      soundPath: _notificationSoundPath,
      flashTaskbar: true,
    );
    _setStatus(ok ? '✅ Toast tin nhắn đã hiện' : '❌ Toast thất bại');
  }

  Future<void> _showCallToast() async {
    final ok = await BmcNotification.showIncomingCall(
      callerName: 'Trần Thị B đang gọi...',
      ringtonePath: _ringtonePath,
      acceptLabel: 'Nghe máy',
      rejectLabel: 'Từ chối',
      flashTaskbar: true,
    );
    setState(() {
      _ringtoneActive = BmcNotification.isRingtonePlaying;
      _status = ok ? '✅ Cuộc gọi đến + nhạc chuông' : '❌ Toast thất bại';
    });
  }

  void _playNotificationSound() {
    if (_notificationSoundPath != null) {
      final ok = BmcNotification.playMessageSound(wavPath: _notificationSoundPath!);
      _setStatus(ok ? '🔔 Đang phát âm thanh tin nhắn' : '❌ Phát âm thanh thất bại');
    }
  }

  void _startRingtone() {
    if (_ringtonePath != null) {
      final ok = BmcNotification.startRingtone(wavPath: _ringtonePath!);
      setState(() {
        _ringtoneActive = ok;
        _status = ok ? '📞 Nhạc chuông đang phát (lặp)' : '❌ Không phát được';
      });
    }
  }

  void _stopRingtone() {
    BmcNotification.stopRingtone();
    BmcNotification.stopFlashTaskbar();
    setState(() {
      _ringtoneActive = false;
      _status = '🔇 Nhạc chuông đã dừng';
    });
  }

  void _flashTaskbar() {
    final ok = BmcNotification.flashTaskbar(count: 5);
    _setStatus(ok ? '💡 Taskbar đang nhấp nháy (5 lần)' : '❌ Flash thất bại');
  }

  void _flashTaskbarInfinite() {
    final ok = BmcNotification.flashTaskbar(count: 0); // 0 = vô hạn
    _setStatus(ok ? '💡 Taskbar nhấp nháy cho đến khi focus' : '❌ Flash thất bại');
  }

  void _stopFlash() {
    BmcNotification.stopFlashTaskbar();
    _setStatus('✅ Dừng nhấp nháy taskbar');
  }

  void _checkRingtone() {
    final playing = BmcNotification.isRingtonePlaying;
    setState(() {
      _ringtoneActive = playing;
      _status = playing ? '📞 Nhạc chuông: ĐANG PHÁT' : '🔇 Nhạc chuông: ĐÃ DỪNG';
    });
  }

  void _setStatus(String msg) => setState(() => _status = msg);

  @override
  void dispose() {
    BmcNotification.dispose();
    super.dispose();
  }

  // ──────────────────────────────────────────────────────────────────────────
  // BUILD
  // ──────────────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // ── HEADER ──────────────────────────────────────────────────
              _buildHeader(),
              const SizedBox(height: 8),

              // ── STATUS BAR ──────────────────────────────────────────────
              _buildStatusBar(),
              const SizedBox(height: 24),

              // ── SECTION: TOAST ──────────────────────────────────────────
              _buildSectionLabel('🔔  Toast Notification'),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.chat_bubble_rounded,
                      label: 'Tin nhắn mới',
                      subtitle: 'Toast + Sound + Flash',
                      color: const Color(0xFF0066FF),
                      onTap: _pluginReady ? _showMessageToast : null,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.phone_in_talk_rounded,
                      label: 'Cuộc gọi đến',
                      subtitle: 'Toast + Ringtone + Flash',
                      color: const Color(0xFF00C851),
                      onTap: _pluginReady ? _showCallToast : null,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 24),

              // ── SECTION: SOUND ──────────────────────────────────────────
              _buildSectionLabel('🎵  Âm Thanh'),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.notifications_rounded,
                      label: 'Âm tin nhắn',
                      subtitle: 'Phát 1 lần',
                      color: const Color(0xFFFF9100),
                      onTap: _pluginReady ? _playNotificationSound : null,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.ring_volume_rounded,
                      label: 'Nhạc chuông',
                      subtitle: 'Phát lặp liên tục',
                      color: const Color(0xFF9C27B0),
                      onTap: _pluginReady && !_ringtoneActive ? _startRingtone : null,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.stop_circle_rounded,
                      label: 'Dừng chuông',
                      subtitle: _ringtoneActive ? '● ĐANG PHÁT' : 'Đã dừng',
                      color: const Color(0xFFFF3D00),
                      onTap: _ringtoneActive ? _stopRingtone : null,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 24),

              // ── SECTION: TASKBAR ─────────────────────────────────────────
              _buildSectionLabel('🖥️  Taskbar Flash'),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.flare_rounded,
                      label: 'Flash 5 lần',
                      subtitle: 'Nhấp nháy có giới hạn',
                      color: const Color(0xFF00BCD4),
                      onTap: _pluginReady ? _flashTaskbar : null,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.all_inclusive_rounded,
                      label: 'Flash vô hạn',
                      subtitle: 'Đến khi focus vào app',
                      color: const Color(0xFFE91E63),
                      onTap: _pluginReady ? _flashTaskbarInfinite : null,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _DemoButton(
                      icon: Icons.stop_rounded,
                      label: 'Dừng flash',
                      subtitle: 'Tắt nhấp nháy',
                      color: const Color(0xFF607D8B),
                      onTap: _pluginReady ? _stopFlash : null,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 24),

              // ── SECTION: STATUS ──────────────────────────────────────────
              _buildSectionLabel('ℹ️  Trạng Thái'),
              const SizedBox(height: 12),
              _DemoButton(
                icon: Icons.info_outline_rounded,
                label: 'Kiểm tra nhạc chuông',
                subtitle: 'Đang phát hay đã dừng?',
                color: const Color(0xFF78909C),
                onTap: _checkRingtone,
              ),
              const SizedBox(height: 32),

              // ── API REFERENCE ────────────────────────────────────────────
              _buildApiCard(),
            ],
          ),
        ),
      ),
    );
  }

  // ──────────────────────────────────────────────────────────────────────────
  // WIDGETS
  // ──────────────────────────────────────────────────────────────────────────

  Widget _buildHeader() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  colors: [Color(0xFF0066FF), Color(0xFF00C4FF)],
                ),
                borderRadius: BorderRadius.circular(12),
              ),
              child: const Icon(Icons.notifications_active_rounded,
                  color: Colors.white, size: 28),
            ),
            const SizedBox(width: 14),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'BMC Notification',
                  style: TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.bold,
                    color: Colors.white,
                  ),
                ),
                Text(
                  'Windows FFI Plugin Demo',
                  style: TextStyle(
                    fontSize: 13,
                    color: Colors.white.withOpacity(0.5),
                  ),
                ),
              ],
            ),
          ],
        ),
      ],
    );
  }

  Widget _buildStatusBar() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: Colors.white.withOpacity(0.06),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: Colors.white.withOpacity(0.1)),
      ),
      child: Row(
        children: [
          AnimatedContainer(
            duration: const Duration(milliseconds: 300),
            width: 8,
            height: 8,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: _pluginReady ? const Color(0xFF00C851) : Colors.orange,
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              _status,
              style: const TextStyle(fontSize: 13, color: Colors.white70),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSectionLabel(String label) {
    return Text(
      label,
      style: const TextStyle(
        fontSize: 14,
        fontWeight: FontWeight.w600,
        color: Colors.white54,
        letterSpacing: 0.5,
      ),
    );
  }

  Widget _buildApiCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'API Quick Reference',
              style: TextStyle(
                fontSize: 15,
                fontWeight: FontWeight.bold,
                color: Colors.white,
              ),
            ),
            const SizedBox(height: 16),
            _buildApiRow('init(appId)', 'Khởi tạo plugin (gọi 1 lần)'),
            _buildApiRow('showMessage(title, message, soundPath)', 'Toast tin nhắn'),
            _buildApiRow('showIncomingCall(callerName, ringtonePath)', 'Toast cuộc gọi'),
            _buildApiRow('playMessageSound(wavPath)', 'Phát âm thanh 1 lần'),
            _buildApiRow('startRingtone(wavPath)', 'Nhạc chuông lặp'),
            _buildApiRow('stopRingtone()', 'Dừng nhạc chuông'),
            _buildApiRow('isRingtonePlaying', 'Kiểm tra trạng thái'),
            _buildApiRow('flashTaskbar(count)', 'Nhấp nháy taskbar'),
            _buildApiRow('stopFlashTaskbar()', 'Dừng nhấp nháy'),
            _buildApiRow('dispose()', 'Giải phóng tài nguyên'),
          ],
        ),
      ),
    );
  }

  Widget _buildApiRow(String method, String desc) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
            decoration: BoxDecoration(
              color: const Color(0xFF0066FF).withOpacity(0.15),
              borderRadius: BorderRadius.circular(4),
              border: Border.all(
                color: const Color(0xFF0066FF).withOpacity(0.3),
              ),
            ),
            child: Text(
              method,
              style: const TextStyle(
                fontSize: 11,
                fontFamily: 'monospace',
                color: Color(0xFF80AAFF),
              ),
            ),
          ),
          const SizedBox(width: 10),
          Expanded(
            child: Padding(
              padding: const EdgeInsets.only(top: 3),
              child: Text(
                desc,
                style: const TextStyle(fontSize: 12, color: Colors.white54),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

// ============================================================================
// DEMO BUTTON WIDGET
// ============================================================================

class _DemoButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final String subtitle;
  final Color color;
  final VoidCallback? onTap;

  const _DemoButton({
    required this.icon,
    required this.label,
    required this.subtitle,
    required this.color,
    this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final disabled = onTap == null;

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        borderRadius: BorderRadius.circular(16),
        child: Ink(
          padding: const EdgeInsets.all(16),
          decoration: BoxDecoration(
            gradient: disabled
                ? null
                : LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [
                      color.withOpacity(0.15),
                      color.withOpacity(0.05),
                    ],
                  ),
            color: disabled ? Colors.white.withOpacity(0.03) : null,
            borderRadius: BorderRadius.circular(16),
            border: Border.all(
              color: disabled
                  ? Colors.white.withOpacity(0.06)
                  : color.withOpacity(0.4),
            ),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Icon(
                icon,
                color: disabled ? Colors.white24 : color,
                size: 28,
              ),
              const SizedBox(height: 10),
              Text(
                label,
                style: TextStyle(
                  fontSize: 13,
                  fontWeight: FontWeight.w600,
                  color: disabled ? Colors.white24 : Colors.white,
                ),
              ),
              const SizedBox(height: 4),
              Text(
                subtitle,
                style: TextStyle(
                  fontSize: 11,
                  color: disabled ? Colors.white12 : Colors.white38,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

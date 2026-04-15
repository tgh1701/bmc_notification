cmake_minimum_required(VERSION 3.14)

project(bmc_notification_library VERSION 0.0.1 LANGUAGES C)

# ============================================================================
# Native library
# ============================================================================

add_library(bmc_notification SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/bmc_notification.c"
)

# C99 — MSVC compatible
target_compile_features(bmc_notification PRIVATE c_std_99)

target_compile_definitions(bmc_notification PRIVATE
  UNICODE
  _UNICODE
  WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0A00    # Windows 10+
  _CRT_SECURE_NO_WARNINGS
)

# Only what we actually use:
#   winmm   → PlaySound, PlaySoundW
#   user32  → FlashWindowEx, GetForegroundWindow
#   kernel32 → CreateProcess, etc. (auto-linked)
target_link_libraries(bmc_notification PRIVATE
  winmm
  user32
)

# ============================================================================
# Install rule — copy DLL next to the Flutter runner
# ============================================================================
install(
  TARGETS bmc_notification
  RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}"
)

# Tell Flutter about the bundled library
set(bmc_notification_bundled_libraries
  "$<TARGET_FILE:bmc_notification>"
  PARENT_SCOPE
)

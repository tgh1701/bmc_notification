cmake_minimum_required(VERSION 3.14)

# Plugin project
project(bmc_notification_library VERSION 0.0.1 LANGUAGES C)

# ============================================================================
# Native library
# ============================================================================

set(SOURCE_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/bmc_notification.c"
)

add_library(bmc_notification SHARED ${SOURCE_FILES})

# Windows SDK & subsystem
target_compile_definitions(bmc_notification PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00   # Windows 10
)

target_link_libraries(bmc_notification PRIVATE
  winmm       # PlaySound
  ole32       # COM
  shell32     # Shell
  propsys     # PropVariant
  shlwapi     # StrCpy helpers
  combase     # RoActivateInstance (WinRT)
  runtimeobject  # Windows Runtime
)

# ============================================================================
# Install rule: copy DLL next to the Flutter runner
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

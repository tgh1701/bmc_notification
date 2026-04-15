cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME "bmc_notification")
project(${PROJECT_NAME} LANGUAGES CXX C)

cmake_policy(VERSION 3.14...3.25)

set(PLUGIN_NAME "bmc_notification_plugin")

# ============================================================================
# Build native library from src/
# ============================================================================
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../src" "${CMAKE_CURRENT_BINARY_DIR}/src")

# ============================================================================
# Expose bundled DLLs to Flutter build system
# ============================================================================
set(bmc_notification_bundled_libraries
  "$<TARGET_FILE:bmc_notification>"
  PARENT_SCOPE
)

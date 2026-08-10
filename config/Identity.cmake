# Central project identity and user-visible strings.
# Keep technical target identifiers such as "macos", "linux", and "tab5"
# stable unless build tooling and CI are intentionally being migrated.

set(TABOS_CMAKE_PROJECT_NAME "TabOS")
set(TABOS_PROJECT_VERSION "0.0.1")
set(TABOS_RUNTIME_VERSION "${TABOS_PROJECT_VERSION}-dev")

set(TABOS_SYSTEM_NAME "TabOS")
set(TABOS_HOST_WINDOW_TITLE "${TABOS_SYSTEM_NAME} Host")
set(TABOS_HOST_PREFERENCES_ORGANIZATION "${TABOS_SYSTEM_NAME}")
set(TABOS_HOST_PREFERENCES_APPLICATION "${TABOS_HOST_WINDOW_TITLE}")
set(TABOS_HOST_WINDOW_STATE_FILENAME "window-position.txt")

set(TABOS_TARGET_NAME_MACOS "macos")
set(TABOS_TARGET_NAME_LINUX "linux")
set(TABOS_TARGET_NAME_TAB5 "tab5")

set(TABOS_SYSTEM_LOG_TAG "tabos")
set(TABOS_PLATFORM_LOG_TAG "tabos_platform")

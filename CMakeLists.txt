cmake_minimum_required(VERSION 3.20)
project(LiteBrowser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization and warning flags
if (MSVC)
    add_compile_options(/W4 /EHsc /O2 /Gy /GL)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
else()
    add_compile_options(-Wall -Wextra -O3 -ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections -static)
endif()

# Include directories
include_directories(
    src
    third_party/sqlite
    third_party/webview2/include
)

# Core sources (shared between main app and tests)
set(CORE_SOURCES
    third_party/sqlite/sqlite3.c
    src/storage/StorageManager.cpp
    src/adblock/RuleParser.cpp
    src/adblock/AdBlockEngine.cpp
    src/reader/ReaderModeEngine.cpp
    src/navigation/InternalPages.cpp
    src/bookmarks/BookmarkImporter.cpp
)

# Main Application
add_executable(LiteBrowser WIN32
    ${CORE_SOURCES}
    src/app/main.cpp
    src/tabs/Tab.cpp
    src/tabs/TabManager.cpp
    src/ui/ThemeManager.cpp
    src/ui/ShieldPopup.cpp
    src/ui/FindBar.cpp
    src/ui/BrowserWindow.cpp
)

target_link_libraries(LiteBrowser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2Loader.dll.lib
    comctl32
    dwmapi
    shlwapi
    ole32
    oleaut32
    uuid
)

# Ensure WebView2Loader.dll is copied to the target output directory
add_custom_command(TARGET LiteBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2Loader.dll"
    $<TARGET_FILE_DIR:LiteBrowser>
)

# Automated Test Suite
enable_testing()
add_executable(LiteBrowserTests
    ${CORE_SOURCES}
    tests/TestRunner.cpp
)

target_link_libraries(LiteBrowserTests PRIVATE
    shlwapi
    ole32
    oleaut32
    uuid
)

add_test(NAME UnitTests COMMAND LiteBrowserTests)

# Installer and Uninstaller
add_executable(LiteBrowser-Setup-x64 WIN32
    installer/Installer.cpp
)
target_link_libraries(LiteBrowser-Setup-x64 PRIVATE
    shlwapi
    ole32
    uuid
)

add_executable(Uninstall WIN32
    installer/Uninstaller.cpp
)
target_link_libraries(Uninstall PRIVATE
    shlwapi
    ole32
    uuid
)

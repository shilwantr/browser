# Build Guide

This document describes how to configure, build, test, and package LITE Browser on Windows.

## System Requirements

- **Operating System**: Windows 10 (version 1809+) or Windows 11 x64
- **Runtime**: Microsoft WebView2 Evergreen Runtime (pre-installed on Windows 10/11)
- **Compilers Supported**:
  - Clang / LLVM (LLVM-MinGW / UCRT) [Recommended]
  - Visual Studio 2022 C++ (MSVC 19.30+)
- **Build Tools**:
  - CMake 3.20 or newer
  - Ninja build system (or MSBuild / Visual Studio generator)

## Directory Structure

```text
├── CMakeLists.txt        # Top-level CMake configuration
├── CMakePresets.json     # Configuration and build presets
├── src/                  # Application source code
├── tests/                # Automated unit test suite
├── third_party/          # Dependencies (SQLite amalgamation, WebView2 SDK)
├── installer/            # Native Win32 installer and uninstaller
├── docs/                 # Technical documentation
└── build/                # Build output directory
```

## Quick Build Instructions

### 1. Build using CMake and Ninja (LLVM / Clang)

```powershell
# Set path to cmake, ninja, and compiler
$env:Path = "$env:Path;C:\Path\To\CMake\bin;C:\Path\To\LLVM\bin"

# Configure Release build
cmake -B build/release -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang

# Compile binaries
cmake --build build/release
```

### 2. Build using Visual Studio MSVC

```powershell
# In Developer Command Prompt for VS 2022:
cmake -B build/msvc -G "Visual Studio 17 2022" -A x64
cmake --build build/msvc --config Release
```

## Running Automated Tests

LITE Browser includes an automated test runner (`LiteBrowserTests.exe`) that executes tests covering storage, ad/tracker rule parsing, request matching, reader mode extraction, HTML sanitization, and internal URL routing:

```powershell
.\build\release\LiteBrowserTests.exe
```

All 36 tests must pass.

## Producing Release Artifacts

To produce the portable release package and installer:

```powershell
# 1. Strip debug symbols
llvm-strip --strip-all build/release/LiteBrowser.exe
llvm-strip --strip-all build/release/LiteBrowser-Setup-x64.exe
llvm-strip --strip-all build/release/Uninstall.exe

# 2. Create Portable ZIP
Compress-Archive -Path build/release/LiteBrowser.exe, build/release/WebView2Loader.dll -DestinationPath dist/LiteBrowser-Portable-x64.zip
```

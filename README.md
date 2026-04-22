# OCB Studio Native

[![CI](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml/badge.svg)](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

OCB Studio Native is a C++20 Dear ImGui desktop tool, command line fallback, and library for inspecting and editing MSI overclocking profile files (`MsOcFile.ocb`). It can apply built-in presets, import/export preset files, write individual field values, compensate checksums, and extend the field catalog from BIOS IFR data.

The project intentionally has no Qt, Electron, or webview dependency. The GUI is built with Dear ImGui, GLFW, and OpenGL; CMake fetches and builds Dear ImGui/GLFW as static libraries. Firmware parsing and IFR extraction are integrated through local C++ wrappers around vendored source code.

## Features

- Load, validate, edit, and save MSI OCB profile files.
- Apply built-in presets or presets imported from `.ocbpreset` / JSON files.
- Export built-in presets to portable preset files.
- Write individual field values by field id or prompt.
- Compensate OCB checksums for BIOS-accepted output.
- Analyze BIOS images through the integrated UEFI/IFR pipeline.
- Build with CMake using a C++20 compiler, the OS toolchain, and statically linked Dear ImGui/GLFW.

## Safety

Firmware and overclocking changes can make a system unstable or unbootable. Treat generated profiles as experimental, keep verified backups, and apply only changes you understand. This project provides tooling; it does not guarantee that a specific board, firmware version, or profile will accept an edited file.

## Layout

```text
app/      Dear ImGui executable, command line fallback, and application controller
core/     OCB profile model, fields, presets, preset files, checksums, BIOS analysis
tools/    C++ wrappers around integrated firmware tools
tests/    native test executable
```

## Requirements

- CMake 3.24 or newer.
- A C++20 compiler:
  - MSVC 2022 on Windows.
  - Current Clang or GCC on Linux.
  - AppleClang on macOS.

No Qt SDK is required. The app uses Dear ImGui `v1.92.7` and GLFW `3.4` through CMake `FetchContent`.

## Build

```powershell
cmake -S . -B build -DOCB_BUILD_APP=ON -DOCB_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

For a library/tools-only build:

```powershell
cmake -S . -B build-core -DOCB_BUILD_APP=OFF -DOCB_BUILD_TESTS=OFF
cmake --build build-core --config Release --parallel
```

MSVC builds use the static runtime (`/MT`) by default via `OCB_STATIC_MSVC_RUNTIME=ON`, so release binaries do not need Visual C++ runtime DLLs next to them.

On Linux, install the usual X11/OpenGL development packages for GLFW before configuring, for example `xorg-dev`, `libglu1-mesa-dev`, and `pkg-config` on Ubuntu.

## GUI Usage

Run the executable without arguments:

```powershell
.\build\app\Release\ocb_studio.exe
```

The Dear ImGui interface provides OCB/BIOS/IFR file loading, OCB saving, checksum compensation, preset import/export, preset application, field search, and direct field editing.

## CLI Usage

List built-in presets:

```powershell
.\build\app\Release\ocb_studio.exe --list-presets
```

Export a built-in preset:

```powershell
.\build\app\Release\ocb_studio.exe --export-preset "Консервативный 200/220W 307A" --output conservative.ocbpreset
```

Apply a built-in preset:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --preset "Консервативный 200/220W 307A"
```

Apply an imported preset file:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --preset-file conservative.ocbpreset
```

Write one field:

```powershell
.\build\app\Release\ocb_studio.exe --input MsOcFile.ocb --output MsOcFile.patched.ocb --write "CPU Lite Load" 30
```

Add `--no-compensate` if you need raw output without checksum compensation.

## Preset File Format

Preset files are JSON objects:

```json
{
  "format": "OCB Studio Preset",
  "version": 1,
  "name": "Example",
  "values": {
    "Long Duration Power Limit (W)": 200,
    "Short Duration Power Limit (W)": 220,
    "CPU Lite Load": "0x1E"
  }
}
```

Values may be non-negative decimal integers or quoted decimal/hex strings.

## Tests

Some integration checks depend on local BIOS/OCB fixture files. Public CI builds the source targets without proprietary fixture data.

```powershell
cmake -S . -B build-test -DOCB_BUILD_APP=ON -DOCB_BUILD_TESTS=ON
cmake --build build-test --config Release --parallel
ctest --test-dir build-test -C Release --output-on-failure
```

## Third-party Sources

The repository includes selected source fragments from:

- [UEFITool](https://github.com/LongSoft/UEFITool), BSD-style license.
- [Universal IFR Extractor](https://github.com/donovan6000/Universal-IFR-Extractor), GPLv3.

Because Universal IFR Extractor is integrated from GPLv3 sources, this project is distributed under `GPL-3.0-only`.

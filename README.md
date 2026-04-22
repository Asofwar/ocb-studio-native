# OCB Studio Native

[![CI](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml/badge.svg)](https://github.com/Asofwar/ocb-studio-native/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

Native desktop tooling for inspecting MSI overclocking profile files and mapping them to BIOS IFR setup fields.

## What It Does

OCB Studio Native is a C++/Qt desktop application for working with MSI `MsOcFile.ocb` overclocking profiles. It can open an OCB profile, inspect known tuning fields, apply conservative presets, write field values, compensate profile checksums, and enrich the field catalog by extracting IFR setup forms from a BIOS image.

The project is intentionally native: no Electron, no webview, and no runtime shelling out to standalone firmware utilities for the core extraction path. UEFI parsing and IFR extraction are integrated from source and wrapped behind project-local C++ interfaces.

## Intended Users

This project is for firmware researchers, advanced motherboard tuners, BIOS modding enthusiasts, and developers who need a maintainable C++ base for automating OCB profile edits. It is not a vendor-supported flashing utility and does not flash firmware.

## Safety Notice

Firmware and overclocking configuration changes can make a machine unstable or unbootable. Treat generated profiles as experimental, keep known-good backups, and only use changes you understand. This project provides tooling; it does not guarantee that a particular board, firmware revision, or profile will accept a modified file.

## Key Features

- Native Qt Widgets desktop UI.
- C++20 core library for loading, validating, editing, and saving MSI OCB profiles.
- Checksum-style compensation for BIOS-accepted OCB output.
- Built-in field catalog and preset support.
- BIOS analysis pipeline: UEFI image parsing, Setup PE32 discovery, native IFR question extraction, and IFR-to-OCB field mapping.
- Vendored source integration for UEFITool engine pieces and Universal IFR extraction logic.
- CMake-first build with modular `core`, `tools`, `ui`, and `app` targets.

## Architecture

```text
app/      Qt application entry point and controller glue
core/     OCB profile model, field catalog, presets, checksums, BIOS analysis service
tools/    Source-integrated firmware tooling wrappers
  ifr/      IFR model, text parser, native IFR extractor
  uefitool/ UEFI firmware tree parser wrapper over vendored UEFITool source
ui/       Qt Widgets views and table model
tests/    Lightweight native test executable
```

The high-level runtime flow is:

```text
BIOS image -> UEFITool-backed parser -> Setup PE32 body -> Native IFR extractor
           -> IFR questions -> Field mapper -> editable OCB field catalog

MsOcFile.ocb -> OCB profile model -> field writes/presets -> checksum compensation -> saved OCB
```

## Requirements

- CMake 3.24 or newer.
- A C++20-capable compiler:
  - MSVC 2022 on Windows.
  - Recent Clang or GCC on Linux.
  - AppleClang on macOS.
- Qt 6 Widgets for the desktop UI.

The core/tools build can be compiled without Qt by disabling the UI target.

## Build

### Core and Tools Only

```powershell
cmake -S . -B build -DOCB_BUILD_UI=OFF -DOCB_BUILD_TESTS=OFF
cmake --build build --config Release --parallel
```

### Full Qt Desktop App

Pass Qt's CMake prefix path if Qt is not globally discoverable:

```powershell
cmake -S . -B build-ui -DOCB_BUILD_UI=ON -DOCB_BUILD_TESTS=ON -DCMAKE_PREFIX_PATH=C:\Qt\6.6.3\msvc2019_64
cmake --build build-ui --config Release --parallel
```

On Linux or macOS, use the equivalent Qt installation path for `CMAKE_PREFIX_PATH`.

## Run

After a full UI build, run the generated `ocb_studio` executable from the `app` target output directory. On Windows release builds this is typically:

```powershell
.\build-ui\app\Release\ocb_studio.exe
```

For a portable Windows folder, deploy Qt runtime files with `windeployqt`:

```powershell
windeployqt --release --dir dist .\build-ui\app\Release\ocb_studio.exe
```

## Example Workflow

1. Open `MsOcFile.ocb`.
2. Open a BIOS image with `Open BIOS` to extract Setup IFR fields.
3. Search for a setting such as `CPU Lite Load`, `CEP`, or `Power Limit`.
4. Select a field and write a new numeric value.
5. Save the OCB profile with checksum compensation enabled.
6. Test the generated profile cautiously on the target board.

## Tests

The test executable is intentionally simple. Some integration checks rely on local BIOS/OCB fixture files that are not included in this public repository. For public CI, the project currently performs a source build of the core/tools targets without proprietary fixtures.

```powershell
cmake -S . -B build-test -DOCB_BUILD_UI=OFF -DOCB_BUILD_TESTS=ON
cmake --build build-test --config Release --parallel
ctest --test-dir build-test -C Release --output-on-failure
```

## Third-Party Source

This repository vendors selected source from:

- [UEFITool](https://github.com/LongSoft/UEFITool), BSD-style license.
- [Universal IFR Extractor](https://github.com/donovan6000/Universal-IFR-Extractor), GPLv3.

Because Universal IFR Extractor is integrated from GPLv3 source, this project is distributed under `GPL-3.0-only`.

## Roadmap

- Improve board/profile detection and metadata display.
- Add richer field validation and value editors for common IFR option types.
- Add import/export of preset files.
- Add packaged releases for Windows, Linux, and macOS.
- Add fixture-free parser tests suitable for public CI.

## Contributing

Contributions are welcome when they keep the project native, maintainable, and careful about firmware safety. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

## License

OCB Studio Native is licensed under [GPL-3.0-only](LICENSE). Third-party source retains its original copyright and license notices.

# Changelog

Notable changes to OCB Studio Native are documented here.

The format follows the spirit of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project will use semantic versioning after public releases begin.

## Unreleased

### Changed

- Replaced the former desktop GUI with a Dear ImGui + GLFW/OpenGL desktop application.
- Added a command line fallback for scripting.
- Removed CMake and CI requirements for a Qt SDK or Qt runtime packaging.
- Added static Dear ImGui/GLFW dependency builds through CMake FetchContent.
- Kept preset import/export as core C++ functionality through `.ocbpreset` JSON files.

## [0.1.0] - 2026-04-22

### Added

- Native C++ core for loading, validating, editing, resetting, and saving MSI OCB profile files.
- Checksum compensation for saved OCB profiles.
- Built-in field catalog and conservative presets.
- Integrated BIOS parsing wrapper based on UEFITool source fragments.
- Native IFR extraction from Setup PE32 modules.
- IFR-to-OCB field mapping.
- Public repository documentation, contribution guide, security policy, issue templates, and CI.

### Notes

- Public CI builds source targets without proprietary BIOS or OCB fixture files.
- Firmware editing remains experimental and should be used only with verified backups.

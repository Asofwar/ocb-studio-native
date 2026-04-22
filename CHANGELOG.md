# Changelog

All notable changes to OCB Studio Native are documented here.

The format follows the spirit of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses semantic versioning once public releases begin.

## [0.1.0] - 2026-04-22

### Added

- Initial native C++/Qt Widgets desktop application.
- MSI OCB profile loading, validation, editing, reset, and saving.
- Checksum-style compensation for saved OCB profiles.
- Built-in field catalog and conservative preset support.
- Source-integrated UEFITool-based BIOS parsing wrapper.
- Native IFR extraction from Setup PE32 modules.
- IFR-to-OCB field mapping and searchable UI table.
- Public repository documentation, contribution guidance, security policy, issue templates, and CI.

### Notes

- Public CI currently builds core/tools without proprietary BIOS or OCB fixtures.
- Firmware editing remains experimental and should be used only with backups and board-specific validation.

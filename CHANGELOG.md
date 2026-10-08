# Changelog

This document records the significant changes made to released versions of vFPC.

## 4.0.0.0 - dd-mm-2026

vFPC 4 represents a substantial rewrite of the previous vFPC implementation, with a new architecture designed to improve validation, reliability and maintainability.

### Added

- Flight plan tracking and automatic revalidation when relevant flight plan information changes.
- Airport-specific validation data loading and management.
- Improved validation result reporting, including support for multiple validation failures.
- Additional diagnostic and logging capabilities.
- Configuration validation with warnings for unrecognised settings.
- Version information embedded consistently in the plugin and Windows DLL resources.
- Source build documentation and third-party software notices.

### Changed

- Reworked the flight plan validation architecture.
- Reworked SID selection and restriction processing.
- Improved route, exit point, flight level, odd/even level and restriction validation.
- Improved handling of time, aircraft type, engine type and SID suffix restrictions.
- Improved route normalisation before validation.
- Improved handling of VFR and flight plans outside the active airport.
- Validation is deferred until a departure runway has been assigned.
- Improved plugin state handling when connecting to or disconnecting from EuroScope.
- Improved error handling when processing configuration and validation data.
- Third-party C++ dependencies are managed through vcpkg.

### Compatibility

- vFPC remains a 32-bit (x86) plugin for EuroScope.
- vFPC 4 supersedes the previous vFPC 3.x implementation.
# Changelog

This document records significant changes in released versions and planned updates to vFPC.

## 4.1.0 - 07-11-2026

### Added

- Sector-controller support for recognised APP and CTR positions: review IFR
  departures from multiple departure-active Runway Activity airports within the
  position's configured UKCP coverage.
- Explicit controller-scope explanations, observer review scope and UKCP lookup
  status and reload commands for support use.
- A warning when a permitted exit point appears in EuroScope's extracted
  waypoint list but is absent from the filed route.

### Fixed

- Route suggestions and mismatch explanations now use only constraints that
  survived earlier validation checks and meet the requested level and odd/even
  requirements. Broad fallback suggestions were removed and duplicate route
  alternatives are omitted.
- Suggested routes and expected-route explanations now respect applicable SID
  restrictions, including aircraft type, SID suffix, EOBT and prohibitions,
  while preserving existing filed-flight validation behaviour.
- Validation results produced after airport data becomes available are now
  stored and logged consistently. Revalidation handles errors per flight and
  records completion/error counts.
- Flights awaiting a departure runway reuse unchanged pending results instead
  of repeatedly attempting validation on tag callbacks. Assigning a runway or
  publishing airport data still triggers the appropriate revalidation.
- Out-of-scope flights retain OFF reporting and the scope explanation in cached
  results, logs, SUMMARY and FULL instead of being reported as VFR.

### Changed

- Exit-point diagnostics distinguish a known exit prohibited for the filed
  departure from a case where no known exit is identified. Alternative departures
  are shown only where supported by the applicable exit-point constraints.
- Existing hard validation failures retain priority over WRN; the warning remains
  visible in Show Checks (FULL).
- The User Guide now explains sector-controller setup, coverage limits, exit-point
  messages and warning priority, with a clearer vector validation diagram.

- Startup logs identify the public version, DLL revision, build configuration,
  Git commit and dirty-source state to help distinguish the build in use.
- Airport-data diagnostics record load duration, JSON memory use/capacity and
  snapshot publication, replacement and release. Shutdown stops and joins workers,
  releases snapshots and flushes logging in a controlled order.
- Background-worker messages are queued for delivery from host callbacks, with
  bounded draining and shutdown/discard handling. Multi-airport data remains
  isolated by flight origin, and obsolete load results are rejected.
- Updated download/release-host and support links, build instructions and guide
  documentation. Scope troubleshooting commands are documented for use on advice
  from the vFPC team.

### Compatibility and status

- vFPC remains a 32-bit EuroScope plugin. Sector support reviews departure flight
  plans; it does not validate aircraft progress through an en-route sector.
- Public version: `4.1.0`. Windows DLL version: `4.1.0.0`.

## 4.0.0.0 - 11-10-2026

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
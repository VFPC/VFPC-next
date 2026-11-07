# VFPC Guide

vFPC is a EuroScope plugin for UK VATSIM that checks filed IFR departure flight
plans against the available validation data and shows the result directly in
the Departure List.

!!! note "Note"
    Use current EuroScope UK sector files and the VATSIM UK Controller Pack
    before using vFPC.

## Updates in 4.1.0

**Sector controllers can use vFPC across multiple departure airports.** Recognised
APP and CTR positions validate departure-active Runway Activity airports within
their configured UKCP coverage. See [Controller Scope](using-vfpc.md#controller-scope)
for setup, exclusions and lookup-failure behaviour. This support concerns
departure flight plans rather than validation of aircraft progress through a sector.

Exit-point diagnostics distinguish known prohibited exits from unidentified
exits, and warn when a permitted exit is extracted but not explicitly filed.
Existing validation failures retain priority over the warning. See
[Exit Point Diagnostics and Warnings](using-vfpc.md#exit-point-diagnostics-and-warnings)
for the SUMMARY and FULL messages.

The update also corrects route suggestions so they respect applicable
constraints and SID restrictions, preserves OFF reporting for out-of-scope
flights, and improves pending-result handling and support logging. These fixes
are included alongside the sector-controller and exit-point features.

## Guide Version

This guide applies to **vFPC 4.1.0**. It covers sector-controller support,
exit-point diagnostics and warnings, and the fixes described above.

## PDF User Guide

- [User Guide for vFPC 4.1.0 (PDF)](downloads/vFPC-User-Guide-v4.1.0.pdf)
- [Archived User Guide for vFPC 4.0.0 (PDF)](downloads/vFPC-User-Guide-v4.0.0.pdf)

The 4.1.0 PDF and these guide pages share the same Markdown content. PDF
download links are omitted from the printable edition. The 4.0.0 guide is
retained for that release.

## Guides

- [Installation and Configuration](installation.md) - installing and configuring vFPC.
- [Using vFPC](using-vfpc.md) - normal operation, sector-controller setup and validation results.
- [Validation Reference](validation-results.md) - result codes and how to interpret them.
- [Troubleshooting and Limitations](troubleshooting.md) - common problems and known limitations.
- [Reporting an Issue](reporting-issues.md) - getting support and reporting issues.

!!! warning "Controller Responsibility"
    vFPC is a controller aid, not a clearance engine. Validation results do not
    replace current procedures, local instructions or controller judgement.

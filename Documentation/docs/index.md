# VFPC Guide

vFPC is a EuroScope plugin for UK VATSIM that checks filed IFR departure flight
plans against the available validation data and shows the result directly in
the Departure List.

!!! note "Note"
    Use current EuroScope UK sector files and the VATSIM UK Controller Pack
    before using vFPC.

## Updates in 4.1.2

vFPC uses EuroScope's airport-level **DEP** selections in Runway Activity
to determine which departure airports are in scope for automatic IFR
validation. Controllers can select multiple airports. This replaces
controller callsign and UKCP coverage filtering. See
[Controller Scope](using-vfpc.md#controller-scope) for setup.

Exit-point diagnostics distinguish known prohibited exits from unidentified
exits, and warn when a permitted exit is extracted but not explicitly filed.
Existing validation failures retain priority over the warning. See
[Exit Point Diagnostics and Warnings](using-vfpc.md#exit-point-diagnostics-and-warnings)
for the SUMMARY and FULL messages.

## Guide Version

This guide applies to **vFPC 4.1.2**.

## PDF User Guide

- [User Guide for vFPC 4.1.2 (PDF)](downloads/vFPC-User-Guide-v4.1.2.pdf)
- [Archived User Guide for vFPC 4.1.0 (PDF)](downloads/vFPC-User-Guide-v4.1.0.pdf)
- [Archived User Guide for vFPC 4.0.0 (PDF)](downloads/vFPC-User-Guide-v4.0.0.pdf)

## Guides

- [Installation and Configuration](installation.md) - installing and configuring vFPC.
- [Using vFPC](using-vfpc.md) - normal operation, sector-controller setup and validation results.
- [Validation Reference](validation-results.md) - result codes and how to interpret them.
- [Troubleshooting and Limitations](troubleshooting.md) - common problems and known limitations.
- [Reporting an Issue](reporting-issues.md) - getting support and reporting issues.

!!! warning "Controller Responsibility"
    vFPC is a controller aid, not a clearance engine. Validation results do not
    replace current procedures, local instructions or controller judgement.

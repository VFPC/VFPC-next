# VFPC Guide

vFPC is a EuroScope plugin for UK VATSIM that checks filed IFR departure flight plans against the current VFPC restriction data and shows the result directly in the departure list.  Specifically flight plans are validated against the following criteria:

- Route validity specified by the SRD.
- SID valid for aircraft type.
- SID valid for time of day.
- SID Suffix is valid.
- Filed altitude matches semicircular rule.
- Filed level is within the level cap.
- Exit point exists and is valid for the route.
- No obvious syntax errors in the route.
- SRD warnings/bans for the filed route.

!!! note "Note"
    VFR flight plans are not validated by the plugin but are tagged in the departure list as "VFR".

!!! warning "Warning"

    vFPC is not a replacement for proper knowledge of basic aerodrome departures and flight plan routes. All controllers must have a working knowledge of SID restrictions, flight level capping, and the semicircular rule. vFPC is an assistive resource and not a substitute for proper knowledge.

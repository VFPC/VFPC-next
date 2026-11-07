# Troubleshooting and Limitations

This page covers common issues that may prevent vFPC from validating a flight
plan or displaying the expected result.

For information about individual validation result codes, see the
[Validation Reference](validation-results.md).

## vFPC Does Not Appear in the Departure List

Check that:

- `VFPC.dll` has been loaded in EuroScope.
- The **VFPC/VFPC** tag item has been added to the Departure List.
- The **FPL** column is visible.

If necessary, repeat the configuration steps in
[Installation and Configuration](installation.md).

## A Flight Plan Is Not Being Validated

vFPC validates IFR departure flight plans for airports within the current controller scope.

A flight plan may not be validated if:

- The flight plan is VFR.
- A departure runway has not yet been assigned.
- Validation checks have been disabled using **Toggle Checks**.
- The departure airport is outside the current controller scope.
- For APP/CTR, the airport is not departure-active in Runway Activity or is outside the position's configured UKCP coverage.
- The required airport validation data is not available.

The code displayed in the **FPL** column may provide more information. See the
[Validation Reference](validation-results.md).

## Validation Remains Pending

A **PDG** result indicates that vFPC is waiting for information required to
perform validation.

This may occur while airport validation data is loading or when a departure
runway has not yet been assigned.

If **PDG** remains displayed after the required information is available,
select **Show Checks (SUMMARY)** or **Show Checks (FULL)** and review the
information displayed in the EuroScope message area.

## An Unexpected Validation Result Is Displayed

If a result does not appear to agree with the filed flight plan:

1. Select **Show Checks (SUMMARY)**.
2. If necessary, select **Show Checks (FULL)** for the individual validation checks.
3. Compare the result with the applicable UK procedures and the filed flight plan.

If the result still appears incorrect, see [Reporting an Issue](reporting-issues.md).

## An Exit Warning Is Not Shown in the FPL Code

An existing validation failure takes priority over **WRN**. Select
**Show Checks (FULL)** and review **Warnings**. A permitted exit can be absent
from the filed route while the FPL code reports a separate failure, such as
**OER**.

## An Exit Is Missing from the Filed Route

EuroScope can extract a permitted intermediate waypoint that is not explicitly
filed. In that case vFPC passes the exit-point check and records a warning.
Review the filed route and the extracted waypoint list; do not interpret the
warning alone as an XIT failure.

If no known exit is identified in the extracted list, vFPC instead reports
**XIT**. Deleting a waypoint from the filed route does not guarantee that it
will remain in EuroScope's extracted list. See
[Exit Point Diagnostics and Warnings](using-vfpc.md#exit-point-diagnostics-and-warnings).

## Known Issues

There are currently no known issues.

Issues that have been resolved are documented in the release notes.

## Known Limitations

vFPC is designed to validate IFR departure flight plans against the available vFPC validation data. It does not validate VFR flight plans.

Validation is dependent on the coverage and accuracy of the available validation data. A successful validation does not remove the controller's responsibility to ensure that a flight plan and clearance are appropriate.
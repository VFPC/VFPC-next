# Troubleshooting and Limitations

This page covers common issues that may prevent vFPC from validating a flight
plan or displaying the expected result.

For information about individual validation result codes, see the
[Validation Reference](validation-results.md).

## vFPC Does Not Appear in the Departure List

Check that:

- `vfpc.dll` has been loaded in EuroScope.
- The **VFPC/VFPC** tag item has been added to the Departure List.
- The **FPL** column is visible.

If necessary, repeat the configuration steps in
[Installation and Configuration](installation.md).

## A Flight Plan Is Not Being Validated

vFPC validates IFR departure flight plans for the active departure airport.

A flight plan may not be validated if:

- The flight plan is VFR.
- A departure runway has not yet been assigned.
- Validation checks have been disabled using **Toggle Checks**.
- The flight plan does not belong to the airport currently being handled by vFPC.
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

## Known Issues

There are currently no known issues.

Issues that have been resolved are documented in the release notes.

## Known Limitations

There are currently no known limitations.
# Using vFPC

Once installed and configured, vFPC operates automatically while you are
controlling. Validation results are displayed in the **FPL** column of the
EuroScope Departure List.

## Normal Operation

vFPC validates IFR departure flight plans for the airports selected within
the current controller scope, using each flight's origin-airport data.

A flight plan is normally validated when:

- A new IFR flight plan is detected.
- A relevant part of an existing flight plan is changed.
- Updated validation data is loaded.

VFR flight plans are not validated.

!!! note
    vFPC requires a departure runway to be assigned before flight plan
    validation can take place.

## Controller Scope

### Using vFPC as an APP or CTR Sector Controller

vFPC supports sector controllers reviewing departures from multiple airports.
Recognised **APP** and **CTR** callsigns can validate IFR departures from the
departure-active Runway Activity airports that fall within the position's
configured UKCP coverage. You do not need to operate a separate airport-specific
position for each departure airport.

1. Connect using your recognised full controller callsign, including any sector
   identifier, for example `SCO_CTR` or `SCO_SOL_CTR`.
2. In EuroScope **Runway Activity**, enable the airport-level **DEP** selection
   for each departure airport you want vFPC to review.
3. Ensure each airport is covered by that position's UKCP mapping. vFPC excludes
   selected airports outside that coverage and explains the exclusion.
4. Ensure the individual flight has an assigned departure runway. Each eligible
   flight is checked using its own origin airport's validation data.

The airport-level DEP selection matters: selecting runway departure symbols
alone, or making an arrival-only selection, does not select the airport for
sector-controller departure validation. An airport appearing in the Departure
List does not automatically put it in vFPC scope.

For example, `SCO_SOL_CTR` can review selected departures from EGPH, EGPF and
EGPK when those airports are within its configured coverage. Selecting EGLL
does not add it to that sector's scope. An out-of-scope flight displays **OFF**;
an eligible flight awaiting airport data or its own departure runway displays
**PDG**.

### Other Positions and Scope Rules

vFPC matches the full controller callsign against UKCP controller-position data.
For a recognised airport-specific DEL, GND or TWR position, it selects the mapped
airport even when that airport is not departure-active in Runway Activity. A
warning explains the missing Runway Activity selection; it does not remove the
airport from scope.

For a callsign ending in `_OBS`, departure-active Runway Activity airports define
the review scope without a UKCP filter. No airport is inferred from aircraft in
the Departure List. UKCP coverage describes configured position coverage; it
does not determine which other controllers are online or resolve delegation.

Unrecognised operational positions, unsupported suffixes and empty UKCP coverage
pause validation. Flights outside the selected scope display `OFF`. In-scope
flights still wait for airport data and their own assigned departure runway;
these pending states display `PDG`.

### Support Commands - vFPC Team Advice Only

These commands are for troubleshooting only. Use them only when advised by the
vFPC team; they are not required for normal setup or operation.

- `.vfpc scope` displays the selected airports, selection explanation and UKCP
  lookup status.
- `.vfpc scope reload` requests fresh UKCP position data. It does not change the
  airport constraint source or EuroScope Runway Activity settings.

UKCP data is held in memory for the connected session and loaded on connection.
A failed refresh retains valid session data. With no usable lookup, operational
validation pauses; OBS selection remains available. There is no automatic retry
loop. If a lookup failure persists, contact the vFPC team for advice. Disconnecting clears the scope
cache. A failure to read Runway Activity pauses APP/CTR/OBS selection; saving the
Runway Activity dialog requests a new scan. Fixed local scope remains eligible.

## The FPL Column

The **FPL** column within the Departure List displays a short code indicating the current validation result.

![vFPC validation results in the EuroScope Departure List](images/Departure-List-Tags.png){.vfpc-screenshot .vfpc-screenshot--50percent}

*Figure 4: vFPC validation results in the EuroScope Departure List.*{ .vfpc-caption }

For example:

- <span class="vfpc-tag vfpc-tag--green">OK!</span> — the flight plan passed validation.
- <span class="vfpc-tag vfpc-tag--yellow">WRN</span> — advisory information is available.
- <span class="vfpc-tag vfpc-tag--red">RTE</span> — the route requires attention.
- <span class="vfpc-tag vfpc-tag--yellow">PDG</span> — validation is pending.
- <span class="vfpc-tag vfpc-tag--grey">VFR</span> — the flight plan is VFR and is not validated.
- <span class="vfpc-tag vfpc-tag--grey">OFF</span> — the flight is outside the selected controller scope, checks are disabled, or EuroScope is disconnected.
- <span class="vfpc-tag vfpc-tag--cyan">OVR</span> — a controller override has been applied.

The code is intended to provide a quick indication only. See the
[Validation Reference](validation-results.md) for a full explanation of
each result.

## Using the FPL Column

The **FPL** column is also used to access vFPC functions.

### Left-click — vFPC Options

Left-click the **FPL** result for a flight plan to open the vFPC options menu.

As shown in **Figure 5**, the menu provides four options:

- **Show Checks (SUMMARY)**
- **Show Checks (FULL)**
- **Toggle Override**
- **Toggle Checks**

The purpose and use of each option are described in the following sections.

![vFPC Options menu](images/Departure-List-Popup-Menu.png){ .vfpc-screenshot .vfpc-screenshot--wide }

*Figure 5: vFPC options available from the Departure List.*{ .vfpc-caption }

### Right-click — SRD Search

Right-click the **FPL** result to open **SRD Search** using the UK Controller
Plugin.

This provides quick access to the applicable Standard Route Document
information when reviewing a flight plan.

## Viewing Validation Results

More detailed information about a validation result can be displayed in the
EuroScope message window by selecting **Show Checks (SUMMARY)** or
**Show Checks (FULL)** from the menu shown in **Figure 5**.

### Show Checks (SUMMARY)

**SUMMARY** displays the principal validation result together with a short
explanation. This will normally provide sufficient information to understand
why a flight plan has not validated successfully.

### Show Checks (FULL)

**FULL** displays the individual checks performed by vFPC and their results.
Use this when more detail is required or when investigating an unexpected
validation result.

!!! note
    Validation details are displayed in the EuroScope message window only
    when **Show Checks (SUMMARY)** or **Show Checks (FULL)** is selected.

## Exit Point Diagnostics and Warnings

vFPC distinguishes the route explicitly filed in the flight plan from
EuroScope's extracted waypoint list. The extracted list can include intermediate
waypoints that are not written individually in the filed route, for example
along an airway.

Exit-point validation uses the extracted waypoint list and the departure
airport's constraints. Route validation remains a separate check of the
normalized filed route. Finding an exit in the extracted list does not by
itself establish that the point is permitted for the filed departure or that
all other route requirements have passed.

### XIT for an Exit Point Failure

When a known exit point is not permitted for the filed departure, **SUMMARY**
identifies the point and departure. For example:

> ALOTI not permitted via BPK

**FULL** explains the failure and lists other departures only when supported
by the airport's exit-point constraints. For example:

> ALOTI is not a permitted exit point for the filed BPK departure. ALOTI is permitted via UMLAT or ULTIB departures.

The alternative departures are an unordered list; their order does not indicate
preference or priority. Exit-point advice does not establish that the complete
flight plan would pass every other check via an alternative departure. Review
the applicable route and procedures before making a change.

If no known exit is identified, **SUMMARY** displays:

> No known exit point identified

**FULL** explains that no known exit was identified in the extracted waypoint
list for the filed departure. This differs from a known point that is not
permitted for that departure. Later checks may be reported as not checked
because exit-point validation failed.

### WRN for a Permitted Exit Missing from the Filed Route

When an exit is permitted for the filed departure and appears in the extracted
waypoint list but is not explicitly included in the filed route, the exit-point
check passes and vFPC records a warning. If there is no higher-priority failure,
the FPL column displays **WRN**.

Example **SUMMARY**:

> ALOTI absent from filed route

Example **FULL** warning:

> ALOTI is a permitted exit point for the filed departure and appears in EuroScope's extracted waypoint list, but is not explicitly included in the filed route.

This warning is not an XIT failure. Review the filed route and EuroScope's
interpretation rather than treating the warning alone as proof of an invalid
route. The actual identified exit point is used in the message.

### A Failure Takes Priority over WRN

An existing validation failure retains its FPL code when the missing-filed-exit
warning also applies. For example, **OER** remains displayed for an odd/even
level failure even if a permitted exit is absent from the filed route.

Select **Show Checks (FULL)** and review **Warnings** to see the warning
alongside the failure. SUMMARY normally explains the principal failure rather
than displaying every warning. No additional unsolicited console messages are
generated for these exit diagnostics; use Show Checks to request the details.

## Toggle Override

**Toggle Override** allows the controller to override the validation status displayed in the **FPL** column.

An override does not change the filed flight plan or the underlying validation result. The **FPL** column displays **OVR** to indicate that a controller override has been applied.

Select **Toggle Override** again to remove the override.

## Toggle Checks

**Toggle Checks** disables validation for the selected flight plan. The
**FPL** column displays **OFF** while validation is disabled.

Select **Toggle Checks** again to re-enable validation.

This can be useful where vFPC validation is not appropriate for a particular
flight plan.

## When a Flight Plan Does Not Validate

A result other than **OK!** does not necessarily mean that the flight plan
should simply be rejected or changed.

1. Review the code displayed in the **FPL** column.
2. Select **Show Checks (SUMMARY)** to view the reason.
3. If further information is required, select **Show Checks (FULL)**.
4. Compare the result with the applicable procedures and current flight plan.
5. Take the appropriate controller action.

If vFPC does not behave as expected, see
[Troubleshooting and Limitations](troubleshooting.md).

!!! warning "Controller Responsibility"
    vFPC is a controller aid. Validation results should be considered alongside
    current procedures and the controller's own assessment of the flight plan.

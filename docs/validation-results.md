# Validation Results

VFPC displays a validation result in the **FPL** column of the EuroScope Departure List.

The displayed code provides a quick indication of the validation status of the flight plan.

---

## How Validation Works

VFPC validates each IFR flight plan against the currently loaded airport validation data and follows the sequence as illustrated below.

Flight Plan
      │
      ▼
Syntax Check
      │
      ▼
SID Validation
      │
      ▼
Destination
      │
      ▼
Exit Point
      │
      ▼
Route
      │
      ▼
Level
      │
      ▼
Direction
      │
      ▼
Restrictions
      │
      ▼
Warnings
      │
      ▼
Result

The validation data is primarily generated from the NATS Standard Route Document (SRD) and may also include airport-specific operational rules and local procedures.

Validation is normally performed when:

- A new IFR flight plan is detected.
- A validated flight plan is amended.
- Airport validation data is refreshed.
- Validation is manually requested.

!!! tip

    Once a flight plan has been validated, the result is cached. VFPC will automatically revalidate the flight plan when a relevant change is detected.

The result of the validation is displayed in the **FPL** column of the Departure List. (See Departure List Configuration for details.)

---

## Result Colours

The colour of the FPL column provides the quickest indication of the validation status.

| Colour    | Meaning                                                                                        |
| --------- | ---------------------------------------------------------------------------------------------- |
| 🟢 Green  | Flight plan validated successfully.                                                            |
| 🟡 Yellow | Flight plan is valid but advisory information is available.                                    |
| 🟠 Amber  | Flight plan contains a level, direction or suffix issue that should be reviewed.               |
| 🔴 Red    | Validation failure. Controller action required.                                                |
| ⚪ Grey    | Flight plan is not currently being validated (for example VFR traffic or validation disabled). |

!!! note
    The exact colours used are configurable within the VFPC configuration file. The colours shown above represent the default behaviour.

---

## Result Codes

### <span style="color: rgb(0,190,0); font-weight: bold;">OK!</span>

All validation checks passed successfully.

No action is required.

---

### <span style="color: rgb(255,140,0); font-weight: bold;">WRN</span>

The flight plan is valid but advisory information is available.

Examples include:

* Operational notes.
* Time-based advisories.
* Route guidance.
* Controller guidance.

---

### <span style="color: rgb(255,215,0); font-weight: bold;">PDG</span>

Validation is pending.

This is normally displayed briefly while airport data is being loaded or refreshed.

---

### <span style="color: rgb(150,150,150); font-weight: bold;">VFR</span>

The flight plan is VFR and is not validated by VFPC.

---

### <span style="color: rgb(150,150,150); font-weight: bold;">OFF</span>

Validation has been disabled or is not applicable.

---

### <span style="color: rgb(0,255,255); font-weight: bold;">OVR</span>

The controller has decided to Override the validation result.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">SID</span>

The filed SID is invalid.

Examples:

* SID not available for the destination.
* SID not valid for the aircraft type.
* SID not available during the filed time period.
* SID not found within the airport data.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">DST</span>

Destination restrictions have failed.

Examples:

* Destination not permitted by the selected SID.
* Destination excluded by the SRD.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">XIT</span>

Exit point restrictions have failed.

Examples:

* Incorrect FIR exit point.
* Mandatory exit point missing.
* Invalid routing following the SID.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">RTE</span>

The filed route does not match any permitted route definition.

Examples:

* Missing airway.
* Incorrect airway.
* Mandatory route segment omitted.
* Route does not match SRD requirements.

---

### <span style="color: rgb(255, 140, 0); font-weight: bold;">LVL</span>

The filed flight level does not satisfy a level restriction.

Examples:

* Flight level below the minimum permitted level.
* Flight level above the maximum permitted level.

---

### <span style="color: rgb(255, 140, 0); font-weight: bold;">DIR</span>

The filed flight level does not comply with the applicable semicircular rule.

Examples:

* Eastbound route filed at an incorrect level.
* Westbound route filed at an incorrect level.

---

### <span style="color: rgb(255, 140, 0); font-weight: bold;">SFX</span>

The SID suffix is not permitted.

Examples:

* Incorrect SID variant selected.
* Runway-specific SID suffix restriction.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">RST</span>

A mandatory route or SID restriction has failed validation.

Controller action is required.

---

### <span style="color: rgb(255, 140, 0); font-weight: bold;">RST</span>

An advisory restriction has been identified.

The flight plan remains valid; however, the advisory should be reviewed before issuing a clearance.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">BAN</span>

The route is explicitly prohibited.

Examples:

* SRD route ban.
* Airport-specific route ban.
* Temporary operational prohibition.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">SYN</span>

A route syntax error has been detected.

Examples:

* Invalid route token.
* Unsupported route format.
* Invalid waypoint format.
* Corrupt route structure.

---

### <span style="color: rgb(190,0,0); font-weight: bold;">ERR</span>

An unexpected validation error has occurred.

This usually indicates a problem with the validation process rather than the flight plan itself.

---

# Viewing Detailed Validation Information

The **FPL** column provides only a short result code and should be used as an indication only.  Where a flight plan does not validate, the detailed output in the message area should always be reviewed.

## Example Departure List

*Screenshot to be added.*

To view the detailed validation result:

1. Right-click the relevant flight plan entry in the EuroScope Departure List.

2. Select one of the VFPC result options:

    - **Summary Results**
    - **Full Results**

3. The selected validation result is then displayed in the EuroScope message window.

!!! note

    The validation result is not displayed automatically each time a flight plan is checked. The controller must request the summary or full result as described in the next section.

---

# Summary and Full Results

VFPC can display either a summary result or a full validation breakdown in the EuroScope message window.

These are accessed by right-clicking the flight plan entry in the Departure List and selecting the required VFPC result option.

## Summary Mode

Summary mode displays only the principal validation result for each flight plan.

This mode is recommended for normal operational use, as it provides concise feedback while minimising the amount of console output.

Example:

```text
BAW123 RTE
Route Failed. No Permitted route match. Examples: FOYLE N650 ERSON \ P600 ASNUD + 5 more.
```
## Example Summary Result

*Screenshot to be added.*

---

## Full Mode

Full mode displays the complete set of validation fields that were  evaluated.

Typical information includes:

- Callsign
- Departure Airport
- SID
- Destination
- Exit Point
- Route
- Flight Level
- Direction
- Restrictions
- Warnings
- Overall Status

Full mode is particularly useful when:

- investigating unexpected validation results;
- assisting pilots with route corrections;
- verifying newly published airport data;
- troubleshooting reported issues.

## Example Full Result

*Screenshot to be added.*

## Important

!!! warning "Controller Responsibility"
    VFPC is an advisory validation tool designed to assist controllers.
    
    Controllers remain responsible for applying current procedures, local instructions, SRD requirements, SID restrictions, level capping requirements and the semicircular rule.

    A green <span style="color: green; font-weight: bold;">OK!</span> result should not be used as a substitute for operational knowledge and judgement.

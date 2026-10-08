# Validation Reference

vFPC displays a short validation code in the **FPL** column of the EuroScope
Departure List. The code provides a quick indication of the validation status
of the flight plan.

This page explains the validation colours and result codes used by vFPC.
For information about viewing Summary and Full validation results, see
[Using vFPC](using-vfpc.md).


## How Validation Works

When an IFR flight plan is ready for validation, vFPC checks the filed
information against the data available for the departure airport.

The diagram below provides a simplified overview of the validation process.
Individual checks may depend on the results of earlier checks.

![Validation Flowchart](images/validation-flow.png){.vfpc-screenshot .vfpc-screenshot--50percent}

*Figure 7: Simplified vFPC validation process.*{.vfpc-caption}

The validation data is primarily generated from the NATS Standard Route Document (SRD) and may also include airport-specific operational rules and local procedures.

## Result Colours

The colour of the FPL column provides the quickest indication of the validation status.

| Colour    | Meaning                                                                                          |
| --------- | ------------------------------------------------------------------------------------------------ |
| <span class="vfpc-colour vfpc-colour--green"></span>Green | Flight plan validated successfully. |
| <span class="vfpc-colour vfpc-colour--yellow"></span>Yellow | Validation is pending or advisory information is available. |
| <span class="vfpc-colour vfpc-colour--amber"></span>Amber | Flight plan contains a level, odd/even or suffix issue that should be reviewed. |
| <span class="vfpc-colour vfpc-colour--red"></span>Red | Validation failure. Controller action required. |
| <span class="vfpc-colour vfpc-colour--grey"></span>Grey | Flight plan is not currently being validated (for example VFR traffic or validation disabled). |
| <span class="vfpc-colour vfpc-colour--cyan"></span>Cyan | Controller override has been applied. |

!!! note
    The exact colours used are configurable within the vFPC configuration file. The colours shown above represent the default behaviour.

## Result Codes

The following table provides a quick reference to the result codes displayed
in the **FPL** column. More detailed information about each result is provided
below.

| Code | Meaning |
| --- | --- |
| <span class="vfpc-tag vfpc-tag--green">OK!</span> | All validation checks passed |
| <span class="vfpc-tag vfpc-tag--yellow">WRN</span> | Advisory information is available |
| <span class="vfpc-tag vfpc-tag--yellow">PDG</span> | Validation is pending |
| <span class="vfpc-tag vfpc-tag--grey">VFR</span> | VFR flight plan — not validated |
| <span class="vfpc-tag vfpc-tag--grey">OFF</span> | Validation has been disabled for the flight plan |
| <span class="vfpc-tag vfpc-tag--cyan">OVR</span> | Controller override applied |
| <span class="vfpc-tag vfpc-tag--red">SID</span> | SID validation failed |
| <span class="vfpc-tag vfpc-tag--red">DST</span> | Destination validation failed |
| <span class="vfpc-tag vfpc-tag--red">XIT</span> | Exit-point validation failed |
| <span class="vfpc-tag vfpc-tag--red">RTE</span> | Route validation failed |
| <span class="vfpc-tag vfpc-tag--amber">LVL</span> | Flight-level validation failed |
| <span class="vfpc-tag vfpc-tag--amber">OER</span> | Odd/even flight level validation failed |
| <span class="vfpc-tag vfpc-tag--amber">SFX</span> | SID suffix validation failed |
| <span class="vfpc-tag vfpc-tag--red">RST</span> / <span class="vfpc-tag vfpc-tag--amber">RST</span> | SID or route restriction requires attention |
| <span class="vfpc-tag vfpc-tag--red">BAN</span> | Applicable route or SID ban triggered |
| <span class="vfpc-tag vfpc-tag--red">SYN</span> | Flight plan syntax validation failed |
| <span class="vfpc-tag vfpc-tag--red">ERR</span> | Error encountered during validation |

---

<span class="vfpc-tag vfpc-tag--green">OK!</span> indicates that all applicable
validation checks have passed.

No further vFPC action is required.

---

<span class="vfpc-tag vfpc-tag--yellow">WRN</span> indicates that the flight
plan has passed validation, but advisory information is available.

Review the Summary result for details of the warning.

---

<span class="vfpc-tag vfpc-tag--yellow">PDG</span> indicates that validation
is pending.

This is normally a temporary condition while vFPC is waiting for information
required to validate the flight plan, for example:

- Airport validation data is still loading.
- A departure runway has not yet been assigned.

Validation will proceed when the required information becomes available.

---

<span class="vfpc-tag vfpc-tag--grey">VFR</span> indicates that the flight
plan is VFR.

vFPC validates IFR departure flight plans only. VFR flight plans are therefore
not validated.

---

<span class="vfpc-tag vfpc-tag--grey">OFF</span> indicates that validation
checks have been disabled for the flight plan using **Toggle Checks**.

Select **Toggle Checks** again to re-enable validation.

---

<span class="vfpc-tag vfpc-tag--cyan">OVR</span> indicates that the controller
has applied an override to the validation status displayed in the **FPL**
column using **Toggle Override**.

The override does not change the filed flight plan or the underlying
validation result.

Select **Toggle Override** again to remove the override.

---

<span class="vfpc-tag vfpc-tag--red">SID</span> indicates that the flight
plan has failed SID validation.

Review the Summary result for details of the SID validation failure.

---

<span class="vfpc-tag vfpc-tag--red">DST</span> indicates that the destination
has failed validation against the applicable route requirements.

Review the Summary result for details of the destination validation failure.

---

<span class="vfpc-tag vfpc-tag--red">XIT</span> indicates that the exit point
has failed validation against the applicable route requirements.

Review the Summary result for details of the exit point validation failure.

---

<span class="vfpc-tag vfpc-tag--red">RTE</span> indicates that the filed route
has failed validation against the applicable route requirements.

Review the Summary result for details of the route validation failure.

---

<span class="vfpc-tag vfpc-tag--amber">LVL</span> indicates that the requested
flight level has failed validation against the requirements for
the applicable route.

Review the Summary result for details of the flight level validation failure.

---

<span class="vfpc-tag vfpc-tag--amber">OER</span> indicates that the requested
flight level has failed the odd/even flight level requirements for
the applicable route.

Review the Summary result for details of the odd/even validation failure.

---

<span class="vfpc-tag vfpc-tag--amber">SFX</span> indicates that the SID suffix
has failed validation against the requirements for the applicable route.

Review the Summary result for details of the SID suffix validation failure.

---

<span class="vfpc-tag vfpc-tag--red">RST</span> or
<span class="vfpc-tag vfpc-tag--amber">RST</span> indicates that a SID or route
restriction applies to the flight plan.

- <span class="vfpc-tag vfpc-tag--red">Red</span> indicates that the restriction
  has caused validation to fail.
- <span class="vfpc-tag vfpc-tag--amber">Amber</span> indicates that the
  restriction is advisory.

Review the Summary result for details of the applicable restriction.

---

<span class="vfpc-tag vfpc-tag--red">BAN</span> indicates that the flight plan
has triggered an applicable route or SID ban.

Review the Summary result for details of the applicable ban.

---

<span class="vfpc-tag vfpc-tag--red">SYN</span> indicates that the flight plan
has failed syntax validation.

Review the Summary result for details of the syntax validation failure.

---

<span class="vfpc-tag vfpc-tag--red">ERR</span> indicates that vFPC encountered
an error while attempting to validate the flight plan.

Review the Summary result for further information. If the cause is not clear,
refer to [Troubleshooting and Limitations](troubleshooting.md).


!!! warning "Controller Responsibility"
    vFPC is an advisory validation tool designed to assist controllers.

    Controllers remain responsible for applying current procedures, local
    instructions, SRD requirements, SID restrictions, level capping
    requirements and the semicircular rule.

    An **OK!** result should not be used as a substitute for operational
    knowledge and judgement.

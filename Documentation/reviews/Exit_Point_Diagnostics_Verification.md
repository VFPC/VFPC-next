# Exit Point Diagnostics Implementation Verification

## Final review status 6 November 2026

**Scoped acceptance complete.** Debug and Release Win32 builds passed with zero warnings/errors. Isolated checks and user-operated SB01–SB07 Sweatbox checks passed for the documented scope. Source fingerprints and the installed normal Release DLL were checked again during final review and match the accepted test candidate. No additional code change or rebuild was required for the final documentation review.

| Case | Final result | Evidence |
|---|---|---|
| SB01 | Passed | 16:08:31 UTC, vfpc-20261106160728.log; operator confirmed FULL pass and no warnings |
| SB02 | Passed after display correction | 16:22:40 UTC, vfpc-20261106162210.log; SUMMARY and corrected FULL screenshots |
| SB03 | Passed | 16:25:20 UTC, vfpc-20261106162210.log; SUMMARY/FULL screenshots; owner accepted unordered alternatives |
| SB04A | Passed with controlled fixture | 16:39:55–56 UTC, vfpc-20261106163538.log; only ULTIB recommended |
| SB04B | Passed with controlled fixture | 16:42:40–41 UTC, vfpc-20261106163538.log; no alternatives recommended |
| SB05 | Passed | 16:48:52 UTC, vfpc-20261106164722.log; unidentified-exit SUMMARY/FULL |
| SB06 | Passed | 16:50:38 UTC, vfpc-20261106164722.log; OER retains priority, warning visible in FULL |
| SB07 | Passed | 16:53:38 UTC, vfpc-20261106164722.log; no identified exit and no missing-filed-exit WRN |

The operator confirmed no unsolicited exit diagnostic/warning console messages during normal Release testing. Intentional diagnostic banners and command responses in the isolated SB04 DLL are excluded from that production check. The normal Release DLL was restored and its diagnostic banner absence confirmed.

Approved clarifications are recorded in ADR-004: alternative order implies no preference, and route validation uses normalized filed-route tokens while exit validation uses extracted points. The warning uses a plain apostrophe to display correctly in EuroScope. Both initially identified documentation discrepancies are closed. AC01–AC08 are satisfied within the recorded scope.

Current source fingerprints:

- src/analyzeFP.cpp SHA256: 8FFBD06E4C5A6D2A015B4BC2CE4015025C2E1BE3BFDF66E1CEECCEFB0F5FC60E
- src/analyzeFP.hpp SHA256: CD865330D7729ACA84F15EE40A089138A10A231CA444D466CCEBFF43E13BEE6A
- Installed normal Release DLL SHA256: 2C44550DA3DB879736401F9537C5A6641E1C2D7208E2103F2DBB84A549A2E7C2

The installed DLL was built from the tested dirty working copy at base HEAD e8d28ef77e1a8f33c569f77050e957e8341d35d6. A subsequent source commit does not change the identity embedded in that already-built DLL. Build logs, harnesses, fixtures, screenshots and session notes are external local evidence, not repository dependencies. Their locations and verification sequence are retained below.

No broader operational-airport regression suite, stress/concurrency verification, merge, push or release publication is claimed. The project owner authorized final review and commit separately after scoped acceptance. Commit identity is provided by Git history; no release publication is included.

## Historical verification entries

The entries below preserve the sequence of preparation, findings, corrections and owner confirmations. Statements that testing, installation or clarification was pending describe their recorded stage and are superseded by the final status above.

- **Date:** 5 November 2026
- **Decision:** [ADR-004](../docs/adr/ADR-004.md)
- **Branch:** feature/xit-exit-point-diagnostics
- **Base HEAD:** e8d28ef77e1a8f33c569f77050e957e8341d35d6
- **Source state:** Working-copy changes, uncommitted
- **Result:** Debug and Release Win32 builds passed with zero warnings and errors; isolated checks passed; live Sweatbox verification pending

## Implementation evidence

`src/analyzeFP.cpp` captures the result of the existing exit-point stage before later stages filter candidates. `BuildExitPointDiagnostics_` generates known-prohibited or unidentified-exit diagnostics and identifies permitted extracted-only exits. It reuses destination and exit-point filters for alternatives, respects points and nopoints, excludes rules with no exit information from advice, and retains configured departure order while removing duplicates.

Warning detail is appended after other reporting so later failures retain their mnemonic while the warning remains in the Warnings field. Explicit failure metadata keeps concise XIT summaries classified as failures. SUMMARY preference applies only to the new diagnostics; existing summary formatting is otherwise unchanged. Structured DEBUG logs include callsign, departure, exit, outcome, reason and both route representations. No unsolicited console call was added.

The existing `checkExitPoint` and `checkRoute` function bodies were compared with the pre-change working source and are unchanged. No controller-scope, threading, runway-gate, route-normalisation or validity-filter ordering changes were made.

## Builds

Both final MSBuild builds used Visual Studio 2022, Win32, the existing installed vcpkg dependencies, and redirected output/intermediate/generated-header directories. Dependency installation was disabled. The installed EuroScope DLL was not replaced.

- Debug: passed, zero warnings and errors.
- Release: passed, zero warnings and errors.

External task-local logs and artifacts are not permanent repository dependencies:

- `C:\Users\peter\Documents\ChatGPT\Validation Checks\build-debug.log`
- `C:\Users\peter\Documents\ChatGPT\Validation Checks\build-release.log`
- Release candidate: `C:\Users\peter\Documents\ChatGPT\Validation Checks\build\Release\VFPC.dll`
- Release DLL SHA256: `47d1d91fa01493e3b6fa6d5644bc805cec27e4d58a39113b83101c5e94d3aeee`

## Isolated verification

The task-local `verify_exit_diagnostics.py` generated a standalone C++ check program by extracting current production helper, destination/exit-filter, field finalization, SUMMARY and mnemonic functions, plus the exact warning-append block. JSON fixtures exercised these functions without EuroScope. This is focused component verification, not full-plugin integration or a complete validation regression suite.

Passed checks covered:

- Exact agreed XIT FULL and SUMMARY, including UMLAT and ULTIB alternatives in configured order.
- Exact agreed WRN FULL and SUMMARY, with exit-check success retained.
- Exclusion of alternatives prohibited by nopoints, destination rules or unsupported empty exit rules.
- Known exit permitted by a negative-only constraint, and no warning inferred from rules without exit data.
- No supported alternatives and no known exit identified.
- Explicit filed waypoint with a speed/level suffix and lowercase spelling; token boundary matching; duplicate extracted exit suppression.
- Warning-only WRN, XIT classification, route-failure precedence with warning retained, and merging with an existing warning.
- Structured log context including the extracted representation.

The check executable reported PASS. The generator, generated source and executable remain in `C:\Users\peter\Documents\ChatGPT\Validation Checks`; they were not added to the repository.

## Acceptance and Sweatbox boundary

ADR-004 SB01–SB07 remain pending in live EuroScope/Sweatbox. Component checks provide evidence for the reporting logic but do not complete operational acceptance. In particular, rendered console text encoding, Show Checks presentation, host route extraction, actual airport configurations, and known airport edge cases require live verification. No tests were claimed against EGCC, EGPH, EGKK, EGBB or EGNT operational datasets.

Unknown-exit wording uses “No known exit point identified” rather than guessing which arbitrary route waypoint is an unknown exit. Exact wording beyond the agreed ALOTI example remains subject to review.

## Requirement description discrepancy

ADR-004 decision item 2 describes route validation as using extracted_points. Source inspection instead confirms `checkRoute` is called with `normalised_route`, built from `row.route_points`, while `checkExitPoint` uses `row.extracted_points`. Both existing algorithms and inputs were preserved, consistent with the explicit requirement not to change route validation. This record flags the description discrepancy; it does not amend the Accepted decision or authorize a route-validation algorithm change.

## Source fingerprints

- `src/analyzeFP.cpp` SHA256: `9313230f5ff43c852c7245b0850ec45dc8da2273083d58cefcb36d1ad4dfabf2`
- `src/analyzeFP.hpp` SHA256: `cd865330d7729aca84f15ee40a089138a10a231ca444d466ccebff43e13bee6a`

No source commit, plugin installation or deployment was performed. Next step: user-operated Sweatbox verification of SB01–SB07 using the Release candidate and controlled constraints.

## Live verification update 6 November 2026

The previously listed fingerprints and no-installation statement describe the initial 5 November build. The initial Release candidate was subsequently installed for authorized user-operated Sweatbox testing, with the prior DLL backed up in the task-local sweatbox/backup directory.

SB01: log vfpc-20261106160728.log at 16:08:31 UTC records BAW62E, ULTIB1J, runway 09R, explicit ALOTI, all checks Passed and no warnings. FULL display confirmation was not independently captured.

SB02: the same log at 16:17:28 UTC records BAW62E, MODMI1J, runway 09R, filed route MODMI M185 MID L151 DRAKE DCT GWC UN859 ETRAT, extracted GWC SITET ETRAT, exit and route Passed, and WRN with SUMMARY SITET absent from filed route. The supplied SUMMARY screenshot confirms the mnemonic and text. The FULL screenshot confirms the warning sentence under Warnings but reveals a garbled UTF-8 apostrophe in EuroScope’s. Behaviour and SUMMARY passed; FULL display requires correction and retest. This evidence does not establish that every unsolicited-message case has been checked.

The warning apostrophe was changed to ASCII without changing its meaning or validation logic. Focused component checks passed again, and Debug and Release Win32 builds passed with zero warnings/errors. The corrected DLL is prepared but has not been installed into the running EuroScope session.

Corrected source analyzeFP.cpp SHA256: 8FFBD06E4C5A6D2A015B4BC2CE4015025C2E1BE3BFDF66E1CEECCEFB0F5FC60E
Corrected Release DLL SHA256: 2C44550DA3DB879736401F9537C5A6641E1C2D7208E2103F2DBB84A549A2E7C2

External evidence: task-local sweatbox/session.md, installed log vfpc-20261106160728.log, and user-supplied screenshots codex-clipboard-2af4f16d-283f-46c3-bb7d-2021b62098ca.png and codex-clipboard-30390d5f-15f1-4056-be2d-a41fbff2a7c1.png. These are not permanent repository dependencies. No commit or release deployment has been performed.

### SB02 corrected FULL retest passed

Following installation of the corrected Release DLL, log vfpc-20261106162210.log at 16:22:40 UTC again confirms BAW62E/MODMI1J, extracted SITET absent from the filed route, exit and route Passed, status Passed and WRN with SUMMARY SITET absent from filed route. User screenshot codex-clipboard-9c6ce604-7b38-49ed-969f-abe88dd9a709.png confirms the FULL warning under Warnings now displays the ASCII apostrophe correctly, with syntax valid and status Passed. SB02 functional, SUMMARY and FULL display checks passed. This does not establish every unsolicited-message check or complete SB03–SB07. No further source changes were required for this retest.

### SB03 live observation 6 November 2026

User screenshots codex-clipboard-b2bf8b87-7698-42e3-bc2a-5b107418bcf9.png and codex-clipboard-e1651471-6dcc-4821-b0dd-7954bc28081d.png confirm BAW62E/BPK6J on runway 09R displays XIT and exact SUMMARY ALOTI not permitted via BPK. FULL identifies the known prohibited exit and recommends ULTIB or UMLAT. These are the required alternative departures, but their displayed order differs from ADR-004's exact example UMLAT or ULTIB. Functional behaviour and SUMMARY passed; the exact FULL-text requirement AC04 remains open for this ordering discrepancy. Route validation is not checked after the exit-stage failure, as expected. No change to the accepted wording or source has been made for this observation.


## SB04 diagnostic setup 6 November 2026

An isolated Release source copy was built in sweatbox/SB04/diagnostic with a controlled EGLL fixture loader and explicit diagnostic banner. Production source was not changed. Initial fixture A; .vfpc sb04 A or .vfpc sb04 B requests reload through the existing generation and worker pipeline. No server data or runtime configuration file was modified.

Build passed with zero warnings/errors. Actual SB04A/B JSON fixtures were tested using extracted production functions: A offers only ULTIB, B offers no alternatives. Live results remain pending.

EuroScope was closed. Verified normal Release SHA256 2C44550DA3DB879736401F9537C5A6641E1C2D7208E2103F2DBB84A549A2E7C2 was backed up at C:\Users\peter\Documents\ChatGPT\Validation Checks\sweatbox\backup\VFPC-normal-before-SB04.dll. Diagnostic installed SHA256 ECA91A3ACD907BBD3FFB1E8A5C5F0A8F21358BEEA6CCF5246895F5A2EB92F1C0. Restore the normal backup after SB04.



### SB04A live result passed on 6 November 2026

User screenshots codex-clipboard-431b83fb-85e4-486f-9b39-5d26a77787d0.png and codex-clipboard-d6cfb4d5-aca8-4528-82af-82bf832cc2ec.png confirm BAW64A/BPK6J runway 09R displays XIT, SUMMARY ALOTI not permitted via BPK, and FULL ALOTI is not a permitted exit point for the filed BPK departure. ALOTI is permitted via ULTIB departure. UMLAT is correctly excluded by nopoints despite its positive points match. Log vfpc-20261106163538.log confirms fixture A loaded at 16:39:55 UTC and revalidation at 16:39:56 UTC with extracted ALOTI, failure outcome and both route representations. SB04A passed under controlled diagnostic data. SB04B remains pending; diagnostic DLL remains installed.



### SB04B live result passed on 6 November 2026

User screenshots codex-clipboard-867dfb82-b4d9-426f-9da3-3f0ab0381d11.png and codex-clipboard-9906f658-f279-497f-9dc3-f63680ca44cd.png confirm BAW64B/BPK6J runway 09R displays XIT, SUMMARY ALOTI not permitted via BPK, and FULL ALOTI is not a permitted exit point for the filed BPK departure. No alternatives are listed. Log vfpc-20261106163538.log confirms fixture B loaded at 16:42:40 UTC and revalidation at 16:42:41 UTC with extracted ALOTI and the expected failure. SB04A and SB04B passed under controlled diagnostic fixtures. The test command/reload notices are explicit diagnostic-build output, not production unsolicited warning messages. The diagnostic DLL remains installed pending EuroScope closure and restoration of the normal Release DLL. SB05–SB07 remain pending; SB03 exact alternative-order discrepancy remains open.



### Normal Release restored after SB04

After the user confirmed EuroScope closed, no EuroScope process was present and the normal Release DLL was restored from VFPC-normal-before-SB04.dll. Installed SHA256 verified as 2C44550DA3DB879736401F9537C5A6641E1C2D7208E2103F2DBB84A549A2E7C2. The SB04 diagnostic DLL is no longer installed. Normal startup and server loading remain to be confirmed on reopening. SB05–SB07 remain pending.



### SB05 live result passed on 6 November 2026

User screenshots codex-clipboard-363205e5-bdd5-4520-acbf-3925541cafee.png and codex-clipboard-9a9a9ce1-8048-470f-bce8-601dbe7881aa.png confirm BAW65E/BPK6J runway 09R displays XIT, SUMMARY No known exit point identified, and FULL explaining that no known exit was identified in EuroScope's extracted waypoint list for the filed BPK departure. No alternative departure advice is shown. Log vfpc-20261106164722.log at 16:48:52 UTC confirms KEFTE is extracted, no identified exit, failure outcome, both route representations, and later route checks Not Checked. The normal Release startup is recorded and the user confirmed the diagnostic banner absent. SB05 passed for the prepared unrecognised-exit-candidate case; no waypoint is guessed as the exit. SB06 and SB07 remain pending.



### SB06 partial live confirmation 6 November 2026

User screenshots codex-clipboard-021a191b-99b6-42ed-9c7e-cd865029da03.png and codex-clipboard-5cccb604-cda7-4c1e-8936-129b14c1df2b.png confirm BAW66E/MODMI1J at FL340 displays OER and SUMMARY Failed Odd-Even Rule. Required: Odd. FULL shows exit, route and min/max level Passed. Log vfpc-20261106164722.log at 16:50:38 UTC confirms SITET extracted but absent from filed route, warning retained in the failed result, OER priority, and odd/even failure. The screenshots do not expose the Warnings row; visual confirmation that the warning remains available in Show Checks under the hard failure is still pending. SB06 is not yet marked fully passed.



### SB06 completed live verification 6 November 2026

Additional user screenshot codex-clipboard-d6f7481e-39c8-4031-a681-31f9ec55d33f.png shows the FULL Warnings field retains the complete permitted-extracted-only SITET explanation while the BAW66E tag remains OER. Combined with earlier SUMMARY/FULL and log evidence at 16:50:38 UTC in vfpc-20261106164722.log, SB06 passed: hard failure mnemonic precedence and warning visibility in Show Checks are confirmed. SB07 remains pending. SB03 exact alternative ordering remains unresolved.



### SB07 live result passed on 6 November 2026

User screenshots codex-clipboard-34d3f0a2-84b7-4fdb-a316-6a2cb4afbfbb.png and codex-clipboard-6373ad9a-5c34-45bd-befa-892333049e86.png confirm BAW67E/ULTIB1J runway 09R displays XIT, SUMMARY No known exit point identified, and FULL explaining the unidentified exit for the filed ULTIB departure. Log vfpc-20261106164722.log at 16:53:38 UTC confirms ALOTI absent from filed and extracted routes, no identified exit, existing exit-stage failure, later route checks Not Checked, and no missing-filed-exit warning. SB07 passed.

### Current verification summary

SB01 log baseline passed; independent FULL display capture remains unavailable. SB02 passed including corrected FULL text. SB03 functional and SUMMARY checks passed; exact FULL ordering differs (ULTIB or UMLAT instead of UMLAT or ULTIB), so AC04 remains open. SB04A and SB04B passed under isolated controlled fixtures; normal Release restored and diagnostic banner absence confirmed. SB05, SB06 and SB07 passed for the prepared cases. All numbered scenarios have now been exercised; this is not a claim that every acceptance criterion, broader regression check or unsolicited-console-message check is complete. No commit or release publication performed. Route-input description discrepancy in ADR-004 remains open.



### SB03 alternative-order clarification accepted 6 November 2026

The project owner explicitly agreed that alternative order has no functional significance and that either ordering is acceptable. ADR-004 now records that the alternatives are an unordered list and that order implies no preference or priority. AC04 retains the message wording and supported alternative set while excluding ordering from exact-text comparison. The observed ULTIB or UMLAT FULL output therefore satisfies the clarified SB03 requirement. SB03 and its AC04 ordering discrepancy are closed as passed on the previously recorded screenshot and log evidence. Historical discrepancy observations above remain as the verification history. No source changes or new builds were needed. The separate ADR-004 route-input description discrepancy and other documented verification limits remain unresolved.


### Route input clarification accepted 6 November 2026

The project owner explicitly agreed to correct ADR-004's route-input description. Decision item 2 now records checkRoute's normalized filed-route tokens from row.route_points and distinguishes checkExitPoint's row.extracted_points input. The discrepancy section preserves the original finding and records its authorized resolution. The route-input description discrepancy is closed. No code changes or rebuilds were required.

## Acceptance evidence review after approved clarifications

- AC01: unchanged checkExitPoint/checkRoute function bodies plus SB01/SB02/SB07 observations support preservation for the prepared cases; broader airport regressions were not run.
- AC02: SB03, SB05 and SB07 distinguish known prohibited exits from no known exit identified and retain failure behaviour.
- AC03: SB04A/B demonstrate positive and negative constraints with supported-only and no-alternative outcomes in isolated diagnostic data.
- AC04: SB03 passed under the approved unordered-alternative clarification.
- AC05: SB02 passed with exact semantic warning content, SUMMARY and FULL; a plain ASCII apostrophe replaced the garbled typographic apostrophe.
- AC06: available logs show no new missing-filed-exit warning in explicit, prohibited and unidentified cases.
- AC07: SB06 passed with OER priority and the warning visible under Warnings in FULL.
- AC08: structured-log fields and both route representations were observed. No new unsolicited console call was added in production code. Complete runtime absence of unsolicited messages has not been expressly confirmed by the operator; isolated diagnostic command/banner messages are intentionally excluded from production assessment.

Remaining evidence follow-up: obtain the SB01 FULL display confirmation if required for the final review, and the operator's confirmation that the normal Release runs introduced no unsolicited exit diagnostic/warning console messages. All SB01–SB07 cases have been exercised and the two previously identified wording/input discrepancies are resolved. This review does not assert a complete regression suite, commit or release approval. The normal Release DLL remains the installed test candidate.


## Scoped Sweatbox acceptance completed 6 November 2026

The project owner confirmed both remaining operator checks: SB01 FULL showed the exit check passed with no warnings, and normal Release testing produced no unsolicited exit diagnostic or warning console messages. The SB01 confirmation is operator evidence rather than an independently retained FULL screenshot. The unsolicited-message confirmation excludes intentional SB04 diagnostic banners and responses to explicit commands.

All scoped cases are now passed: SB01, SB02, SB03 under the approved unordered-alternative clarification, SB04A and SB04B with controlled fixtures, SB05, SB06 and SB07. AC01–AC08 are satisfied for the documented test scope and approved clarifications. The FULL warning uses a plain apostrophe for EuroScope console compatibility. The route-input description discrepancy is resolved by owner-approved documentation correction; validation algorithms were preserved.

ADR-004 and the ADR register now reflect scoped acceptance completion. The normal Release candidate remains installed; no diagnostic hooks were introduced into production source. This does not claim broader operational airport regressions, source commit, merge or release publication. Those remain separate steps. Historical pending/discrepancy entries above record the sequence of verification and are superseded by this completion entry.

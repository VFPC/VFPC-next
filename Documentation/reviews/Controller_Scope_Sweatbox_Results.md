# Controller-scope Sweatbox results

Recorded 3 November 2026. Testing was operated by the project owner with guided
steps and screenshot review, using local EuroScope Sweatbox sessions. Supporting
logs were read after testing. This is scoped verification, not release approval.

Test DLL: Release Win32, SHA-256
`A17335A3AFE21831C88F950D52F6D25417D75BA46D7DE7519FD6614C69E8A3F7`.

## Observed results

| Check | Observation | Result |
|---|---|---|
| EGPH_TWR without EGPH departure activation | EGPH selected; missing-activity warning; EGPH constraints loaded | Passed |
| EGPH_TWR with EGPF departure-active | Scope remains EGPH; EGPF RYR46XP displays OFF | Passed |
| EGPH runway gate | RYR86GY waits as PDG with no assigned runway; after runway/SID assignment normal validation runs and reports RTE | Passed for gating; route correctness not assessed |
| SCO_CTR multi-airport scope | EGPF and EGPH selected; EGPF flight becomes eligible and reports RTE | Passed |
| SCO_CTR out-of-coverage airport | Active EGLL explicitly excluded; EGPF/EGPH retained | Passed |
| PR_OBS scope | EGLL, EGPF and EGPH selected through Runway Activity | Passed |
| OBS successful UKCP reload | 342 positions loaded; scope unchanged; cache available, request idle | Passed |
| EGPH_APP filtering | EGPH selected, EGPF excluded, EGPF flight OFF | Passed |
| ESSEX_APP shared coverage | EGGW, EGSC and EGSS selected, EGPH excluded; EGPH flight OFF | Passed for selection; no Essex-origin aircraft validated |
| EGPH_F_APP empty mapping | Explicit empty-top_down explanation, no selected airports, flight OFF | Passed |
| EGPH_TEST_APP unmatched position | Explicit no-exact-mapping explanation, no selected airports, flight OFF | Passed |
| EGPH_DEL independent local scope | EGPH retained while EGPF alone departure-active; warning and EGPF flight OFF | Passed |
| EGPH_GND independent local scope | Same retained EGPH scope and warning; EGPF flight OFF | Passed |
| SCO_CTR successful UKCP reload | 342 positions loaded; EGPF/EGPH retained, EGLL excluded; request idle | Passed |
| Ordinary disconnect/reconnect | New identity and lookup acquired across tested positions; old snapshots cleared | Passed for observed sequential transitions |
| Final disconnect/unload | Log records snapshot clearance, worker shutdown and logging shutdown | Observed |
| Simulated failed UKCP refresh with valid cache | FAIL mode; cache retained; EGPF/EGPH still selected and EGLL excluded | Passed in isolated diagnostic DLL |
| Simulated initial UKCP failure after reconnect | Cache unavailable; no selected airports; both exposed flights OFF | Passed in isolated diagnostic DLL |
| Recovery without reconnect | NORMAL mode and reload restore lookup and EGPF/EGPH scope, retaining EGLL exclusion | Passed in isolated diagnostic DLL |
| OBS without UKCP cache | PR_OBS in FAIL mode still selects EGLL/EGPF/EGPH; flights retain normal RTE/PDG behaviour | Passed in isolated diagnostic DLL |
| Diagnostic removal | Owner confirmed banner absent after loading normal Release DLL; normal Release startup log exists | Confirmed |

Airport-level DEP checkboxes and runway departure symbols are separate inputs.
After reconnect, selecting only runway symbols did not select airports. Enabling
the airport DEP boxes produced the expected scope. Arrival-only flags were not
treated as departure selection by the source; a dedicated arrival-only runtime
case was not performed.

## Evidence and limits

Supporting logs are external, uncommitted local evidence in
`C:\Users\peter\Documents\Codex\2026-11-02\referenced-chatgpt-conversation-this-is-an-2\outputs\sweatbox\logs`:

- `vfpc-20261102170909.log`: TWR, runway pending/validation, CTR, OBS, APP and ESSEX cases.
- `vfpc-20261103105614.log`: empty mapping, unmatched position, DEL, GND, CTR reload and final shutdown.

Diagnostic evidence is external, uncommitted local evidence in
`C:\Users\peter\Documents\Codex\2026-11-02\referenced-chatgpt-conversation-this-is-an-2\outputs\sweatbox-diagnostic\logs`:

- `vfpc-20261103112348.log`: failure with cache at 11:27:10 UTC, no-cache failure
  at 11:28:49 UTC, successful recovery at 11:31:11 UTC.
- `vfpc-20261103140349.log`: PR_OBS with injected failure and no cache; all three
  airports selected at 14:06:09 UTC.

The diagnostic DLL SHA-256 is
`2198414120443b17bc63a43fa8e1a6d8d8784aa34df46cd75e06b4998c9fef15`.
The owner reported the normal DLL's diagnostic banner absent after restoration;
`logs/vfpc-20261103140747.log` is the subsequent normal Release startup log.

The logs contain actual SDK identity values: PR_OBS had facility 0 and
`server_controller=true` in this Sweatbox session. DEL/GND/APP/CTR records included
facilities 2/3/5/6 respectively. These observations apply to the tested simulator;
they are not a general guarantee for all EuroScope connection modes. The toolbar
`[APP]` text must not be treated as the logged SDK facility value.

No test here establishes that the test flight routes should pass. RTE demonstrates
that constraint checking ran, not that the route result itself was audited.
Likewise scope exclusion messages establish the selected airport set; they do
not prove current real-world staffing or delegation.

## Outstanding verification

- Real DNS/HTTP/TLS failures, timeouts and cancellation latency. The four injected
  cases above exercise the failed-worker-result handling before an HTTP request,
  not real transport failure mechanisms.
- Malformed/duplicate response rejection and retention of a valid previous lookup.
- Disconnect while a lookup is actively pending, ensuring a late old-session
  result cannot publish into the new session.
- Host callback concurrency and unload overlap under stress.

These remaining cases were not deliberately induced and must not be marked passed.

## Separate diagnostic finding

Out-of-scope IFR aircraft visibly display OFF, but their cached/logged mnemonic
can be VFR. Source inspection finds `TagMnemonicFromResult_` returns VFR for every
`ValidationStatus::NotChecked` result (`src/analyzeFP.cpp:737–744`), including the
OFF result generated for an unselected origin. The tag's explicit scope guard
correctly displays OFF. The NotChecked branch of `OutputValidationResultToConsole_`
also unconditionally says "Flight plan is VFR" (`src/analyzeFP.cpp:696–699`), so the
problem is not confined to logging. No correction was made while recording these
results. The smallest proposed change is to preserve the explicit OFF mnemonic
for NotChecked results and use the existing origin/scope explanation for their
summary output; genuinely VFR results should remain VFR. Validation eligibility,
constraint checks and runway gating do not need to change.

## Completed controlled failure check

Use a separately labelled diagnostic DLL from an isolated source copy with a
test-only switch that makes only the UKCP lookup return a controlled failure.
Keep the normal Release DLL and production source unchanged. Do not change system
network settings, hosts entries, firewall rules, TLS verification, or the normal
airport/version service configuration.

The sequence established a valid SCO_CTR cache, enabled the injected UKCP failure
and reloaded, confirmed retained scope, reconnected with failure still enabled
and confirmed no-cache pause, restored NORMAL mode and recovered without
reconnecting, then confirmed OBS selection in a fresh failed-lookup session.
This verifies the scope coordinator's
failure handling, not real DNS/HTTP/TLS failure mechanics. A separate Release Win32
diagnostic DLL was subsequently prepared on 3 November 2026 with zero build
warnings/errors. Its file description and startup banner identify it as a
diagnostic; test-only commands toggle simulated lookup failure without changing
the production source. The external Codex `outputs/sweatbox-diagnostic/README.md` and manifest record its isolated source provenance and commands.
The normal Release DLL's hash is unchanged. The planned failure-injection runtime
sequence passed, and the normal Release DLL has been restored. This does not
establish full release readiness or resolution of the diagnostic finding above.

## Reporting correction follow-up — 3 November 2026

The targeted OFF/VFR reporting correction is now implemented in the working source. It preserves OFF in cached/logged results, shows the origin/scope explanation in summary output, and includes Origin in FULL output. Debug and Release Win32 builds passed with zero warnings/errors. The new Release DLL is in the external Codex outputs/reporting-fix directory; SHA-256 30B8E3AD053BC95009701FF2EE45A259D512CE5AB1ECC34545F3E0D91160F27B. Focused runtime checks remain pending. Controller scope, IFR detection, constraints and runway gating were not changed. No commit was made.

## Reporting verification completed — 3 November 2026

All planned focused reporting checks passed in user-operated local Sweatbox: excluded IFR OFF in tag, SUMMARY, FULL and log; correct in-scope VFR reporting; PDG after returning to IFR with no assigned runway; and normal validation resuming after runway/SID assignment (RTE for this test route). The runtime log is external local evidence at C:\Users\peter\Documents\Codex\2026-11-02\referenced-chatgpt-conversation-this-is-an-2\outputs\reporting-fix\logs\vfpc-20261103142431.log. OFF is recorded at 14:25:56 UTC, VFR at 14:35:14, Waiting for Runway at 14:36:28 and RTE at 14:38:24. The log confirms disconnect at 14:39:24 and plugin shutdown at 14:39:34. Route correctness was not audited. Real transport failures, malformed payloads and timing/concurrency stress remain untested. No commit or release deployment was performed.

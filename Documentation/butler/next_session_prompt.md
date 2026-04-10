# VFPC-next — Next Session Prompt

_Last updated: 2026-04-10_

---

## Current State

- **main:** `040151bd` — feat(check): add vfpc_check offline route validation CLI (#9)
- **Status:** Clean. No open work from this session.
- **Open PRs (external):** #3 (Peter), #5 (Ari) — neither from this session

---

## What Was Completed This Session

- `vfpc_check.exe` CLI built, reviewed through 4 rounds of Ari patch review, merged
- Full plugin-equivalence contract established and enforced:
  - ERR vs SYN advisory distinction (`FinalizeValidationResult_` / `FieldIndicatesFailure_`)
  - VFR short-circuit at IFR gate (exit 0, not PASS/FAIL)
  - `first_waypoint`/`sid_suffix` always derived from SID, no JSON override
  - Missing `origin`/`destination`/`route` all exit 1 (validation ERR), not exit 2

---

## Next Steps

### Immediate (when Peter reports back)
- [ ] If Peter finds routes that produce unexpected results from `vfpc_check`, investigate
      whether the gap is in the CLI, `ValidatorCore`, or the `out.json` data

### Next priority (board item)
- [ ] `test(harness): add fixture-driven FlightPlanRow runner against real out.json`
      — GTest companion to the CLI; loads real out.json + JSON fixtures; runs
      `ValidatorCore::validateFlightPlan` in batch; asserts PASS/FAIL per case

### External (do not start without owner)
- [ ] Review and merge PR #5 (Ari: `fix-sid-applicability`)
- [ ] Review PR #3 (Peter: `peter-clean-port-2026-04-03`)

---

## Context for Next Agent

`vfpc_check.exe` is Peter's tool for testing routes without EuroScope. The contract
is plugin-equivalence: every gate, short-circuit, and normalisation in `main.cpp`
mirrors a specific code path in `analyzeFP.cpp`. The key reference for understanding
why each gate exists is `FinalizeValidationResult_` (what constitutes a hard failure)
and `ParseFlightPlanRow_` (what normalisation the plugin applies).

The two open PRs (#3, #5) predate this session and are not agent-owned.

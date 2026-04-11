# VFPC-next — Next Session Prompt

_Last updated: 2026-04-11 (session 92 — harness hardened; --verbose flag added)_

---

## Current State

**`main`:** `f3d2d768` — clean. Contains `vfpc_check.exe` CLI tool (PR #9).

**`test-harness/main`:** Active sidecar harness lane. **122/122 GTest tests passing.**

**Python harness:** `C:\Temp\vfpc_plugin_test.py` — 53 seeds, 121 mutations, **0 wrong outcomes**.

---

## What Was Completed

### Session 92 (2026-04-11)
- Hardened all five mutation guards in `vfpc_plugin_test.py` to eliminate false positives from
  sibling-constraint interactions (TYPE_VIOLATION, MISSING_POINT, DIRECTION_FLIP, RFL_ABOVE_MAX,
  ROUTE_MISMATCH)
- Added `--verbose` / `-v` flag: shows every baseline flight plan (human-readable summary +
  reproducible JSON) and every mutation input — makes the harness usable without AI assistance

### Session 91 (2026-04-10)
- `vfpc_check.exe` CLI built and merged via PR #9 (`040151bd`)
- Full plugin-equivalence contract implemented after 5 rounds of Ari patch review
- Butler docs created for this repo (first session)
- Python harness `vfpc_plugin_test.py` created: pool-based seed discovery, mutation generation,
  JSON artifact persistence

---

## Immediate Next Steps

1. **Wait for Peter's feedback** on `vfpc_check.exe` with real routes from his sessions
2. If something produces unexpected results: investigate CLI → ValidatorCore → out.json in that order
3. When Peter confirms he's happy with the backup branch state, remove remote safety refs:
   - `backup/main-pre-recovery-2026-04-03`
   - `backup/test-harness-pre-rebase-2026-04-03`

## Parked (do not start without explicit reason)

- **PR #5 (Ari's SID applicability fix):** good design; do not merge without Ari/Peter coordination
- **Issue #6 (vcpkg cleanup):** Peter owns vcpkg direction; do not push
- **Issue #7 (RapidJSON self-contained):** parked; Peter has accepted the current vendored setup
- **Broader dependency / logger cleanup:** intentionally not started; do not resume

---

## Operating Rules for This Repo

- This is a **sidecar verification lane**. Do not let it drift into "fix Peter's branch".
- Do not merge PR #3 (Peter) or PR #5 (Ari) without the respective owner.
- Add new GTest fixture cases as JSON-only edits to `VFPC_Tests/fixtures/constraint_cases.json`
  — no recompile needed.
- The Python harness lives at `C:\Temp\vfpc_plugin_test.py`; it is not in the repo.
  If it needs to be preserved long-term, move it to `VFPC-next/scripts/` and commit it.
- When in doubt about plugin behavior, read `src/analyzeFP.cpp` — that is the ground truth.

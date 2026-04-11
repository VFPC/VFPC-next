# VFPC-next — Next Session Prompt

_Last updated: 2026-04-11 (session 93 — OutJsonSeedTests; PR #5 merged; SidApplicability cleanup)_

---

## Current State

**`main`:** `dcaad1c2` — clean. Contains `vfpc_check.exe` CLI (PR #9), SID applicability fix
(PR #5), and `SidApplicability.hpp` `using namespace` cleanup.

**`test-harness/main`:** Active sidecar harness lane. **125/125 GTest tests passing.**
New suite: `OutJsonSeedTests` — loads `fixtures/out_json_seeds.json`, drives each confirmed
seed through `validateFlightPlan()`, asserts `overall_pass == true`. User guide at
`Documentation/out-json-seed-tests.md`.

**Python harness:** `C:\Temp\vfpc_plugin_test.py` — 60 seeds, mutations clean, **0 wrong outcomes**.
Seeds now store `first_waypoint` and `sid_suffix` using the same normalization as the plugin.

**PR #3 (Peter's rewrite):** Open, explicitly unfinished. Peter's own note: validation logic
"requires testing and debugging." Do not merge. See PR #3 decision note below.

---

## What Was Completed

### Session 93 (2026-04-11)
- **`OutJsonSeedTests.cpp`** added to `test-harness/main` (issue #8 closed):
  - Loads `fixtures/out_json_seeds.json` + live `out.json`, runs each seed through
    `ValidatorCore::validateFlightPlan()`, asserts `overall_pass == true`
  - Fixture validation enforced in loader: all replay-contract fields required; missing
    `flight_plan` or wrongly-typed fields fail fast in `SeedsFileLoads`, not silently
  - `rowFromSeed()` is unconditional reads — no synthetic fallbacks
  - User guide: `Documentation/out-json-seed-tests.md`
- **PR #5 merged** (`ari/fix-sid-applicability` → `main`):
  - `SidApplicability.hpp` added — `ShouldEvaluateConstraints()` fixes RST-for-jets bug
  - One-line change in `analyzeFP.cpp` at line 1862
  - Issue #4 closed (auto-closed by merge)
- **`SidApplicability.hpp` cleanup** (`dcaad1c2`):
  - Removed `using namespace rapidjson` from header; all `Value`/`SizeType` uses now
    explicitly qualified as `rapidjson::Value` / `rapidjson::SizeType`
- **Python harness updated**: `first_waypoint` and `sid_suffix` now stored in seed
  `flight_plan` using same derivation rules as `analyzeFP.cpp` (alphabetic SID prefix,
  last-char suffix, EGLL/CHK special case)
- **Issues #7, #10** closed by Peter directly (his fixes described in issue comments;
  not yet pushed to any PR — Peter to push when ready)

### Session 92 (2026-04-11)
- Hardened mutation guards in `vfpc_plugin_test.py`; added `--verbose` flag

### Session 91 (2026-04-10)
- `vfpc_check.exe` CLI built and merged via PR #9; Python harness created

---

## PR #3 — Decision for Next Session

**What it is:** Peter's full plugin rewrite — spdlog logging, config file, caching
(airport/version data fetched once per session), new validation approach that continues
past first error, new output format. Peter's own note: validation logic "requires testing
and debugging — probably the last area of significant development."

**Current state:** Open, unfinished, not based on current `main` (misses PR #5 fix).

**Decision:** Do not merge. Leave as Peter's working branch. When he says it's ready:
1. He must rebase onto `main` — the conflict will be in `analyzeFP.cpp` (our PR #5
   change at line 1862 vs his validation rewrite)
2. Before merging, confirm his new validation logic handles the SID applicability case:
   ask "what happens when a jet hits a SID with only a non-jet restriction at the SID
   root — does it fall through to constraint evaluation?"
3. If his rewrite already handles it, the rebase conflict is mechanical. If not, he
   needs to incorporate `SidApplicability::ShouldEvaluateConstraints`.

---

## Immediate Next Steps

1. **PR #3 conversation with Peter** — explain the above; let him drive timing
2. **Peter's fixes for #10 and #7** — he fixed both locally; let him push when ready
3. When Peter confirms backup refs no longer needed, remove:
   - `backup/main-pre-recovery-2026-04-03`
   - `backup/test-harness-pre-rebase-2026-04-03`

## Parked (do not start without explicit reason)

- **Issue #6 (vcpkg cleanup):** Peter owns vcpkg direction; do not push
- **Issue #1 (bootstrap harness on Peter's line):** superseded by current harness work
- **Broader dependency / logger cleanup:** intentionally not started; do not resume

---

## Operating Rules for This Repo

- This is a **sidecar verification lane**. Do not let it drift into "fix Peter's branch".
- Do not merge PR #3 without Peter confirming it's ready and the applicability fix is present.
- Add new GTest fixture cases as JSON-only edits to `VFPC_Tests/fixtures/constraint_cases.json`
  — no recompile needed.
- Refresh seeds after a new AIRAC: run `vfpc_plugin_test.py`, copy `vfpc_seeds.json` to
  `VFPC_Tests/fixtures/out_json_seeds.json`, rebuild and run.
- The Python harness lives at `C:\Temp\vfpc_plugin_test.py`; not in the repo.
  If it needs long-term preservation, move to `VFPC-next/scripts/` and commit.
- When in doubt about plugin behavior, read `src/analyzeFP.cpp` — that is the ground truth.

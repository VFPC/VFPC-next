# VFPC-next — Project Status

_Last updated: 2026-04-11 (session 93 — OutJsonSeedTests; PR #5 merged; SidApplicability cleanup)_

---

## Repo Purpose

`VFPC-next` is the home for:
1. **`VFPC_Tests` — GTest harness** for verifying the pure-logic seams extracted from the plugin
2. **`VFPC_Check` — offline route validation CLI** (`vfpc_check.exe`) for Peter to test routes
   without EuroScope

It is a **sidecar verification lane**, not a production plugin branch. Changes here are for
our verification workflow and may never merge to Peter's eventual trunk.

---

## Branch State

| Branch | Purpose | Status |
|--------|---------|--------|
| `main` | Production baseline | Clean at `dcaad1c2` — includes PR #5 + namespace cleanup |
| `test-harness/main` | GTest harness lane | Active — 125/125 passing |
| `peter-clean-port-2026-04-03` | Peter's rewrite (PR #3) | Open, unfinished — do not merge; see PR #3 note in next_session_prompt.md |

| `backup/main-pre-recovery-2026-04-03` | Remote safety ref | Pending Peter confirmation to remove |
| `backup/test-harness-pre-rebase-2026-04-03` | Remote safety ref | Pending Peter confirmation to remove |

---

## GTest Harness (`VFPC_Tests`) — current state

**125/125 passing** on `test-harness/main`.

Seams extracted as pure headers (no EuroScope / spdlog / curl dependency):
- `src/FlightPlanRow.hpp` — plain flight plan data struct
- `src/SidLookup.hpp` — `TryFindSidIndex()`, `ExtractAirportFromDocument()`
- `src/ValidatorCore.hpp` — `validateFlightPlan()` mid-stack entry point
- `src/RouteSyntax.hpp` — `normaliseAndValidateRoute()` free function
- `src/ConstraintChecks.hpp` — all `checkOne*` pure constraint-evaluation functions

Test files:
- `VFPC_Tests/ValidatorCoreTests.cpp` — 7 integration tests (SID-level restrictions + all 7 rounds)
- `VFPC_Tests/SidLookupTests.cpp` — 17 unit tests (airport extraction, SID by point/alias)
- `VFPC_Tests/RouteSyntaxTests.cpp` — 29 tests (all five passes, end-to-end cleanup, round-2 wiring)
- `VFPC_Tests/fixtures/constraint_cases.json` — 87 fixture cases; adding new cases is JSON-only

Fixture-driven batch layer is adversarially hardened:
- 10 cases are mutation-verified: three specific code mutations broke exactly the right cases

RapidJSON: vendored submodule v1.1.0 under `VFPC_Tests/third_party/rapidjson`
(plugin's own copy under `lib/include/rapidjson` is intentionally untouched)

**Remaining blind spots:**
- IFR/VFR gating and active-airport locking remain EuroScope-dependent
- Output string formatters (display-only) are not exercised
- Extracted seam headers mirror production logic — drift is possible until shared headers exist

---

## CLI Tool (`VFPC_Check`) — current state

**`vfpc_check.exe`** — merged to `main` via PR #9 (`040151bd`). ~433 KB Release x64, no DLLs.

**Input:** `out.json` path + flight-plan JSON on stdin (bare array or `{cycle,airports}` envelope)

**Output:**
- `PASS` / `PASS_WARN` — exit 0
- `FAIL round N (...)` — exit 1
- `ERR: <field> missing` — exit 1
- `NOT CHECKED (VFR)` — exit 0
- `SYN ADVISORY: ...` — exit 0 (advisory, not a hard failure)
- Tool error — exit 2

**Required fields:** `origin`, `destination`, `route`, `is_ifr`

**Derived fields:** `first_waypoint` and `sid_suffix` always derived from `sid` — JSON overrides
are intentionally not accepted (mirrors plugin behavior).

**Plugin-equivalence contract:** mirrors `ParseFlightPlanRow_`, IFR gate,
`FinalizeValidationResult_`, and `FieldIndicatesFailure_` exactly.

---

## Python Test Harness (`C:\Temp\vfpc_plugin_test.py`)

Plugin-behaviour verification runner operating from `out.json` as the source of truth.

**Usage:**
```powershell
# Normal run (terse)
python C:\Temp\vfpc_plugin_test.py

# Verbose run (shows every baseline flight plan + every mutation input)
python C:\Temp\vfpc_plugin_test.py --verbose
```

**Current result:** 53 seeds, 121 mutations, 0 wrong outcomes (~23 s runtime)

**Seed pools:**
| Pool | Count | What it covers |
|------|-------|----------------|
| plain | 15 | Route + FL + direction constraints |
| points | 15 | Exit-point requirement constraints |
| timed | 10 | Time-window constraints |
| typed | 13 | Engine-type restricted SIDs |

**Mutations exercised:**
- `DEST_VIOLATION` — wrong destination (all 53 seeds)
- `RFL_BELOW_MIN` — FL below constraint minimum (all 53 seeds)
- `TIME_MISS` — time outside window (15 timed seeds)

**Mutations suppressed by safety guards** (data shape prevents isolation):
- `ROUTE_MISMATCH` — sibling constraints with no route filter would survive
- `MISSING_POINT` — siblings without points field would survive
- `DIRECTION_FLIP` — same
- `RFL_ABOVE_MAX` — siblings with no max bound would survive
- `TYPE_VIOLATION` — LISTO SID restriction is a props allowlist; `J` not in types list

**Artifacts:** `C:\Temp\vfpc_seeds.json`, `C:\Temp\vfpc_discarded.json`, `C:\Temp\vfpc_mutations.json`

---

## Open Issues

| Issue | Description | Status |
|-------|-------------|--------|
| #1 | GTest harness - bootstrap on Peter's line | Parked / superseded |
| #3 | Peter's clean port (PR open) | Peter owns - do not merge; needs rebase + applicability check |
| #6 | Remove hard-wired vcpkg.exe dependency | Parked - Peter owns |
| #4 | RST for jets on type-gated SIDs | **Closed** - fixed by PR #5 (SidApplicability) |
| #7 | RapidJSON dependency self-contained | **Closed** - Peter fixed locally |
| #8 | Fixture-driven FlightPlanRow runner | **Closed** - OutJsonSeedTests merged session 93 |
| #10 | Empty-point SID bucket unreachable | **Closed** - Peter fixed locally |

---

## Next Steps

1. **PR #3 conversation with Peter** - he needs to rebase onto main; confirm his new validation
   logic handles the SID applicability case before merging (exact question in `next_session_prompt.md`)
2. Peter's fixes for #10 and #7 are local - let him push when ready
3. Confirm with Peter that remote backup refs can be removed
4. Refresh seeds for AIRAC 2604 when new `out.json` is produced

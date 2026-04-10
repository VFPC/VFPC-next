# VFPC-next Test Harness

**Branch:** `test-harness/main`
**Base branch:** `main`
**Purpose:** standalone verification harness for the pure constraint-evaluation logic in Peter's recovery line
**Status:** active sidecar lane — not proposed for Peter's trunk unless Peter requests it

**Results (last run: 2026-04-04):**
```
[==========] 122 tests from 26 test suites ran.
[  PASSED  ] 122 tests.
Batch result: 87 passed (total 87)
```

---

## What this harness actually tests — and what it does not

### What IS tested

The harness covers the full **standalone evaluation kernel** plus the path from a
pre-loaded airport JSON object to a validation result:

| Area | Headers / files | Tests | What it covers |
|---|---|---|---|
| Time window | `TimeWindow.h` | 48 (Groups 1–7) | Day-of-week + time boundary logic, all window shapes |
| Constraint evaluation | `ConstraintChecks.hpp` | unit + fixture | `checkOneDestination`, `checkOneExitPoint`, `checkOneRoute`, `checkOneMinMax`, `checkOneDirection`, `checkOneAlerts` |
| Fixture batch | `constraint_cases.json` | 87 cases | Full diversity of constraint + flight data combinations |
| Airport/SID selection | `SidLookup.hpp` | 17 | `TryFindSidIndex` (point + alias matching), `ExtractAirportFromDocument` (single-object + array root), full selection chain |
| Mid-stack validation | `ValidatorCore.hpp` | 7 | `validateFlightPlan()` — airport → SID → restrictions → 7-round constraint loop → `fail_round` / `overall_pass` |
| Route normalisation | `RouteSyntax.hpp` | 29 | All five normalisation passes, end-to-end route cleanup, round-2 route-check interaction through `ValidatorCore` |

The fixture-driven batch layer (`VFPC_Tests/fixtures/constraint_cases.json`) has **87 cases** covering:

- 28 original constraint-field cases
- 14 time-windowed cases (constraint + simulated time)
- 9 FL boundary cases (inclusive boundary semantics)
- 10 route-prefix edge cases (prefix length, wildcards, noroute)
- 12 SRD-derived cases (translated from real AIRAC constraint structures in `SrdTraceSemanticTests.cs`)
- 7 adversarial cases (mutation-verified; see below)
- 3 `nopoints` cases (forbidden-exit-point logic)
- 7 above-FL410 direction cases (RVSM parity formula verification)

### What is NOT tested

```
out.json (loaded from live API)         ← NOT TESTED (EuroScope-dependent)
  └── IFR/VFR gating                    ← NOT TESTED (EuroScope-dependent)
  └── active-airport locking            ← NOT TESTED (EuroScope-dependent)
  └── find airport (EGAA)               ← TESTED (SidLookup + ValidatorCore)
        └── find SID (BELZU...)         ← TESTED (SidLookup + ValidatorCore)
              └── SID-level restrictions   ← TESTED (ValidatorCore)
              └── route normalisation      ← TESTED (RouteSyntax + ValidatorCore)
              └── for each constraint
                    └── checkTimeWindow()         ← TESTED
                    └── checkOneDestination()     ← TESTED
                    └── checkOneExitPoint()       ← TESTED
                    └── checkOneRoute()           ← TESTED
                    └── checkOneMinMax()          ← TESTED
                    └── checkOneDirection()       ← TESTED
                    └── checkOneAlerts()          ← TESTED
  └── output string formatting          ← NOT TESTED (display-only)
```

**Remaining untested areas:**
- API data loading and hot-reload (requires EuroScope + live network)
- IFR/VFR gating (`GetFlightPlanData().GetPlanType()` — EuroScope runtime)
- Active-airport locking (`active_mtx_` + `active_airport_` — plugin instance state)
- Output string formatters (`BansOutput`, `WarningsOutput`, `DirectionOutput`, etc.) — display-only

---

## Adversarial cases and mutation verification

Ten of the 87 fixture cases are explicitly adversarial — designed to catch specific
plausible code mistakes rather than just verify happy-path behaviour. Three mutations
were applied to the production source, run through the harness, then reverted:

| Mutation | Location | Case that caught it |
|---|---|---|
| `currentMin >= starttime[1]` → `>` | `TimeWindow.h` | `ADVERS_time_start_boundary_exact_active` |
| `currentMin <= endtime[1]` → `<` | `TimeWindow.h` | `ADVERS_time_end_boundary_exact_active` |
| `icao.rfind(prefix,0)` → `find` | `ConstraintChecks.hpp` | `ADVERS_dest_substring_not_prefix_passes` |

Each mutation broke exactly one case and nothing else. Additional adversarial cases cover:
- `max: 0` sentinel (no upper ceiling — the `> 0` guard in `checkOneMinMax`)
- `route` + `noroute` interaction (more-specific noroute blocks even when route prefix matches)
- `nodests` evaluation order (fires before `dests` can whitelist)
- Pattern-length guard in `cc_routeHas` (too-long `noroute` pattern is silently skipped)

---

## Field naming: exit-point vs. route checks

A clarification locked in by the `RouteSyntaxTests` mid-stack cases:

| JSON field | Check function | Round | Uses |
|---|---|---|---|
| `points` / `nopoints` | `checkOneExitPoint` | Round 1 | Raw `route_points` (pre-normalisation) |
| `route` / `noroute` | `checkOneRoute` | Round 2 | Normalised route (post-`RouteSyntax`) |

These are completely separate fields. A constraint with `points: ["WOD"]` is an
exit-point constraint, not a route constraint.

---

## Test executable structure

`VFPC_Tests` is a standalone console application, second project in `VFPC.sln`. It:

- Uses **Google Test v1.15.2** (submodule at `VFPC_Tests/third_party/googletest/`)
- Uses **RapidJSON v1.1.0** (submodule at `VFPC_Tests/third_party/rapidjson/`)
  — isolated from the plugin's vendored `lib/include/rapidjson/` (v0.11) via include-path order
- Requires Visual Studio 2022 with the **"Desktop development with C++"** workload
- Builds to `bin/Release/tests/VFPC_Tests.exe`
- Has no EuroScope SDK dependency, no curl, no PCH

**To build and run from the command line:**
```bat
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
msbuild VFPC.sln /t:VFPC_Tests /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal
bin\Release\tests\VFPC_Tests.exe
```

**To run only the fixture-driven batch suite:**
```bat
bin\Release\tests\VFPC_Tests.exe --gtest_filter="ConstraintBatch.*"
```

**To add a new fixture case:** edit `VFPC_Tests/fixtures/constraint_cases.json` and
rerun the executable. No recompile needed.

---

## Day-of-week convention

`checkTimeWindow()` uses **Monday = 0** (after the `(day+6)%7` conversion from the
server's Sunday-0 representation):

| Value | Day |
|-------|-----|
| 0 | Monday |
| 1 | Tuesday |
| 2 | Wednesday |
| 3 | Thursday |
| 4 | Friday |
| 5 | Saturday |
| 6 | Sunday |

`ValidatorCore::validateFlightPlan()` passes `day/hour/min` directly to `checkTimeWindow`
using the same Monday-0 convention.

---

## Known remaining gaps

| Gap | Notes |
|---|---|
| Active-airport locking / IFR gating | Hard dependency on EuroScope plugin instance state. Cannot be tested without a live EuroScope session. |
| Output string formatters | `BansOutput`, `WarningsOutput`, `DirectionOutput`, `MinMaxOutput`, `RestrictionsOutput`, etc. are display-only and not covered. |
| Mirror drift risk | `SidLookup.hpp`, `FlightPlanRow.hpp`, `ValidatorCore.hpp`, and `RouteSyntax.hpp` mirror logic from `analyzeFP.cpp`. If production code changes, these mirrors must be manually kept in sync. |
| Destination case sensitivity | `rfind(prefix,0)` is case-sensitive. If EuroScope ever passes a lowercase ICAO, the block would be silently missed. Plugin always uses uppercase in practice. |

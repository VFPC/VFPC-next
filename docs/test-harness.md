# VFPC-next Test Harness

**Branch:** `test-harness/main`
**Base branch:** `main`
**Purpose:** standalone verification harness for the pure constraint-evaluation logic in Peter's recovery line
**Status:** active sidecar lane — not proposed for Peter's trunk unless Peter requests it

**Results (last run: 2026-04-04):**
```
[==========] 69 tests from 14 test suites ran.
[  PASSED  ] 69 tests.
Batch result: 77 passed (total 77)
```

---

## What this harness actually tests — and what it does not

### What IS tested

The harness covers the **evaluation kernel**: given a constraint object and a flight's
properties, does the evaluator return the right answer?

Specifically it tests all six pure evaluation functions from `src/ConstraintChecks.hpp`
and the time-window check from `src/TimeWindow.h`:

| Function | Tests | What it checks |
|---|---|---|
| `checkTimeWindow()` | 48 (Groups 1–7) | Day-of-week + time boundary logic, all window shapes |
| `checkOneDestination()` | fixture + unit | `dests` / `nodests` prefix matching |
| `checkOneExitPoint()` | fixture + unit | `points` / `nopoints` matching |
| `checkOneRoute()` | fixture + unit | Route prefix matching, `noroute`, wildcards |
| `checkOneMinMax()` | fixture + unit | FL band boundaries, zero-sentinel guards |
| `checkOneDirection()` | fixture + unit | EVEN/ODD RVSM direction rule |
| `checkOneAlerts()` | fixture + unit | `ban` / `warn` alert semantics |

The fixture-driven batch layer (`VFPC_Tests/fixtures/constraint_cases.json`) currently
has **77 cases** covering:

- 28 original constraint-field cases
- 14 time-windowed cases (constraint + simulated time)
- 9 FL boundary cases (inclusive boundary semantics)
- 10 route-prefix edge cases (prefix length, wildcards, noroute)
- 12 SRD-derived cases (translated from real AIRAC constraint structures in `SrdTraceSemanticTests.cs`)
- 10 adversarial cases with confirmed mutation-catching power (see below)

### What is NOT tested

**The constraint-selection step.** In the real plugin, before any of these functions
run, the plugin must:

1. Read `out.json` from the API
2. Find the constraints that apply to the **airport currently being controlled**
3. Find the constraints that apply to the **SID the aircraft is departing on**
4. For each applicable constraint, evaluate it against the flight

None of steps 1–3 are exercised by this harness. The harness starts at step 4 — with
a constraint already in hand — and tests whether that constraint is evaluated correctly.

Concretely: a bug in how the plugin navigates the `out.json` airport/SID tree to select
which constraints to evaluate would **not** be caught here. That part of the code path
has a hard dependency on EuroScope's `CFlightPlan` and the plugin's JSON-loading state,
which means it cannot be tested from a standalone executable without faking a session.

**In short:**

```
out.json
  └── find airport (EGAA)             ← NOT TESTED
        └── find SID (BELZU...)       ← NOT TESTED
              └── for each constraint
                    └── checkTimeWindow()         ← TESTED
                    └── checkOneDestination()     ← TESTED
                    └── checkOneRoute()           ← TESTED
                    └── checkOneMinMax()          ← TESTED
                    └── checkOneDirection()       ← TESTED
                    └── checkOneAlerts()          ← TESTED
```

---

## Adversarial cases and mutation verification

Ten of the 77 fixture cases are explicitly adversarial — designed to catch specific
plausible code mistakes rather than just verify happy-path behaviour. Three mutations
were applied to the production source, run through the harness, then reverted:

| Mutation | Location | Case that caught it |
|---|---|---|
| `currentMin >= starttime[1]` → `>` | `TimeWindow.h` | `ADVERS_time_start_boundary_exact_active` |
| `currentMin <= endtime[1]` → `<` | `TimeWindow.h` | `ADVERS_time_end_boundary_exact_active` |
| `icao.rfind(prefix,0)` → `find` | `ConstraintChecks.hpp` | `ADVERS_dest_substring_not_prefix_passes` |

Each mutation broke exactly one case and nothing else. The adversarial cases also cover:
- `max: 0` sentinel (no upper ceiling — the `> 0` guard in `checkOneMinMax`)
- `route` + `noroute` interaction (more-specific noroute blocks even when route prefix matches)
- `nodests` evaluation order (fires before `dests` can whitelist)
- Pattern-length guard in `cc_routeHas` (too-long `noroute` pattern is silently skipped)

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

---

## Known remaining gaps

| Gap | Notes |
|---|---|
| Above-FL410 direction logic | `checkOneDirection` has a separate formula for RVSM upper (`>41000 ft`). No test cases cover FL430+. Low priority for UK departures but the formula is unverified. |
| `nopoints` field | `checkOneExitPoint` has a `nopoints` (forbidden exit) path. Covered by unit tests in `ApiConstraintTests` but not yet in the fixture batch. |
| Constraint selection (airport/SID navigation) | Structural gap — requires EuroScope session machinery. See "What is NOT tested" above. |
| Destination case sensitivity | `rfind(prefix,0)` is case-sensitive. If EuroScope ever passes a lowercase ICAO, the block would be silently missed. Plugin always uses uppercase in practice. |

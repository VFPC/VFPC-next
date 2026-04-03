# VFPC-next Test Harness

**For:** `VFPC-next` branch `test-harness/peter-recovery-2026-04-02`
**Base branch:** `peter-recovery-2026-04-02`
**Purpose:** keep a standalone harness attached to Peter's line so it can move forward with the branch that is expected to become trunk later
**Status:** harness wired into the repo; current scope covers `checkTimeWindow()` plus extracted pure constraint checks

**Results (last run: 2026-04-03):**
```
[==========] 67 tests from 13 test suites ran.
[  PASSED  ] 67 tests.
```

---

## The problem with testing this code

`checkRestriction()` takes a `CFlightPlan` object as its first parameter. `CFlightPlan` is an EuroScope SDK type — you cannot instantiate one outside of a running EuroScope session. This means the function cannot be called from a standalone test executable in its current form.

There are two ways to deal with this:

**Option A — Extract first, then test**

Pull the pure time/date logic out of `checkRestriction()` into a free function that takes only plain data types (integers, strings). Test that function directly. This is the right long-term answer and aligns with the refactoring goal, but requires a code change beyond the four bug fixes.

**Option B — Test via a thin wrapper**

Write a minimal test wrapper that constructs the minimum fake state needed to call `checkRestriction()` — populating `timedata` directly and passing a mock or minimal `CFlightPlan`. The EuroScope SDK header is already in the repo at `lib/include/EuroScopePlugIn.h`. A stub that satisfies the interface without a running EuroScope may be feasible.

**Implemented in this bootstrap harness:** Option A for the stable seam that already exists in Peter's branch. The time/date logic is available as `checkTimeWindow()` in `src/TimeWindow.h`. This function takes only plain data types (`int day, int hour, int min, const rapidjson::Value& restriction`) and has no EuroScope dependency. That makes it the right first anchor for a harness that needs to survive ongoing refactor work.

The next stable slice is now in place as well: a pure `ConstraintChecks.hpp` seam extracted for direct tests of destination, exit-point, route, min/max, direction, and alert evaluation. That keeps the harness growing at the pure-logic boundary rather than trying to fake a EuroScope session.

---

## Test executable structure

`VFPC_Tests` is a standalone console application added as a second project in `VFPC.sln`. It:

- Uses **Google Test v1.15.2** (git submodule at `VFPC_Tests/third_party/googletest/`)
- Requires Visual Studio 2022 with the **"Desktop development with C++"** workload
- Includes `src/TimeWindow.h`, `src/ConstraintChecks.hpp`, and `lib/include/rapidjson/` — no EuroScope SDK, no curl, no PCH
- Builds to `bin/Debug/tests/VFPC_Tests.exe`
- Uses parameterised `TEST_P` / `INSTANTIATE_TEST_SUITE_P` pattern, one suite per restriction group

## Branching rule

- `peter-recovery-2026-04-02` is treated as the evolving Peter line and expected future trunk candidate
- `test-harness/peter-recovery-2026-04-02` is branched from it directly
- Harness work should target `peter-recovery-2026-04-02`, not current `main`
- If Peter's line later becomes the default branch, the harness branch can be rebased or merged forward without changing its conceptual parent lane

**To build and run from the command line:**
```bat
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
msbuild VFPC.sln /t:VFPC_Tests /p:Configuration=Debug /p:Platform=x64
bin\Debug\tests\VFPC_Tests.exe
```

**To build and run from Visual Studio:** right-click `VFPC_Tests` → Build, then set as startup project and run.

---

## Convention reminder

`timedata[5]` uses **Monday=0** convention internally (after the `(day+6)%7` conversion from the server's Sunday=0):

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

## Test cases

Each case specifies: the restriction JSON, the simulated current time (`timedata`), and the expected result.

---

### Group 1 — Wrap-around weekday (Bug 1 fix)

Restriction: `{ "start": { "date": 4, "time": "1600" }, "end": { "date": 0, "time": "0745" } }`
(Friday 16:00 → Monday 07:45 — the most common pattern in the dataset)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| W01 | Saturday (5) | 12:00 | PASS | Interior wrap day |
| W02 | Sunday (6) | 12:00 | PASS | Interior wrap day |
| W03 | Friday (4) | 16:00 | PASS | Start boundary, exactly at start time |
| W04 | Friday (4) | 16:30 | PASS | Start boundary, after start time |
| W05 | Friday (4) | 15:59 | FAIL | Start boundary, before start time |
| W06 | Monday (0) | 07:44 | PASS | End boundary, before end time |
| W07 | Monday (0) | 07:45 | PASS | End boundary, exactly at end time |
| W08 | Monday (0) | 07:46 | FAIL | End boundary, after end time |
| W09 | Tuesday (1) | 12:00 | FAIL | Outside wrap range — was incorrectly PASS before fix |
| W10 | Wednesday (2) | 12:00 | FAIL | Outside wrap range — was incorrectly PASS before fix |
| W11 | Thursday (3) | 12:00 | FAIL | Outside wrap range — was incorrectly PASS before fix |

---

### Group 2 — Same-day restriction (Bug 2 fix)

Restriction: `{ "start": { "date": 0, "time": "0900" }, "end": { "date": 0, "time": "1615" } }`
(Monday 09:00 → Monday 16:15 only)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| S01 | Monday (0) | 10:00 | PASS | Correct day, inside window |
| S02 | Monday (0) | 09:00 | PASS | Correct day, exactly at start |
| S03 | Monday (0) | 08:59 | FAIL | Correct day, before window |
| S04 | Monday (0) | 16:15 | PASS | Correct day, exactly at end |
| S05 | Monday (0) | 16:16 | FAIL | Correct day, after window |
| S06 | Tuesday (1) | 10:00 | FAIL | Wrong day — was incorrectly PASS before fix |
| S07 | Wednesday (2) | 10:00 | FAIL | Wrong day — was incorrectly PASS before fix |
| S08 | Saturday (5) | 10:00 | FAIL | Wrong day — was incorrectly PASS before fix |
| S09 | Sunday (6) | 10:00 | FAIL | Wrong day — was incorrectly PASS before fix |

---

### Group 3 — Adjacent overnight (non-wrap, existing logic — regression check)

Restriction: `{ "start": { "date": 0, "time": "1700" }, "end": { "date": 1, "time": "0815" } }`
(Monday 17:00 → Tuesday 08:15)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| A01 | Monday (0) | 17:00 | PASS | Start boundary, at start time |
| A02 | Monday (0) | 17:30 | PASS | Start boundary, after start time |
| A03 | Monday (0) | 16:59 | FAIL | Start boundary, before start time |
| A04 | Tuesday (1) | 08:14 | PASS | End boundary, before end time |
| A05 | Tuesday (1) | 08:15 | PASS | End boundary, at end time |
| A06 | Tuesday (1) | 08:16 | FAIL | End boundary, after end time |
| A07 | Wednesday (2) | 20:00 | FAIL | Outside range entirely |
| A08 | Sunday (6) | 20:00 | FAIL | Outside range entirely |

---

### Group 4 — Wrap-around overnight (regression check for Friday→Monday overnight variant)

Restriction: `{ "start": { "date": 4, "time": "1700" }, "end": { "date": 0, "time": "0815" } }`
(Friday 17:00 → Monday 08:15)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| O01 | Friday (4) | 17:30 | PASS | Start boundary, after start |
| O02 | Friday (4) | 16:59 | FAIL | Start boundary, before start |
| O03 | Saturday (5) | 12:00 | PASS | Interior wrap day |
| O04 | Sunday (6) | 12:00 | PASS | Interior wrap day |
| O05 | Monday (0) | 08:00 | PASS | End boundary, before end |
| O06 | Monday (0) | 08:16 | FAIL | End boundary, after end |
| O07 | Tuesday (1) | 12:00 | FAIL | Outside wrap range |
| O08 | Thursday (3) | 12:00 | FAIL | Outside wrap range |

---

### Group 5 — Date-only range, no time (regression check for existing working logic)

Restriction: `{ "start": { "date": 0 }, "end": { "date": 4 } }`
(Monday through Friday, no time constraint)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| D01 | Monday (0) | 03:00 | PASS | In range |
| D02 | Wednesday (2) | 14:00 | PASS | In range |
| D03 | Friday (4) | 23:00 | PASS | In range |
| D04 | Saturday (5) | 12:00 | FAIL | Outside range |
| D05 | Sunday (6) | 12:00 | FAIL | Outside range |

---

### Group 6 — Time-only, no date (regression check for existing logic)

Restriction: `{ "start": { "time": "0800" }, "end": { "time": "2000" } }`
(08:00–20:00 any day)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| T01 | Monday (0) | 08:00 | PASS | At start |
| T02 | Thursday (3) | 14:00 | PASS | Inside window |
| T03 | Sunday (6) | 20:00 | PASS | At end |
| T04 | Wednesday (2) | 07:59 | FAIL | Before window |
| T05 | Friday (4) | 20:01 | FAIL | After window |

---

### Group 7 — Time-only, midnight wrap (regression check for existing logic)

Restriction: `{ "start": { "time": "2300" }, "end": { "time": "0100" } }`
(23:00–01:00, wraps midnight)

| ID | Simulated day | Simulated UTC time | Expected | Reason |
|----|--------------|-------------------|----------|--------|
| M01 | Monday (0) | 23:00 | PASS | At start |
| M02 | Tuesday (1) | 00:30 | PASS | Inside wrap window |
| M03 | Friday (4) | 01:00 | PASS | At end |
| M04 | Saturday (5) | 22:59 | FAIL | Before window |
| M05 | Sunday (6) | 01:01 | FAIL | After window |

---

## Cases that specifically demonstrate the bugs were real

The following subset is the minimum needed to show the maintainer that the bugs existed and are now fixed. These are the cases that would have returned the wrong result before the fixes:

| ID | Description | Before fix | After fix |
|----|-------------|-----------|-----------|
| W09 | Fri→Mon restriction, tested on Tuesday | PASS (wrong) | FAIL (correct) |
| W10 | Fri→Mon restriction, tested on Wednesday | PASS (wrong) | FAIL (correct) |
| W11 | Fri→Mon restriction, tested on Thursday | PASS (wrong) | FAIL (correct) |
| S06 | Mon 09-16 restriction, tested on Tuesday | PASS (wrong) | FAIL (correct) |
| S07 | Mon 09-16 restriction, tested on Wednesday | PASS (wrong) | FAIL (correct) |
| S08 | Mon 09-16 restriction, tested on Saturday | PASS (wrong) | FAIL (correct) |

---

## Notes on the out-of-bounds bug (Bug 3)

The loop-bound fix (Change 1) cannot easily be tested with a pass/fail assertion — the original code accessed `lastupdate[5]` which is undefined behaviour. It may crash, silently corrupt memory, or do nothing depending on the runtime. The fix is verified by code inspection only. Running the fixed code under AddressSanitizer or Valgrind (if building on Linux via MinGW) would confirm the access violation is gone.

## Next expansion

- add runtime or fixture-backed coverage once Peter's line exposes a stable data-loading seam worth testing outside EuroScope
- keep extracting pure helpers from Peter's line rather than copying broad `VFPC` mainline helpers blindly
- keep the harness branch small and rebased on Peter's line instead of turning it into a parallel implementation branch

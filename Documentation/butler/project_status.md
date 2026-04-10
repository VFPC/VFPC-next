# VFPC-next Project Status

_Last updated: 2026-04-10 (session 2026-04-10)_

---

## Status: ACTIVE DEVELOPMENT

VFPC-next is the C++ plugin repository in the process of being restructured with a
standalone test harness (`VFPC_Tests`) and tooling (`VFPC_Check`).

---

## What Exists

### Plugin core (`src/`)
- `analyzeFP.cpp` / `analyzeFP.hpp` — main validation logic (EuroScope-coupled)
- Standalone seams extracted for testing:
  - `FlightPlanRow.hpp` — plain data, no EuroScope dependency
  - `SidLookup.hpp` — airport extraction + SID index lookup
  - `RouteSyntax.hpp` — route normalisation and syntax validation
  - `ConstraintChecks.hpp` — per-round constraint evaluation functions
  - `ValidatorCore.hpp` — mid-stack entry point; full 7-round constraint loop

### Test harness (`VFPC_Tests/`)
- GTest suite covering `SidLookup`, `RouteSyntax`, `ValidatorCore`,
  `ConstraintChecks`, `TimeWindow`, and fixture-driven API constraint tests
- `fixtures/constraint_cases.json` — batch test cases

### CLI tool (`VFPC_Check/`)
- `vfpc_check.exe` — standalone offline route validator
- Accepts `out.json` (bare array or `{cycle,airports:[...]}` envelope) +
  flight-plan JSON; prints PASS/FAIL with round detail
- Exit codes: 0=PASS or NOT CHECKED (VFR), 1=FAIL/ERR, 2=tool error
- Required input fields: `origin`, `destination`, `route`
- Derived automatically: `first_waypoint`, `sid_suffix` (always from SID)

---

## Open PRs (not from this session — do not merge without owner review)

| PR | Branch | Owner | Notes |
|----|--------|-------|-------|
| #3 | peter-clean-port-2026-04-03 | Peter | Peter's port work |
| #5 | ari/fix-sid-applicability | Ari | SID restriction fix |

---

## What Needs Work

- `test(harness): add fixture-driven FlightPlanRow runner against real out.json`
  (board item — Next priority, blocked on Peter having feedback from CLI first)
- PR #5 (Ari's SID fix) needs review and merge
- PR #3 (Peter's port) needs review

---

## Repo Layout

```
VFPC-next/
  src/                    Plugin source + standalone seam headers
  VFPC_Tests/             GTest harness
  VFPC_Check/             vfpc_check CLI tool
  Documentation/butler/   Session history (this file)
  docs/                   Design docs (test-harness.md)
```

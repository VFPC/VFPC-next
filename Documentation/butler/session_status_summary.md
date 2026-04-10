# VFPC-next Session Status Summary

_Most recent session first._

---

### Session 2026-04-10: vfpc_check offline route validation CLI

**Scope:** VFPC-next

**vfpc_check CLI — PR #9 merged to main (`040151bd`):**
- Built `VFPC_Check/main.cpp` — standalone Windows console tool that runs the full
  `ValidatorCore` constraint-evaluation path against a real `out.json` with no
  EuroScope or Sweatbox dependency
- Added `VFPC_Check/VFPC_Check.vcxproj` (Console Application, reuses rapidjson from
  `VFPC_Tests/third_party`; outputs `vfpc_check.exe`)
- Wired into `VFPC.sln`; `.gitignore` exception added for `VFPC_Check/`
- Five rounds of Ari patch review; each finding addressed before merge:
  - Round 1: SYN advisory vs hard ERR distinction; input normalisation (uppercase,
    `#` strip, `first_waypoint`/`sid_suffix` derivation from SID)
  - Round 2: VFR plans now exit 0 with NOT CHECKED; missing route text now ERR exit 1
    (not SYN advisory — `VF_SYNTAX="ERR"` vs `"Invalid Syntax"` is the key distinction)
  - Round 3: missing destination now ERR exit 1; `route_points`-only input rejected
    (plugin always has raw route string alongside waypoints)
  - Round 4: missing origin now ERR exit 1 (was exit 2); `first_waypoint`/`sid_suffix`
    JSON overrides removed — always derived from SID, no caller override permitted

**Key design decisions:**
- Plugin-equivalence is the hard contract: every ERR gate, VFR short-circuit, and
  normalisation step mirrors the exact code path in `analyzeFP.cpp`
- `FinalizeValidationResult_` / `FieldIndicatesFailure_` is the final arbiter:
  `"ERR"` → hard failure; `"Invalid Syntax"` → advisory; `"Not Checked"` → not a failure
- `first_waypoint` and `sid_suffix` are always derived — no JSON override path, matching
  `ParseFlightPlanRow_` in `analyzeFP.cpp` which never allows external override

**Validation:** Release x64 builds clean; smoke-tested against 2603 out.json

**Commits on feat/vfpc-check-cli (squashed into main):**
- `040151bd` — feat(check): add vfpc_check offline route validation CLI (#9)

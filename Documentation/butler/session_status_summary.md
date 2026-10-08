# VFPC-next — Session Status Summary

---

### Session 2026-10-08: branch hygiene pass — test-harness lane and `ari/fix-sid-applicability` retired

**Scope:** VFPC-next local clone + remote branches (no plugin code touched)

**Context:** Triggered by a Hub-side review of Peter's new user-guide PDF and
`README_revision_2.md` draft (that review changed no `VFPC-next` files). While
confirming local clone state, found it sitting on the stale `fix/route-selector-minimal`
branch with 4 dirty working-tree files, which prompted a wider look at the branch list
this repo's own docs had already flagged as cleanup candidates back on 2026-10-04.

**Dirty working tree resolved:** 3 of the 4 modified files (`Documentation/butler/`'s
three files) were byte-identical to `origin/main` — pure CRLF noise, confirmed via
`git diff --stat` showing zero lines changed. The 4th, `docs/vfpc-next-supplement.md`,
had a small 7-line improvement sitting on an otherwise globally-stale file — not worth
preserving in isolation. All 4 discarded (`git checkout --`), then switched to `main`.
Exposed an untracked `VFPC_Tests/third_party/{googletest,rapidjson}` leftover (vendored
submodule checkouts git couldn't `rmdir` on branch switch due to nested `.git` dirs) —
force-cleaned with `git clean -ffdx`.

**5 branches deleted**, each individually verified safe before deletion (PR state via
`gh pr list`, ancestry via `git merge-base --is-ancestor`, content via `git diff --stat`
against `main`):
- `chore/remove-unused-test-harness` (PR #26, `MERGED`) — local-only, force-deleted
  (squash merge, expected non-ancestor)
- `docs/remove-stale-test-harness-doc` (PR #28, `MERGED`) — local-only, force-deleted
- `test-harness/main` — no PR ever opened; diff vs `main` was 100% the old
  `VFPC_Tests`/rapidjson-vendor/googletest-submodule content PR #26 already removed, plus
  stale butler-doc snapshots. Remote (`origin/test-harness/main`) and local both deleted.
- `test/plugin-mutation-coverage` — local-only, same obsolete harness content plus a copy
  of the already-"never used" `scripts/vfpc_plugin_test.py` Python mutation runner.
  Force-deleted.
- `ari/fix-sid-applicability` — PR #5, `MERGED` 2026-04-11, confirmed a **true ancestor**
  of `main` (real merge, not squashed) — cleanest possible case. Its separate worktree
  (`VFPC-next-ari-sid-applicability`) removed first, then remote and local branch deleted
  with a plain `git branch -d` (no force needed).

**Net effect:** local branches 10 → 3 (`main`, `fix/route-selector-in-restrictions`,
`fix/route-selector-minimal`); remote branches 12 → 10. `git status -sb` clean on `main`,
in sync with `origin/main`, zero untracked files.

**Not touched** (flagged, needs explicit confirmation before any further deletion):
`Refactor-of-FP-validation-code` (Peter's WIP, remote gone), 4 old `backup/*`/`cleanup/*`
refs from the April recovery incident, `v4.0.0-Beta3` (already merged, remote-only),
`fix/route-selector-in-restrictions`/`fix/route-selector-minimal` (superseded by issue
#13's independent fix but not yet confirmed for deletion), `Release-Candidate-05102026`
(Peter's current live branch, not stale).

**Butler docs updated:** all three files in this directory, reflecting the new branch
count and the trimmed "remaining cleanup" list.

---

### Session 2026-10-04: sidecar harness retired; v4.0.0.0 release PR opened for Peter's review

**Scope:** VFPC-next `main`

**Context:** Peter pushed `245b3bbc` ("VFPC Release Candidate v4.0.0.0") to `v4.0.0-Beta3`
around 11:34 UK time. Before opening a release PR, found that his cleanup (trimming `src/`
to only build-relevant files) deletes several headers — `SidApplicability.hpp`,
`ValidatorCore.hpp`, `RouteSyntax.hpp`, `ConstraintChecks.hpp`, `FlightPlanRow.hpp`,
`SidLookup.hpp`, `UrlData.h/.cpp` — that the dormant `VFPC_Tests`/`VFPC_Check` sidecar
projects (see all session entries below this one) still referenced by path. Confirmed via
direct inspection that neither sidecar project was ever wired into `VFPC.sln` or any CI
workflow, and J. Kino confirmed the harness was never actually used.

**PR #26 merged** (`chore/remove-unused-test-harness` → `main`): removed `VFPC_Tests/`,
`VFPC_Check/`, and the resulting empty `.gitmodules` (googletest + rapidjson vendor
submodules). This landed first so the release PR wouldn't carry forward dangling
references to the deleted headers.

**PR #27 opened** (`v4.0.0-Beta3` → `main`, "VFPC Release Candidate v4.0.0.0"): Peter
requested as reviewer. Not merged — waiting on his sign-off.

**Butler docs rewritten:** `next_session_prompt.md` and `project_status.md` updated to
drop the now-retired sidecar-lane framing and reflect current reality (this repo is just
Peter's production plugin; these docs exist solely for AI-session continuity, Peter does
not use them). This file's entries below (sessions 91–93) are preserved as historical
record of why `VFPC_Tests`/`VFPC_Check` existed — they describe removed code and should
not be treated as current state.

---

### Session 2026-04-11 (session 93): OutJsonSeedTests; PR #5 merged; SidApplicability cleanup

> **Note (2026-10-04): everything in this entry and the two below it describes the
> `VFPC_Tests`/`VFPC_Check` sidecar lane, which was removed in PR #26. Historical record
> only — do not act on anything here.**

**Scope:** VFPC-next `main` + `test-harness/main`

**OutJsonSeedTests (issue #8 — closed):**
- New `VFPC_Tests/OutJsonSeedTests.cpp`: loads `fixtures/out_json_seeds.json` + live `out.json`,
  drives each seed through `ValidatorCore::validateFlightPlan()`, asserts `overall_pass == true`
- Fixture loader enforces all replay-contract fields (`first_waypoint`, `sid_suffix`, `is_ifr`,
  `rfl`, `route_points`, etc.) — missing or wrongly-typed fields fail fast in `SeedsFileLoads`
- `rowFromSeed()` unconditional reads; no synthetic fallbacks
- Python harness updated: `first_waypoint`/`sid_suffix` now stored using same derivation as plugin
- User guide at `Documentation/out-json-seed-tests.md`

**PR #5 merged (issue #4 — closed):**
- `SidApplicability.hpp` — `ShouldEvaluateConstraints()` fixes RST-for-jets on type-gated SIDs
- One-line change in `analyzeFP.cpp` at line 1862; rebased cleanly onto `main`
- Follow-up: removed `using namespace rapidjson` from header; all types now explicitly qualified

**Issues #7, #10:** Closed — Peter fixed both locally (described in issue comments); not yet pushed.

**PR #3 decision:** Leave open as Peter's working branch. Do not merge until Peter says it's ready
AND confirms his new validation logic handles the SID applicability case (or incorporates
`SidApplicability::ShouldEvaluateConstraints`). Rebase conflict in `analyzeFP.cpp` is expected.

**Validation:** 125/125 GTest tests passing on `test-harness/main`.

**Commits on `main`:**
- `0e78ddc6` — fix: allow constraint evaluation when no SID restriction applies (PR #5)
- `dcaad1c2` — fix(SidApplicability): remove using namespace rapidjson from header

**Commits on `test-harness/main`:**
- `e4eb5126` — test(harness): add fixture-driven OutJsonSeedTests GTest suite
- `e74edd94` — fix(harness): store and replay exact first_waypoint/sid_suffix
- `afc62acf` — fix(harness): enforce replay-contract fields in fixture loader
- `aee1a01b` — docs: add OutJsonSeedTests user guide

---

### Session 2026-04-11 (session 92): plugin test harness hardened; --verbose flag added

**Scope:** VFPC-next (Python test harness — `C:\Temp\vfpc_plugin_test.py`)

**vfpc_plugin_test.py — harness hardening:**
- Fixed `TYPE_VIOLATION` guard: changed from `baseline_eng != "J"` to checking whether `"J"` is
  explicitly present in any restriction's `types` list. LISTO's restriction is an allowlist-redirect
  for props; jets are simply unmentioned and therefore unrestricted — the old guard suppressed the
  right cases for the wrong reason.
- Fixed `MISSING_POINT` guard: only emit when every sibling constraint in the SID entry also has a
  `points` field. Siblings without `points` survive the mutation and produce PASS_WARN (EGAA oceanic).
- Fixed `DIRECTION_FLIP` guard: same `all_siblings_have_points` rule applied. EGAA oceanic siblings
  without `points` survive the direction-flipped route, producing PASS_WARN via a BanAlert.
- Fixed `RFL_ABOVE_MAX` guard: skip when any sibling has no `max` field. LUVUM has an unbounded
  sibling that accepts any RFL, causing the above-max mutation to pass.
- Fixed `ROUTE_MISMATCH` guard: tightened `MAX_SIBLINGS` from 30 → 20; changed sibling filter
  from "empty route only" to "empty route AND no points" — a constraint needs both absent to survive
  a junk route. EGBB BRUMI (exactly 30 siblings) now correctly falls outside the cap.
- Added `--verbose` / `-v` flag: prints every baseline flight plan (one-line summary + reproducible
  JSON) and every mutation input under each result. Normal run stays terse.

**Final run result:** 53 seeds, 121 mutations, **0 wrong outcomes**. Runtime ~23 s.

**Artifacts written:** `C:\Temp\vfpc_seeds.json`, `C:\Temp\vfpc_discarded.json`,
`C:\Temp\vfpc_mutations.json`

**No VFPC-next C++/GTest changes this session.**

---

### Session 2026-04-10 (session 91): vfpc_check offline CLI merged (PR #9)

**Scope:** VFPC-next

**VFPC-next — vfpc_check CLI (PR #9 merged → `040151bd`):**
- Built `VFPC_Check/main.cpp` — standalone Windows console tool that runs the full
  `ValidatorCore` 7-round constraint evaluation against a real `out.json` with no
  EuroScope or Sweatbox dependency; for Peter to test routes without the live plugin
- Added `VFPC_Check/VFPC_Check.vcxproj` (Console Application; outputs `vfpc_check.exe`,
  ~433 KB Release x64, no DLLs; reuses rapidjson from `VFPC_Tests/third_party`)
- Five rounds of Ari patch review; all findings addressed before merge:
  - Round 1: SYN advisory vs hard ERR; input normalisation (uppercase, `#` strip,
    `first_waypoint`/`sid_suffix` derived from SID, not from JSON overrides)
  - Round 2: VFR plans exit 0 NOT CHECKED; missing route text → ERR exit 1 (not SYN)
  - Round 3: missing destination → ERR exit 1; `route_points`-only input rejected
  - Round 4: missing origin → ERR exit 1 (was exit 2); `first_waypoint`/`sid_suffix`
    JSON overrides removed — always derived from SID
- Key design principle: `FinalizeValidationResult_` / `FieldIndicatesFailure_` is the
  arbiter — `"ERR"` is hard failure, `"Invalid Syntax"` is advisory, `"Not Checked"` is not a failure
- VFPC-next butler docs created (this is the first session for this repo's butler)

**Validation:** Release x64 builds clean; smoke-tested against 2603 out.json
(PASS, FAIL, SYN, VFR, ERR cases all verified)

**Commits pushed to VFPC-next:**
- `040151bd` — feat(check): add vfpc_check offline route validation CLI (#9)
- `f3d2d768` — docs: create butler docs — session 2026-04-10 vfpc_check CLI

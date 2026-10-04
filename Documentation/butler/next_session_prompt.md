# VFPC-next — Next Session Prompt

_Last updated: 2026-10-04 (session: sidecar harness retired; v4.0.0.0 released)_

---

## Important: these docs are for AI-session continuity only

Peter does not read or maintain `Documentation/butler/`. This directory exists so an AI
assistant picking up this repo in a future session has accurate state without re-deriving
it from scratch. Keep it honest and current — if it goes stale again, the next session
will waste time rediscovering what's below.

---

## Current State

**`main`:** `19761a9a` — clean. **v4.0.0.0 is released** (PR #27 merged, real merge
commit, consistent with how Beta2 landed via PR #15). This is now **just Peter's
production VFPC plugin repo**. The sidecar verification lane (`VFPC_Tests` GTest harness,
`VFPC_Check` CLI) described in older entries below was **removed 2026-10-04** (PR #26) —
it was never wired into `VFPC.sln`, had no CI build step, and per J. Kino was "never
used." Do not resurrect it without explicit instruction.

**`v4.0.0-Beta3` (merged into `main` via PR #27, 2026-10-04):** Peter's **v4.0.0.0 release**
(`245b3bbc`, pushed 2026-10-04 ~11:34 UK time). Summary of what it does:
1. Trims `src/` to only files actually part of the build (removed several now-dead headers:
   `SidApplicability.hpp`, `ValidatorCore.hpp`, `RouteSyntax.hpp`, `ConstraintChecks.hpp`,
   `FlightPlanRow.hpp`, `SidLookup.hpp`, `UrlData.h/.cpp`).
2. `README.md` rewritten to build-only instructions; full MkDocs User Guide site added
   under `Documentation/docs/`.
3. Fixed a config-loader message bug (was reporting "default value applied" when the real
   issue was an unknown field; now names the unknown field).
4. Removed commented-out code; resource/version files updated to show DEBUG builds in
   DLL Properties->Details; version bumped to `v4.0.0.0`, all beta references removed.

PR #27 was closed once by accident (Peter clicked "Close pull request" thinking it
finalized his review -- he'd already left "Reviewed and looks okay to me" at the same
timestamp). Reopened via `gh pr reopen`, nothing lost. Given he authors this code himself
and doesn't use GitHub's formal review UI naturally, got his actual go-ahead via a plain
"are you happy to ship this" question rather than routing through Files-changed/
Review-changes -- worth defaulting to that lighter path for his future release PRs.
Merged 2026-10-04 16:32 UTC as `19761a9a`. No CI/build/publish step is wired to `main`
(only `cacheclear.yml`), so the merge triggered no automatic distribution; if it ever
needs undoing, GitHub's one-click "Revert" on the merged PR is clean and complete.

**PR #3 (`peter-clean-port-2026-04-03`, Peter's earlier rewrite attempt):** **Closed, never
merged.** Superseded entirely by the `v4.0.0-Beta3` line (Builds 0–6, now the RC in PR #27).
The old "PR #3 decision" guidance below is historical only — do not act on it.

---

## What Was Completed (2026-10-04 session)

- **PR #26 merged** (`chore/remove-unused-test-harness` → `main`): removed `VFPC_Tests/`,
  `VFPC_Check/`, and the now-empty `.gitmodules` (the two vendored submodules —
  googletest, rapidjson — were under `VFPC_Tests/third_party/`). Confirmed via direct
  inspection that neither project was referenced by `VFPC.sln` or any CI workflow before
  removing.
- **PR #28 merged**: removed `docs/test-harness.md` (fully described the deleted
  harness). Left `docs/vfpc-next-supplement.md` alone -- its mentions are scoped to the
  still-separate `Refactor-of-FP-validation-code` branch, which still has both sidecar
  directories intact.
- **PR #27 merged**: Peter's `v4.0.0-Beta3` → `main`, **v4.0.0.0 released**. See "Current
  State" above for the close/reopen incident and final merge details.

---

## Immediate Next Steps

1. v4.0.0.0 is released on `main`. No GitHub tag or formal Release was cut as part of
   this merge -- if Peter wants one, that's a separate explicit step, not done yet.
2. Not yet done, flagged but out of scope for the 2026-10-04 session — only act on these if
   explicitly asked:
   - `docs/test-harness.md` and `docs/vfpc-next-supplement.md` still describe the removed
     sidecar harness.
   - A number of stale branches exist (`backup/main-pre-recovery-2026-04-03`,
     `backup/refactor-pre-legacy-prune-2026-04-20`, `cleanup/pre-trash-removal-2026-04-13`,
     `cleanup/recovered-clean-main-2026-04-13`, `test-harness/main`,
     `peter-clean-port-2026-04-03` (closed-PR branch), and a handful of Peter's own WIP
     branches: `Refactor-of-FP-validation-code`, `ari/fix-sid-applicability`,
     `fix-analyzefp-after-merge`, `fix/route-selector-in-restrictions`,
     `fix/route-selector-minimal`). Per the local-git-cleanliness rule, do not delete any
     of these without J. Kino's explicit confirmation — several may be Peter's active work.
   - Issue **#6** (remove hard-wired `vcpkg.exe` dependency) is the only open GitHub issue;
     Peter owns it, parked.

## Parked (do not start without explicit reason)

- **Issue #6 (vcpkg cleanup):** Peter owns vcpkg direction; do not push.
- **Stale branch cleanup** (see list above): needs J. Kino confirmation per repo rules,
  not an AI judgment call.

---

## Operating Rules for This Repo

- This repo is **Peter's production plugin**, not a Hub-owned verification lane anymore.
  The sidecar GTest/CLI experiment is retired; do not recreate it without explicit
  instruction.
- Peter does not use `Documentation/butler/`. Keep these docs accurate for AI-session
  handoff, but do not expect Peter to read or act on anything written here.
- Use PRs for everything landing on `main`, per the repo-wide `vfpc-use-prs` rule. Request
  Peter as reviewer on anything touching the actual plugin release (as done for #27).
- When in doubt about plugin behavior, read `src/analyzeFP.cpp` — that is the ground truth.

---

## Historical Note (superseded — kept for audit trail only)

Everything below this line describes the 2026-04 sidecar verification lane
(`VFPC_Tests`/`VFPC_Check`), which was removed 2026-10-04 (PR #26). Do not act on it; it
is preserved so a future session can see why those files existed if the question ever
comes up. Full session-by-session detail is in `session_status_summary.md`.

- `VFPC_Tests` was a GTest harness (125/125 passing at its peak) extracting pure-logic
  seams (`ValidatorCore.hpp`, `RouteSyntax.hpp`, `SidLookup.hpp`, `ConstraintChecks.hpp`,
  `FlightPlanRow.hpp`) from the plugin for fixture-driven testing.
- `VFPC_Check` was an offline CLI (`vfpc_check.exe`) built for Peter to test routes without
  EuroScope; per J. Kino, this was never actually used.
- Neither was ever added to `VFPC.sln`, so neither was ever part of Peter's real build.

# VFPC-next — Project Status

_Last updated: 2026-10-04 (v4.0.0.0 released -- PR #27 merged)_

---

## Repo Purpose

`VFPC-next` is **Peter Richardson's production VFPC EuroScope plugin**.

These `Documentation/butler/` docs exist purely for **AI-session continuity** — Peter does
not read or maintain them. Their only job is to let an AI assistant picking up this repo
in a future session get accurate current state without re-deriving it. Keep them honest;
see `next_session_prompt.md` for the fuller explanation and the historical note on why
this repo used to also host a sidecar test lane.

As of 2026-10-04, this repo **no longer hosts** a separate verification lane — the
`VFPC_Tests` GTest harness and `VFPC_Check` offline CLI were removed (PR #26) because
neither was ever wired into `VFPC.sln` or any CI step, and per J. Kino the harness was
never actually used.

---

## Branch State

| Branch | Purpose | Status |
|--------|---------|--------|
| `main` | Production baseline | Clean at `19761a9a` — **v4.0.0.0 released** (merge commit for PR #27) |
| `v4.0.0-Beta3` | Peter's v4.0.0.0 release candidate | Merged into `main` 2026-10-04 via PR #27 (real merge commit, consistent with PR #15's Beta2 release). Branch itself left in place, not deleted. |
| `peter-clean-port-2026-04-03` (PR #3) | Peter's earlier rewrite attempt | Closed, never merged — superseded by the Beta3 line |

Other remote branches exist (`backup/*`, `cleanup/*`, `test-harness/main`, several of
Peter's `fix/*` and `Refactor-of-*` branches) but have not been audited or touched this
session — see "Immediate Next Steps" in `next_session_prompt.md`. Do not delete any branch
without J. Kino's explicit confirmation per the repo-wide local-git-cleanliness rule.

---

## v4.0.0.0 Release — shipped 2026-10-04

Commit `245b3bbc`, authored by Peter Richardson. Full description of changes is in
`next_session_prompt.md` and the PR body itself
(https://github.com/VFPC/VFPC-next/pull/27). Merged into `main` as `19761a9a`.

**Process note for next time:** Peter closed PR #27 once by mistake (clicked "Close pull
request" thinking it was how you finalize a review — he'd already left an approving
comment, "Reviewed and looks okay to me," at the same timestamp). Reopened via
`gh pr reopen`, nothing was lost. Given Peter authors this code himself and doesn't use
GitHub's PR-review UI naturally, a plain-language "are you happy to ship this" question
got a clearer signal than asking him to work the Files-changed/Review-changes flow. Worth
defaulting to that lighter-weight confirmation path for his future release PRs rather than
routing through formal GitHub review mechanics.

**Reversibility:** No CI/build/publish step is wired to this repo's `main` (only
`cacheclear.yml` exists), so merging doesn't trigger any automatic distribution. If this
needs to be undone, GitHub's one-click "Revert" button on the merged PR is a clean,
complete undo.

---

## Open Issues

| Issue | Description | Status |
|-------|--------------|--------|
| #6 | Remove hard-wired vcpkg.exe dependency | Open — Peter owns, parked |

(#1, #3, #4, #7, #8, #10 are all closed — #3 was closed without merging, the rest closed
as fixed/superseded. No need to re-check these unless something resurfaces.)

---

## Next Steps

1. v4.0.0.0 is released. No GitHub tag or formal release notes were cut as part of this
   merge — if Peter wants a tagged GitHub Release, that's a separate, explicit step not
   done yet.
2. Everything else is parked — see `next_session_prompt.md` for the full flagged list
   (stale docs, stale branches) that was found but intentionally not acted on this session.

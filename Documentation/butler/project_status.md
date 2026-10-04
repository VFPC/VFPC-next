# VFPC-next — Project Status

_Last updated: 2026-10-04 (sidecar harness retired; v4.0.0.0 release PR open for Peter's review)_

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
| `main` | Production baseline | Clean at `290b0c61` (post sidecar-removal) |
| `v4.0.0-Beta3` | Peter's v4.0.0.0 release candidate | **Open PR #27 → `main`, awaiting Peter's review. Do not merge without his sign-off.** |
| `peter-clean-port-2026-04-03` (PR #3) | Peter's earlier rewrite attempt | Closed, never merged — superseded by the Beta3 line |

Other remote branches exist (`backup/*`, `cleanup/*`, `test-harness/main`, several of
Peter's `fix/*` and `Refactor-of-*` branches) but have not been audited or touched this
session — see "Immediate Next Steps" in `next_session_prompt.md`. Do not delete any branch
without J. Kino's explicit confirmation per the repo-wide local-git-cleanliness rule.

---

## v4.0.0.0 Release Candidate (PR #27) — current state

Commit `245b3bbc`, authored by Peter Richardson, 2026-10-04. Full description of changes
is in `next_session_prompt.md` and the PR body itself
(https://github.com/VFPC/VFPC-next/pull/27). No merge conflicts with `main` (verified via
`git merge-tree`). Peter requested as reviewer; **not yet merged.**

---

## Open Issues

| Issue | Description | Status |
|-------|--------------|--------|
| #6 | Remove hard-wired vcpkg.exe dependency | Open — Peter owns, parked |

(#1, #3, #4, #7, #8, #10 are all closed — #3 was closed without merging, the rest closed
as fixed/superseded. No need to re-check these unless something resurfaces.)

---

## Next Steps

1. Wait for Peter's review on PR #27. Do not merge in the meantime.
2. Once merged, update this doc's branch table and drop the "Release Candidate" framing
   in favor of whatever `main` then represents (tag/release notes if Peter cuts one).
3. Everything else is parked — see `next_session_prompt.md` for the full flagged list
   (stale docs, stale branches) that was found but intentionally not acted on this session.

# Contributing to vFPC

vFPC is a C++ plugin for EuroScope that helps VATSIM UK controllers check IFR departure flight plans. Bug reports, test cases, documentation improvements and code contributions are welcome. Use [VFPC-next](https://github.com/VFPC/VFPC-next) for issues, pull requests and releases. For installation, configuration and day-to-day use, see the [vFPC User Guide](https://vfpcdata.org/docs/).

## 1. Open an issue and agree the scope

Every proposed change should start with an [open GitHub issue](https://github.com/VFPC/VFPC-next/issues). Search open and closed issues first. If an issue already covers the work, add your evidence or proposal there. If you want to help and have no change in mind, ask a maintainer which open issue would be a good starting point.

For a bug, give the vFPC version or build, EuroScope version, steps to reproduce, expected behaviour and actual behaviour. For a validation bug, include the relevant flight plan, departure runway and SID, requested flight level, airport-level **DEP** selections, and the airport data source and cycle if known. Add a short log extract or screenshot if it helps. Remove credentials and personal information from anything posted publicly.

If you need to open a new issue, select New issue and choose Bug report or Feature request, whichever fits best.

For a feature or change in validation policy, explain the controller problem, the proposed behaviour, examples and how it could be verified. If you suspect the airport constraint data is the cause, include the relevant data or rule reference so the maintainer can identify where the fix belongs.

Comment on the issue if you want to take it on. A maintainer can assign it to you where GitHub permissions allow. Agree the scope, expected behaviour and test coverage in the issue before implementing the change. Record decisions reached elsewhere in the issue so reviewers can find them. If the change needs access to the airport data service, website or release infrastructure, agree who will handle that part.

## 2. Create a branch

A **fork** is your GitHub copy of VFPC-next. A **clone** is a local copy on your computer. A **branch** keeps the work for one issue together. A **commit** records a set of changes, and a **pull request** (PR) asks the maintainers to review and merge your branch.

On GitHub, select **Fork** in VFPC-next. Clone your fork with Git for Windows. Replace `YOUR-USERNAME` and the example issue number below. In this setup, `origin` is your fork and `upstream` is the official repository.

```powershell
git clone https://github.com/YOUR-USERNAME/VFPC-next.git
cd VFPC-next
git remote add upstream https://github.com/VFPC/VFPC-next.git
git fetch upstream
git switch -c fix/123-dep-selection upstream/main
```

Add the `upstream` remote once per clone. For each new issue, fetch again and branch from the current `upstream/main`, unless a maintainer specifies a different base. Contributors with write access may create a topic branch directly in VFPC-next. Use a short branch name with the issue number, such as `feature/123-validation-summary` or `docs/123-sweatbox-setup`.

## 3. Make and build the change

Keep each PR focused on one agreed issue. Follow the surrounding C++ style, reuse existing parsing and validation logic, and discuss wider refactoring or dependency changes before including them. Preserve compatibility with existing configuration files and airport data unless the issue expressly calls for a format change.

Follow [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for Visual Studio, the EuroScope SDK and vcpkg setup. Use the repository's C++20 project and build the **Win32 (x86)** DLL using the documented `x86-windows-static-md` triplet. Build **Release | Win32** for runtime verification. Before testing, check where Visual Studio writes the DLL, keep a copy of your working DLL, and confirm EuroScope's startup log identifies the build you intend to test.

EuroScope and its current plugin SDK require care around threading. Keep calls to `DisplayUserMessage()` on the EuroScope main thread. Use the existing worker notification queue for messages produced by background work.

## 4. Handle versioning and documentation

The published vFPC version has three parts, for example `4.1.2`. The DLL's internal version can have a fourth component for the build, for example `4.1.2.1`. Identify test builds by their DLL version and Git commit. Check the Git build information in the startup log when available, including whether the build came from a working tree with local changes. If you tested with uncommitted changes, record that and verify the committed revision before review.

Keep version changes out of routine PRs unless agreed with the maintainer. For an agreed version change, follow the project's version definitions in [src/VersionInfo.h](src/VersionInfo.h) and DLL resource in [src/Resource.rc](src/Resource.rc). The maintainer coordinates public version numbers, release tags, GitHub Releases and the server-side update notification.

Update documentation in the same PR when the change affects users or developers:

- Add a short entry under `Unreleased` in [CHANGELOG.md](CHANGELOG.md) for significant user-facing changes. Create the heading if needed, and describe the behaviour a controller will see.
- Update the affected [User Guide source](Documentation/docs/) for changes to setup, commands, configuration, tag results, diagnostics or controller workflow. Note if the online guide or release PDF needs refreshing.
- Update [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for changed build steps, and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) when a dependency change affects notices.

## 5. Test and record the evidence

For plugin code changes, build the DLL and verify the affected behaviour in EuroScope. Use Sweatbox or an agreed offline setup for repeatable scenarios. If a change is documentation-only, check the rendered Markdown, follow affected links and verify any steps or commands that changed. Run relevant automated checks where available.

Choose tests that exercise the issue and the most relevant existing behaviour:

| Change area | Example verification |
| --- | --- |
| Flight plan validation | Reproduce the reported case, then check a valid plan and a nearby invalid plan. Record the expected and observed tag result and relevant console or log details. |
| Departure scope | With vFPC 4.1.2 behaviour as the baseline, use the airport-level **DEP** selections in Runway Activity. Exercise one selected airport, multiple selected airports and removal from scope. Include an IFR departure from an unselected airport and check VFR handling where relevant. |
| Revalidation and per-flight state | Change the route, RFL or runway as applicable and confirm results refresh. If the change touches overrides or disabled checks, exercise setting and clearing them and check what happens when a flight leaves scope or returns. |
| Airport data, startup or logging | Exercise the affected success and failure paths, such as delayed or failed data loading and recovery. For shutdown or background-work changes, check unload or exit while work is in progress as applicable. |

Put the verification record in the PR description, a PR comment, or a linked record accessible to reviewers. For each run, include the commit SHA, DLL and EuroScope versions, build configuration, test environment and relevant data cycle or fixture. Record the steps or scenario, expected and observed outcomes, and mark each case **Pass**, **Fail**, or **Not run**. Attach short log extracts, scenarios or screenshots where they help someone reproduce or assess the result. If you cannot run a case, say why and agree who will run it and when. The agreed verification and any exceptions should be recorded before merge. Repeat affected checks if review changes the code that was tested.

## 6. Commit, push and open a pull request

Review the changes before committing. Stage specific files and keep related changes together. Use a commit message that describes what changed. Keep generated DLLs, local configuration, logs and credentials out of commits.

```powershell
git status --short
git diff
git add -- path/to/changed-file
git diff --cached
git commit -m "Refresh validation when departure scope changes"
git push -u origin fix/123-dep-selection
```

On GitHub, open **Compare & pull request**. Check that the base is **VFPC/VFPC-next → main** unless agreed otherwise, and the compare branch is your topic branch. Give the PR a descriptive title. Use `Fixes #123` in the description if the PR completes issue #123. GitHub closes a linked issue when the PR merges into the default branch. If the issue will need follow-up work, use `Related to #123`. List any prerequisite PRs in the description.

You can open a **draft PR** for early feedback. Use the following outline when preparing it for review:

```markdown
## Issue
Fixes #123

## Change
Describe the previous behaviour, the change and what controllers will observe.

## Verification
- Commit SHA, DLL version, EuroScope version and build configuration
- Sweatbox scenario or reproduction steps, plus data source and cycle if relevant
- Expected result, observed result and status for each case
- Evidence links and any remaining checks, with an agreed owner and date

## Documentation
List the changelog and guide updates, or explain why none are needed.

## Dependencies
List prerequisite PRs or external changes, if any.
```

## 7. Respond to review and follow through

Reply to review comments in the PR and push updates to the same branch. Mention which checks you repeated after significant changes. Address failing checks and outstanding review feedback. If plans or availability change, update the PR with what remains and when it can be verified.

The maintainer reviews the scope, code, evidence and documentation before merging. Once merged, you may delete your topic branch. A merge puts the change into the source branch. The maintainer decides when it is included in a public release, builds and verifies the release DLL, and coordinates the release notes, guide updates and update notification where applicable.

## Licence and conduct

Submit material you have permission to contribute under the project's GNU GPL version 3 terms. Identify third-party material and its licence in the PR so the maintainer can review it. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Keep technical feedback specific and respectful. The [Code of Conduct](CODE_OF_CONDUCT.md) applies to participation in the project.

For help with the GitHub interface, see [Fork a repository](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo) and [Link a pull request to an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue).

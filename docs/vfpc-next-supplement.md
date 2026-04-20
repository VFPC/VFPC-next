# VFPC-next Supplement

This note is the developer-facing companion to the top-level `README.md`.

It describes the actual shape of the `Refactor-of-FP-validation-code` branch rather than older or planned repo layouts.

## Repo Shape On This Branch

The active Visual Studio solution currently contains one project:

- `VFPC`, the EuroScope plugin DLL

That is visible in [VFPC.sln](../VFPC.sln) and [VFPC.vcxproj](../VFPC.vcxproj).

The repo also contains supporting directories that are not active build targets in this branch, including:

- `lib/` for bundled local libraries
- `VFPC_Tests/` for vendored third-party test dependencies and historical test-harness material

Those directories should not be read as proof that this branch currently builds a separate CLI or a separate test executable.

## Current Build Target

### `VFPC`

- project type: Visual Studio C++ DLL
- intended configuration: `Release | Win32`
- output path: `%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\`
- language standard: C++20
- vcpkg manifest: [vcpkg.json](../vcpkg.json)

Current manifest dependencies:

- `spdlog`
- `fmt`
- `curl`
- `rapidjson`

Important build facts from this branch:

- the DLL is the only active solution target
- the project builds `Win32`, not `x64`
- `src\Resource.rc` is the live resource file for the project
- pressing `F5` on the DLL project without a host executable configured will fail, because this is a EuroScope plugin rather than a standalone app

## Runtime And Validation Model

The branch has moved beyond the older single-pass tag-code model. The current codebase includes:

- structured per-field validation results in `ValidationResult`
- a compact tag mnemonic derived from those field results
- full or summary console output modes
- active-airport session locking and airport inference
- background worker loading for airport data snapshots

The main implementation surface is in:

- [src/analyzeFP.hpp](../src/analyzeFP.hpp)
- [src/analyzeFP.cpp](../src/analyzeFP.cpp)

### Current Tag Mnemonics

The current tag mnemonics on this branch are:

- pass or non-fail: `OK!`, `WRN`, `VFR`, `PDG`, `OFF`
- attention or fail: `SID`, `DST`, `XIT`, `RTE`, `LVL`, `DIR`, `SFX`, `RST`, `BAN`, `SYN`, `ERR`

This is materially different from the older public README on the branch, which still documents legacy codes such as `ENG`, `OER`, `SUF`, and `CHK`.

## Popup Actions And Chat Commands

Current popup actions:

- `Show Checks`
- `Toggle Checks`

Current chat commands implemented in `OnCompileCommand`:

- `.vfpc load`
- `.vfpc file`
- `.vfpc check`
- `.vfpc full`
- `.vfpc summary`
- `.vfpc config`

Notable documentation hazard:

- `LOG_COMMAND` still exists in constants, but `.vfpc log` is not currently wired into command handling on this branch

## Configuration Surface

The plugin creates `vfpc_config.json` on first run if it does not exist.

Current top-level config keys:

- `spdlog`
- `curl`
- `colours`
- `console_results_view`

Important current defaults and supported settings include:

- curl endpoints:
  - `base_url`
  - `airport_endpoint`
  - `version_endpoint`
  - `timeout_sec`
  - `verify_ssl`
  - `user_agent`
  - `default_headers`
- colours:
  - `red`
  - `amber`
  - `green`
  - `yellow`
  - `grey`
- results mode:
  - `console_results_view = summary|full`

The authoritative implementation for that is in:

- [src/JsonConfigLoader.h](../src/JsonConfigLoader.h)
- [src/PluginConfig.h](../src/PluginConfig.h)
- [src/PluginConfig.cpp](../src/PluginConfig.cpp)

## Branch Notes

This branch is ahead of `main` in core plugin files, but it was also forked from an older repo state. That is why the branch recently needed a prune of stale tracked files already deleted on `main`.

There is also a `test-harness/main` branch in history. Treat that as a sidecar verification lane, not as evidence that this branch currently ships multiple maintained build targets.

## Suggested Documentation Split

Keep the top-level `README.md` focused on:

- what VFPC does
- installation
- EuroScope setup
- current tag results
- operator-facing chat commands
- operational caveats

Keep branch/build/runtime details in this supplement so the public README does not drift back into half-true developer notes.

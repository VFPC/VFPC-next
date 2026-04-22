# VFPC

VFPC is a EuroScope plugin for UK VATSIM that checks filed IFR departure flight plans against the current VFPC restriction data and shows the result directly in the departure list.

It is a controller aid, not a clearance engine. A clean result does not remove the need to read the flight plan sensibly, and stale sector files, stale navdata, or stale VFPC data can still produce bad outcomes.

For developer and branch-structure notes, see [docs/vfpc-next-supplement.md](docs/vfpc-next-supplement.md).

## Community

VFPC community Discord:

`https://discord.gg/xucfd2K523`

## What The Plugin Checks

VFPC is focused on departure-side validation. On this branch it can check:

- whether the filed SID or non-SID departure route exists for the active airport
- whether the destination matches the selected departure route
- whether the exit point is valid
- whether the filed route remains valid after route normalization and syntax checks
- whether the requested level fits the permitted level band
- whether the odd/even direction rule is satisfied
- whether the SID suffix is valid
- whether SID-level restrictions apply for aircraft type, time, or ban state
- whether route-level warnings or bans are present
- whether the route text contains obvious syntax problems

## Read Before Use

VFPC only makes sense when the underlying data is current.

Before relying on it, make sure:

- your UK sector files are current
- your EuroScope navigation data is current
- VFPC has current airport data from the live service, or you are deliberately loading a current local `Sid.json`

After updating sector files or navigation data, restart EuroScope.

## Installation

1. Create `%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\` if it does not already exist.
2. Copy `VFPC.dll` into that folder.
3. Start EuroScope and load the plugin.

On first run, VFPC creates `vfpc_config.json` in the same folder if it is missing.

## EuroScope Setup

Add a new departure-list tag item with:

- Tag Item Type: `VFPC (UK)/VFPC`
- Mouse Button Function: `VFPC (UK)/Options`
- Recommended Header: `FPC`
- Recommended Item Width: `3`
- Align To Center: `Off`
- Colour: `Default Other Item`

Then enable that tag item in the departure list.

The current popup menu provides:

- `Show Checks`
- `Toggle Checks`

`Show Checks` outputs the current validation result for the selected flight plan. `Toggle Checks` disables or re-enables VFPC for that specific flight plan.

## Tag Results

The tag is intentionally compact. It shows the most important current state for the flight plan.

### Pass And Non-Fail States

- `OK!` all checks passed
- `WRN` checks passed, but warning-style data was raised
- `VFR` the flight plan was not checked because it is not IFR
- `PDG` validation is pending, usually because airport data is still loading
- `OFF` checks were disabled for that flight plan, or the flight plan is outside the active airport lane for the session

### Failure And Attention States

- `SID` SID or departure-route problem
- `DST` destination restriction failure
- `XIT` exit-point restriction failure
- `RTE` route restriction failure
- `LVL` level-band failure
- `DIR` odd/even direction failure
- `SFX` SID suffix failure
- `RST` SID-level restriction failure
- `BAN` route ban present
- `SYN` route syntax problem
- `ERR` validation could not complete correctly

For anything non-trivial, use `Show Checks` or `.vfpc check` and read the detailed output.

## Console Output And Commands

VFPC supports these chat commands on this branch:

- `.vfpc load`
  Re-enable API loading and request a fresh airport-data reload.

- `.vfpc file`
  Disable API loading and attempt a one-time load from local `Sid.json`.

- `.vfpc check`
  Output the current validation result for the selected flight plan.

- `.vfpc full`
  Set console output to the full per-field validation view.

- `.vfpc summary`
  Set console output to the shorter summary view.

- `.vfpc config`
  Show config-default warnings and the current console-results mode.

## Data Loading

VFPC normally loads airport and SID data from the configured live service.

It can also load from a local `Sid.json` via `.vfpc file`. That file mode is temporary and is intended as an operator fallback, not the normal path.

If loading or parsing fails, VFPC may stop automatically reloading until you correct the problem and run `.vfpc load`.

## Configuration File

VFPC stores runtime settings in `vfpc_config.json`.

The current top-level settings are:

- `spdlog`
- `curl`
- `colours`
- `console_results_view`

The default service settings are:

| Key | Value |
| --- | --- |
| `base_url` | `https://vfpcplugin.org/` |
| `airport_endpoint` | `airport?icao=` |
| `version_endpoint` | `version` |

The service URL should not be changed unless you have been told to do so by the VFPC team.

## Operational Notes

- VFPC is an aid, not a substitute for controller judgement.
- `WRN` means the route may still be acceptable, but you should read the detail.
- `SYN` means the route text itself needs attention even if later checks still ran.
- `VFR` and `OFF` are not hard failures; they mean the normal IFR validation path was not applied.

## Disclaimer

VFPC is still software. If the data is stale, the config is wrong, or the live feed is unavailable, results may be wrong or incomplete.

Use it as a controller aid and keep using your own judgement.

## Acknowledgements

This plugin was originally created by **@DrFreas** and significantly expanded by **@hpeter2** and **@svengcz**. **@lennycolton** took over development in January 2021,re-writing almost all of the plugin's original code. In 2025 **@Peter** took over development of the plugin resulting in a major change to the architecture and significant re-write of the code.  This will become v4 of the UK vFPC.

VFPC has had multiple contributors over time, including Peter Richardson, Jan Fries, Lenny Colton, Hendrik Peter, Sven Czarnian, and DrFreas.

The wider VFPC ecosystem has also included supporting work on the data/API side and parser side.

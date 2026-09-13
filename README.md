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
- VFPC has current airport data from the live service, or you are deliberately loading a current local `Sid.json`

After updating sector files restart EuroScope.

## Installation

1. Create `%APPDATA%\EuroScope\UK\Data\Plugin\VFPC\` if it does not already exist.
2. Copy `VFPC.dll` into that folder.
3. Start EuroScope and load the plugin.

On first run, VFPC creates `vfpc_config.json` in the same folder if it is missing.

## EuroScope Setup

Add a new departure-list tag item with:

- Tag Item Type: `VFPC/VFPC`
- Mouse Button Function: `VFPC/Options`
- Recommended Header: `FPC`
- Recommended Item Width: `3`
- Align To Center: `Off`
- Colour: `Default Other Item`

Then enable that tag item in the departure list.

The current popup menu provides:

- `Show Checks (SUMMARY)` – Displays a condensed validation summary for the selected aircraft, showing only the key outcome of each check.
- `Show Checks (FULL)` – Displays the complete flight plan validation result for the selected aircraft, including all individual checks and detailed explanations.
- `Toggle Override` – Marks or unmarks the selected flight plan as controller-approved, overriding normal validation results for that aircraft.
- `Toggle Checks` – Enables or disables VFPC validation for the selected flight plan.

## Tag Results

The tag is intentionally compact. It shows the most important current state for the flight plan.

### Display Colours

VFPC uses configurable colours to make flight plan validation status easy to identify at a glance within EuroScope tags and plugin output.

The default colour assignments are:

- **Green** – Validation passed successfully with no issues detected.
- **Amber** – Validation passed, but a warning or advisory condition applies.
- **Red** – Validation failed due to an error, restriction, or prohibited route.
- **Grey** – Validation is disabled for the flight plan, not applicable, or no validation result is available.
- **Cyan** – Controller override has been applied, indicating the flight plan has been manually accepted.
- **Yellow** – Reserved for additional alert or attention states where required.

### Pass And Non-Fail States

- `OK!` all checks passed
- `WRN` checks passed, but warning-style data was raised
- `VFR` the flight plan was not checked because it is not IFR
- `PDG` validation is pending, usually because airport data is still loading
- `OFF` checks were disabled for that flight plan, or the flight plan is outside the active airport lane for the session
- `OVR` the flight plan is controller-approved, overriding normal validation results

**NOTE:** `???` indicates that the plugin may have not been configured correctly in EuroScope (Refer to tEuroScope setup sections above).

### Failure And Attention States

- `SID` SID or departure-route problem
- `DST` destination restriction failure
- `XIT` exit-point restriction failure
- `RTE` route restriction failure
- `LVL` level-band failure
- `OER` odd/even direction failure
- `SFX` SID suffix failure
- `RST` SID-level restriction failure
- `BAN` route ban present
- `SYN` route syntax problem
- `ERR` validation could not complete correctly

## Console Output And Commands

VFPC supports these chat commands on this branch:

- `.vfpc config`
  Displays the current plugin configuration and highlights any settings using default values.

- `.vfpc file`
  Disables API loading and loads validation data for the active airport from the local`Sid.json` JSON file instead.

- `.vfpc load`
  Enables automatic API data loading, downloads validation data for the active airport, and revalidates tracked flight plans.

- `.vfpc nodebug`
  Reduces logging verbosity back to INFO level (normal operational logging).

- `.vfpc trace`
  Sets the plugin logging level to TRACE for detailed diagnostic logging.

## Data Loading

VFPC normally loads airport and SID data from the configured live service.

It can also load from a local `Sid.json` via `.vfpc file`. That file mode is temporary and is intended as an operator fallback, not the normal path.

If loading or parsing fails, VFPC may stop automatically reloading until you correct the problem and run `.vfpc load`.

## Configuration File

VFPC uses a JSON configuration file (`vfpc_config.json`) to control logging, API connectivity, display colours, and default output behaviour.

Most users will **not need to modify this file**, as the default settings are suitable for normal operation.
Configuration changes should generally only be made for troubleshooting, customisation, or development purposes.

The contents of this file are validated on plugin startup, and any invalid settings will be reset to their default values with a warning in the logs.
This file **must** be located within the same directory as `VFPC.dll`.  If it is missing, the plugin will create it with default settings on first run.

### User-adjustable settings

The following settings may be safely changed by advanced users:

- **console_results_view**  
  Controls the default level of detail shown in console validation output.  
  Valid values:
  - `"summary"` – concise validation results (recommended)
  - `"full"` – detailed validation breakdown

- **colours**  
  Customises the colours used by VFPC for tag display and status indications.  
  Colours are defined using RGB values (`r`, `g`, `b`) ranging from 0–255.

- **spdlog.level**  
  Controls logging verbosity.  
  Common values:
  - `"info"` – normal operational logging (recommended)
  - `"trace"` – detailed diagnostic logging for troubleshooting

### Advanced settings (normally leave unchanged)

These settings are intended for internal use and should only be changed if specifically instructed:

#### Logging (`spdlog`)
- **log_directory** – Location where log files are written
- **max_files** – Number of rotated log files to retain
- **flush_interval_sec** – How frequently logs are written to disk
- **pattern** – Log message formatting
- **flush_level** – Logging level that triggers immediate disk flush
- **truncate** – Whether existing log files are overwritten

#### API connectivity (`curl`)
- **base_url** – VFPC API server address
- **airport_endpoint** – Airport data endpoint
- **version_endpoint** – Version check endpoint
- **timeout_sec** – Network timeout in seconds
- **verify_ssl** – Enables SSL certificate verification
- **user_agent** – HTTP user agent string
- **default_headers** – HTTP request headers

### Warning

Incorrect changes to advanced settings may prevent VFPC from loading data, writing logs, or communicating with the validation API.

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

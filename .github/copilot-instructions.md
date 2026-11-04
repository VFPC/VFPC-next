# VFPC Development Instructions

## Purpose

These instructions are intended for AI-assisted development tools (e.g. GitHub Copilot, ChatGPT, Codex) working on the VFPC codebase.

VFPC is a EuroScope plugin for validating UK IFR departure flight plans.

This is a legacy codebase under active refactor. Existing architecture and behaviour must be preserved unless an intentional design change is being made.

When suggesting or generating code:

- prefer minimal targeted fixes
- preserve current architecture
- avoid broad rewrites
- explain regression risks where appropriate
- identify the exact location within a file when suggesting changes

---

## Architectural Decision Authority

The [ADR register](../Documentation/docs/adr/README.md) is the authoritative record of architectural decisions. Work/Codex must consult the register and relevant Accepted ADRs before proposing or implementing architectural changes.

Accepted ADRs take precedence over conflicting historical documentation and architectural descriptions, including these instructions. Acceptance does not establish that functionality is implemented, tested or released. Explicitly labelled proposals and unresolved questions within an Accepted ADR are not accepted implementation choices.

Report conflicts with Accepted ADRs to the project owner and obtain approval before implementation. Do not silently modify or supersede accepted decisions; follow the register's decision-history and supersession conventions.

---

# Project Overview

VFPC validates IFR departure flight plans against airport-specific JSON validation data.

Validation includes:

- SID suitability
- destination validity
- exit point validity
- route compliance
- minimum / maximum levels
- odd / even direction
- aircraft restrictions
- time-based restrictions
- warnings
- bans

The plugin integrates with EuroScope and operates within its callback-driven plugin model.

---

# Development Environment

Current environment:

- Language: C++20
- IDE: Visual Studio Community
- Platform: Win32 / x86
- SDK: EuroScope Plugin SDK
- Package manager: vcpkg

Dependencies include:

- rapidjson
- curl
- spdlog
- fmt

Assume Windows / MSVC compatibility requirements.

---

# Coding Style and Conventions

## Legacy Code Warning

VFPC contains legacy code with inconsistent naming, formatting, structure, and style.

This should be improved gradually, but **do not perform broad cosmetic rewrites unless part of an intentional refactor**.

When modifying existing code:

- prefer consistency with the immediate surrounding area
- avoid mixing formatting-only changes with logic changes
- avoid unnecessary churn in diffs

---

## Code Comments and Documentation

These standards apply to future development. Do not retrospectively add comments throughout the existing codebase solely to meet them.

- New functions and significantly modified functions should have concise descriptive comments explaining their purpose and, where relevant, parameters, return values, side effects and important assumptions.
- Add explanatory comments for non-obvious logic, particularly flight-plan validation, restriction precedence, airport selection, caching, threading and snapshot management.
- Prefer comments explaining why a decision or implementation exists rather than simply repeating what the code does.
- Preserve useful existing comments and update them whenever the associated behaviour changes.
- Remove obsolete or misleading comments when modifying the affected code.
- Avoid excessive comments for self-explanatory statements and large blocks of commentary that duplicate the implementation.
- Document important safety constraints, including locking requirements, thread ownership, snapshot immutability and EuroScope callback restrictions.
- Comments describing architectural decisions must be consistent with the Accepted ADRs in the [ADR register](../Documentation/docs/adr/README.md). Reference the relevant ADR where it helps explain a non-obvious design constraint.
- Use clear, professional English and keep comments concise enough to remain maintainable.

---

## Standards for New Code

For new code:

### General Principles

Prefer:

- clarity
- readability
- maintainability
- explicit behaviour

Avoid:

- clever but unclear code
- hidden side effects
- broad rewrites for narrow bugs

---

### Naming

Preferred conventions for new code:

- functions: `PascalCase_()` where consistent with project style
- local variables: `snake_case`
- member variables: `snake_case_`
- constants: existing project convention
- enums: follow local style

When editing existing code, preserve nearby style unless refactoring deliberately.

---

### Modern C++

Use C++20 where it improves safety or clarity.

Prefer:

- `std::string`
- `std::vector`
- `std::optional`
- `std::shared_ptr`
- `std::unique_ptr`
- `std::jthread`
- `std::lock_guard`
- `std::scoped_lock`
- range-based loops

Avoid:

- raw ownership pointers
- manual memory management
- unnecessary macros
- unsafe shared mutable state

---

### Error Handling

Do not silently ignore:

- API failures
- JSON parse failures
- missing airport data
- invalid flight plan state
- file load failures
- unexpected validation states

Prefer explicit checks and useful logging.

---

# Core Architecture Rules

## Airport-Specific Immutable Snapshots

Validation uses airport-specific immutable snapshots. Support multi-airport loading for facilities whose selection policy uses Runway Activity, while preserving airport-specific data isolation. Select validation data for the flight plan's origin; do not mix data or results between airports.

Prepare airport data and parse JSON off-thread. Publish only complete, valid immutable snapshots atomically, with thread-safe access. Do not impose a global one-airport or one-snapshot limit. These requirements do not prescribe a cache size, worker count, queue design or retry policy.

---

## Airport Selection — Accepted Policy

[ADR-003](../Documentation/docs/adr/ADR-003.md), accepted on 2 November 2026,
supersedes ADR-001 and ADR-002's earlier advisory-only UKCP policy.

| Recognised callsign suffix | Airport-selection authority |
|---|---|
| DEL / GND / TWR | One UKCP `top_down` airport matching the callsign's airport prefix, independent of Runway Activity |
| APP / CTR | Departure-active Runway Activity airports intersected with the exact UKCP position's `top_down` list |
| OBS | Departure-active Runway Activity airports, without a UKCP coverage filter |
| Unknown operational position, unsupported suffix, or empty mapping | Pause validation with an explanation |

- Full normalised callsigns are matched exactly; sector identifiers must not be removed.
- Callsign suffix selects the mode; facility and server controller permission are supporting diagnostics. In particular, `SCO_CTR` with APP facility retains area selection.
- Do not use controller rating or departure-list membership to establish responsibility.
- The SDK does not document an OBS facility value. Do not confuse observer rating with facility or treat `IsController()==false` as an observer test.
- Local scope remains eligible if departure Runway Activity omits its airport; warn without removing eligibility.
- Preserve per-flight assigned departure-runway gating and origin-based airport snapshots.

## UKCP Scope Cache and Recovery

- Load `https://ukcp.vatsim.uk/api/controller-positions-v2` off-thread and validate the entire replacement lookup before publication.
- Retain the current session's valid lookup if a refresh fails. Do not broaden scope when data is missing, malformed, unmatched or explicitly empty.
- Refresh on connection and `.vfpc scope reload`; `.vfpc scope` reports current scope/cache status. OBS selection can operate without UKCP data.
- Clear the cache on disconnect; reject late results from an earlier connection generation. No disk cache or periodic retry is introduced.
- UKCP `top_down` describes configured coverage; it does not establish live staffing, delegation or operational ownership.
- Workers must not call EuroScope or store SDK handles. Publish plain data through the protected pending-result slot; consume it in the host callback before airport results.
- Debug and Release Win32 builds passed on 2 November 2026 with zero warnings/errors. User-operated normal-scope and isolated simulated-failure Sweatbox checks passed on 2–3 November; see [verification record](../Documentation/reviews/Controller_Scope_Sweatbox_Results.md). Real transport failures, malformed responses and concurrency/race cases remain unverified. Keep diagnostic hooks out of production source.

---

# Flight Plan Tracking and Caching

VFPC tracks and caches flight plans to avoid repeated validation on frequent EuroScope callbacks.

Behaviour:

- newly seen flight plans are validated immediately
- cached flight plans are not revalidated unless relevant data changes
- changes are detected using a fingerprint / tracked state comparison
- stale tracked entries are pruned after inactivity

Revalidation may also occur when:

- airport validation data is reloaded
- `.vfpc load` is used
- `.vfpc file` loads replacement data
- the relevant airport snapshot or effective airport set changes

Changes of facility, callsign or effective airport set must not leave obsolete eligibility or cached results in use. Keep cached results associated with the correct airport and revalidate affected flight plans when relevant data or scope changes.

Critical rule:

**Do NOT validate on every `OnGetTagItem()` callback.**

That causes:

- unnecessary CPU usage
- repeated log spam
- degraded plugin performance

---

# Threading Model

VFPC performs background work asynchronously to avoid blocking the EuroScope UI thread.

Current threaded operations include:

- version checking
- airport data loading
- JSON parsing
- snapshot preparation

Implementation uses:

- `std::jthread`
- atomics
- mutex-protected shared state
- immutable published snapshots

---

## Version Thread

Used by:

- `StartVersionCheckAsync()`
- `VersionCall_Worker()`

Rules:

- must not block UI
- must honour stop requests
- must update only thread-safe shared state
- logging should remain lightweight

---

## Airport Worker Thread

Responsibilities:

- API load
- file load
- JSON parsing
- airport-specific extraction
- snapshot preparation for selected airports
- atomic publication of airport-specific immutable snapshots

Rules:

- build complete data off-thread
- publish only complete valid snapshots
- never expose partial airport data
- support loading for multiple selected airports without mixing airport data or blocking callbacks

---

## Shared State Rules

Shared state includes:

- effective validation airport set
- pending airport-specific snapshots
- published airport-specific immutable snapshots
- reload flags
- request flags
- version status
- tracked flight plan cache
- override state

Rules:

- protect mutable shared state appropriately
- use atomics where suitable
- avoid unsafe concurrent access
- avoid data races

---

## Snapshot Publication Model

Required pattern:

1. build each airport-specific snapshot off-thread
2. validate completeness
3. publish atomically
4. consumers access the appropriate published immutable snapshot for the flight-plan origin through thread-safe access

Do not mutate live published snapshots.

---

# EuroScope Callback Safety

EuroScope callbacks may be called frequently.

Examples:

- `OnGetTagItem()`
- popup handlers
- command handlers

Rules:

- keep callbacks fast
- no blocking network calls
- no synchronous API requests
- no heavy JSON parsing
- avoid repeated expensive validation
- avoid repeated identical logging

---

# API Endpoints

VFPC currently uses two HTTP endpoints.

---

## Version Endpoint

Purpose:

Checks plugin version and AIRAC status.

Endpoint:

```text
https://vfpcplugin.org/version
```

Used by:

- `StartVersionCheckAsync()`
- `VersionCall_Worker()`

Expected behaviour:

- asynchronous
- non-blocking
- safe failure handling
- useful logging

---

## Airport Validation Endpoint

Purpose:

Downloads airport validation JSON.

Pattern:

```text
https://vfpcplugin.org/airport?icao={ICAO}
```

Example:

```text
https://vfpcplugin.org/airport?icao=EGCC
```

Rules:

- load airport-specific data for the selected validation airports; do not impose a global single-airport limit
- response may be array wrapped

Handled by:

- `ExtractSingleAirportToObject_()`

---

# JSON Format Assumptions

Do NOT assume all JSON inputs use identical top-level structure.

Supported forms may include:

---

## API Response

```json
[
  {
    "icao": "EGCC",
    "sids": [...]
  }
]
```

---

## Direct Airport Object

```json
{
  "icao": "EGCC",
  "sids": [...]
}
```

---

## Metadata-Wrapped Local File

```json
{
  "cycle": "2604",
  "generated": { ... },
  "airports": [...]
}
```

Code must safely handle supported formats.

Never hardcode assumptions about top-level JSON layout.

---

# Airport JSON Structure

Expected airport structure:

```json
{
  "icao": "EGCC",
  "sids": [
    {
      "point": "SANBA",
      "restrictions": [],
      "constraints": []
    }
  ]
}
```

Constraint fields may include:

- `dir`
- `min`
- `max`
- `dests`
- `nodests`
- `points`
- `nopoints`
- `route`
- `noroute`
- `restrictions`
- `alerts`

Special case:

Non-SID departures may use:

```json
"point": ""
```

---

# Validation Pipeline

Validation uses staged filtering.

Current logical order:

1. destination
2. exit point
3. constraint restrictions
4. route
5. min/max level
6. odd/even direction
7. alerts / warnings

Each round progressively filters candidate constraints.

Do not reorder without understanding regression risk.

---

# Route Validation Rules

Route validation is largely token-based.

Known behaviour:

- route token normalisation strips speed / level suffixes
- wildcard logic exists for permissive matching
- airway mismatches may become warnings rather than failures
- some SID-level restrictions may be overridden by route validity

Be cautious changing:

- route parsing
- token comparison
- wildcard logic
- airway handling

---

# Time Handling

Important rules:

- EOBT is UTC
- restriction windows may be local airport time
- DST conversion matters
- overnight windows must be handled correctly

Messages should show explicit time windows where appropriate.

Avoid vague wording.

Prefer:

```text
Only available during 2300-0600 local
```

rather than:

```text
Not permitted
```

---

# Logging Policy

Use UTC timestamps.

---

## Info

Use for:

- plugin startup
- version lifecycle
- airport snapshot publication
- first-seen flight plans
- validation result summaries

---

## Debug

Use for:

- validation inputs
- result summaries
- useful diagnostics

---

## Trace

Use for:

- per-round filtering
- detailed route comparisons
- route mismatch diagnostics

---

## Error

Use for:

- API failures
- parse failures
- invalid states
- load failures

---

## Logging Rules

Avoid repeated spam from frequent callbacks.

Especially avoid repeated identical logs from:

- `OnGetTagItem()`

Logging should be useful, not noisy.

---

# UI / Output Expectations

Known tag mnemonics include:

- `OK!`
- `ERR`
- `RST`
- `RTE`
- `LVL`
- `WRN`
- `BAN`
- `OFF`
- `PDG`

Rules:

- `.vfpc check` must always show full output
- summary output should remain concise
- failure output should be meaningful
- most relevant failure should be shown

---

# Known Regression-Sensitive Areas

Use caution when modifying:

- `ValidateFlightPlan_()`
- `ExtractSingleAirportToObject_()`
- route parsing
- route normalisation
- SID restriction logic
- warning generation
- ban logic
- time-window logic
- facility/callsign-based scope resolution and Runway Activity selection changes
- tracked flight plan caching
- `.vfpc load`
- `.vfpc file`
- popup command handling
- override toggling

---

# Regression-Sensitive Airports / Scenarios

Always assess impact on known edge cases.

High-risk airports:

- EGCC
- EGPH
- EGKK
- EGBB
- EGNT

Known tricky scenarios:

- EGPH TLA time restrictions
- EGPH route overrides
- EGCC LISTO
- EGCC SANBA
- EGCC SONEX runway restrictions
- EGKK route/time restrictions
- airway equivalence warnings

Fixing one airport must not break another.

---

# Git Hygiene

Do not commit:

- `Debug/`
- `Release/`
- `x64/`
- `.vs/`
- `ipch/`
- `vcpkg_installed/`
- `*.aps`
- temporary test files
- machine-local artefacts

---

# Change Workflow

Before changing behaviour, consult the ADR register and relevant Accepted ADRs; report conflicts to the project owner for approval before implementation.

1. identify exact failing case
2. inspect JSON source
3. identify failing validation stage
4. review logs
5. assess regression impact
6. add targeted diagnostics if needed
7. test known edge cases

---

# AI Assistant Rules

When generating code:

DO:

- preserve architecture
- preserve threading model
- preserve snapshot model
- preserve caching behaviour
- prefer targeted fixes
- explain risks when changing validation logic
- keep EuroScope callbacks lightweight

DO NOT:

- validate on every callback
- add blocking HTTP calls to UI callbacks
- introduce unsafe shared mutable state
- mutate published snapshots
- assume JSON formats are identical
- perform broad rewrites for narrow bugs
- add excessive logging spam
- break known regression scenarios
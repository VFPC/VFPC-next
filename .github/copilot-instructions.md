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

## Single Airport Snapshot Model

VFPC uses a **single-airport snapshot validation model**.

Rules:

- only one airport may be active at a time
- validation uses exactly one published airport snapshot
- airport data is prepared off-thread
- published snapshots are immutable
- snapshot publication is atomic

Do not introduce multi-airport validation unless explicitly redesigning the architecture.

---

## Active Airport Resolution

The active airport may be determined from:

### Controller Identity

Example:

EGPH_TWR ? EGPH

Controller callsign determines airport.

---

### Observer Inference

Observer sessions (`_OBS`) do not provide airport identity.

VFPC may infer the airport from repeated IFR flight plan origins.

Rules:

- only IFR flights are considered
- origins are counted
- threshold must be met before locking
- once locked, only that airport is validated

Be cautious modifying this behaviour.

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
- active airport snapshot changes

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
- airport extraction
- snapshot preparation
- atomic publication

Rules:

- build complete data off-thread
- publish only complete valid snapshots
- never expose partial airport data

---

## Shared State Rules

Shared state includes:

- active airport
- pending snapshot
- published snapshot
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

1. build snapshot off-thread
2. validate completeness
3. publish atomically
4. consumers read published immutable snapshot

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
https://vfpcplugin.org/airport/{ICAO}
```

Example:

```text
https://vfpcplugin.org/airport/EGCC
```

Rules:

- only one airport loaded at a time
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
Only available during 2300–0600 local
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
- active airport locking
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

Before changing behaviour:

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
# SID Applicability Fix

## Summary

This branch fixes a correctness bug in `VFPC-next` where SID-level restrictions
could short-circuit the constraint loop even when no SID-level restriction was
actually applicable to the current flight.

That behavior turns what should be downstream outcomes like `RTE` or `DST` into
`RST`.

## Why The Old Behavior Was Wrong

In `ValidateFlightPlan_`, the plugin did this:

1. evaluate `matched_sid["restrictions"]`
2. set `sid_level_pass = sid_restriction_result.passed`
3. skip the entire constraint loop when `sid_level_pass == false`
4. fall into the SID-level `Failed Restrictions` output path

That logic is only correct when a SID-level restriction is genuinely in scope
for the flight and actively blocks it.

It is not correct when the restriction array exists, but every restriction in
that array is out of scope for the current flight because of selectors like:

- aircraft / engine type
- SID suffix
- time window

## Live EGPH/TLA Shape That Matters

Current `2603` live data has this SID-root restriction shape for `EGPH/TLA`:

```json
[
  {
    "types": ["T", "P", "E"],
    "alt": ["GOSAM"]
  }
]
```

For a jet on `TLA`, that SID-level restriction exists in the array but is not
applicable to the flight. The plugin should therefore continue to normal
constraint evaluation.

If it does not, an unmatched route can surface as `RST` instead of the correct
constraint-layer result.

## New Rule

The branch adds a small pure helper in `src/SidApplicability.hpp`.

Rule:

- if SID restrictions passed, run constraints
- if SID restrictions failed but none of the SID restrictions are currently
  applicable to this flight, still run constraints
- only keep the SID gate closed when at least one SID restriction is currently
  applicable and the SID-level evaluation failed

In practice, "currently applicable" means the restriction's selectors match:

- suffix matches, if a suffix selector exists
- engine type or aircraft type matches, if a type selector exists
- time window is active, if a time window exists

`banned` is not treated as a selector. It is the effect of an applicable
restriction, not a scope condition.

## Scope

This fix is intentionally narrow:

- it does not redesign restriction aggregation
- it does not change per-constraint restriction behavior
- it only changes whether SID-level non-applicability is allowed to mask the
  normal constraint loop

## Regression Source

This branch also adds `tests/SidApplicabilityRegression.cpp`, a tiny standalone
regression executable source that checks the core gating rule for:

- `EGPH/TLA` type-mismatch on jets
- time-window mismatch
- active banned restriction
- suffix mismatch

## Running The Regression Locally

Because `main` does not yet carry a committed test project, the regression is
kept as a small standalone source file.

Compile it with any C++17-capable compiler that can see the repo headers, for
example:

```powershell
cl /std:c++17 /EHsc /I lib\include /I src tests\SidApplicabilityRegression.cpp /Fe:tests\SidApplicabilityRegression.exe
```

or the equivalent for your local toolchain.

Expected output:

```text
SidApplicabilityRegression: PASS
```

## Production Touch Point

The production behavior change is in:

- `src/analyzeFP.cpp`

The helper used there is:

- `src/SidApplicability.hpp`

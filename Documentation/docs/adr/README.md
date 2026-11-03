# vFPC Architecture Decision Records

## Purpose

This directory is the authoritative register of recorded vFPC architectural decisions. ADRs preserve requirements, rationale and consequences so that future work can follow accepted decisions without repeatedly reconstructing earlier discussions.

The preceding read-only audit found no ADR files, authoritative register or established numbering convention in the inspected repository and locally available Git history. Previously discussed identifiers alone do not establish that records exist.

## Register

| ID | Title | Decision status | Implementation status |
|---|---|---|---|
| [ADR-001](ADR-001.md) | Departure Airport Selection and Validation Scope | Superseded by ADR-003 | Working source contains multi-airport Runway Activity selection; agreed facility/callsign policy not yet implemented; multi-airport behaviour not yet confirmed through live EuroScope testing |
| [ADR-002](ADR-002.md) | UKCP Advisory Validation of Airport Selections | Superseded by ADR-003 | No airport-selection advisory integration identified during drafting; not confirmed implemented or tested |
| [ADR-003](ADR-003.md) | Controller Scope from UKCP Position Coverage | Accepted, 2 November 2026 | Working source builds passed; normal scope, simulated failure/recovery and OFF/VFR reporting Sweatbox checks passed; see verification record for remaining limits |

## Historical notes for ADR-001 and ADR-002

Acceptance was recorded on 24 October 2026 following the project owner's explicit instruction. It does not imply complete implementation, successful testing or release.

ADR-002 records subsequent verified production API evidence supplied by the project owner: controller callsigns and `top_down` airport associations. Whether those associations definitively represent live operational responsibility remains unverified. API evidence does not establish implementation of vFPC advisory warnings.

ADR-002's warning requirements are Accepted: compare departure-active selections against a recognised position's nonempty `top_down` list; warn in EuroScope and the log about unexpected airports without unchanged duplicate warnings. Empty lists mean unknown coverage. UKCP must not affect constraint loading, validation, the effective airport set or Runway Activity settings. The technical implementation details identified in ADR-002 remain unresolved.

These are the first two records in this register. No titles or decisions are inferred for previously mentioned ADR-003, ADR-004, ADR-006 or ADR-009. Those documents were not found by the audit.

## Evidence terminology

- **Implemented:** Confirmed in inspected source. Does not imply committed, tested or released.
- **Agreed:** Explicitly accepted by the project owner.
- **Proposed:** Suggested approach awaiting approval.
- **Unresolved:** Requires clarification or further investigation.

An Accepted ADR can contain explicitly labelled proposals and unresolved implementation questions. Those sections do not become accepted implementation choices merely because the architectural decision is Accepted.

## Management conventions

Accepted ADRs are authoritative architectural requirements. Before proposing or implementing an architectural change, Work/Codex must consult this register and the relevant Accepted ADRs. Any conflict with an Accepted ADR must be reported to the project owner and approved before implementation proceeds.

1. Use filenames `ADR-NNN.md`, with a descriptive title inside each document. Allocate sequential IDs and never reuse an allocated ID.
2. Record decision status separately from implementation and testing status. Decision statuses are Proposed, Accepted, Rejected and Superseded.
3. Record the acceptance date when approval is actually recorded. Do not invent earlier dates or infer approval from implementation.
4. Include context, decision, rationale, alternatives, consequences, implementation evidence, discrepancies and outstanding questions.
5. Preserve IDs and accepted decision history. Material changes to accepted decisions require a new ADR or an explicitly approved supersession process, with explicit supersession links and a register update where applicable. Clarifications must not silently change the decision's meaning.
6. Update implementation evidence only after inspecting the relevant source or results. Identify whether evidence describes a working copy, commit, test or release.
7. Keep links relative where the evidence is in the repository. Label external or untracked provenance so it is not mistaken for permanent repository evidence.
8. Maintain this index alongside the records. Record any recovered historical drafts explicitly before deciding their numbering or authority.
9. ADRs must be reviewed with the project owner and committed to Git through the normal authorized review/commit process so the decisions are preserved. Architectural acceptance does not itself authorize source changes, builds, deployment or commits. This correction task leaves the documents uncommitted for review.

## Existing guidance and discrepancies

The [development instructions](../../../.github/copilot-instructions.md), [handover](../../../VFPC_EnRoute_Support_Handover.md) and [user guide](../using-vfpc.md) contain older architectural descriptions. ADR-001 records the known discrepancies. The handover was untracked at the time of the audit.

This initial documentation change does not update those documents or MkDocs navigation. Reconciliation remains separate work requiring authorization. Neither ADR should be read as a claim that those discrepancies have already been fixed.

## Current authority

[ADR-003](ADR-003.md) supersedes the selection and advisory-only policies above following the project owner's explicit agreement to UKCP filtering and subsequent authorisation of source changes. Historical statements about advisory-only UKCP use do not apply to the new policy. UKCP remains configured coverage, not proof of live operational responsibility.

# Project documentation

This directory contains detailed accessibility feature specifications,
investigation plans, and milestone implementation records.

The root directory retains the documents needed for immediate repository use:

- `README.md`
- `AGENTS.md`
- `ACCESSIBILITY.md`
- `ACCESSIBILITY_ARCHITECTURE.md`
- `THIRD_PARTY_NOTICES.md`

Completed milestone records are stored in `documentation/milestones/`.
The existing `docs/` directory contains upstream game-engine notes and retains
its established path.

Current feature-specific engineering records include:

- `ACCESSIBILITY_FEATURE_REFERENCE.md` — the detailed current-behavior and
  implementation reference that formerly occupied the root player document;
- `ACCESSIBILITY_CANE_ACCEPTANCE_EVIDENCE.md` — value-only blind-user cane
  results preserved independently of disposable local diagnostic logs.
- `ACCESSIBILITY_CANE_QUERY_PROFILING.md` — structured cane observations,
  evidence versus audible policy, replay verification, and timing scopes.
- `ACCESSIBILITY_SPECIAL_DEVICE_TARGET_AUDIT.md` — campaign setup inventory,
  target-tag coverage, deliberate exclusions, and maintenance rules for
  special-device lock tones.

The root `ACCESSIBILITY.md` is the practical blind-player guide. Detailed
implemented behavior is preserved in `ACCESSIBILITY_FEATURE_REFERENCE.md`.
The other root accessibility documents describe project status and engineering
boundaries. Files in this directory preserve design decisions, implementation
handoffs, and investigation history; their original requirements can therefore
differ from later accepted behavior. When a historical plan and a current
document disagree, use `ACCESSIBILITY.md` for player operation,
`ACCESSIBILITY_FEATURE_REFERENCE.md` for exact current user-visible behavior,
and `ACCESSIBILITY_ARCHITECTURE.md` for current engine boundaries.

Some historical implementation plans refer to the retired root roadmap and
testing guide. Those documents remain available in Git history but are no
longer maintained as current project guidance.

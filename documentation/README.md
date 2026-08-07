# Project documentation

This directory contains detailed accessibility feature specifications,
investigation plans, and milestone implementation records.

The root directory retains the documents needed for immediate repository use:

- `README.md`
- `AGENTS.md`
- `ACCESSIBILITY.md`
- `ACCESSIBILITY_ARCHITECTURE.md`
- `ACCESSIBILITY_ROADMAP.md`
- `ACCESSIBILITY_TESTING.md`
- `THIRD_PARTY_NOTICES.md`

Completed milestone records are stored in `documentation/milestones/`.
The existing `docs/` directory contains upstream game-engine notes and retains
its established path.

Current feature-specific engineering records include:

- `ACCESSIBILITY_SPECIAL_DEVICE_TARGET_AUDIT.md` — campaign setup inventory,
  target-tag coverage, deliberate exclusions, and maintenance rules for
  special-device lock tones.

The root accessibility documents describe current behavior and project status.
Files in this directory preserve design decisions, implementation handoffs, and
investigation history; their original requirements can therefore differ from
later accepted behavior. When a historical plan and a root document disagree,
use `ACCESSIBILITY.md` for current user-visible behavior,
`ACCESSIBILITY_ARCHITECTURE.md` for current engine boundaries, and
`ACCESSIBILITY_ROADMAP.md` for current completion status.

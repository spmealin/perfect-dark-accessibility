# Perfect Dark accessibility branch guide

This branch is preparing an accessibility layer for the Perfect Dark PC port. Preserve the port's existing behaviour and keep accessibility work separable, testable, and suitable for upstream review.

## Read first

Before accessibility work, read:

- `README.md` for supported ROM versions, dependencies, and build commands.
- `ACCESSIBILITY.md` for scope, users, and design principles.
- `ACCESSIBILITY_ARCHITECTURE.md` for confirmed engine boundaries and proposed interfaces.
- `ACCESSIBILITY_ROADMAP.md` for milestone order and acceptance criteria.
- `ACCESSIBILITY_TESTING.md` for evidence and playtest requirements.
- The matching `documentation/milestones/ACCESSIBILITY_MILESTONE_XX_PLAN.md`, when one exists, before implementing that milestone.

Treat statements marked **Confirmed** as repository observations. Treat **Proposed** and **Question** as design work that still needs implementation or validation.

## Repository boundaries

- Do not add, copy, modify, or commit a Perfect Dark ROM, extracted ROM content, generated asset output, or copyrighted game data. Testers provide a supported ROM legally.
- Do not edit `build/`, `src/generated/`, extracted assets, `tools/recomp`, `port/fast3d`, or vendored headers for accessibility work unless the task explicitly requires it and the reason is documented.
- Keep platform-independent accessibility code under the existing `src/accessibility/` core. Put new port-facing implementations under `port/src/accessibility/`, with public or shared contracts in the corresponding include trees.
- Keep changes to established game files to small semantic hooks. Put policy, speech queuing, formatting, deduplication, configuration, and logging in the accessibility modules.
- Do not infer accessibility from rendered pixels when the engine already has the semantic value. Announce the menu label, objective state, HUD message, weapon, target, or world object that the engine knows.
- Windows speech support must sit behind a backend interface. Core accessibility code must build without Windows APIs and must tolerate no speech backend.
- Stay on the `accessibility` branch unless the user requests another branch.

## Build

Use normal workspace tools and the default shell for repository inspection, searches, file manipulation, documentation, Git operations, and other non-build work. Do not launch MinGW64 merely to read or edit files.

Use the MSYS2 MinGW64 environment when configuring or building the game and whenever launching a built Perfect Dark executable. For interactive use, enter through `C:\msys64\mingw64.exe`. Noninteractive tooling may initialize the same environment with `MSYSTEM=MINGW64` and `CHERE_INVOKING=1`, then invoke `C:\msys64\usr\bin\bash.exe -lc`; the executable itself must still be started by that initialized shell. Never start `build/pd.x86_64.exe` directly from PowerShell, Command Prompt, or another normal command line, because the missing MinGW runtime search environment can produce missing-DLL errors. This runtime-launch rule applies to smoke tests, scripted tests, and debugging runs. The baseline Windows build is:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

The executable is `build/pd.x86_64.exe` for the default `ntsc-final` configuration. Other supported ROM configurations and executable names are described in `README.md`.

Documentation-only changes do not require a rebuild unless they alter build inputs. Still check links, paths, symbols, the diff, and repository status.

## Implementation rules

- First prove the smallest end-to-end path: initialization, structured logging, one speech backend experiment, and clean shutdown.
- Default new accessibility features to off only during initial development while their behaviour and failure mode are being understood. Before handing a build to the project owner for blind-user acceptance testing, change every implemented accessibility feature to default on and verify the effective `pd.ini` and session-start log. Do not require the acceptance tester to discover or manually enable a newly implemented feature.
- Never block the game loop on speech, logging, device enumeration, or assistive technology.
- Give announcements priorities, deduplication keys, replacement groups, expiry times, and player context. Do not call a platform speech API directly from gameplay systems.
- Prefer polling stable semantic state once per logical tick when that avoids several invasive hooks. Prefer a hook when polling would lose an event, source, or ordering.
- Accessibility development logs should capture any feature-relevant state that may help implementation or diagnosis. Privacy redaction and data minimization are not requirements for these explicitly enabled local logs; resolved text, names, paths, arguments, positions, input, identifiers, and pointers may be recorded when useful.
- During blind-user acceptance testing, all implemented accessibility features default to enabled, including new milestone features such as gameplay beacons, along with comprehensive logging and any required output backends. Each remains configurable in `pd.ini`. Before handoff, inspect the actual configuration used beside the executable and confirm every feature's effective enabled state in the session-start log. Store accessibility logs separately from the general `pd.log`, ignore them in Git, and never upload them automatically. Never log ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets.
- Update the upstream hook ledger in `ACCESSIBILITY_ARCHITECTURE.md` whenever an established source file gains or loses an accessibility call.
- Document new configuration keys, default values, shortcuts, announcement rules, and backend limitations in the same change that introduces them.
- Use existing localized strings where they express the correct semantic value. Any new user-facing text needs a localization plan; do not hide English literals in hooks.

## Verification and handoff

Compilation proves integration, not accessibility. Runtime smoke tests prove startup and shutdown, not usability. A feature is not accessibility-validated until a blind or screen-reader-dependent tester can complete its stated task and the report records barriers as well as successes.

Every handoff should state:

- files changed and the user-visible behaviour;
- established files touched and why each hook was necessary;
- assumptions and unresolved questions;
- build configuration and executable tested;
- runtime, speech, interaction, and independent-user evidence collected;
- relevant accessibility log session ID and the diagnostic fields used to reach the conclusion;
- rollback steps and known regressions.

Do not describe a feature as complete when its acceptance criteria in `ACCESSIBILITY_ROADMAP.md` have not been met.

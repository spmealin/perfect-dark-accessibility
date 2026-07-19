# Milestone 2 implementation plan: initialization and diagnostic logging

This is the implementation handoff for Milestone 2 of `ACCESSIBILITY_ROADMAP.md`. It is intentionally prescriptive so a lighter-weight coding model can execute it without reopening architectural decisions.

Plan status: **executed**

Planning baseline: branch `accessibility`, commit `d3dc71e88`

Implementation status: **complete**

## Goal

Add a small, disabled-by-default accessibility service that:

- registers persistent accessibility configuration before `configInit`;
- initializes after filesystem and configuration services are ready;
- shuts down safely during the existing normal cleanup path;
- creates a separate, opt-in JSON Lines diagnostic log;
- exposes a reusable logging API for later accessibility milestones;
- makes no speech, menu, HUD, player, target, input, audio, or gameplay changes.

## Project-owner logging decision

Diagnostic usefulness takes precedence over privacy minimization during accessibility development. When accessibility logging is explicitly enabled, future features may record any state that helps diagnose or implement them, including resolved text, names, paths, arguments, precise positions, input events, identifiers, pointers, and detailed game state.

Do not add redaction, privacy filtering, consent prompts, field allowlists, or data-minimization logic in this milestone. Do not silently omit a useful diagnostic field because it could identify a user or machine.

The following restrictions still apply:

- logging is disabled by default and must be explicitly enabled in `pd.ini`;
- the log is local and separate from `pd.log`;
- the repository must ignore the runtime log;
- logs must never be committed automatically;
- never dump or reproduce ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets;
- logging failures must never stop the game;
- no telemetry, network upload, or automatic report submission is allowed.

This is a development diagnostic log, not a production telemetry system. Privacy hardening can be reconsidered before public distribution, but it is not part of Milestone 2.

## Definition of done

Milestone 2 is engineering-complete when all of the following are true:

1. The normal MinGW64 Windows build succeeds from a CMake reconfiguration.
2. `Accessibility.Enabled` and `Accessibility.LoggingEnabled` are registered in `pd.ini`, both defaulting to `0`.
3. With accessibility disabled, no accessibility log is opened or created.
4. With accessibility enabled and logging disabled, the service initializes as an inert coordinator and creates no accessibility log.
5. With both settings enabled, `$S/accessibility.log` is created/truncated for the current session and contains valid JSONL lifecycle records.
6. Every record contains schema, sequence, session, monotonic timestamp, build metadata, category, event, and message fields.
7. Normal shutdown writes a final lifecycle record and closes the file.
8. Initialization, shutdown, and log shutdown are idempotent by implementation and verified with temporary local instrumentation or a focused harness.
9. Failure to open, allocate for, write, flush, or close the accessibility log is nonfatal and reported through `sysLogPrintf` where possible.
10. The runtime log is ignored by Git and no ROM, generated asset, build artifact, config, save, or log is committed.
11. `ACCESSIBILITY_ARCHITECTURE.md`, `ACCESSIBILITY_ROADMAP.md`, and this plan reflect the implemented files and hook status.

Compilation and smoke tests complete this infrastructure milestone. They do not demonstrate accessible gameplay.

## Explicit non-goals

- No speech backend or test utterance.
- No announcement queue, priorities, interruption, deduplication, or cancellation.
- No `accessibilityTick` call and no change to `port/src/pdmain.c`.
- No menu, HUD, subtitle, objective, player, weapon, targeting, scanner, navigation, or input hooks.
- No in-game accessibility settings dialog.
- No command-line switches unless implementation is blocked without one; edit `pd.ini` for this milestone.
- No background log thread, mutex, compression, upload, rotation, size cap, or multi-session archive.
- No privacy redaction or sanitization beyond JSON string escaping needed to keep records parseable.
- No test framework introduction or unrelated refactoring.

## Confirmed repository facts

- `port/src/main.c:main` currently calls `sysInit`, `fsInit`, `configInit`, `videoInit`, `inputInit`, `audioInit`, and `romdataInit`, in that order.
- `port/src/main.c:cleanup` is registered with `atexit` on the normal startup path and saves input/config before shutting down video and crash handling.
- `PD_CONSTRUCTOR` is defined in `src/include/platform.h` and is already used by input, audio, video, and game configuration registration.
- `configRegisterInt` in `port/include/config.h` registers bounded integer settings before `configInit` loads `$S/pd.ini`.
- `fsInit` resolves `$S`; `fsFileOpenWrite` opens a resolved path with `wb`, and `fsFileFree` closes it.
- `sysGetMicroseconds` returns elapsed microseconds after `sysInit` establishes its time base.
- `sysLogPrintf` is the existing nonfatal diagnostic channel and can report accessibility-log failures.
- CMake force-includes generated `versioninfo.h`, which defines `VERSION_TARGET`, `VERSION_ARCH`, `VERSION_ROMID`, `VERSION_BUILD`, `VERSION_HASH`, and `VERSION_BRANCH` for every compiled source.
- `CMakeLists.txt` recursively includes `src/game/*.c` and `port/*.c`, but not `src/accessibility/*.c`. The new core sources require explicit registration.
- There is no repository test framework for these port services.

## Locked implementation shape

Create exactly these files:

```text
src/accessibility/accessibility.c
src/accessibility/accessibility_log.c
src/include/accessibility/accessibility.h
src/include/accessibility/accessibility_log.h
```

Modify only these established files unless compilation proves another edit is necessary:

```text
CMakeLists.txt
port/src/main.c
.gitignore
ACCESSIBILITY_ARCHITECTURE.md
ACCESSIBILITY_ROADMAP.md
milestones/ACCESSIBILITY_MILESTONE_02_PLAN.md
```

Do not create `port/src/accessibility/` yet. The logger uses the existing cross-platform `fs.h` and `system.h` interfaces and belongs with the platform-independent accessibility core. Windows-only code begins in Milestone 3.

## Public API

Use the repository's C naming and header-guard style. `src/include/accessibility/accessibility.h` should declare:

```c
void accessibilityInit(void);
void accessibilityShutdown(void);
s32 accessibilityIsEnabled(void);
```

Include `<PR/ultratypes.h>` for `s32`.

`src/include/accessibility/accessibility_log.h` should declare:

```c
s32 accessibilityLogInit(void);
void accessibilityLogShutdown(void);
s32 accessibilityLogIsOpen(void);
void accessibilityLogEvent(const char *category, const char *event, const char *fmt, ...)
		__attribute__((format(printf, 3, 4)));
```

The logger API is deliberately broad. Later hooks may put arbitrary feature-specific diagnostics in `message`. The fixed fields make ordering and session correlation machine-readable.

If the compiler rejects the format attribute in a supported configuration, guard it using an existing compiler macro pattern rather than removing format checking on GCC/Clang.

## Configuration contract

Define two file-local `s32` settings in `accessibility.c`:

```ini
[Accessibility]
Enabled=0
LoggingEnabled=0
```

Register them from a `PD_CONSTRUCTOR static void accessibilityConfigInit(void)` using:

```c
configRegisterInt("Accessibility.Enabled", ..., 0, 1);
configRegisterInt("Accessibility.LoggingEnabled", ..., 0, 1);
```

Behaviour is fixed by this truth table:

| Enabled | LoggingEnabled | Service state | Log file |
| ---: | ---: | --- | --- |
| 0 | 0 | Disabled | Not opened or created |
| 0 | 1 | Disabled | Not opened or created |
| 1 | 0 | Initialized, inert | Not opened or created |
| 1 | 1 | Initialized with diagnostics | Opened/truncated and populated |

`accessibilityIsEnabled` returns the effective service state after initialization, not merely the raw config integer. Repeated `accessibilityInit` calls must not truncate or reopen the log.

Do not add configuration getters to `config.c`, put accessibility settings in game save/profile data, or add options-menu UI in this milestone.

## Lifecycle contract

### Initialization

In `port/src/main.c`:

1. Include `accessibility/accessibility.h`.
2. Call `accessibilityInit()` immediately after `configInit()` and before `videoInit()`.

This placement guarantees that `sysInit`, `fsInit`, and configuration loading have completed and that logging can capture failures or progress from later accessibility integrations before the first UI appears.

`accessibilityInit` must:

1. Return immediately if it has already run.
2. Mark initialization state once, so repeated calls remain no-ops even when disabled.
3. Read the registered configuration variables.
4. If disabled, leave the log closed and return without creating a file.
5. If enabled, report the service state through `sysLogPrintf(LOG_NOTE, ...)`.
6. If logging is enabled, call `accessibilityLogInit`.
7. If log initialization succeeds, write `category="lifecycle"`, `event="session_start"` with the effective configuration and resolved log path in the message.
8. If log initialization fails, keep the accessibility service alive without a log. `accessibilityLogInit` owns the single `sysLogPrintf` warning; the coordinator must not emit a duplicate warning.

### Shutdown

In `port/src/main.c:cleanup`, call `accessibilityShutdown()` before `configSave`, `videoShutdown`, and `crashShutdown`.

`accessibilityShutdown` must:

1. Return safely if initialization never ran or shutdown already ran.
2. If the log is open, write `category="lifecycle"`, `event="session_stop"` with elapsed session time and any available counters.
3. Flush and close through `accessibilityLogShutdown`.
4. Clear effective service state.
5. Never call `exit`, `abort`, or `sysFatalError`.

Do not rearrange any existing lifecycle calls beyond inserting these two accessibility calls.

The existing `atexit(cleanup)` registration occurs after several startup services. A fatal error before that registration may omit `session_stop`; document this limitation rather than refactoring global shutdown in this milestone.

## Diagnostic log contract

### File location and lifetime

- Path constant: `$S/accessibility.log`.
- Open with `fsFileOpenWrite`, so each enabled launch replaces the previous session.
- Keep one `FILE *` in `accessibility_log.c`.
- Flush after each complete record. Development diagnostic durability is more important than peak logging throughput at this stage.
- Do not implement append, rotation, size limits, or session archives yet.
- Add `/accessibility.log` to `.gitignore` for the common repository-root save-directory case.

### JSONL schema version 1

Every physical line is one valid JSON object:

```json
{"schema":1,"seq":0,"session":"1721400000","t_us":2418,"build_hash":"d3dc71e88","build_branch":"accessibility","target":"x86_64-windows","arch":"x86_64","rom_config":"ntsc-final","build_type":"RelWithDebInfo","category":"lifecycle","event":"session_start","message":"enabled=1 logging=1 path=C:\\work\\accessibility.log"}
```

Fixed fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `schema` | integer | Always `1` for this milestone |
| `seq` | unsigned integer | Starts at zero and increments once per attempted record |
| `session` | string | Session identifier derived from startup wall-clock time; uniqueness only needs to be practical for a single local log |
| `t_us` | unsigned integer | `sysGetMicroseconds()` at event creation |
| `build_hash` | string | `VERSION_HASH` |
| `build_branch` | string | `VERSION_BRANCH` |
| `target` | string | `VERSION_TARGET` |
| `arch` | string | `VERSION_ARCH` |
| `rom_config` | string | `VERSION_ROMID`, never ROM contents |
| `build_type` | string | `VERSION_BUILD` |
| `category` | string | Caller-provided subsystem category |
| `event` | string | Caller-provided event name |
| `message` | string | Fully detailed caller-formatted diagnostics |

The log is intentionally verbose. Build metadata may be repeated on each line so a detached excerpt remains identifiable.

### JSON escaping

Implement a private function that writes one JSON string directly to the file:

- escape `"`, `\\`, backspace, form feed, newline, carriage return, and tab;
- encode control bytes below `0x20` as `\u00xx`;
- pass bytes `0x20` and above through unchanged so existing UTF-8 remains UTF-8;
- treat a null category, event, or format pointer as an empty string rather than crashing.

JSON escaping is for parseability, not privacy. Do not redact or normalize the message contents.

### Formatted messages

Do not impose a small fixed message buffer. Use a two-pass `vsnprintf` to determine the required length, allocate exactly enough memory, format, write, and free it. Use a short stack fallback message if sizing or allocation fails. Never pass user/game text as the format string; call sites must use `"%s"`.

Use `<inttypes.h>` and the portable `PRIu64` macros for `u64` sequence/timestamp fields rather than assuming a platform-specific `printf` width.

### Error behaviour

| Failure | Required behaviour |
| --- | --- |
| File open fails | Return `0`, leave logger closed, emit one `LOG_WARNING` |
| Message sizing fails | Write a fallback message identifying the format failure |
| Allocation fails | Write a fallback message identifying allocation failure |
| Record write/flush fails | Emit one `LOG_WARNING`, close/disable the logger, return to the game |
| Shutdown when closed | No-op |
| Initialization when already open | Return success without truncating |
| Event while closed | No-op |

Avoid recursive error logging: accessibility-log errors go to `sysLogPrintf`, and the accessibility logger must not capture `sysLogPrintf` globally.

The milestone logger is main-thread-only. Document this in the header. A later speech worker must add synchronization or marshal its events to the main thread before using it.

## Build-system change

In the `# Sources` section of `CMakeLists.txt`, add an explicit source list:

```cmake
set(SRC_ACCESSIBILITY
  "${CMAKE_SOURCE_DIR}/src/accessibility/accessibility.c"
  "${CMAKE_SOURCE_DIR}/src/accessibility/accessibility_log.c"
)
```

Add `${SRC_ACCESSIBILITY}` once to `set(SRC ...)`, preferably immediately after `${SRC_GAME}`. Do not broaden the existing game glob, move source blocks, or refactor CMake.

A fresh CMake configuration is required before building because these files are new build inputs.

## Implementation sequence

### Step 0 — Preflight

- Read `AGENTS.md`, all accessibility overview documents, and this plan.
- Confirm branch `accessibility` and record `git status --short`.
- Preserve unrelated user changes; stop if they overlap any planned file.
- Confirm every path/API listed in “Confirmed repository facts” still exists.

### Step 1 — Add headers and source registration

- Create both headers with minimal declarations and comments.
- Create compilable source skeletons with no gameplay dependencies.
- Add the explicit CMake source list.
- Configure and compile in MinGW64 immediately to catch include/build-boundary errors.

Checkpoint: the build succeeds even before lifecycle integration; no executable behaviour changes.

### Step 2 — Implement the logger

- Add logger state: file pointer, sequence, session identifier, warned/failure flags.
- Implement init, open-state query, JSON escaping, dynamically formatted event writing, flush/error handling, and idempotent shutdown.
- Use only standard C plus `fs.h` and `system.h`.
- Include forced `versioninfo.h` macros directly through the existing build setup; do not add a generated-file dependency manually.
- Add `/accessibility.log` to `.gitignore`.

Checkpoint: compile with full warnings and review every `fprintf`/format type.

### Step 3 — Implement coordinator and configuration

- Add raw config variables, effective initialized/enabled/shutdown state, and the configuration constructor.
- Implement the lifecycle truth table exactly.
- Write `session_start` and `session_stop` only when the log is successfully open.
- Report enabled/disabled logging state to `sysLogPrintf` without making disabled startup noisy.

Checkpoint: repeated calls are obvious no-ops by state inspection; no config or filesystem access occurs before `configInit`/`fsInit` at runtime.

### Step 4 — Integrate the lifecycle

- Add one include and exactly two calls to `port/src/main.c`.
- Do not touch `pdmain.c` or any game system.
- Review `git diff -- port/src/main.c` and confirm no unrelated formatting changed.

Checkpoint: established-source modifications remain a tiny hook-only diff.

### Step 5 — Build verification

Use MinGW64 only for configuration and compilation:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

Expected executable: `build/pd.x86_64.exe`.

Record CMake version, compiler version, `ROMID`, build type, warnings, and exit status. Do not fix unrelated warnings.

If practical in available CI or a suitable environment, compile one non-Windows target to prove the core has no Windows dependency. If unavailable, mark it untested rather than blocking the Windows milestone.

### Step 6 — Runtime matrix

Use a legally supplied supported ROM without recording or committing it.

Run all four configuration rows:

1. `Enabled=0`, `LoggingEnabled=0`: no `accessibility.log`.
2. `Enabled=0`, `LoggingEnabled=1`: no `accessibility.log`.
3. `Enabled=1`, `LoggingEnabled=0`: service note in console/general log, no accessibility log.
4. `Enabled=1`, `LoggingEnabled=1`: valid start/stop records.

For the enabled-log run:

- launch the game;
- reach the first normal menu;
- exit normally;
- parse every line with PowerShell `ConvertFrom-Json` or another JSON parser;
- verify sequence starts at zero and strictly increments;
- verify `t_us` is nondecreasing;
- verify build fields match the built configuration;
- verify exactly one `session_start` and one normal `session_stop`;
- verify a second launch truncates/replaces the previous session as designed.

Do not commit the produced `pd.ini`, save data, `accessibility.log`, executable, or build output.

### Step 7 — Failure and idempotence tests

Open failure: use a temporary save directory containing a directory named `accessibility.log`, enable both settings in that temporary directory's `pd.ini`, and run with `--savedir` pointing there. Opening the path as a file should fail. Verify the warning is visible and the game continues. Remove the temporary directory afterward.

Idempotence: use a temporary, uncommitted instrumentation patch or focused harness to call `accessibilityInit` twice and `accessibilityShutdown` twice. Verify one file open, one start, one stop, no truncation on the second init, and no crash on the second shutdown. Revert only that temporary instrumentation without disturbing implementation work.

Write/flush failure is difficult to reproduce portably. Exercise it if a safe local method exists; otherwise inspect the `ferror`/`fflush` path and record runtime verification as unavailable.

### Step 8 — Documentation and final audit

- Update the architecture hook ledger: mark `CMakeLists.txt` and `port/src/main.c` implemented with exact calls; leave `port/src/pdmain.c` proposed for later.
- Update Milestone 2 status and record actual deviations from this plan.
- Set this plan's implementation status to engineering-complete or identify the exact blocker.
- Run whitespace checks and `git diff --check`.
- Review `git diff --stat`, every established-file diff, and `git status --short`.
- Confirm the status contains only intended source/header/CMake/docs changes.
- Confirm no ROM, generated asset, build artifact, config, save, or log is staged.

## Expected implementation diff

Expected new implementation files: four.

Expected established code/build files changed: `CMakeLists.txt`, `port/src/main.c`, `.gitignore`.

Expected established game-system files changed: zero.

Expected Windows-specific files: zero.

Expected runtime output with defaults: no accessibility log and no user-visible gameplay change.

Stop and explain before expanding this list. A compile failure alone is not permission to refactor an unrelated system.

## Review checklist

### Architecture

- [x] Core contains no Windows headers or APIs.
- [x] No frame/gameplay hook was added.
- [x] Only two calls were added to `port/src/main.c`.
- [x] New source registration is explicit and narrow.
- [x] Disabled mode is a fast no-op.

### Configuration

- [x] Both keys default to zero and clamp to `0..1`.
- [x] Registration occurs through `PD_CONSTRUCTOR`.
- [x] Config is read before service initialization.
- [x] All four truth-table rows behave as specified.

### Logger

- [x] Each line parses as one JSON object.
- [x] All fixed schema fields are present.
- [x] JSON string escaping covers quotes, slashes, control characters, newlines, and tabs.
- [x] UTF-8 bytes are preserved (confirmed by implementation inspection).
- [x] Large formatted messages allocate dynamically rather than silently truncating.
- [x] Records flush promptly.
- [x] Open/write/format/allocation failures are nonfatal (open failure exercised; remaining paths inspected).
- [x] Initialization/shutdown/event-while-closed are safe and idempotent.
- [x] Logger main-thread limitation is documented.
- [x] No privacy redaction or diagnostic field filtering was introduced.
- [x] No ROM or extracted asset contents are logged.

### Verification

- [x] Fresh MinGW64 CMake configuration succeeds.
- [x] MinGW64 build succeeds and produces `build/pd.x86_64.exe`.
- [x] Disabled cases create no log.
- [x] Enabled case produces start/stop JSONL and parses cleanly in the real executable.
- [x] Second enabled launch replaces the prior session.
- [x] Open failure is nonfatal.
- [x] Idempotence is exercised.
- [x] Full-game accessibility-disabled runtime regression is checked.
- [x] Worktree/staging contains no runtime or copyrighted artifacts.

## Implementation result

Milestone status: **engineering-complete**

The implementation adds a platform-neutral accessibility coordinator and JSONL logger in `src/accessibility`, with public headers in `src/include/accessibility`. `CMakeLists.txt` registers the two sources explicitly. `port/src/main.c` contains the only established-code hooks: `accessibilityInit()` immediately after `configInit()`, and `accessibilityShutdown()` during normal cleanup. No frame, gameplay, menu, player, HUD, input, or Windows-specific hook was added.

Both configuration keys default to zero and clamp to `0..1`. The four configuration combinations were exercised before ROM loading: logging remains closed unless both keys are enabled. With both enabled, the logger creates `$S/accessibility.log`, writes a `session_start` record, flushes each record, and replaces the previous session at the next launch. A deliberately unopenable log path produced one warning and allowed startup to continue to the later video failure.

The required clean configuration and build commands succeeded in the MinGW64 environment and produced `build/pd.x86_64.exe`. With accessibility disabled, the real executable opened its SDL window, ran for eight seconds, accepted a standard window-close event, returned exit code zero, and created no accessibility log. With both settings enabled, the same normal-exit test produced exactly two valid JSON records: `session_start` and `session_stop`, with sequence values `0,1`, nondecreasing timestamps, and the expected `x86_64-windows` target. The generated configuration was restored to disabled and the test log was removed afterward.

A focused harness linked the real coordinator and logger and separately verified duplicate initialization and shutdown, exactly one start and stop record, monotonically increasing sequence values, valid JSON escaping, null strings, a 20,000-character message without truncation, and a disabled state after shutdown. All five harness records parsed successfully as JSON. Runtime write/flush failures and a non-Windows compile were not reproduced; their paths were reviewed and remain nonfatal by construction.

Deviation from the plan: `.gitignore` uses an unanchored `accessibility.log` entry so logs produced under temporary or custom save directories inside the worktree are ignored as well as a root-level log. Temporary smoke-test directories and harness artifacts were removed after verification.

## Known limitations accepted for Milestone 2

- Abnormal termination before or during `main` cleanup may omit `session_stop`.
- The single log file is overwritten at the next enabled launch.
- A very long or highly instrumented future session can create a large file.
- Logging is synchronous and flushes each record; later performance tests may justify buffering or a worker.
- The logger is not thread-safe.
- Only lifecycle events exist, so the log does not yet diagnose menu/gameplay accessibility.
- No non-Windows runtime test is required on a Windows-only development machine.
- Runtime write/flush failure injection was not available; those nonfatal paths were verified by inspection.

These limitations must be documented, not silently “fixed” by broadening the milestone.

## Handoff format for the implementation agent

The final implementation report must include:

```text
Milestone status:
Commit/base used:
Files added:
Files modified:
Established-file hooks and why:
Deviations from plan:

Configuration truth-table results:
Build commands and result:
Executable tested:
Runtime cases completed:
JSON parser result:
Failure/idempotence tests:
Non-Windows compile status:

Known limitations:
Unexpected regressions:
git status --short:
Recommended next task:
```

The recommended next task after this milestone is Milestone 3, a Windows speech-backend proof of concept. Do not begin it in the same implementation change.

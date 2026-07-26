# Milestone 3 implementation plan: Tolk Windows speech-backend proof

This is the implementation handoff for Milestone 3 of `ACCESSIBILITY_ROADMAP.md`. It is intentionally prescriptive so a lighter-weight coding model can execute it without reopening dependency, lifecycle, or test-scope decisions.

Plan status: **executed 2026-07-19**

Planning baseline: branch `accessibility`, commit `39f41ef91`

Implementation status: **code and machine-verifiable Windows/NVDA proof complete; human audible/braille confirmation not independently observable**

## Execution result

The implementation followed the locked shape and introduced no gameplay, menu, input, or frame-tick hook. Tolk is pinned at `e5149f0cb6ef9b941673017e0e7b7c409e485fbe`, built as a separate `Tolk.dll`, loaded by absolute executable-sibling path, and omitted from the game's static imports. The x64 NVDA controller hash is `41c1f5df5997e798fcfbf7c8f2589de811e768b069a60710600cf57cb23a0b09` (153,600 bytes). Tolk and NVDA notices are copied under `build/licenses/tolk/`.

The exact documented MinGW64 configure/build commands passed. The real executable passed all four accessibility/speech enable combinations, explicit-test and no-test-flag cases, normal window close, alternate current directory, missing Tolk, missing controller, and missing-export substitution. Disabled states never loaded Tolk. The missing-export fixture logged eight resolution attempts and seven missing names, then continued normally. Every produced JSONL file parsed.

With NVDA 2026.1 (`2026.1.0.55743`) running, Tolk detected `NVDA`, speech and braille capabilities both reported true, and the fixed request was accepted once. Representative real-game measurements were 1,523 microseconds for Tolk load/detection and 589 microseconds for the fixed `Tolk_Output` call. A focused harness accepted multilingual/supplementary Unicode and a 4,024-byte request, rejected invalid UTF-8 with Windows error 1113, cancelled output in 358 microseconds, accepted immediate later output, and verified duplicate init/shutdown plus post-shutdown output/cancel safety. A logger harness proved invalid octets are escaped and the resulting JSONL remains parseable.

The final post-build real-game verification used accessibility log session `1784492165`: 22 JSONL records parsed, NVDA detection completed in 1,684 microseconds, the 39-unit fixed request was accepted in 361 microseconds, and the lifecycle ended with `session_stop` after a normal window close. The ignored test save/log directory was removed afterward.

One implementation-discovered deviation was necessary: `src/accessibility/accessibility_log.c` now validates UTF-8 while writing JSON strings and emits invalid octets as `\u00xx`. Without this change, the required deliberately invalid UTF-8 test would corrupt JSONL before the backend could reject the request. This preserves diagnostic byte values without emitting malformed JSON.

The API results establish that NVDA accepted the requests and cancellation. The implementation agent cannot independently attest audible output, audible first-output latency, audible interruption, or braille-display output. A true no-screen-reader session was not performed because stopping the user's running NVDA would be disruptive; the missing-controller case exercised Tolk's no-active-reader path instead. i686 runtime and non-Windows compilation were unavailable. These are recorded limitations, not claimed passes.

## Goal

Add a replaceable speech-output boundary and prove one Windows implementation using Tolk. The proof must:

- leave all accessibility and speech behavior disabled by default;
- dynamically load Tolk so a missing or broken speech dependency never prevents the game from starting;
- use an active screen reader through Tolk, with NVDA as the required tested path;
- send one explicit, fixed diagnostic utterance without adding menu or gameplay narration;
- accept UTF-8 at the platform-independent boundary and convert it safely for Tolk's wide-character API;
- cancel pending output and release Tolk on normal shutdown;
- log complete backend discovery, dependency, request, result, cancellation, timing, and failure information;
- keep Windows APIs and Tolk details out of the platform-independent accessibility core;
- keep non-Windows builds working through a null backend.

This milestone proves transport, deployment, and failure behavior. It does not prove accessible menus or gameplay.

## Owner technology decision

Use [Tolk](https://github.com/dkager/tolk) as the Windows screen-reader abstraction for this proof. Pin the upstream repository as a Git submodule at commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe`; do not track a moving branch and do not download dependencies during CMake configuration or compilation.

Build Tolk as a separate shared `Tolk.dll`. The game must load it at runtime with Windows dynamic-loading APIs rather than linking its import library. This preserves the LGPL shared-library boundary and, more importantly for runtime behavior, allows the game to continue when `Tolk.dll`, a controller DLL, a required export, or an active screen reader is absent.

NVDA is the required validation target. Use Tolk's existing controller client binary and copy it next to the game executable:

```text
third_party/tolk/libs/x64/nvdaControllerClient64.dll
  -> build/nvdaControllerClient64.dll
```

For an i686 build, select `libs/x86/nvdaControllerClient32.dll` and copy it beside that build's executable. Do not rename either controller DLL: Tolk loads these exact filenames.

Do not enable Tolk's SAPI fallback in this milestone. A running supported screen reader is required for output. This avoids unexpected speech from a system voice when no screen reader is active and keeps the experiment focused on assistive-technology interoperability.

Tolk is a provisional backend choice, not an irreversible project-wide commitment. Keep every use behind the backend contract so a later measured result can replace it.

## Upstream research snapshot

Research date: 2026-07-19.

Primary source: Tolk default branch `master`, commit `e5149f0cb6ef9b941673017e0e7b7c409e485fbe`.

Relevant upstream evidence:

- The [Tolk README](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/docs/README.md) describes a Windows DLL, asynchronous output/cancellation calls, non-thread-safe operation, same-thread COM considerations, dynamic screen-reader detection, and controller DLL discovery through the working directory or `PATH`.
- [Tolk.h](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/src/Tolk.h) exposes the C ABI used by this plan: `Tolk_Load`, `Tolk_IsLoaded`, `Tolk_Unload`, `Tolk_DetectScreenReader`, `Tolk_HasSpeech`, `Tolk_HasBraille`, `Tolk_Output`, and `Tolk_Silence`.
- [Tolk.cpp](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/src/Tolk.cpp) initializes COM, owns driver detection, defaults SAPI off, and requires matched load/unload calls.
- The [NVDA driver](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/src/ScreenReaderDriverNVDA.cpp) calls `LoadLibrary` for `nvdaControllerClient64.dll` on 64-bit builds and resolves the speech, braille, cancellation, and running-test exports dynamically.
- The repository contains [`libs/x64/nvdaControllerClient64.dll`](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/libs/x64/nvdaControllerClient64.dll). It is 153,600 bytes at the pinned commit, with SHA-256 `41c1f5df5997e798fcfbf7c8f2589de811e768b069a60710600cf57cb23a0b09`.
- The repository also contains `libs/x86/nvdaControllerClient32.dll`, 132,608 bytes, SHA-256 `a0d193883dcfbeae6e69aa00a0c5aa52a194b1a265617b8f920d0c7d4164c587`.
- Tolk has no GitHub release assets and the repository does not contain a prebuilt `Tolk.dll`. Build it from the pinned source rather than assuming a binary exists.
- Tolk is licensed under LGPLv3 in [`LICENSE.txt`](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/LICENSE.txt). The included NVDA controller client has its own LGPLv2.1 notice in [`LICENSE-NVDA.txt`](https://github.com/dkager/tolk/blob/e5149f0cb6ef9b941673017e0e7b7c409e485fbe/LICENSE-NVDA.txt).

The upstream README says the project is not currently being developed. Pinning, isolation, comprehensive errors, and replaceability are therefore requirements, not optional cleanup.

## Completed planning probe

A disposable checkout of the pinned Tolk commit was compiled with this project's current MinGW64 GCC 16.1.0 environment. Unmodified upstream source has one MinGW-specific integration issue: `ScreenReaderDriverSAPI.cpp` includes MinGW's SAPI headers before the base Windows definitions they require. The probe succeeded without editing upstream after:

- force-including `windows.h` for `ScreenReaderDriverSAPI.cpp`;
- defining `INITGUID` for that translation unit so `CLSID_SpVoice` and `IID_ISpVoice` are emitted;
- linking `user32`, `ole32`, and `oleaut32`;
- statically linking the GCC and C++ runtimes for the Tolk target.

The resulting file was a PE32+ x86-64 DLL and exported the expected undecorated `Tolk_*` C symbols. It still depended on `libwinpthread-1.dll`, which the current x86-64 game executable already requires. Upstream warning noise about its private assignment operator and old generated C headers was observed; do not patch vendored source merely to silence it.

This probe establishes build feasibility only. It did not establish compatibility with a running NVDA version, speech correctness, cancellation, or user experience. Those remain implementation acceptance tests.

## Definition of done

Milestone 3 is engineering-complete only when all of the following are true:

1. Tolk is added as a pinned Git submodule and the build does not fetch a moving dependency.
2. A normal MinGW64 configuration and build produce `pd.x86_64.exe`, `Tolk.dll`, and `nvdaControllerClient64.dll` in the same output directory.
3. The output contains Tolk's and NVDA controller's license files in a documented license directory.
4. `Accessibility.SpeechEnabled` is registered in `pd.ini`, defaults to `0`, and is ignored unless `Accessibility.Enabled=1`.
5. With accessibility or speech disabled, the game does not load Tolk and produces no speech.
6. On Windows with speech enabled, the backend loads `Tolk.dll` from the executable directory, resolves every required symbol, calls `Tolk_Load` once, and detects an active screen reader without a fatal error.
7. On non-Windows targets, the same core API compiles against a null backend with no Windows or Tolk headers.
8. With NVDA running, the explicit `--accessibility-speech-test` action outputs the fixed test phrase through `Tolk_Output` and the output result is logged.
9. Pending output can be cancelled through the public core API and is cancelled during shutdown.
10. Missing `Tolk.dll`, missing NVDA controller DLL, missing required exports, no active screen reader, invalid UTF-8, output failure, and cancellation failure are all nonfatal and logged precisely.
11. Initialization, output-while-unavailable, cancellation, and shutdown are idempotent and safe.
12. The fixed request and cancellation call durations are measured. No Tolk call is added to a frame/gameplay tick in this milestone.
13. A real executable starts and exits normally with accessibility disabled, speech disabled, Tolk missing, NVDA absent, and NVDA active where those states are locally available.
14. The architecture ledger, roadmap status, testing notes, third-party notices, and this plan reflect the actual implementation and any deviation.
15. No built DLL, executable, log, configuration, save, ROM, or generated game asset is committed from `build/`.

A passing build without an audible NVDA test is not enough to call the milestone complete. If no NVDA environment is available to the implementation agent, report the implementation as code-complete with NVDA runtime validation pending.

## Explicit non-goals

- No menu, HUD, subtitle, objective, status, weapon, target, scanner, navigation, or gameplay narration.
- No announcement queue, priority policy, deduplication, replacement groups, expiry, or per-player routing.
- No permanent keyboard/controller accessibility bindings.
- No settings menu UI.
- No background speech thread or general task queue.
- No `accessibilityTick` call and no change to `port/src/pdmain.c`.
- No automatic startup utterance. The fixed phrase requires the explicit command-line test flag.
- No SAPI fallback and no Tolk backend-selection setting.
- No voice, rate, pitch, volume, punctuation, or screen-reader preference controls.
- No direct NVDA controller calls from game code; NVDA remains behind Tolk.
- No platform speech calls from `src/game` or from the platform-independent core.
- No claim of general screen-reader compatibility based solely on NVDA.
- No build-time network download, package-manager introduction, or unpinned binary URL.
- No changes inside the Tolk submodule.
- No unrelated warning cleanup or refactoring.

## Confirmed repository facts at the planning baseline

- Milestone 2 initializes `accessibilityInit()` after `configInit()` and calls `accessibilityShutdown()` in the existing normal cleanup path.
- `Accessibility.Enabled` and `Accessibility.LoggingEnabled` are constructor-registered bounded integers and both default to zero.
- `accessibilityLogEvent` is available for structured, flushed JSONL diagnostics and safely does nothing when logging is closed.
- `sysInitArgs` runs before accessibility initialization, so `sysArgCheck("--accessibility-speech-test")` is available during `accessibilityInit` without editing argument parsing.
- `CMakeLists.txt` explicitly lists the current accessibility sources and already chooses platform libraries with `WIN32`.
- Port C and C++ sources are globbed recursively, but platform-specific speech backends still need explicit selection to avoid compiling Windows and null implementations together.
- The default Windows output is `build/pd.x86_64.exe` and the project already supports recursive Git submodules.
- The current x86-64 executable depends on `libwinpthread-1.dll`; a MinGW-built Tolk DLL does not introduce that dependency category for the first time.
- There is no unit-test framework for the accessibility or port service layers.

## Locked implementation shape

Add the Tolk repository as:

```text
third_party/tolk  (Git submodule pinned to e5149f0cb6ef9b941673017e0e7b7c409e485fbe)
```

Create exactly these project-owned files:

```text
cmake/Tolk.cmake
src/accessibility/accessibility_speech.c
src/include/accessibility/accessibility_speech.h
src/include/accessibility/accessibility_speech_backend.h
port/src/accessibility/speech_tolk.c
port/src/accessibility/speech_null.c
THIRD_PARTY_NOTICES.md
```

Modify these established/project documents unless compilation proves another edit is necessary:

```text
.gitmodules
CMakeLists.txt
src/accessibility/accessibility.c
ACCESSIBILITY_ARCHITECTURE.md
ACCESSIBILITY_ROADMAP.md
ACCESSIBILITY_TESTING.md
documentation/milestones/ACCESSIBILITY_MILESTONE_03_PLAN.md
```

Do not modify `port/src/main.c`: the coordinator's existing lifecycle calls are sufficient. Do not modify any file under `src/game`, `src/lib`, `port/src/input.c`, or `port/src/pdmain.c`.

If an extra established file truly becomes necessary, stop and document the concrete compile/runtime reason before expanding this list.

## Git submodule and supply-chain contract

Add this entry to `.gitmodules`:

```ini
[submodule "third_party/tolk"]
	path = third_party/tolk
	url = https://github.com/dkager/tolk.git
```

The recorded gitlink must resolve to the pinned commit above. Verify with:

```sh
git submodule status third_party/tolk
git -C third_party/tolk rev-parse HEAD
```

The expected commit must appear exactly. Do not update to whatever `master` contains at implementation time. Do not copy a Tolk binary from an unofficial download.

On Windows, CMake should fail at configuration with a concise instruction if `third_party/tolk/src/Tolk.cpp` is absent, because silently compiling without the requested backend would invalidate the milestone. The instruction should be:

```text
git submodule update --init --recursive
```

Non-Windows configurations must not require the Tolk submodule merely to select and build the null backend.

Record the controller DLL byte size and SHA-256 during implementation verification. A mismatch against the research snapshot is a reason to stop and inspect the submodule pin, not to update the expected hash casually.

## Third-party licensing contract

Create `THIRD_PARTY_NOTICES.md` with:

- Tolk name, upstream URL, exact pinned commit, copyright notice, LGPLv3 designation, and pointer to `third_party/tolk/LICENSE.txt`;
- NVDA controller client name, its origin inside Tolk, LGPLv2.1 designation, and pointer to `third_party/tolk/LICENSE-NVDA.txt`;
- a statement that Tolk is built as a replaceable shared library and dynamically loaded by the game;
- a statement that the repository's MIT license continues to cover project-owned code, while the third-party components retain their licenses.

For every Windows build, copy these notices into:

```text
build/licenses/tolk/LICENSE.txt
build/licenses/tolk/LICENSE-NVDA.txt
```

Do not rename the upstream license files inside the submodule. Do not interpret this plan as a complete release-distribution legal review; before an official binary release, confirm that packaging supplies all notices and source/relinking obligations required by the applicable licenses.

## Tolk shared-library build contract

Put Tolk target construction in `cmake/Tolk.cmake`, included only under `if(WIN32)`.

Build a shared target with project-local target name `tolk` and output filename exactly `Tolk.dll`. Compile the upstream sources listed by its own `src/Makefile`, excluding `TolkJNI.cpp` because Java/JNI is outside this C/C++ game integration:

```text
Tolk.cpp
ScreenReaderDriverJAWS.cpp
ScreenReaderDriverNVDA.cpp
ScreenReaderDriverSA.cpp
ScreenReaderDriverSNova.cpp
ScreenReaderDriverWE.cpp
ScreenReaderDriverZT.cpp
ScreenReaderDriverSAPI.cpp
fsapi.c
wineyes.c
zt.c
Tolk.rc
```

Requirements:

- include `third_party/tolk/src` privately;
- define `_EXPORTING`, `UNICODE`, and `_UNICODE` for the Tolk target;
- apply `INITGUID` and a forced `windows.h` include only to `ScreenReaderDriverSAPI.cpp` under MinGW;
- link `user32`, `ole32`, and `oleaut32`;
- on MinGW, use `-static-libgcc` and `-static-libstdc++` for this DLL target;
- set `PREFIX ""` and `OUTPUT_NAME "Tolk"` so the runtime filename is exact;
- do not link `pd` against `tolk` or its generated import library;
- make `pd` depend on the `tolk` build target so a normal game build produces both;
- copy the finished `Tolk.dll` into `$<TARGET_FILE_DIR:pd>` if the generator places it elsewhere;
- copy the architecture-matching NVDA controller DLL beside `$<TARGET_FILE:pd>` using `copy_if_different`;
- copy the license files to the license directory using CMake commands;
- quote all paths and use generator expressions so paths containing spaces remain valid.

Do not add all of `third_party/tolk` to the game target include path. Do not compile Tolk sources into `pd.exe`. Do not patch upstream warnings. If the resource file fails under MinGW, first prove whether omitting only `Tolk.rc` affects runtime; it carries metadata rather than API behavior. Record that as a deviation rather than editing upstream resource source casually.

For non-Windows builds, do not include `cmake/Tolk.cmake`, do not build/copy DLLs, and compile only the null backend.

## Public core speech API

Create `src/include/accessibility/accessibility_speech.h` with this project-owned UTF-8 API:

```c
s32 accessibilitySpeechInit(void);
void accessibilitySpeechShutdown(void);
s32 accessibilitySpeechIsAvailable(void);
const char *accessibilitySpeechGetBackendName(void);
s32 accessibilitySpeechOutput(const char *text, s32 interrupt);
s32 accessibilitySpeechCancel(void);
```

Include `<PR/ultratypes.h>` for `s32`.

Semantics:

- all text crossing this boundary is UTF-8;
- `init` is idempotent and returns whether usable speech is currently available;
- `shutdown` is idempotent, cancels pending output, releases the backend, and clears availability;
- `isAvailable` reports effective runtime availability, not the raw setting;
- `getBackendName` returns a stable UTF-8 string owned by the speech module, or an empty string when unavailable;
- `output` returns `1` only when the backend accepted the request;
- `cancel` returns `1` only when the backend reported successful cancellation;
- null text, empty text, calls before initialization, calls after shutdown, and calls while unavailable are safe nonfatal failures;
- the API is main-thread-only for Milestone 3 and must say so in the header.

The core module owns lifecycle state and structured diagnostic events. It must not include `windows.h`, `Tolk.h`, or any file from the Tolk submodule.

## Platform backend contract

Create `src/include/accessibility/accessibility_speech_backend.h` as a private platform bridge used only by `accessibility_speech.c` and the selected backend. Use project-owned names and UTF-8 strings. It should expose enough operations to:

- initialize and shut down;
- report availability;
- report the detected backend/screen-reader name;
- output UTF-8 with an interrupt flag;
- cancel output.

Do not expose `HMODULE`, `wchar_t`, COM types, Tolk function pointers, or Windows error types through this header.

`CMakeLists.txt` must choose exactly one implementation:

| Target | Selected implementation |
| --- | --- |
| Windows | `port/src/accessibility/speech_tolk.c` |
| Apple/Linux/Switch/other supported non-Windows | `port/src/accessibility/speech_null.c` |

Because the existing `SRC_PORT` recursive glob would otherwise collect both files, explicitly filter both `speech_tolk.c` and `speech_null.c` out of `SRC_PORT`, then append only the selected backend. Do not rely on preprocessor guards inside two simultaneously compiled implementations, and do not allow both to define the backend symbols.

The null backend returns unavailable, uses no Windows/Tolk types, accepts repeated init/shutdown safely, and lets the core compile unchanged. Do not fake successful speech on the null backend.

## Configuration and activation contract

Add one constructor-registered bounded integer in `accessibility.c`:

```ini
[Accessibility]
Enabled=0
LoggingEnabled=0
SpeechEnabled=0
```

Register it as:

```c
configRegisterInt("Accessibility.SpeechEnabled", ..., 0, 1);
```

Truth table:

| Enabled | SpeechEnabled | Required speech behavior |
| ---: | ---: | --- |
| 0 | 0 | Do not initialize or load backend |
| 0 | 1 | Do not initialize or load backend |
| 1 | 0 | Accessibility/logging may operate; do not initialize or load backend |
| 1 | 1 | Initialize selected platform backend; continue nonfatally if unavailable |

`LoggingEnabled` changes evidence only; it must not control speech availability.

After the existing logger initialization/session-start logic, call `accessibilitySpeechInit` only when effective accessibility and speech are enabled. Do not change `port/src/main.c` ordering.

During `accessibilityShutdown`, shut speech down before closing the accessibility log so cancellation and unload results can be recorded. Preserve idempotence.

## Explicit diagnostic test action

Recognize this existing argument-system flag inside the accessibility coordinator:

```text
--accessibility-speech-test
```

The flag is acted upon only when both accessibility and speech are enabled. It must not implicitly enable either setting.

If the backend is available, submit exactly this UTF-8 diagnostic text with `interrupt=1`:

```text
Perfect Dark accessibility speech test.
```

Do not speak automatically on ordinary enabled launches. Do not localize this diagnostic-only string in game assets. Do not add a general-purpose command-line text argument; accepting arbitrary shell text would expand parsing and security scope unnecessarily.

If the test flag is supplied while speech is disabled or unavailable, log the reason and continue startup without a fatal error.

Cancellation is exposed through the public API and always called during speech shutdown. A focused harness should exercise explicit mid-utterance cancellation; the real-game test should also close the game during a deliberately repeated harness utterance or pending output and confirm silence/unload. Do not add a sleep to the game's startup path merely to make cancellation audible.

## Windows Tolk loader contract

`speech_tolk.c` owns all Windows and Tolk ABI details.

### Secure path resolution

1. Call `GetModuleFileNameW(NULL, ...)` using a dynamically resized buffer; do not assume `MAX_PATH` is sufficient.
2. Remove the executable filename and append `Tolk.dll`.
3. Load that absolute sibling path. Do not call bare `LoadLibraryW(L"Tolk.dll")`.
4. Record the resolved DLL path and Windows error code/message in the accessibility log when logging is enabled.

Keeping both `Tolk.dll` and the NVDA controller DLL in the executable directory satisfies the requested deployment arrangement and Tolk's internal bare-name controller load under normal Windows DLL search behavior. Runtime tests must launch the executable from a different current working directory to prove the game-side Tolk lookup does not accidentally depend on `cwd`.

### Required Tolk exports

Resolve these exact names with `GetProcAddress`:

```text
Tolk_Load
Tolk_IsLoaded
Tolk_Unload
Tolk_DetectScreenReader
Tolk_HasSpeech
Tolk_HasBraille
Tolk_Output
Tolk_Silence
```

Use private function-pointer typedefs matching Tolk's C ABI and `__cdecl` calling convention. Do not include/link Tolk's import declarations in a way that creates a process-load dependency.

If any required export is absent:

- record every missing name, not only the first;
- leave the backend unavailable;
- free the DLL;
- return normally to the game.

### Initialization

When all exports are resolved:

1. call `Tolk_Load` exactly once on the main thread;
2. check `Tolk_IsLoaded`;
3. call `Tolk_DetectScreenReader`;
4. query speech and braille capability;
5. require a detected reader with speech support for `accessibilitySpeechIsAvailable=1`;
6. convert the detected wide name to UTF-8 and retain a stable copy;
7. log load, detection, capabilities, timing, and final availability.

Tolk output calls are asynchronous, but load/detection calls are synchronous. They occur before the game loop in this milestone. Measure them; do not add detection polling to a frame tick.

Do not call `Tolk_TrySAPI` or `Tolk_PreferSAPI`. Their default disabled state is the milestone policy.

### Output and cancellation

Use `Tolk_Output`, not `Tolk_Speak`, so Tolk can provide both speech and braille according to the active reader's capabilities. Pass the requested interrupt flag exactly.

Use `Tolk_Silence` for cancellation. Record its Boolean result and call duration. A failure disables no unrelated game service.

On shutdown:

1. if Tolk was loaded, call `Tolk_Silence` once;
2. call `Tolk_Unload` exactly once on the same thread that called `Tolk_Load`;
3. clear all function pointers and retained names;
4. free `Tolk.dll`;
5. mark the backend unavailable.

Repeated shutdown is a no-op. Never call Tolk after freeing the module.

## UTF-8 and wide-string conversion contract

The Windows backend converts UTF-8 to UTF-16 with `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` using a two-pass dynamically allocated buffer.

Requirements:

- reject invalid UTF-8 without sending replacement garbage to Tolk;
- preserve ASCII, punctuation, accented Latin text, non-Latin text, and supplementary characters;
- include the terminating wide null;
- check size/allocation/conversion failures and log Windows error details;
- free the buffer after the asynchronous Tolk call returns, because Tolk/controller APIs consume or queue the text during the call;
- do not use `mbstowcs`, the current locale, ANSI code pages, or a fixed-size buffer;
- do not sanitize, redact, or truncate the logged source text.

Wide-to-UTF-8 conversion for the detected screen-reader name uses `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)` with the same dynamic/error discipline.

The null backend requires no conversion.

## Diagnostic logging contract

Use the existing JSONL logger. Do not change schema version 1 merely to add event-specific information in `category`, `event`, and `message`.

Use `category="speech"` and these event names where applicable:

```text
backend_init_start
dll_load_result
export_resolution
backend_detected
backend_unavailable
backend_init_stop
output_request
output_result
cancel_request
cancel_result
backend_shutdown_start
backend_shutdown_stop
```

Messages should include every useful field available at that point, including:

- configured enabled state and test-flag state;
- platform/backend name;
- absolute Tolk path;
- Tolk submodule commit/build identification documented by the build;
- Windows `GetLastError` numeric value and formatted message;
- each required export and whether it resolved;
- detected screen-reader name;
- speech and braille capability flags;
- complete UTF-8 request text;
- request byte length and converted UTF-16 unit count;
- interrupt flag;
- Tolk Boolean result;
- initialization, detection, conversion, output-call, cancellation, and shutdown durations in microseconds;
- unavailability/fallback reason;
- idempotence/no-op reason when useful.

Never log ROM contents, extracted assets, passwords, authentication tokens, or unrelated operating-system secrets. The owner has otherwise requested comprehensive feature diagnostics, so do not redact paths, reader names, text, error details, handles, or function addresses when they help debug the backend.

Logging failure must not change speech behavior. Speech failure must not change game/logging behavior.

## Error-state contract

| State or failure | Required result |
| --- | --- |
| Accessibility disabled | Backend is never touched |
| Speech disabled | Backend is never touched |
| Non-Windows target | Null backend reports unavailable |
| Tolk submodule absent during Windows configuration | Clear CMake error with submodule command |
| `Tolk.dll` missing at runtime | Speech unavailable; game continues |
| Controller DLL missing | Tolk may load, NVDA remains unavailable; game continues |
| Required Tolk export missing | Record all missing exports, unload DLL, game continues |
| `Tolk_Load` does not report loaded | Backend unavailable; unload safely |
| No supported reader active | Backend loaded but unavailable; no speech; game continues |
| Active reader has no speech | Report capabilities; speech unavailable |
| Invalid/empty/null UTF-8 request | Reject nonfatally; no Tolk output call |
| Allocation/conversion failure | Reject nonfatally with detailed log |
| `Tolk_Output` returns false | Report request failure; keep game running |
| `Tolk_Silence` returns false | Report cancellation failure; still unload on shutdown |
| Repeated init | Return existing state without reloading or re-speaking test phrase |
| Repeated shutdown | No-op |
| Output/cancel before init or after shutdown | Safe failure |

Do not call `exit`, `abort`, `sysFatalError`, or a blocking retry loop for any runtime speech failure.

## Implementation sequence

### Step 1 — Preflight and pin

- Confirm branch `accessibility`, baseline commit, and clean/expected status.
- Re-read this plan and the five root accessibility documents.
- Add the Tolk submodule at the exact commit and verify both controller DLL hashes.
- Inspect upstream licenses before writing CMake integration.
- Do not update the pin based on branch freshness.

### Step 2 — Build Tolk independently

- Add `cmake/Tolk.cmake` and the Windows-only shared target.
- Encode the MinGW SAPI source properties from the completed probe.
- Produce `Tolk.dll` without linking it into `pd.exe`.
- Inspect its exports and imported DLLs with MinGW `objdump`.
- Verify no accidental `Tolk.dll` or import library is tracked outside ignored `build/`.

Stop here if Tolk cannot be built as a replaceable shared library. Do not respond by compiling its source directly into the game executable.

### Step 3 — Package development output

- Add architecture-aware controller DLL copy rules.
- Copy Tolk and NVDA license files to `build/licenses/tolk`.
- Verify all three runtime files are adjacent/available as required after an ordinary build.
- Verify repeated incremental builds use `copy_if_different` and do not dirty the repository.

### Step 4 — Add backend abstraction

- Add the public core API and private backend contract.
- Add the null backend first and compile it in a non-Windows preprocessing/compile path if available.
- Add the Windows dynamic loader with absolute executable-directory lookup.
- Keep all Windows/Tolk includes in `speech_tolk.c`.

### Step 5 — Add coordinator configuration and lifecycle

- Register `Accessibility.SpeechEnabled=0`.
- Initialize speech only when accessibility and speech are both enabled.
- Recognize the explicit test flag and output the fixed phrase once.
- Shut speech down before the logger so cancel/unload events remain visible.
- Do not modify the established entry point.

### Step 6 — Add comprehensive evidence

- Emit every listed speech event with timings and complete details.
- Confirm logging-off operation still speaks without creating `accessibility.log`.
- Confirm logging-on operation produces parseable JSONL and exact request/result ordering.

### Step 7 — Build and inspect

Use only the MSYS2 MinGW64 environment for build commands:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

Verify:

```text
build/pd.x86_64.exe
build/Tolk.dll
build/nvdaControllerClient64.dll
build/licenses/tolk/LICENSE.txt
build/licenses/tolk/LICENSE-NVDA.txt
```

Inspect `pd.x86_64.exe` to confirm it has no static import entry for `Tolk.dll`. Inspect `Tolk.dll` for the expected exports and runtime dependencies.

If an i686 toolchain is readily available, configure a separate build directory and verify the 32-bit controller selection. Otherwise record i686 as untested without changing the x86-64 acceptance result.

### Step 8 — Configuration and absence matrix

Use a temporary save directory/config and preserve user data. Test:

1. `Enabled=0`, `SpeechEnabled=0`: no Tolk load, no speech.
2. `Enabled=0`, `SpeechEnabled=1`: no Tolk load, no speech.
3. `Enabled=1`, `SpeechEnabled=0`: no Tolk load, no speech.
4. `Enabled=1`, `SpeechEnabled=1`, no test flag: backend detection only, no diagnostic phrase.
5. Same settings with test flag and logging off: speech works if available; no accessibility log.
6. Same settings with test flag and logging on: speech works and ordered backend/request/result evidence parses.
7. Test flag while speech disabled: no phrase and a logged skip reason when logging is active.

Use a tool such as Process Explorer, Process Monitor, or an equivalent module query if available to verify that disabled rows never load `Tolk.dll`; do not infer this only from silence.

### Step 9 — Dependency failure matrix

Operate only on ignored build outputs and restore every renamed file in `finally`/equivalent cleanup:

- temporarily remove/rename `Tolk.dll`, launch, and verify normal game startup with backend-unavailable evidence;
- restore Tolk, temporarily remove/rename `nvdaControllerClient64.dll`, and verify Tolk loads but NVDA cannot be selected;
- launch from a current directory other than `build` and verify the absolute Tolk lookup still succeeds;
- if practical, use a temporary harmless DLL with missing exports to verify all required names are reported and the game continues;
- run with no supported screen reader active;
- run repeated init/shutdown and output/cancel-while-unavailable through a focused harness.

Never alter or replace a system-installed NVDA file. Only manipulate the build-local copies created for this game.

### Step 10 — NVDA output and cancellation proof

With a currently supported x64 NVDA running:

- launch the real executable with accessibility and speech enabled plus `--accessibility-speech-test`;
- confirm the exact fixed phrase is spoken once and, where supported, appears on braille output through `Tolk_Output`;
- confirm the active reader is logged as NVDA with speech/braille capabilities;
- let one request complete normally;
- use a focused harness to submit a long UTF-8 diagnostic string, wait outside the game loop, cancel it, and immediately submit the short fixed phrase again;
- verify cancellation audibly stops the first request and does not prevent later speech;
- close the real game normally while output is pending and verify clean exit/unload;
- repeat initialization/shutdown and verify no duplicate phrase, double unload, or crash;
- test ASCII, punctuation/numbers, `café`, representative Japanese text, and a supplementary-plane character;
- submit deliberately invalid UTF-8 through the focused harness and verify rejection without a Tolk call.

Record the NVDA version, Tolk pin, Windows version/architecture, exact settings, whether braille hardware/display was available, and what was heard. Do not claim braille validation when no braille output was observed.

### Step 11 — Timing and regression evidence

Measure with `sysGetMicroseconds` and record:

- Tolk DLL load;
- export resolution;
- `Tolk_Load`;
- screen-reader detection/capability queries;
- UTF conversion;
- `Tolk_Output` call duration;
- time from request to first audible output, measured externally as closely as practical;
- `Tolk_Silence` call duration;
- unload duration.

Do not invent a pass threshold before observing the technology. Record measurements, obvious stalls, and whether calls occur before or during the game loop. Any frame-loop integration or worker-thread redesign belongs to a later milestone unless the proof reveals a startup-blocking defect severe enough to prevent safe use.

Re-run the real executable with accessibility fully disabled and verify normal window creation, input/audio, and normal exit with no accessibility speech or log.

### Step 12 — Documentation and final audit

- Mark the module layout and CMake backend selection implemented in `ACCESSIBILITY_ARCHITECTURE.md`.
- Update the hook ledger; `port/src/main.c` should remain unchanged.
- Update Milestone 3 status and actual evidence in `ACCESSIBILITY_ROADMAP.md`.
- Update `ACCESSIBILITY_TESTING.md` with actual setup, commands, measured timings, NVDA/controller versions, and limitations.
- Change this plan status to executed and record deviations/results.
- Run whitespace and `git diff --check` checks.
- Review every established-file diff, submodule pin, binary hash, staged state, and `git status --short`.
- Confirm no generated Tolk/game binary, ROM, config, save, or log is staged.

## Expected implementation diff

Expected new project-owned implementation files: seven, plus this already-created plan.

Expected new dependency: one pinned `third_party/tolk` gitlink and one `.gitmodules` entry.

Expected established code/build files changed:

```text
CMakeLists.txt
src/accessibility/accessibility.c
```

Expected established game-system files changed: zero.

Expected Windows-specific project code: one backend implementation under `port/src/accessibility` and one Windows-only CMake dependency target.

Expected runtime behavior with defaults: no Tolk load, no speech, and no user-visible change.

Expected explicit test behavior: one fixed diagnostic phrase only when accessibility and speech are enabled and `--accessibility-speech-test` is present.

Stop and explain before adding a gameplay/frame hook, input binding, arbitrary text argument, worker thread, SAPI fallback, or direct NVDA integration.

## Review checklist

### Dependency and build

- [x] Tolk gitlink is pinned to the exact researched commit.
- [x] No build-time network fetch is introduced.
- [x] Tolk builds as a separate replaceable DLL under MinGW64.
- [x] MinGW compatibility flags are source-scoped and upstream files remain unmodified.
- [x] Game executable has no static Tolk import.
- [x] Architecture-matching NVDA controller DLL is beside the executable.
- [x] Controller DLL hash matches the pinned source.
- [x] Tolk and NVDA license files are present in build output.
- [ ] Missing Windows submodule produces a useful configuration error.
- [ ] Non-Windows null builds do not require the submodule.

### Architecture

- [x] Core speech source/header contain no Windows or Tolk types.
- [x] Exactly one platform backend is selected per target.
- [x] No established game-system hook was added.
- [x] `port/src/main.c` remains unchanged.
- [x] No frame/tick call or worker thread was added.
- [x] Tolk is used only from the Windows backend and only on the main thread.

### Configuration and lifecycle

- [x] `SpeechEnabled` defaults to zero and clamps to `0..1`.
- [x] Accessibility-disabled and speech-disabled states never load Tolk.
- [x] Test flag never implicitly enables speech.
- [x] Initialization and shutdown are idempotent.
- [x] Shutdown cancels output before unloading and before logger close.
- [x] All exercised failure states degrade to unavailable without stopping the game.

### Text and output

- [x] Public speech text contract is UTF-8.
- [x] UTF-8/UTF-16 conversion is dynamic, strict, and locale-independent.
- [x] Null, empty, invalid, long, and multilingual inputs are safe.
- [x] Tolk output uses `Tolk_Output` with the exact interrupt flag.
- [x] Cancellation uses `Tolk_Silence` and later output still works.
- [ ] The fixed test phrase speaks exactly once per explicit test launch.
- [x] Ordinary enabled launch makes no unsolicited output request.

### Diagnostics

- [x] Backend discovery and every required export are logged.
- [x] Full request text, flags, results, paths, errors, and timings are logged.
- [x] Logs remain valid JSONL, including deliberately invalid input bytes.
- [x] Logging off does not prevent backend initialization/output acceptance.
- [x] Logging failure does not change speech/game behavior.
- [x] No prohibited ROM/asset/secret content is introduced.

### Runtime verification

- [x] Fresh MinGW64 configure/build succeeds.
- [x] Disabled real-game launch and normal exit pass.
- [x] Missing Tolk DLL is nonfatal.
- [x] Missing NVDA controller DLL is nonfatal.
- [ ] No active screen reader is nonfatal.
- [x] Launch from another current directory succeeds.
- [ ] Active NVDA speaks the fixed phrase.
- [ ] Explicit cancellation is audible and nonfatal.
- [x] Unicode cases work and invalid UTF-8 is rejected.
- [x] Initialization/output/cancel/unload timings are recorded.
- [x] No runtime, generated, or copyrighted game artifacts are staged.

## Known limitations accepted for Milestone 3

- Tolk is not thread-safe and all calls remain on the main thread.
- Tolk load/detection is synchronous at startup; no ongoing frame polling is added.
- Only a fixed diagnostic phrase is exposed; no useful game state is spoken.
- SAPI fallback is deliberately disabled.
- NVDA is the required validated reader; other Tolk-supported products remain unverified unless explicitly tested.
- Tolk's README says the project is not currently developed, so compatibility with future screen-reader releases is not guaranteed.
- The upstream NVDA driver loads its controller DLL by filename. Co-location beside the executable is the controlled deployment assumption.
- Tolk's `IsSpeaking` support is limited and is not used to promise completion timing.
- Abnormal process termination may prevent cancellation and `Tolk_Unload`.
- An i686 controller is available, but i686 runtime validation may remain pending if no 32-bit environment is available.
- Official release packaging and license compliance need a separate final distribution review.
- Independent blind-user validation is not required for this transport-only proof, but it becomes essential for Milestone 4 menu narration.

Do not silently broaden the milestone to fix these limitations.

## Handoff format for the implementation agent

The final implementation report must include:

```text
Milestone status:
Commit/base used:
Files and submodules added:
Files modified:
Established-file hooks and why:
Tolk commit and controller hash:
Deviations from plan:

Configuration truth-table results:
Build commands and result:
Executable/Tolk/controller files tested:
DLL import/export inspection:
Runtime absence/failure cases:
NVDA version and output result:
Exact spoken output:
Cancellation result:
Unicode/invalid-input result:
Timing measurements:
Accessibility log session and relevant events:
Non-Windows/i686 status:

Known limitations:
Unexpected regressions:
git status --short:
Recommended next task:
```

The recommended next task after this milestone is Milestone 4, the main-menu narration slice. Do not begin menu hooks or speech queue policy in the Milestone 3 implementation change.

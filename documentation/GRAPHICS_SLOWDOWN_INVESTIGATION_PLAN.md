# Intermittent graphics slowdown investigation plan

## Goal

Find the phase and external condition responsible for the intermittent drop from
normal rendering cadence to severe choppiness, then produce enough repeatable
evidence to fix it without weakening accessibility features or guessing from
memory use alone.

This is an investigation plan, not a claim that the accessibility layer or the
graphics driver is at fault.

## Implementation status

The diagnostic infrastructure in phases 1 and 2 is implemented behind the
existing `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` compile-time flag. The
in-process path uses a 180-frame pre-trigger ring and a 480-frame episode
buffer. The external capture and report helpers are:

- `tools/accessibility/capture_graphics_diagnostics.ps1`
- `tools/accessibility/analyze_graphics_diagnostics.ps1`

The normal build leaves the flag disabled; a capture build must enable it
explicitly. A new runtime reproduction, focused A/B comparison, root-cause
conclusion, and corrective patch remain pending.

## Evidence from the 2026-07-25 session

The latest `build/accessibility.log` contains session `1784987321`, build
`8b3f6c95f`, default `ntsc-final` x86-64 `RelWithDebInfo`, with
`performance_diagnostics=1`. The session ran for about 19 minutes.

- Rendering was stable at approximately 60 FPS through `t_us=395500000`.
- The first degraded window at about 396.5 seconds reported 49.7 FPS and an
  82 ms maximum frame gap.
- From about 397.6 through 406.1 seconds, rendering remained between 10.7 and
  12.3 FPS. Individual frame gaps were commonly 82-205 ms.
- The logical clock still advanced at approximately 60 ticks per second before
  the menu opened. This means each rendered frame was catching up by several
  logical ticks; it was not simply a 12 Hz game-speed setting.
- A menu opened at about 398.8 seconds. Targeting, beacon, and cane scope were
  suspended, all procedural tone slots became inactive, and the audio mixer
  subsequently reported only pass-through calls. Rendering nevertheless
  remained at 11-12 FPS for another seven seconds.
- Working set and private bytes remained essentially flat through the onset:
  roughly 155.5 MiB working set and 555.3 MiB private bytes. There was no
  monotonic allocation ramp associated with the failure.
- The probable Windows graphics-reset interval appears as one 2.067-second
  frame gap in the window ending around 409.1 seconds. Rendering recovered to
  59 FPS in the next window and 60 FPS thereafter.
- Private bytes temporarily rose by roughly 16 MiB immediately after the reset,
  then returned to the prior level two seconds later. That pattern is more
  consistent with graphics-stack recovery or remapping than a retained game
  allocation, but this is only an inference.
- No matching Display, DxgKrnl, NVIDIA, WHEA, or Kernel-Power event was present
  in the System event log within two minutes of the episode. The keyboard reset
  does not necessarily generate a conventional driver-timeout event.
- The machine currently reports an NVIDIA GeForce RTX 4080 Laptop GPU with
  driver 610.74. The game used VSync, borderless fullscreen, framebuffer
  effects, and MSAA level 1.

The strongest current hypothesis is a graphics presentation/driver scheduling
problem. A game-side render workload or synchronization problem remains
possible. An accessibility audio workload or ordinary process-memory leak is
low probability for this particular episode because the slowdown persisted
with those systems inactive and memory flat.

## Current observability gap

`performance/frame_window` measures complete main-loop cadence but not the
parts of a frame. The relevant path is:

1. `videoStartFrame` / `gfx_start_frame`: SDL events, drawable dimensions, and
   framebuffer-size maintenance.
2. `videoSubmitCommands` / `gfx_run`: display-list translation, OpenGL command
   submission, framebuffer resolve/composite, frame limiting, and
   `SDL_GL_SwapWindow`.
3. `videoEndFrame` / `gfx_end_frame`: renderer finish and swap completion.
4. Remaining simulation, menu, input, audio scheduling, and accessibility work.

The present log cannot distinguish a slow display-list translation from a
blocked OpenGL call or a blocked buffer swap. Because the failure is
intermittent, the next diagnostic build should capture all of these boundaries
in one session rather than requiring a new build after each reproduction.

## Phase 1: low-overhead in-process instrumentation

Extend the existing compile-time
`ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS` facility. It must remain off by default
and must add no heap allocation, file I/O, OpenGL query stall, or lock in a
normal build.

### Frame-phase timing

Use one monotonic timer and fixed counters to record count, total time, and
maximum time for:

- main-loop interval;
- `videoStartFrame`;
- SDL event and drawable-dimension handling;
- `videoSubmitCommands`;
- display-list translation and `gfx_flush`;
- default/game framebuffer setup;
- framebuffer resolve/composite;
- rendering API `end_frame`;
- explicit frame-limiter wait;
- `SDL_GL_SwapWindow`;
- `videoEndFrame` and renderer `finish_render`;
- unaccounted time outside the measured rendering phases.

The Fast3D implementation is a repository boundary normally left untouched.
For this diagnosis, any change under `port/fast3d` must be instrumentation-only,
compile out completely when the flag is off, expose a small read-only
diagnostic structure through `gfx_api.h`, and contain no accessibility policy
or logging calls. `port/src/video.c` or the accessibility performance observer
will consume the structure and perform the rate-limited log write.

### Automatic stall episode capture

Maintain a preallocated ring of the last 180 frame timing records. Define:

- a slow frame as at least 50 ms;
- episode start as three slow frames within one second or a one-second render
  rate below 30 FPS;
- recovery as two consecutive one-second windows above 50 FPS;
- a severe frame as at least 250 ms.

Do not write a log line for every frame and do not flush the log while the
graphics path is degraded. On recovery or orderly shutdown, emit:

- one `graphics/stall_episode` summary with start/end time, stage, menu state,
  window/fullscreen state, dimensions, swap interval, frame count, and worst
  phase;
- aggregates for every phase;
- the ten worst retained frames and a small evenly sampled before/during/after
  timeline;
- process-memory and accessibility-audio snapshots already available to the
  performance observer.

If the process is force-closed, the ordinary one-second phase aggregates must
still leave useful evidence even if the episode summary cannot be flushed.

### Graphics context metadata and errors

Log once at startup:

- OpenGL vendor, renderer, version, and shading-language version;
- SDL video driver;
- requested and effective swap interval;
- desktop/display refresh rate;
- window mode, drawable size, framebuffer-effects state, and MSAA;
- availability of `KHR_debug` and robustness/reset-status extensions.

Do not call `glFinish` as a diagnostic because it changes synchronization and
can hide the issue. Do not poll `glGetError` after every command. If a reset
status query is supported without changing context creation, sample it once per
performance window and on recovery. Capture existing asynchronous GL debug
messages in a fixed bounded buffer only if doing so does not require a
synchronous debug callback.

### Diagnostic independence

When the compile-time flag is enabled and local accessibility logging is
enabled, graphics/performance records should remain available even if
`Accessibility.Enabled=0`. This makes the all-accessibility-off control useful.
Normal builds and normal logging behavior remain unchanged.

## Phase 2: external Windows/GPU collector

Add a PowerShell helper under `tools/accessibility/` which starts before the
game and samples at 2 Hz into an ignored directory such as
`build/diagnostics/<timestamp>/`. It should discover the game PID rather than
requiring manual PID entry and record:

- process CPU, working set, private bytes, commit, handles, threads, GDI
  objects, and I/O rates;
- NVIDIA utilization, clocks, power/performance state, temperature, dedicated
  memory, and driver version through the locally available `nvidia-smi`;
- Windows GPU-engine utilization and dedicated/shared adapter memory counters
  associated with the process when those counters are available;
- DWM CPU/GPU activity;
- active power plan and whether the machine is on AC or battery;
- relevant System event-log records at collector shutdown;
- an exact copy of the effective video/accessibility settings and Git build
  identity, but no ROM or extracted asset data.

The helper must use bounded sampling, flush incrementally, tolerate unavailable
performance counters, and stop cleanly after the game exits. A companion parser
should correlate these samples with `graphics/stall_episode` and produce a
compact timeline showing FPS, slowest in-process phase, GPU utilization,
clocks/power state, GPU memory, CPU, and process memory.

If phase timing identifies `SDL_GL_SwapWindow` or another GPU wait as the
blocked phase but the 2 Hz collector does not explain it, capture one focused
Windows Performance Recorder GPU trace. WPR is available on this machine.
Because ETW GPU traces are larger and more intrusive, use them only after a
normal diagnostic session reproduces the fault.

## Efficient reproduction protocol

Use one saved profile and a repeatable route in the same mission/stage where
the latest episode occurred. Each run should last 20 minutes or until an
episode and recovery have both been captured.

For every run:

1. Start the external collector.
2. Launch `build/pd.x86_64.exe` only from the initialized MinGW64 environment.
3. Record the accessibility session ID and test variant.
4. Exercise ordinary movement, combat, scanner audio, menus, pause/resume, and
   at least one stage or result-screen transition.
5. If choppiness begins, leave it untouched for five seconds, open the pause
   menu for five seconds, then resume for five seconds. This establishes
   whether the fault survives gameplay/accessibility suppression.
6. Use Windows+Ctrl+Shift+B only after that observation window. Wait at least
   ten seconds after recovery, then exit normally if reliable enough.
7. Preserve the log and collector directory before starting the next run.

Do not begin with a large matrix. Make the next variant depend on the measured
slow phase:

| Result from diagnostic run | Next focused comparison |
| --- | --- |
| `SDL_GL_SwapWindow` or renderer finish dominates | VSync 1 versus VSync 0 plus 60 FPS limiter; then borderless versus windowed |
| Framebuffer setup/resolve dominates | Framebuffer effects on versus off; verify dimensions and resize events |
| Display-list translation/flush dominates | Compare the same scene and camera route; add bounded texture/shader/cache counters |
| `videoStartFrame`/event handling dominates | Log SDL window/focus/size/display events and test borderless versus windowed |
| Unaccounted game-side time dominates | Use the existing engine profile markers, then split only the dominant gameplay/accessibility phase |
| External GPU clocks or utilization collapse | Repeat on AC power and capture WPR/NVIDIA state before changing game code |
| Failure occurs with all accessibility features off | Remove accessibility subsystems from the primary suspect set |
| Failure occurs only with accessibility enabled | Disable one feature family at a time, beginning with procedural audio, then world queries, then speech/logging |

Run the current all-features-enabled configuration first, then one
all-accessibility-off control. Only run the VSync, window-mode, or framebuffer
comparisons indicated by the first captured phase. This avoids spending hours
on combinations unrelated to the blocking call.

## Decision and fix criteria

Do not implement a speculative recovery such as recreating the OpenGL context,
toggling VSync, or automatically resetting graphics until the blocking phase is
known. Such workarounds can lose resources, mask a driver fault, or introduce a
new crash.

A root-cause claim requires:

- at least one captured degraded episode and recovery;
- one phase accounting for the lost frame time, or an external GPU/driver event
  that explains it;
- a focused comparison that changes the failure consistently or a trace that
  identifies the blocking API/resource;
- evidence distinguishing cause from effects such as catch-up ticks and
  temporary post-reset memory mappings.

A proposed fix passes engineering verification only after:

- three consecutive 30-minute runs of the original stress route;
- one run with all accessibility features active and overlapping;
- pause/menu, stage transition, death/restart, and normal shutdown coverage;
- no sustained render rate below 55 FPS for more than two seconds;
- no unexplained frame gap over 100 ms;
- no monotonic process/GPU-memory, handle, thread, oscillator-slot, or
  framebuffer growth;
- no regression in speech, spatial audio, scanner state restoration, or normal
  graphics behavior.

Blind-user acceptance remains necessary after engineering verification because
stable timing alone does not establish that speech and spatial cues remain
usable during combat.

## Planned deliverables

1. Compile-time-only frame-phase and stall-episode instrumentation.
2. External Windows/NVIDIA capture script and correlated report script.
3. One baseline diagnostic package from the failing configuration.
4. One focused A/B package chosen from the measured dominant phase.
5. Root-cause report with supporting session IDs and timelines.
6. Separately reviewable fix, rollback instructions, and long-session
   verification evidence.

# Accessibility logging latency diagnostics

## Purpose and scope

Measure before changing delivery. The logger still writes synchronously on the
main thread with its existing 64 KiB stdio buffer, 4 KiB format buffer, complete
oversized-message fallback and first/one-second/lifecycle/shutdown flush policy.
It does not use a worker queue, discard records, truncate large messages or
change personal settings. No engine hook or speech/audio behavior changes.

Buffering reduces the frequency of writes; it does **not** guarantee nonblocking
I/O. A full buffer can drain during JSON serialization, not only during explicit
`fflush`. Large formatted records allocate temporary storage that is freed after
writing, including error paths. Allocation is not itself evidence of a leak.

## Enable and collect

In MinGW64, use the existing compile-time switch:

```sh
cmake -G "Unix Makefiles" -Bbuild -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON .
cmake --build build -j4 -- -O
```

Keep local accessibility logging enabled, play normally and save Shift+F2
captures when useful. Do not overwrite a valuable active-session log by starting
another game before preserving it. The diagnostics flag remains off in normal
builds; this change does not enable it for the owner automatically.

With diagnostics enabled, `performance/log_window` appears roughly once a
second through the existing performance observer, with a final partial report
before `lifecycle/session_stop`. Normal builds have no profiling counters,
additional timing calls or summary records. Reports contain:

| Fields | Meaning |
| --- | --- |
| `window_us`, `events` | Observed interval and serialized event attempts |
| `format_calls`, `format_total_us`, `format_max_us` | Formatted-wrapper calls and formatting/allocation time before writing |
| `write_total_us`, `write_max_us` | JSON serialization plus stdio writes, including implicit buffer drains; excludes explicit flush time |
| `flushes`, `flush_total_us`, `flush_max_us` | Explicit flush calls/time, including failed attempts |
| `allocation_attempts`, `allocations`, `allocated_bytes`, `frees`, `allocation_failures` | Oversized-message temporary storage; bytes are cumulative, not live memory |
| `io_failures` | Nonfatal write/flush failure handling during the observed window |
| `worst_write_category`, `worst_write_event` | Fixed-size labels for the slowest observed serialization/write attempt |

Snapshots reset before their summary is written. The summary's own work belongs
to the next window, not recursive profiling output. Normal shutdown leaves the
terminal lifecycle record last. The final summary cannot include its own cost,
the later session-stop record, final flush or close. Open/close, bookkeeping and
failure-warning costs are not covered by the reported brackets. Main-thread-only
accessors expose counters to tests; there is no new cross-thread shared state.

These intervals are not a comprehensive frame-time measure. Formatting, write
and explicit flush totals are separate (do not double-count them as inclusive
nested timers). Correlate with the same session's `graphics/frame_window`,
`performance` windows and saved episodes. A large write maximum suggests inspecting
implicit buffer drains or record serialization; a large flush maximum points at
explicit I/O. Compare allocation counts with frees before alleging a leak.
Failure may disable logging before it can report itself; the existing general
log warning and diagnostic accessor remain the evidence in that case.

## Decision criteria

Do not introduce a worker solely because synchronous I/O is theoretically able
to stall. First collect sessions that show repeatable log cost near bad frames.
If it is material, design a bounded preallocated queue with explicit full-queue,
ordering, complete-record, error, crash-tail and shutdown semantics. Keep the
owner's requirement for complete useful logs; do not silently sample away
diagnostic evidence. The previous graphics slowdown's cause remains unproven.

Disable instrumentation after collection by configuring
`-DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=OFF` and rebuilding in MinGW64.

## Verification and limits

The logger fixture runs in both diagnostics modes using anonymous temporary
files. Synthetic clocks and forced flush/write/allocation/formatting failures
verify accounting, cleanup, JSON escapes, valid/invalid UTF-8, complete long
messages, flush policy, report self-accounting and safe repeated shutdown.
The full regression suite has 26 tests. These are not real storage-latency,
GPU-cause, runtime speech, or independent blind-user acceptance results. No new
game session or log ID is claimed. No user configuration is changed.

On 2026-10-28 all 26 regressions passed. Full Windows x64 `ntsc-final`
RelWithDebInfo builds were checked with diagnostics enabled in the isolated
`build/logging-diagnostics-check` directory and disabled in the normal developer
build. No game was launched; compiler/link and synthetic evidence must not be
read as real-session latency or accessibility acceptance. Existing upstream
warnings were left unchanged. The developer build keeps diagnostics off and its
existing enabled accessibility/logging/scanner configuration.

Reverting this commit removes the measurements and documentation changes;
there is no save/configuration migration. Historical milestone evidence remains
historical; the architecture's current-evidence section now distinguishes
implemented systems and owner-reported Prism tests from outstanding validation.

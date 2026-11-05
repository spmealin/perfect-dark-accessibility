# Priority-aware speech scheduler design

Status: initial implementation, owner listening acceptance pending. The policy
below is the design contract; this section records concrete implementation
choices and limitations.

## Owner-requested pickup pacing refinement

Pickup messages have a separate total speech budget, default 1,000 ms, with
no extra startup padding or inter-message gap. Configuration:
`Accessibility.SpeechPickupDurationMs=1000` (250–3000; restart after editing).
Native pickup producers supply semantic ammo/weapon/item flags, including
localized weapon overrides; the HUD strips them after the existing observer
call so visual behavior is unchanged. Do not infer category from message text.

A weapon pickup immediately interrupts active ammo and discards earlier
pending ammo reports, without replaying that interrupted ammo sentence.
Subsequent ammo remains eligible after the weapon. Weapon selection still
outranks pickup speech; weapon pickups do not interrupt foreground menus,
requests, or weapon follow-ups. Other HUD entries retain FIFO/replay semantics.
This is an explicit exception to the original blanket HUD-retention policy
below, requested after listening tests found excessive pickup delays.

For pickups, observed completion can release the slot early; otherwise the
one-second budget expires even if the provider's busy indication lingers.
Lower-priority output still appends rather than interrupting. This pacing is
not a change to speech rate and cannot guarantee provider delivery. Tests cover
ammo/weapon/ammo, queued ammo replacement, one-second cadence, selection
priority and item pickups. Startup and Shift+F2 logs include the pickup budget.

The scheduler lives in `src/accessibility/accessibility_speech_queue.c` behind
existing announcement adapters. Immediate quick-selection names use existing
weapon hooks; settled ammo waits for matching weapon and wield state. A 64 HUD
queue and separate 64 weapon/follow-up queue share a fixed 132-record pool.
Prism speaking-state lookup is optional and the null backend reports unsupported.
Completion uses 50 ms polling, a conservative startup grace, a text-length
estimate, and a stuck-busy watchdog. Retry is capped at three output attempts,
250 ms apart. Diagnostic capture records queues, replay/overflow counts and
output/query call costs. Configuration defaults are
`Accessibility.SpeechMillisecondsPerCharacter=60` (10–250) and
`Accessibility.SpeechStartupGraceMs=200` (100–2000); restart after edits.

No per-utterance completion receipt exists. When timing is estimated, provider
queuing may outlive our estimate; logs identify estimated completion rather
than claiming delivery. Preemption replay can repeat text. Speaking-state
support and usefulness still need owner tests with each provider. The active
initial respawn prompt is kept separately from replaceable countdown updates.
Foreground requests use immediate dispatch. The configuration is default-on
through existing speech/feature enable switches, with no separate opt-in.

Verification: all 42 regression tests pass in diagnostics-off/on variants,
including production quick-selection code and injected-provider scheduling.
The MinGW64 Windows x64 `ntsc-final` build passes. No game session or real
screen-reader listening test was launched by the agent. Existing graphics and
binaural diagnostics build choices are preserved. This is not owner acceptance.

Handoff build: `build/pd.x86_64.exe`, RelWithDebInfo, with
`ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON` and
`ACCESSIBILITY_BINAURAL_EXPERIMENT=ON`. The actual adjacent `pd.ini` has
Enabled, SpeechEnabled, HudMessages and WeaponChangeAnnouncements enabled,
and CompassMode=1. The two new timing keys are absent and use registered
defaults; they will be saved by the normal configuration path. No new
session-start log was generated because launching would overwrite the owner's
last session. Its pre-change ordering evidence remains session `1793842800`.

Rollback: remove/revert this speech-scheduler feature as one change, including
its announcement/weapon adapters, optional backend query, loop/teardown hooks,
configuration, and tests, then rebuild. Do not revert the preceding audio mixer,
mine cue, checkbox narration or upstream merge. No save/ROM/mission data changed.
Known behavioral tradeoffs are replayed HUD prefixes, deliberate loss of stale
compass words, bounded overflow/failure loss with diagnostics, estimated
completion on unsupported providers, and possible HUD starvation while the
player continuously cycles weapons. Runtime provider usefulness and actual
speech timing remain owner-test questions.

## Goal and evidence

The automatic gameplay priority is **weapon selection > HUD > compass**.
Lower-priority automatic speech must never interrupt higher-priority speech.
The scheduler must preserve distinct HUD events and immediately announce every
valid weapon-selection change, including intermediate wheel selections. Weapon
cycling is also inventory exploration, not merely a way to reach a final weapon.
Only compass history is disposable replaceable navigation feedback.

Session `1793842800` provides a reproducible ordering example: at
414.516 seconds, a quick-change Callisto NTG announcement was accepted; about
84 ms later, an interrupting compass East announcement was accepted. Current
announcement functions forward directly to Prism; their priority/group log
labels do not currently implement arbitration.

## Policies

| Class | Pending policy | Can interrupt | When interrupted |
| --- | --- | --- | --- |
| Weapon selection | Every valid selection, in event order; immediate dispatch, no debounce/coalescing | Older weapon, HUD, compass | A later selection may interrupt already-dispatched speech; never silently skip an undispatched selection |
| HUD event | FIFO; preserve separate event IDs, including identical text | Compass only | Retain and replay from beginning after weapon speech |
| Compass crossing | One newest, short-lived candidate per observer | Nothing | Discard; do not replay an old crossing |

Weapon-function speech belongs to the weapon state group, but must not erase
a pending selection announcement. A function change after selection can be
appended to that pending weapon report or queued as its latest follow-up.
Single/dual-wield changes use the same selection identity and arbitration.

On each valid quick-change selection, dispatch its name immediately through
the central scheduler. Do not wait for another wheel event, an end-of-frame
coalescing pass, speech completion, or the equip animation. Use the selection
resolved by the native cycling logic, not raw wheel movement: a wheel event
that selects nothing new must not invent a weapon change. Preserve distinct
single/dual selections even when the weapon number is unchanged. Returning to
the equipped weapon after an intermediate selection is another valid event,
not an already-equipped suppression. Multiple distinct selections in one tick
must each reach the dispatcher in order.

For responsiveness, a new weapon selection may interrupt an older weapon name
that was already dispatched. This is not pending-event coalescing: each valid
selection starts speech, and the player controls the pace of exploration.
There is no promise that a full name can finish if input arrives faster than
speech can pronounce it. Do not queue completed old selection events to replay
after scrolling stops, which would announce an inventory trail out of sync
with current input. Owner testing must confirm this interrupt-and-follow
behavior; if full uninterrupted names are desired, that would require a
different, deliberately backlogged policy.

Resolve each immediate name from that selection's inventory/weapon identity,
not from the still-equipped old weapon. Ammo that depends on settled equipment
can follow after confirmation, but must not delay the name, describe the wrong
weapon, or cause a duplicate name announcement. Pending ammo reports for a
weapon no longer selected can expire: the requirement to preserve each name
does not mean reading stale ammunition. Radial focus names remain immediate;
release-to-equip ammunition retains its existing semantics.

Do not merge separate selection events simply because their normalized strings
match. Only suppress duplicate hooks for the same identified selection event.
If backend rejection requires pending retry, preserve selection event order
with explicit bounded storage; never overwrite it with a latest-only slot.

HUD entries are events, not replaceable state: two pickups with the same text
remain two announcements. A weapon can preempt an active HUD entry, but the
same retained entry returns to the head of the HUD queue. Replay the complete
sentence rather than guessing the word where it stopped. Multiple weapon
changes must not insert duplicate copies of the interrupted entry. Do not
reset its original timestamp when replaying.

A compass candidate expires 500 ms after its crossing, is replaced by a newer
crossing, and is invalidated by an observer/context change. If speech is busy
with higher-priority work, do not accumulate compass history. Revalidate the
candidate against the latest heading before dispatch. A stale crossing is
discarded, not read later as if it were current orientation. Compass clicks
remain immediate and independent, even when speech yields. This deliberately
relaxes click/speech synchronization when the speech channel is occupied.

## One owner of speech output

Implement policy in `src/accessibility/accessibility_announcement.c`, with a
separate pure queue/state module if needed to keep it testable. Existing
announcement entry points submit structured messages instead of calling speech
directly. Only the dispatcher may output or cancel automatic speech.

Each entry records an ID, category, player/context generation, source, text,
creation time, optional expiry, replacement key, and retry/preemption state.
Keep one active record and bounded pending storage. Start with 64 fixed HUD
records, a separate bounded 64-record weapon retry/event queue, and one pending
compass slot; reuse the existing maximum announcement text length. Weapon
submissions take an immediate dispatcher fast path, bypassing the periodic
tick when the backend is available. No per-frame heap allocation or text formatting
unless an event is submitted. All policy runs on the main thread, using a
monotonic clock rather than simulation ticks.

Submission means accepted into our queue, not spoken. Distinguish queued,
dispatched, backend-accepted, estimated-finished, and provider-observed-idle in
APIs and diagnostics. Never log delivery as confirmed merely because output
returned success.

Tick once at a consistent end-of-frame point for queue service, completion and
expiry, including while paused or in menus; immediate weapon submissions do
not wait for this tick. This requires a small port-loop integration hook;
update the architecture hook ledger during implementation. No new gameplay
hooks are expected. Ensure stage transitions also reach context teardown.

## Provider completion and limitations

Extend the speech backend interface with optional speaking-state support.
The bundled Prism header exposes capability flags and
`prism_backend_is_speaking`, but the current adapter does not load or use them.
Probe the selected backend's capability and validate the behavior independently
with NVDA, OneCore, and SAPI. Do not assume NVDA or JAWS provides reliable
completion or per-utterance acknowledgements.

When a provider supports useful state, use a nonblocking, rate-limited poll
(initially 50 ms) with startup grace: an immediate idle response after output
must not be mistaken for completion before speech starts. Check transient
errors, unavailable capabilities, external reader speech, and a stuck-busy
watchdog. Do not poll or wait synchronously for an utterance to finish.

Otherwise, reserve the active slot using a conservative text-length duration
estimate with startup/punctuation allowance. Make that fallback timing tunable
and log that it is estimated. Dispatch lower-priority text non-interruptingly,
even after the estimate elapses, so a timing underestimate cannot cut off the
previous higher-priority message. A newer weapon still interrupts immediately.

Without reliable completion, exact-once audible HUD delivery cannot be
guaranteed: preemption/replay may repeat text, and estimates cannot know speech
rate or whether a reader/user stopped speech. Prefer retaining/replaying an
uncertain HUD message over silently losing it; document this limitation.
Keep an uncertainty/dispatch ledger sufficient to diagnose provider-side
queuing after an underestimated duration. Do not advertise estimated completion
as proof a HUD event was heard. Backend rejection retains the message for
bounded retry, rather than losing it immediately or retrying every frame.

## Other speech and context

Menus are an exclusive interaction context, not a fourth gameplay priority.
Opening one cancels automatic gameplay speech, clears stale weapon/compass
state, and allows immediate focus/repeat narration. Retain unexpired HUD
entries during ordinary pause; resume them after returning to gameplay.
Stage changes, session end, player changes, and shutdown discard old-context
entries. No gameplay compass/weapon announcement may interrupt menu focus.
Preserve the current F5 repeat text independently of queue ownership.

Explicit F1/Shift+F1 queries, mode confirmations, and diagnostic confirmations
are deliberate user requests: give them a separate foreground transaction,
allowed to interrupt automatic speech. Retain an interrupted HUD event. On
completion, resume the normal gameplay priority order. This is an explicit
design assumption, separate from the three automatic categories.

Respawn countdown is replaceable HUD state, not a FIFO of every missed second.
Keep only the current valid count and discard it on respawn/context change.
The initial death/wait message must not be erased by a countdown update before
it has had a chance to speak. Speech tests also route through the dispatcher.

## Expiry, overload and starvation

Do not impose a short blanket timeout on ordinary HUD events: pickup bursts
must survive brief weapon changes. Initially retain ordinary events until
delivery, explicit context invalidation, or bounded-queue overflow. Add
source-specific expiry only for demonstrably transient state such as countdown.
Never deduplicate independent HUD events based only on matching strings.

At capacity, evict stale compass or obsolete ammo-follow-up state where applicable;
if the HUD queue itself is full, discard the oldest HUD entry with an explicit
overflow record and counters. A finite queue cannot promise losslessness under
an unlimited event stream. Record oldest age, queue high-water mark, and losses
so the capacity can be tested rather than hidden.
Weapon retries have their own capacity and overflow records; overflow must
never be disguised as intentional coalescing. With an available backend,
immediate name dispatch should ordinarily avoid a weapon backlog altogether.

Continuous weapon changes may legitimately starve HUD speech under the chosen
strict priority. Preserve its queue and service it as soon as selection stops;
do not violate the priority by forcing a HUD interruption. Compass starvation
is acceptable because its clicks remain available.

## Diagnostics and verification

Log message IDs and transitions: submitted, coalesced, dispatched, preempted,
requeued, invalidated, expired, overflowed, rejected, retried, provider-idle,
and estimated-finished. Include preempting ID, category, queue age/depth,
context, backend, capability, completion method, and output-call duration.
Shift+F2 should include active/pending speech records and cumulative counters.
Do not perform per-poll logging or add I/O inside an audio callback.

Pure tests with an injected clock and fake backend must cover:

1. The recorded weapon-then-compass-at-84-ms sequence: compass cannot interrupt.
2. HUD interrupted by weapon: one retained event, replay after weapon.
3. Three identical pickups: three events, preserved FIFO ordering.
4. Rapid wheel input, return-to-original selection, and single/dual changes:
   every distinct selection produces an immediate backend request in order,
   including multiple selections in one tick, with no settle/debounce delay.
   Verify names use the requested identity, stale ammo does not leak, and
   duplicate hooks for one event do not repeat its name.
5. Compass expiry, later crossings, heading revalidation and CamSpy switches.
6. Menu entry, focus/repeat, pause/resume, death/respawn and stage teardown.
7. Unsupported/error/stuck speaking-state queries and startup-idle responses.
8. Slow/fast duration estimates, provider rejection, retries, and queue overflow.
9. Existing non-dialogue/subtitle exclusions and accessibility disable/shutdown.
10. Deliberate user requests interrupting automatic output without losing HUD.

Run the full regression suite and MinGW64 build. During owner acceptance, test
single wheel steps and fast scrolling during pickup bursts while rotating
across compass headings in Combat Simulator; confirm each selection begins
speech promptly and that intermediate weapons can be identified while taking
inventory at the owner's chosen pace. Confirm HUD events resume after cycling.
Repeat with NVDA and without it (OneCore fallback). Compare
frame timing and speech-call latency with the previous build. No acceptance
claim follows from queue tests or provider success alone.

## Delivery sequence

1. Pure scheduling module, fake backend tests, and completion-capability adapter.
2. Route existing producers and context/cancel paths through the scheduler.
3. Add diagnostics, documentation, and performance checks; build for owner tests.

Keep the change in one reviewable feature series, separable from audio DSP.
No audio oscillators, gameplay rules, screen-reader selection order, subtitle
policy, or sound-scanner scheduling should change. The initial implementation
must document any new fallback-timing settings and defaults. Implementation
was subsequently requested; do not commit or push without a separate request.

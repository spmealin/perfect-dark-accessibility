# Perfect Dark accessibility guide

This guide is for blind and screen-reader-dependent players using the
accessibility build of the Perfect Dark PC port. It explains what the current
features do, how to operate them, and what to report when something is not
accessible.

The project is still under active development. It has been exercised across
the Carrington Institute, campaign missions, and one-local-player Combat
Simulator games, but it does not yet guarantee that every objective or level
can be completed without sight. The game has not been simplified: the goal is
to communicate the same useful state and spatial information that a sighted
player receives.

## Quick start

1. Extract the complete accessibility-enabled Windows package. Do not move
   `pd.x86_64.exe` away from the DLL files beside it.
2. Create a `data` directory beside the executable if the package does not
   already contain one.
3. Put your legally obtained US revision 1 ROM in that directory as
   `pd.ntsc-final.z64`.
4. Start your screen reader, if you use one, then run `pd.x86_64.exe`. When no
   supported screen reader is active, the game uses a built-in Windows voice.
5. Use headphones. Stereo position is essential to the scanners, virtual cane,
   markers, radar, and combat cues.

An accessibility-enabled package starts with speech and the implemented audio
assistance enabled. The virtual cane begins in Slow mode, the compass begins in
Speech and sound mode, and the four world scanners begin enabled. Those mode
and scanner choices are saved in `pd.ini`, so later sessions restore your
choices.

When the main menu opens, the screen reader should announce the menu title and
focused option. Moving to another option reads only that option. Returning to a
menu after visiting another screen reads its title again.

## Mouse weapon selection

Hold the weapon radial-menu binding (Q or the middle mouse button by default).
The mouse position when the menu opens becomes its neutral center. Move in
one of the eight directions to highlight a weapon; no click is needed. Return
to the small center dead zone to clear the selection, or release the menu
binding while a weapon is highlighted to equip it. Returning to center and
releasing leaves your weapon unchanged. A long outward movement does not move
the center: reverse that movement to return to it.

This also applies to the function and bot-order radial pages. Existing
screen-reader focus announcements still follow the highlighted item. Mouse
capture must be active, as during ordinary mouse gameplay; unlocked mouse
input and menus outside these gameplay radials are unchanged. Adjust Mouse
Speed and Radial Menu Speed under Extended Options → Mouse if needed.

## Keyboard shortcuts

Accessibility commands use keys that are unbound in the default PC gameplay
scheme. Plain and Shift-modified keys can have different functions. Alt-modified
commands are ignored so operating-system shortcuts such as Alt+F4 continue to
work.

| Shortcut | Context | Action |
| --- | --- | --- |
| F1 | Gameplay | Speak a compact player report: health, nonzero shields, and relevant Combat Simulator status. |
| Shift+F1 | Team Combat Simulator gameplay | Speak team name, score, rank, kills, deaths, leader gap, limits, and match time. It does nothing outside a team scenario. |
| Shift+F2 | Gameplay | Save a numbered diagnostic capture, including roughly the preceding 15 seconds, to `accessibility.log`. |
| F3 | One-player Combat Simulator gameplay | Ping the contacts shown by the native radar once each, nearest to farthest, with soft directional sounds. |
| Shift+F3 | One-player Combat Simulator gameplay | Toggle automatic radar alerts: three pings for far, two for medium, and repeating single pings for close enemies not already sounding a combat beacon. |
| F4 | Gameplay | Select the next virtual-cane mode: Slow sweep, Fast sweep, Off, then Slow again. |
| Shift+F4 | Gameplay | Select the next compass mode: Speech and sound, Sound only, Off, then Speech and sound again. |
| F5 | Gameplay | Toggle interactable-object beacons. The choice persists between levels and game sessions. |
| F5 | Menus and dialogs | Repeat the current screen's title, important static text, and focused control. |
| F6 | Gameplay | Toggle door beacons. The choice persists between levels and game sessions. |
| F7 | Gameplay | Toggle friendly and neutral character beacons. The choice persists between levels and game sessions. |
| F8 | Gameplay | Toggle pickup-item beacons. The choice persists between levels and game sessions. |
| F9, F10, F11, or F12 | Gameplay | Place a temporary audible marker in slot 1, 2, 3, or 4. Pressing the same key again moves that marker to your current position. |
| Shift+F9, Shift+F10, Shift+F11, or Shift+F12 | Gameplay | Remove the corresponding audible marker. |
| End | Gameplay | Reset vertical view to straight ahead without changing compass direction. |

An Xbox-style controller's right stick click also resets the vertical view. The
other accessibility shortcuts currently require a keyboard, even when the game
itself is being played with a controller.

### Shortcut confirmation sounds

Scanner activation uses a normal beep followed by a higher beep. Deactivation
uses a normal beep followed by a lower beep. The virtual cane uses the same
language: two rising beeps mean Slow, a normal beep plus two higher beeps mean
Fast, and two falling beeps mean Off.

The compass speaks its new mode when speech is available. Its directional
clicks are one for North, two for East, three for South, and four for West.

## Screen-reader output

The Windows build sends text through Prism. It prefers a running supported
screen reader, then falls back to Windows OneCore speech and finally SAPI.
NVDA is the primary tested screen reader. Other Prism-supported screen readers
and both built-in fallbacks should be treated as provisional until tested.

Speech covers much more than the focused menu control. Current support includes:

- menu titles, options, buttons, meaningful slider values, disabled state, and
  many scrolling or non-focusable information panels;
- mission names including subtitles, briefings, objectives and completion
  state, pause information, results, statistics, rankings, and confirmation
  dialogs;
- Combat Simulator challenge descriptions, setup controls, scenario results,
  player-save dialogs, and post-death countdown messages;
- HUD messages such as pickups and mission notifications, while deliberately
  excluding dialogue and subtitles from the generic HUD-message path;
- weapon radial menus, command radial menus, weapon names, dual-wield state,
  ammunition after selection, and visible firing-function labels;
- important device and training information in the Carrington Institute.

Press F5 when a menu's static information was spoken too quickly or focus alone
does not repeat it.

In Combat Simulator → Player Setup → Statistics, use Up and Down (or the
D-pad) to read one stat at a time. F5 repeats the focused stat; Back and the
usual cancel control still leave the screen.

Automatic gameplay speech prioritizes **weapon selection, then HUD messages,
then compass directions**. Every valid quick weapon selection starts its name
immediately, including intermediate mouse-wheel selections; a newer selection
can interrupt the previous name. Ammunition follows when the equipment settles.
There is no debounce or final-weapon-only filter. Interrupted HUD messages are
normally retained and replayed from the beginning, so you may hear part of one
twice. Weapon pickups are more important than ammo pickups: a weapon pickup
interrupts ammo chatter and clears older pending ammo reports instead of
replaying them. Ammo picked up afterward follows the weapon normally.
Compass words never interrupt higher-priority speech; stale directions are
discarded, while compass clicks still happen immediately. Menus and deliberate
requests such as F1 retain foreground access.

The game uses provider speaking-state information where available, otherwise
estimated timing. In `[Accessibility]`, `SpeechMillisecondsPerCharacter=60`
(10–250) and `SpeechStartupGraceMs=200` (100–2000) tune the estimate and initial
completion-query grace. Restart after editing. A provider accepting text is
not confirmation that it was heard; very fast scrolling can cut names short,
and replay after interruption can repeat text. These settings do not delay
new weapon names. See the
[speech scheduler design](documentation/ACCESSIBILITY_SPEECH_SCHEDULER_DESIGN.md).

Pickup announcements use a one-second speech budget rather than the longer
general text estimate, with no extra pause between queued messages. In
`[Accessibility]`, `SpeechPickupDurationMs=1000` (250–3000) adjusts that budget
for your speech rate. It includes startup time; grace is not added again.
This does not change your screen reader's speaking speed. Restart after editing.

Checkboxes and checkable list rows announce **selected ITEM** when checked,
and just **ITEM** when unchecked. Changing a checkmark is announced even if
focus stays on the same row. Menu titles still read on entry and with F5;
unavailable controls retain their availability warning.

This also applies to device checkmarks in the pause inventory, soundtrack and
random-weapon filters, and Combat Simulator setup import/export selections.
Checkable rows in an open dropdown use the same wording. Inventory descriptions
remain available, without an extra “checked” or “not checked” suffix.

## Audio glossary

Open **Extended Options → Accessibility → Audio glossary** from the game's
options menus. This uses the ordinary menu font, scrolling list, navigation,
activation button, and Back control; no new keyboard shortcut is required.

Moving through the list speaks the cue name without playing it. Activate an
entry to hear one example, and activate it again to replay. Continuous sounds
have short demonstrations rather than looping indefinitely. The usual selection
beep is suppressed for previews so it cannot mask the example; navigation,
Back, and other menus retain their normal sounds. Moving to another
entry, selecting Back, or closing the glossary stops playback.

The list covers all current accessibility sound families and their meaningful
variants: object, door, pickup and character scanners; enemy presence, elevation,
precision guidance and lock-on; head/arm and through-glass locks; range and
special-device targeting; cane barriers, contours, stairs, ladders, crouch
passages, drops and vehicle collisions; lasers and rockets; tracker/IR and
Combat Simulator radar; hill status; numbered player markers and removal;
mission landmarks, vulnerable spikes and intact pillars; scanner/cane mode
confirmations, compass directions, stance and weapon-function changes.
X-Ray uses the normal scanner sounds rather than a separate cue.

Examples use the actual procedural sound generators with simulated positions
and terrain, not recorded game assets or live scanning. They work while menus
suspend gameplay cues and do not enable features or change saved settings.
Your configured cue volumes and pitches apply. A zero-volume cue remains
silent, and the global `Accessibility.Enabled` switch must be enabled.
The initial glossary labels are English-only.

The friendly-combat-character example plays two full on/off cycles of its
upper note (four seconds). The localized Back entry is the last row in the
sound list, reachable with the same navigation controls; the ordinary cancel
button also returns to Accessibility.

## World scanners

Pickup beacons (F8) require an item to intersect the current viewport as well
as pass the existing line-of-sight and pickup checks. Items merely nearby or
behind you do not chirp. Eligibility is checked again before each pickup cue.

Mine pickups add a low 220 Hz drone beneath the usual three pickup chirps,
including the gaps, to distinguish them from other items. This covers timed,
proximity, remote, and ECM mines that already qualify for the pickup scanner;
it does not add detection of otherwise ineligible deployed mines. Try **Mine
pickup** in the audio glossary to hear the difference.

### Accessibility mix protection

Overload protection is enabled by default. It keeps the stereo channels linked
when many cues overlap, rather than independently clipping them, and modestly
lowers original game audio while cues are active. Cue patterns and scheduling
are unchanged; this is not an automatic feature-muting system.

In `[Accessibility]` in `pd.ini`, `MixProtection=0` restores the previous mix.
`WorldCueVolume=1.0` and `FeedbackCueVolume=1.0` independently scale directional
cues and centered feedback. `GameAudioHeadroomDb=3.0` sets the original game's
temporary attenuation; zero disables that attenuation while retaining protection.
Restart after editing. See the [mix reference](documentation/ACCESSIBILITY_AUDIO_MIX.md)
for behavior, limits and listening tests.

### Experimental 3D headphone audio

The current experimental build uses binaural headphone audio for ordinary
enemy beacons, all four object/person scanners, and the virtual cane (including
contours, boundary sequences and the moving drop-edge warning), R-Tracker
contacts, Combat Simulator radar contacts and King of the Hill guidance.
Radar launch/empty/unavailable confirmations remain centered.
Player markers (including ID chirps) and authored mission landmarks also use
3D positioning; marker deletion confirmations stay centered.
Rocket warnings, moving laser cues and threat-detector alerts also use 3D
positions without changing their visibility rules or warning patterns.
IR-scanner highlights use the same 3D positioning as R-Tracker contacts,
retaining their existing faster cue and on-screen filtering.
It adds front/back and elevation filtering
while retaining their existing pitch patterns, cadence, range and visibility.
Other cues and original game audio are unchanged. Held-aim directional
guidance and the exact lock-on tone deliberately keep their existing sound.
Use ordinary stereo headphones; avoid stacking another virtual-surround effect.

Use Options → Extended → Audio → **3D Accessibility Audio** to switch between
3D and the previous stereo behavior immediately. The checkbox defaults on,
and its choice is saved as `Accessibility.BinauralAudio` in `pd.ini`.
It is unavailable in builds without the experimental backend or when the SDK
cannot initialize. No restart is needed to change it from the menu.
If the adjacent `phonon.dll` cannot initialize, cues fall back to stereo.
This is a listening experiment, not part of the public release.
The audio glossary still plays its original stereo examples.

### Scanner behavior

Four player-controlled scanners provide spatial sounds during gameplay:

| Key | Category | Sound concept |
| --- | --- | --- |
| F5 | Interactable objects, computers, switches, usable panels, and specifically authored movable puzzle objects | A high chirp; the Skedar Ruins movable puzzle rock uses a short crossing sweep followed by a low pulse |
| F6 | Doors | A lower, prominent repeating chirp |
| F7 | Friendly and neutral characters | A continuous two-note drone; friendly combat characters pulse the upper note |
| F8 | Weapons, devices, ammunition, and other collectible items | Three rapid high chirps |

Multiple candidates are scheduled so their sounds do not all begin together.
Pan and volume locate each candidate. These scanners generally require the
object to be on screen and to have a valid visual path from the active camera;
transparent breakable glass is treated like visible glass rather than an opaque
wall. X-Ray vision can expose otherwise hidden semantic objects to their normal
scanner category. Scanner sounds pause in menus, pause screens, cutscenes, and
other non-gameplay states, then resume without changing your saved choices.

An interactable-object cue means that something usable is visible, but the game
may still require it to be centered vertically before the normal interact
button works. If the cue is sounding and the interact button does nothing, move
your view up or down while facing the object and try again. This is especially
common in the Carrington Institute training areas, where laptops sit below the
usual straight-ahead view on desks and tables.

When controlling the CamSpy, supported scanners and navigation systems switch
to its camera and return to Joanna when remote control ends. Interactable and
pickup scanning pause in CamSpy view because those objects cannot be used or
collected from the remote camera.

### Pelagic II X-Ray reactor switches

While the X-Ray Scanner reveals the reactor puzzle switches, F5 distinguishes
their visible states: a green switch uses the normal 880 Hz interaction chirp;
a red reset switch uses a lower 660 Hz chirp; a black, already-activated switch
is silent. Resetting the sequence restores the normal chirps as switches return
to green, on the scanner's normal refresh. This follows the live randomized
assignments, not fixed locations. The ordinary scanner visibility, range and
toggle rules still apply; the color distinction is not exposed without X-Ray.
The audio glossary includes **X-Ray puzzle: reset switch** for the lowered cue.

## Enemy awareness and targeting

Visible hostiles produce positioned pulses. Their timing and duration change
with distance: distant enemies use longer, slower pulses, while close enemies
become faster and eventually continuous near punching distance. Dedicated
voices allow several enemies to remain audible at once. Dead, unconscious,
hidden, and otherwise invalid characters are removed from the hostile set.
Automated turrets, security cameras, firing-range targets, and relevant scanner
threats use the same general combat interface when the game's rules consider
them valid.

The centered aiming tone is separate from the positioned enemy sound:

- A solid tone means the current aim is accepted as a hostile or other safe
  combat target.
- A lower-pitched interrupted tone—90 milliseconds on, 10 milliseconds
  silent—means a protected character, breakable pane, crate, computer, or
  another destructible target. It warns that the aim is valid without implying
  that the target is an enemy.
- A modulation indicates that the shot line reaches a valid target through
  breakable glass rather than through open space.
- Pitch and the second half of the tone communicate fine vertical alignment;
  head, body, and limb regions can also alter the result where semantic data is
  available.
- Holding precise aim exaggerates left, right, up, and down guidance. This is
  especially useful with scoped weapons, fixed targets such as turrets and
  cameras, and the FarSight.

The system reports aim and target state; it does not change weapon spread,
damage, projectile behavior, or mission rules.

In the firing range, the accuracy tone rises in pitch as your aim approaches
the center, at 80% of the normal targeting volume. Inside the center bullseye,
the standard solid enemy lock-on tone plays alongside that accuracy tone at
normal targeting volume. Back-facing or otherwise invalid
targets do not produce a lock. This describes aim, not a guarantee that weapon
spread will put the shot in the bullseye.

Mines, grenades, rockets, and a deployed Dragon in proximity-mine mode use the
lower interrupted destructible lock when your exact aim hits them and the
equipped attack can damage them. This does not depend on who placed the item.
Ordinary dropped guns do not qualify merely because they are weapon pickups.
Threat Detector contact alerts remain available, but directly aiming at a
shoot-detonatable weapon uses this destructible lock, not the solid enemy lock.
Its volume uses the existing independent `InterruptedTargetingVolume` setting.

The current precision-aim experiment keeps left/right separation and the
up/down pitch difference stronger near the target. Small or distant targets
now get a narrower centered band and stronger corrections based on their
on-screen width and height, including the current scope zoom. Large targets
retain the previous mapping. The reference-first, altered-second beep
remains: higher means move your aim up, lower means move it down. These
pulsed directions are acquisition guidance, not confirmation of a hit.
While precise aim is held, each 100 ms beep retains its 60 ms gap. Ordinary
on-screen enemy beeps now have shorter silent gaps: 160 ms at long distance
and 75 ms near melee range, with the existing distance ramp between them.
The separate confirmed lock-on tone, including body-region and through-glass
feedback, is unchanged. Target-size-relative scaling affects only the
directional guidance, never the exact lock-on test.

Special devices reuse the alignment tone only for objects the device can
actually affect. This includes supported Data Uplink, Door Decoder, ECM Mine,
Reprogrammer, CamSpy photograph, security-hub, taxi, terminal, panel, and
mission-specific targets. The IR Scanner, X-Ray Scanner, Threat Detector,
R-Tracker, and FarSight expose their native visual contacts through appropriate
existing cue categories.

## Virtual cane

The virtual cane repeatedly sweeps a fan of rays from left to right in front of
the active camera. Each audible point is spatialized at the place where its ray
meets the environment. Nearby barriers are higher and louder; distant barriers
are lower and quieter. F4 selects a two-second Slow sweep, a one-second Fast
sweep, or Off, including a short pause at the end of each sweep.

The cane also analyzes traversable space rather than treating every height
change as a wall. Its cue vocabulary includes:

- solid barriers and the end of open space;
- floor contours, meaningful stairs and ramps, and the boundaries of a narrow
  ascending or descending route;
- sudden drops, placed at the floor-to-drop transition; when adjacent rays
  confirm a substantial exposed edge, a separate warning sound travels back
  and forth along it without stopping the regular cane sweep;
- crouch-height passages that continue into usable space;
- climbable ladders with a larger upward pitch sweep;
- side openings, including wide peripheral rays intended to catch passages as
  you pass them.

The cane follows Joanna, a controlled CamSpy, a hoverbike, and supported
grabbed-object movement. It ignores the object currently being carried so the
route beyond it can still be heard. Natural terrain and complicated legacy
collision geometry remain the most experimental part of the project, so a cue
may occasionally be ambiguous or absent.

## Orientation and navigation

### Compass

Shift+F4 controls the compass. Crossing a cardinal direction produces the
direction word in Speech and sound mode and the matching one-to-four-click
pattern in either audible mode. The compass follows the active camera, including
the CamSpy. A small re-arm angle prevents repeated chatter when the view jitters
around one heading.

### Player markers

F9 through F12 place four temporary landmarks at your current camera position.
Each marker uses two crossing pitch sweeps plus one, two, three, or four 800 Hz
identity chirps. Markers can be heard outside the viewport, including behind
you, but only within range and when no wall or closed door blocks the direct
line. They clear when the level ends and are not stored in a player profile.

### Mission landmarks

Some objectives that are visually obvious but are neither ordinary pickups nor
normal interactables have authored landmarks. They use the same crossing base
sound as a player marker but no numbered identity chirps. Examples include a
placement point, objective conveyor, mission consoles, and selected boss targets.
The cue disappears when the associated object is completed, destroyed, or no
longer relevant. In Skedar Ruins, the bridge-puzzle pressure pad uses this cue
until the movable rock is positioned on it; moving the rock away restores the
cue. Coverage is intentionally semantic and is not yet exhaustive.
All five intact pillars in Battle Shrine also have a distinct low two-note
drone, independent of whether the R-Tracker selects them. This cue remains
after placing an amplifier and stops when the pillar is destroyed. It requires
line of sight and uses the authored-landmark range and volume settings; it does
not require the R-Tracker or F5 to be enabled. The R-Tracker still identifies
only the selected, unmarked objective pillars.
During the Skedar King fight, vulnerable spikes use a louder, higher crossing
sweep. The King's ordinary on-screen enemy beep pauses during each recharge
window, when shooting the King is ineffective, and resumes when combat does.

### Resetting your view

Press End, or right stick click on an Xbox-style controller, to put the camera
at horizontal. This is useful after looking down to interact or after losing
vertical orientation in combat.

### Learning levels and objectives

The accessibility build currently provides no automatic pathfinding or
automovement. The player must learn each level's layout, decide where to travel,
and work out how to complete its objectives. The cane, compass, scanners, and
markers provide information for doing that; they do not calculate or follow a
route.

If you become stuck, a mainstream written guide such as the
[IGN Perfect Dark walkthrough](https://www.ign.com/wikis/perfect-dark/Walkthrough)
can provide objective instructions and level context that you can follow using
the accessibility tools.

## Other automatic cues

- One stance beep means standing, two mean crouching, and three mean
  double-crouching. The cue follows the character's actual stance, including
  weapon-driven changes.
- One short beep means a weapon's primary function became active; two mean its
  secondary function became active. If the game displays the function name,
  that text is also spoken.
- Damaging horizontal laser barriers produce a short-range spatial sweep when
  the player faces them.
- Hostile rockets produce a rough spatial warning buzz while they are on
  screen, in range, and unobstructed. The buzz becomes higher and repeats more
  quickly as a rocket gets closer or approaches the player.
- The R-Tracker announces its state and renders tracked contacts as spatial
  audio. IR and X-Ray modes expose the objects highlighted by their native
  visual systems.
- King of the Hill uses a distinct beacon at the hill center and changes its
  behavior when the player is occupying and scoring on the hill.
- Mission-authored navigation targets and Combat Simulator radar contacts are
  scheduled to avoid simultaneous cues when possible.

## Configuration

Accessibility settings are stored in the `pd.ini` beside the executable. Close
the game before editing it and restart afterward. The most important switches
are:

```ini
[Accessibility]
Enabled=1
SpeechEnabled=1
SpeechBackend=auto
SpeechFallback=onecore
LoggingEnabled=1
MenuNarration=1
HudMessages=1
ProjectileHazards=1
ProjectileHazardRange=3000
ProjectileHazardVolume=0.25
InteractableBeacons=1
TargetingFeedback=1
VirtualCaneMode=1
CompassMode=1
```

Use `1` to enable a switch and `0` to disable it. `VirtualCaneMode` uses
`0=off`, `1=slow`, and `2=fast`. `CompassMode` uses `0=off`, `1=speech and
sound`, and `2=sound only`.

`SpeechBackend=auto` tries active screen-reader integrations first, then the
configured built-in fallback. Set it to `screenreader` to prohibit built-in
speech, or force `nvda`, `jaws`, `uia`, `onecore`, `sapi`, or `none` when
diagnosing output. Other supported explicit values are `pc_talker`, `zdsr`,
`boy_pc_reader`, `zoomtext`, `sense_reader`, `system_access`, and
`window_eyes`. `SpeechFallback` accepts `onecore`, `sapi`, or `none`; the
default `onecore` setting tries SAPI if OneCore cannot initialize. Automatic
UIA selection depends on Windows reporting an active screen reader. Narrator
users can set `SpeechBackend=uia` if Windows fails to report it correctly.

The file also exposes ranges, pitches, and independent volume controls for the
virtual cane, hostile presence cues, solid targeting tone, interrupted
non-hostile or breakable targeting tone, markers, and Combat Simulator radar.
Defaults and safe ranges are recorded in the
[detailed feature reference](documentation/ACCESSIBILITY_FEATURE_REFERENCE.md).
If configuration becomes confusing, move your personal `pd.ini` somewhere
safe and let the game create a fresh one on its next run.

## Troubleshooting

### The game starts but nothing is spoken

1. If you expect screen-reader output, confirm that it was running before the
   game started. Otherwise confirm that Windows has a OneCore or SAPI voice.
2. Confirm that `prism.dll` remains beside `pd.x86_64.exe`.
3. Open `pd.ini` and verify `Accessibility.Enabled=1`,
   `Accessibility.SpeechEnabled=1`, and `Accessibility.MenuNarration=1`.
4. Restart the game after changing the file.
5. Check `accessibility.log` for the session-start record and detected screen
   reader.

The game should continue to run if no speech backend is available; only speech
output is lost.

### A scanner is silent

- Press the scanner's key once and listen for the rising enabled earcon.
- Return to ordinary gameplay if a menu, pause screen, cutscene, death state,
  or unsupported multiplayer view is active.
- Turn toward the object. Most scanners require it to be in the active viewport
  and visibly reachable from the camera.
- Remember that an object can be visually present without being semantically
  usable at the current mission state.

### An important object, target, or route is wrong

Press Shift+F2 while the problem is happening. The game speaks a capture number
and writes a detailed snapshot plus roughly 15 seconds of prior history to
`accessibility.log` beside the executable. Record:

- the capture number;
- level, difficulty, and current objective;
- what you were facing, holding, or controlling;
- what sound or speech you heard;
- what you expected instead.

Capture numbers restart each time the game starts, so include the log's session
identifier when possible. Logs are never uploaded automatically, but they are
intentionally comprehensive and may contain resolved text, file paths, input,
positions, identifiers, and detailed game state. Review a log before sharing it
if you do not want to disclose that information.

### The executable reports missing DLLs

Restore the complete contents of the Windows package. Do not download random
copies of `libwinpthread-1.dll`, `libgcc_s_seh-1.dll`, `SDL2.dll`, `zlib1.dll`,
or `prism.dll` from unrelated sites. A source
build made through this repository's MinGW64 process copies the matching runtime
files beside the executable automatically.

## Current limitations

- Windows is the supported accessibility speech platform. Non-speech core code
  is portable, but other speech backends have not been implemented.
- Full-campaign, every-difficulty, and multiplayer accessibility validation is
  incomplete. Combat Simulator audio features currently assume one local
  player.
- There is no automatic pathfinding or automovement. Players must learn level
  layouts and determine how to complete objectives.
- Some mission landmarks and special-device targets require semantic entries;
  undiscovered edge cases may still need to be added.
- Visual accessibility options such as scalable text, contrast themes, and
  reduced motion remain future work.
- English accessibility-only phrases are not yet backed by a localization
  catalog, although existing game text uses the game's localized strings when
  available.

For engineering details and exact current behavior, see the
[feature reference](documentation/ACCESSIBILITY_FEATURE_REFERENCE.md),
and [architecture](ACCESSIBILITY_ARCHITECTURE.md).

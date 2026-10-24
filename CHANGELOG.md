# Player changelog

## Unreleased

- Precision aiming keeps small remaining left/right and up/down corrections
  more noticeable, with a narrower vertical level band and quicker response
  when the required vertical correction changes direction. Confirmed lock-on
  tones, body-part feedback, target selection and hit detection are unchanged.

## v0.92 — October 15, 2026

### In-game audio glossary

- Added **Extended Options → Accessibility → Audio glossary**, available through the normal options menus. Use the usual keyboard or controller navigation and activation controls; no new shortcut is required.
- Browse 92 named examples covering all current accessibility sound families and important variants: scanners, enemy presence and targeting, body-region and through-glass locks, cane navigation, hazards, devices, radar, player and mission landmarks, compass, stance, and confirmation sounds.
- Moving through the list reads the cue name without playing it. Activate an entry to hear one example, or activate it again to replay. Continuous sounds use bounded demonstrations, and changing focus or leaving the menu stops playback.
- Sound previews use the actual gameplay sound generators and respect existing volume and pitch settings without changing scanner selections or saved preferences. The normal selection beep is suppressed for previews so it does not mask the example; other menus retain their normal sounds.
- The friendly-combat-character example plays two full upper-note on/off cycles, making it easier to distinguish from the steady neutral-character drone.
- A localized **Back** entry at the bottom of the sound list returns to Accessibility. The ordinary cancel/back command also works.

### Testing and upgrade notes

- The project owner confirmed the glossary workflow, selection-beep fix, longer friendly-character demonstration, and Back navigation. Automated playback and cleanup checks cover all 92 entries; this does not establish that every cue is recognizable to every player.
- Glossary names are currently English-only. Existing keyboard shortcuts and gameplay cue behavior are unchanged.
- Download and extract the complete Windows package, keeping its executable and DLLs together. Supply your own supported ROM; no ROM, extracted game assets, saves, personal settings, logs, or diagnostic captures are included.

## v0.91 — October 11, 2026

Player-facing changes since the initial v0.90 release (published under the
`v0.9` tag).

### Speech

- Windows speech now uses Prism instead of Tolk. Automatic selection prefers an active supported screen reader, then tries a built-in Windows OneCore voice, and finally SAPI if OneCore is unavailable. A running screen reader is no longer required to hear the game's announcements.
- In the `[Accessibility]` section of `pd.ini`, `SpeechBackend=auto` enables automatic selection. Use `screenreader` to prohibit built-in speech, or select `nvda`, `jaws`, `uia`, `onecore`, `sapi`, or `none` explicitly. `SpeechFallback` accepts `onecore`, `sapi`, or `none`; its default is `onecore`, which also tries SAPI if necessary.
- Speech detection occurs at startup. Start your screen reader before launching the game and restart the game after starting or stopping it or editing speech settings. NVDA output and OneCore fallback have been tested by the project owner; other screen readers and SAPI fallback still need user testing.

### Virtual cane and drop warnings

- Substantial exposed drop edges confirmed by adjacent cane rays now receive a separate warning that moves back and forth along the edge. It sounds alongside the regular cane rather than replacing the sweep, helping distinguish a dangerous edge from a traversable elevation change. Small steps do not qualify for the edge warning.
- Adjacent rays describing the same broad ascent or descent now share one representative contour rather than repeating similar contours from left to right. Structured stair and ramp boundary guidance remains available.
- Connected floor traces that reach their sample capacity no longer unnecessarily fall back to the older elevation sounds.

### R-Tracker and IR Scanner

- R-Tracker contacts have higher-pitched sounds that stand out from the cane. They become louder and repeat faster as you approach, with a stronger proximity response near the destination.
- Multiple contacts take turns on a round-robin schedule rather than overlapping. Each contact keeps its own distance-dependent cadence; several contacts can delay a beep while they take turns.
- IR-highlighted objects, including doors, now sound only when some part of their projected bounds is on screen. Turning away stops the cue. IR-required objects in Battle Shrine use the same distinctive fast object cue as the R-Tracker.

### Aircraft and incoming rockets

- Active, armed hostile aircraft attacking the player now receive enemy-presence sounds, manual-aim guidance, and the solid hostile lock-on tone. This includes airborne hostile interceptors; grounded, inactive, or destroyed aircraft do not receive enemy feedback simply for being aircraft.
- Hostile rockets now produce a spatial warning buzz while on screen, within range, and unobstructed. Pitch and repetition speed increase with proximity and predicted approach. Up to four rockets use dedicated audio voices without replacing cane or targeting sounds.
- Player-fired and friendly rockets are excluded. Rocket warnings are suppressed during CamSpy control, menus, pause, cutscenes, and death.
- The `[Accessibility]` settings `ProjectileHazards=1`, `ProjectileHazardRange=3000`, and `ProjectileHazardVolume=0.25` control rocket warnings independently. These are the defaults; edit `pd.ini` while the game is closed.

### Skedar Ruins: Battle Shrine

The following details contain puzzle and final-boss spoilers.

- All five intact pillars have a distinctive low two-note drone, whether or not they are selected by the R-Tracker. The sound remains after placing an amplifier and stops when the pillar is destroyed. It uses the authored-landmark line-of-sight, range, and volume settings and does not require the R-Tracker or F5.
- Target Amplifier feedback follows the pillars selected for the current playthrough. A selected pillar stops sounding on the accessible R-Tracker after its amplifier is placed, independently of its intact-pillar drone. Use the R-Tracker to distinguish the selected, unmarked pillars and the equipped Target Amplifier's lock-on tone to confirm alignment.
- The destructible shrine entrance rock now receives the interrupted destroyable-object lock-on tone when aimed at with an appropriate explosive weapon. Equivalent scripted destructible scenery can use the same detection.
- The movable bridge-puzzle rock uses a distinctive short crossing sweep followed by a low pulse on the F5 scanner. The pressure pad uses the separate mission-marker sound. The pad stops sounding when the rock is correctly positioned to hold the bridge open and sounds again if the rock moves away.
- Vulnerable final-boss spikes use louder, higher-pitched crossing sweeps with gentler distance attenuation and greater range, making them audible from the chamber entrance. Their cues stop individually when destroyed or no longer vulnerable. Exact aiming uses the interrupted destroyable-object lock-on tone.
- During the King's recharge phase, his ordinary enemy-presence beep pauses so it does not mask the vulnerable spikes. His exact-aim feedback remains available; ordinary enemy-presence feedback resumes when the recharge window ends.

### Upgrade and testing notes

- Keep the package's runtime DLLs together with the executable, including `prism.dll`. The new speech backend does not use the old Tolk runtime.
- Accessibility keyboard shortcuts are unchanged. See [ACCESSIBILITY.md](ACCESSIBILITY.md) for the player guide, configuration, and troubleshooting.
- There is still no automatic pathfinding or movement. Cues do not guarantee a safe route, successful shot, or rocket avoidance. Coverage and usability vary by mission and Combat Simulator scenario.
- Experimental gyro aiming on a separate branch is not included. Not every new feature has passed independent accessibility testing. The new all-pillar drone has build and regression-test coverage, but in-game blind-user acceptance testing remains pending.

## v0.90 — September 15, 2026

Initial accessibility version.

# Accessibility feature reference

This is the detailed engineering and current-behavior reference. For setup,
shortcuts, cue summaries, and troubleshooting written for players, see the root
`ACCESSIBILITY.md`. Paths in this document are relative to the repository root.

## Purpose

This work aims to make the Perfect Dark PC port meaningfully playable by blind and low-vision players, starting with nonvisual access to menus and essential game state and progressing through small, testable gameplay slices.

The current branch contains the accessibility coordinator/logger, Prism screen-reader/OneCore/SAPI speech backend, menu-agnostic focus narration, non-subtitle HUD-message speech, single-player interaction/door/pickup beacons, non-hostile-character beacons, damaging-laser hazard cues, firing-range, hostile-character/autogun/security-camera, FarSight-visible hostile targeting, and initial device-target feedback, weapon-function state cues, a nine-direction virtual-cane prototype, four player-authored audible landmarks, a nonvisual R-Tracker interface, a Combat Simulator audio-radar prototype, IR Scanner highlight audio, and X-Ray-aware semantic scanner visibility. All implemented feature backends default to enabled for blind-user acceptance testing; the cane defaults to Slow mode. Menu narration has passed project-owner blind-user acceptance, while newer gameplay slices retain the narrower evidence and pending tests documented below and in their historical milestone and feature records. Automatic route and goal guidance, other non-character combat categories, and full-game accessibility are not implemented.

The firing-range weapon list announces the same completed bronze, silver, and gold proficiency stars rendered beside each weapon. It reads only the filled stars represented by the saved score and does not infer incomplete progress or expose state absent from the visual row.

The holo-training computer announces the selected exercise's localized static description when its details dialog opens and on F5 repeat, before announcing the focused `OK` or `Resume` control. The speech comes from the same `htGetDescription()` value rendered in the dialog's non-focusable text panel.

The pre-mission Overview announces every objective visibly applicable to the selected difficulty, in displayed order and with the same numbering, before the focused `Accept` or `Decline` button. The in-mission pause Status screen announces the same visible objective list with each objective's localized `Complete`, `Incomplete`, or `Failed` state before the focused control. Moving among controls does not repeat the objectives; F5 reconstructs the stage title, complete objective list, and current control.

The pause Inventory list announces the focused item's visible name, manufacturer when present, primary and secondary functions, complete marquee description, and the checked state shown for active or inactive devices. Mission- and device-dependent description variants use the same resolver as the visual panel. Pause Briefing narration retains the complete localized source, including all background, Carrington, and objective sections, rather than truncating it to a short control-value field. The pause Abort confirmation also announces its visible question before the focused Cancel or Abort button.

Mission-completion and mission-failure screens announce the full visible results panel on entry: mission and agent status, elapsed and target time where shown, difficulty, newly unlocked cheat where shown, weapon of choice, kills, accuracy, and the visible shot breakdown. Their sibling Objectives screens announce every displayed objective and localized completion state. Retry and Next Mission objective pages announce the objectives before the focused action.

Combat Simulator challenge confirmation and details dialogs announce the complete localized challenge description rendered in their non-focusable scrolling panel before the focused `Accept`, `Cancel`, `Start`, or `Abort` control. This applies to the normal confirmation flow, the challenge list/details flow, the current-challenge details screen, and the 4 MB confirmation variant. Moving focus does not repeat the description; F5 reconstructs the title, description, and current control.

Carrington Institute Hangar Information pages announce the localized subtitle rendered beneath each location or vehicle name, such as `Base of operations`, before the page's scrollable description. The subtitle comes from the same custom title renderer and embedded localized string used by the visual page; it is not inferred from the selected entry.

Combat Simulator Advanced Setup controls use the same semantic values as their visual widgets. The Limits sliders announce minutes, score, team score, or `No Limit`; Player Handicaps announce the displayed damage-scale percentage rather than the slider's internal position; and Simulant slot buttons include the generated simulant name rendered beside the slot number. Other sliders still fall back to a rounded position percentage when their handler does not provide a display label.

After a Combat Simulator session, the Save Player confirmation announces its visible localized question, `Save new player and statistics?`, before the initially focused `Save Now` control. Moving between `Save Now` and `No Thanks!` reads only the new control; F5 reconstructs the title, question, and current control.

The Combat Simulator results sequence announces the semantic contents of every custom-rendered page. Challenge and team rankings read each displayed team and score in visual order; player rankings read each displayed player, deaths, and score; the Game Over page reads placement, title, weapon of choice, and any displayed awards; and Stats for Player reads suicides plus the displayed kills and deaths against each opponent. These values come from the same multiplayer records used by the renderers. F5 repeats the complete current page.

Mission Select list options announce the complete localized mission name rendered across the location and subtitle lines, such as `dataDyne Central - Extraction`, followed by each bright completion star by its localized difficulty name: Agent, Special Agent, or Perfect Agent. Solo narration follows the renderer's highest-completed-difficulty rule, co-op narration reports the independently stored completion markers, and anti mode announces no completion state because it renders no stars. Combat Simulator challenge lists announce every bright player-count completion star, from one through four players or one through two in the 4 MB configuration. The Completed Challenges page uses the current player's markers, while the general Combat Challenges list uses the same shared-profile markers as its renderer.

### Active weapon menu narration

With `Accessibility.MenuNarration=1`, holding the active-menu control announces the localized title and focused label of every active-menu radial: Weapon, Function, cooperative Perfect Buddies, and Combat Simulator Orders. The title is included on entry to each radial screen and subsequent focus movement speaks only the new label. Weapon labels include `Unarmed`; stable focus and renderer animation do not repeat speech. Closing the radial resets selection state without cancelling an already-started short announcement. Only the primary local player publishes speech; selection, weapon-function, and bot-command behavior are unchanged.

While holding the active-menu control (L1/LB in the default controller layout), press the right trigger to cycle from Weapon to Function and then to an available buddy/order radial. Perfect Buddies is available in cooperative missions with an AI buddy. Orders is available in team Combat Simulator games with friendly simulants; each buddy can have an individual command screen, and holding the right trigger while on an order screen addresses all friendly bots.

### HUD message narration

`Accessibility.HudMessages` defaults to `1` for blind-user acceptance testing and is subordinate to both `Accessibility.Enabled` and the speech setting. Every live-player message offered to the common HUD queue is eligible regardless of the stage or script that created it. The accessibility layer observes the resolved semantic text before the visual queue applies duplicate and capacity policies, so identical back-to-back pickups are each queued for speech even when the sighted HUD retains one copy. It speaks at normal, non-interrupting priority after replacing line breaks and other control whitespace with spaces. Messages rejected by the native alive or subtitle-option rules remain unannounced, and dialogue/subtitle message types remain explicitly excluded.

The explicit in-game and cutscene subtitle HUD types are never spoken by this feature. This keeps dialogue and subtitles out of generic gameplay-message narration and leaves their timing, splitting, and user preference policy for a separate subtitle feature. Logs retain admitted subtitle metadata and record the exclusion without sending it to speech.

Combat Simulator's death overlay is drawn directly rather than admitted to the common HUD queue. When it becomes visible, this feature speaks the localized `Press START` prompt with the currently displayed respawn-countdown number, then speaks each newly displayed positive integer once. Pausing, leaving the overlay, respawning, ending the match, disabling HUD messages, changing stages, or shutting down clears the fixed per-player observation state. Dialogue and subtitles remain excluded.

### Environmental laser hazard cue

`Accessibility.EnvironmentalHazards` defaults to `1` and is subordinate to `Accessibility.Enabled`. During single-player gameplay, the adapter recognizes active `DOORTYPE_LASER` objects carrying the game's damaging-contact flag. It does not depend on Carrington Institute tags or holo-training object IDs, so the same policy can cover equivalent laser barriers elsewhere.

The nearest eligible beam produces an independent 220 Hz sine tone only while its closest point is within 500 world units, inside a 25-degree camera-facing cone, and visible through background geometry. Eligibility refreshes every three logical ticks while the virtual source traverses the longest centerline of the beam and returns over a 90-tick cycle. Existing prop-sound attenuation and stereo-pan calculations follow that moving source; the mixer interpolates frequency and pan per audio buffer and applies a 10 ms gain ramp. A 75-unit selection margin keeps adjacent bars from rapidly replacing one another. Only one hazard voice plays at once.

Inactive, fully faded, open, or non-colliding lasers are excluded. Turning away, losing line of sight, leaving range, opening a menu, pausing, entering a cutscene, dying, changing stage, disabling the feature, or shutting down stops the voice. The hazard lane is separate from the centered fine-aim tone and beacon chirp lane, uses the existing fixed mix buffer, and allocates no memory or game sound channel at runtime.

### R-Tracker audio interface

`Accessibility.RTrackerAudio` defaults to `1` and is subordinate to `Accessibility.Enabled`. Activating or deactivating the native R-Tracker speaks `R-Tracker on` or `R-Tracker off`. If activation produces no native marker after a six-logical-tick settling delay, `No tracked targets` is spoken once.

The audio adapter shares one classification function with the native radar. It therefore admits exactly the yellow mission/training objects, red tracked characters, and blue cheat items that the visual R-Tracker admits. It deliberately does not add line-of-sight, render, room, or path restrictions. Native death, cloak, device, and cheat state remain authoritative, and horizontal distance is capped at the visual radar's 4,000-world-unit edge.

Ten preallocated slots retain all audited base-game markers without consuming game sound channels. A single procedural lane schedules due slots in round-robin order, so tracker contacts never sound simultaneously. Yellow, red, and blue markers use 1450, 1200, and 1800 Hz respectively, keeping the tracker above most virtual-cane terrain cues. The engine's established property-sound pan calculation communicates left/right bearing; a light 30 Hz amplitude modulation distinguishes markers behind the player. Nonlinear per-contact distance curves shorten the requested interval from approximately 1.4 seconds at the radar edge to 0.15 seconds nearby and raise gain from 35 percent to full tracker volume as the player approaches. A 120 ms no-overlap floor preserves the full vertical phrase; a due contact may be delayed under contention but never replaces or overlaps another contact. A level marker uses one chirp, an above marker uses a rising double chirp, and a below marker uses a falling double chirp. Identities remain stable until the native marker disappears.

Menus, pause, cutscenes, death, unsupported multiplayer, stage teardown, feature disable, and shutdown silence the fixed voices. Temporary suppression does not falsely announce device deactivation. The full contract, audited marker counts, performance thresholds, and test matrix are in `documentation/ACCESSIBILITY_RTRACKER_AUDIO_SPEC.md`.

### Combat Simulator audio radar

`Accessibility.CombatRadarAudio` defaults to `1` and is subordinate to `Accessibility.Enabled`. It applies only to a normal Combat Simulator match with one local player. Press F3 for an immediate clockwise audio pulse of the markers drawn by the native radar, excluding the current player's own marker. Press Shift+F3 to toggle automatic enemy-contact alerts; the usual rising or falling two-beep confirmation reports the new state. The runtime toggle is sticky across matches for the current process and starts from `Accessibility.CombatRadarContactAlerts`, which defaults to `1`.

The adapter captures the final semantic calls made by `radarRender`, so native Radar, No Radar, No Player on Radar, death, cloak, team, and scenario-objective rules remain authoritative. It adds no line-of-sight, viewport, room, or path filter. A fixed snapshot retains at most 16 markers, prioritizing enemies, objectives, allies, then other markers. Distance raises pitch from 650 Hz at the native 4,000-unit radar edge to 1,400 Hz nearby; stereo pan conveys bearing, rear modulation conveys front/back, and native +/-250-unit vertical classification produces rising or falling double patterns. Category timbre distinguishes enemies, objectives, allies, and other markers.

Automatic alerts establish a silent baseline, then report new enemy contacts and inward crossings through medium and close bands. Defaults are 2,000 and 750 world units, with outward hysteresis at 2,300 and 900 units, a three-second per-transition cooldown, and a 30-logical-tick disappearance window. Close events have highest queue priority. `Accessibility.CombatRadarMediumDistance`, `Accessibility.CombatRadarCloseDistance`, and `Accessibility.CombatRadarVolume` configure the thresholds and master volume; their defaults are `2000`, `750`, and `0.20`.

One dedicated procedural mixer voice and a fixed 48-event queue avoid runtime allocation and game sound-channel use. Menus, pause, cutscenes, death, unsupported player counts, match teardown, and native radar unavailability stop output and force a fresh silent baseline on resume. An empty manual pulse and an unavailable native radar have distinct centered earcons. The complete contract and remaining blind-user test matrix are in `documentation/ACCESSIBILITY_COMBAT_RADAR_AUDIO_PLAN.md`.

### King of the Hill beacon

`Accessibility.KingOfTheHillBeacon` defaults to `1` and is subordinate to `Accessibility.Enabled`. In a one-local-player King of the Hill match, it uses the scenario's exact floor-adjusted `hillpos`, not a room estimate or map-specific coordinate. Within the configured audible-marker range, direct line of sight to the hill center produces the player-marker base sound: two opposing oscillators sweep between 300 and 600 Hz. A single positioned 800 Hz identity chirp repeats every second, instead of the player markers' one-to-four chirps every two seconds. The cue follows `Accessibility.MarkerRange` and `Accessibility.MarkerVolume`, including their distance curve.

When the native renderer actually draws the Hill on Radar marker, the same sound remains available outside local line of sight and range at half gain. The radar guide includes the once-per-second identity chirp so it remains identifiable in combat, and uses stereo pan and rear modulation to communicate the route direction. It disappears if Hill on Radar is disabled, the complete radar is disabled or hidden, or a mobile hill is between locations. The stronger of the distance-shaped local base and radar base controls overall gain. The ordinary F3 radar pulse continues to include the hill as an objective marker.

The beacon's identity chirp accelerates to twice per second only while the player occupies the native hill room, the player's team controls the hill, and the native point countdown is advancing. Leaving the room, losing control, or another team contesting and pausing the countdown restores the ordinary once-per-second locator cadence. This reports the same scoring state represented by the native hill room, colour, and countdown rather than estimating occupancy from distance to the beacon center.

The hill has one dedicated fixed procedural voice, so it does not consume a player-marker slot, replace a combat-radar event, allocate memory, or use a game sound channel. Menu, pause, cutscene, player death, match teardown, unsupported player counts, feature disable, and shutdown silence it. Logs use the `hill_beacon` feature and distinguish `local`, `radar`, and suppressed states while recording native room occupancy, team control, countdown progress, and the resulting scoring cadence.

### IR Scanner highlight audio

`Accessibility.IRScannerAudio` defaults to `1` and is subordinate to `Accessibility.Enabled`. While the native IR Scanner is active, an object that received the visual scanner highlight in the preceding rendered frame uses the R-Tracker's yellow-object 1450 Hz spatial pattern. Up to ten highlighted objects share the same fixed round-robin slot pool; the game does not permit the R-Tracker and IR Scanner to be active together.

Eligibility shares `objIsHighlightedByInfrared` with the object renderer, covering `OBJHFLAG_CONDITIONALSCENERY` and `OBJFLAG3_INFRARED` objects such as the CI training secret door. The accessibility adapter additionally requires `PROPFLAG_ONANYSCREENPREVTICK` in single-player, which is the engine's retained evidence that the object was rendered on the player's preceding frame. Turning away removes the sound after that one-frame observation delay. Ordinary objects merely recolored by the overall IR palette and characters rendered in the scanner's global red treatment are not classified as special highlighted-object targets.

The cue inherits R-Tracker bearing, rear modulation, distance cadence, relative-height pattern, lifecycle suppression, fixed capacity, and allocation-free mixer behavior. It adds no line-of-sight scan, projection estimate, object name, or off-screen awareness. Logs identify `source=ir_scanner` and `category=infrared_highlight`.

### X-Ray-aware semantic scanner visibility

`Accessibility.XRayScannerAudio` defaults to `1` and is subordinate to `Accessibility.Enabled`. It no longer creates a generic 700 Hz cue for every object recolored by the native X-Ray renderer. Instead, active X-Ray vision supplies an alternate visual-access path to the established semantic scanners. An X-Ray-rendered interactable, door, pickup, hostile character, or non-hostile character uses exactly its normal category sound and its existing F5, F6, F8, combat, or F7 selection state.

The shared predicate requires the uninhibited X-Ray Scanner device, the native `VISIONMODE_XRAY`, `PROPFLAG_ONANYSCREENPREVTICK` evidence that the prop appeared in the preceding rendered frame, and the same `objGetXrayHighlightDistance` eraser-radius calculation used by the renderer. A qualifying prop may bypass ordinary room-neighbour and physical line-of-sight gates because the active device visually reveals it through that geometry. All semantic health, relationship, collectability, actionability, viewport, scanner range, capacity, and lifecycle rules remain in force. The Farsight's separate use of X-Ray vision does not enable this path.

Ordinary scenery receives no sound merely because X-Ray recolors it. Turning away, moving it outside the native X-Ray radius, inhibiting or deactivating the device, disabling `Accessibility.XRayScannerAudio`, or leaving supported gameplay immediately restores the normal scanner visibility rules. Logs retain X-Ray device-state transitions and mark admitted candidates with `xray_semantic_visibility` or `xray_exposed=1`.

### Hostile combat targeting

Outside an active firing-range session, the targeting adapter uses the engine's onscreen-prop list and finalized non-shooting attack query to admit ordinary hostile characters, automated gun turrets, and `OBJTYPE_CCTV` security cameras in any single-player gameplay stage. It is not keyed to Carrington Institute, DataDyne Research, holo-training, setup identifiers, or model names. A character must be active, enabled, alive, rendered within the viewport, hostile according to the existing team comparison, and visually observable through background, doors, path blockers, and objects. Props carrying the engine's `OBJFLAG_AISEETHROUGH` semantic transparency flag do not block that presence test. Character visibility uses an early-exit five-point probe at the body center, upper and lower body, and the left and right upper-body edges, so an exposed head or a guard peeking around cover is not rejected merely because the body midpoint is obstructed. Friendly, neutral, dead or knocked-out, hidden, untargetable, and imperceptibly cloaked characters are excluded. An autogun must be active, enabled, healthy, armed, non-deactivated, non-malfunctioning, and configured by its native target-team mask to attack the current player; this excludes the player's deployed Laptop Gun. A security camera must be active, enabled, healthy, non-deactivated, and not carry the native camera-disabled flag. All three categories require finite projected bounds intersecting the viewport and semantic visual line of sight; object targets retain a single center ray. For an exact combat aim, the adapter also consumes the next hit already present in the native ordered shot list when the direct hit is explicitly classified as penetrable, non-bulletproof glass. This covers standalone glass props and the glass display-list node of a native windowed door without treating the opaque door frame as transparent. Device and firing-range profiles retain the direct hit because bullet penetration does not imply valid interaction.

While the equipped weapon function carries the native `FUNCFLAG_THREATDETECTOR` flag, accessibility also consumes the player's four native threat-tracking slots—the exact list used to draw the sight's visible threat boxes. Each active tracked threat is treated as a hostile combat candidate and receives the ordinary enemy presence and direct-aim feedback. This covers the detector's existing semantic set: eligible autoguns, Skedar shuttles, dangerous deployed grenades/mines and secondary-function Dragons. Native code can also classify one-hit explosive firing-range targets; the dedicated firing-range target profile remains authoritative for ongoing presence and fine aim during an active range session.

Whenever a stable prop first enters the native detector list, a separate positioned 120 ms alert sweeps upward from 1,000 to 2,000 Hz using the bright enemy harmonic timbre. It uses the configurable long-range enemy attenuation and master volume rather than the firing range's shorter presence curve, so a newly mine-bearing distant range target remains audible. Up to four simultaneous additions are serialized 12 logical ticks apart on one dedicated fixed lane. A two-observation disappearance grace prevents a transient slot loss from retriggering the alert; after a real disappearance, the same prop alerts again if the native sight later shows it as new.

While the exact valid aim target is also present in the native threat list, that same rising sweep starts immediately and repeats every 15 logical ticks (approximately 250 ms). It overlays the ordinary centered alignment tone: the base tone continues to communicate target acquisition or range accuracy, while the repeated sweep distinguishes the detector-marked target from adjacent ordinary targets. The repeat stops on the first observation that aim leaves the threat, the target becomes unshootable, or the detector becomes inactive. An initial new-contact sweep already in progress counts as the first aimed pulse instead of being restarted. Leaving the detector function, opening an unsupported overlay, pausing, or resetting targeting clears pending alerts and stops the lane. The adapter does not rescan props, expand the four-target visual capacity, reveal off-screen objects, or recreate those rules from names or models. Native threats are admitted before the generic combat scan and deduplicated by prop, so a turret shown by both sources receives one ongoing voice.

Up to ten visible threats receive stable slots in a preallocated procedural-oscillator pool. Characters and autoguns emit a bright spatial cue using live distance attenuation and stereo pan. Its configurable carrier defaults to 900 Hz and includes a quiet second harmonic at 1,800 Hz, distinguishing combat presence from the pure 440 Hz door/person and 880 Hz interactable chirps without adding a second pulse. An experimental vertical-direction mapping compares the target's projected midpoint with the same screen-space aim point used by the engine's no-spread targeting query. It does not compare the target with Joanna's position, floor height, or the camera center. Each ordinary enemy chirp plays the configured carrier for its first half, then uses the elevation result for its second half: a vertically aligned enemy therefore produces one uniform normal enemy beep, while an enemy above or below the crosshairs produces a higher or lower second half that communicates the required correction. The elevation result remains at the carrier within 5 degrees of alignment, falls exponentially toward two-thirds of the configured value when the target is 45 degrees below the aim point, and rises toward five-thirds when it is 45 degrees above. With the default carrier this is approximately 600 Hz below, 900 Hz aligned, and 1,500 Hz above. Relative angles clamp at 45 degrees and each assigned voice smooths 35 percent toward its new target per logical observation. At or inside melee range, where the distance cue becomes continuous rather than a repeating chirp, the whole tone retains the smoothed elevation pitch; it resolves to the normal carrier at alignment. Security cameras instead emit a distinctive high descending electronic scan from 1,600 to 1,000 Hz over 140 ms every 500 ms. Camera scans share the same stable slots, distance curve, volume, panning, phase staggering, and fixed mixer storage, but retain their fixed sweep and do not use elevation or melee-distance semantics.

Combat uses a dedicated configurable long-range curve, defaulting to full distance volume through 4,000 world units, fading through 5,500, and silent at 6,000. Narrowing the live gameplay FOV blends smoothly into a scoped curve that is full through 6,000, fades through 9,000, and becomes silent at 12,000. The blend begins below the configured normal player FOV and reaches the complete scoped curve after a 25-percent reduction; it therefore follows any weapon scope and custom FOV scaling without identifying particular guns. A bounded accessibility-local attenuation calculation supports the extended scoped range without changing the native game's 6,000-unit positional-sound clamp. It does not broaden semantic eligibility: the enemy must still be hostile, alive, rendered inside the narrowed viewport, and pass line of sight. Its configurable mixer gain defaults to 0.25; firing-range and device feedback retain their existing range and volume. For characters and autoguns, speed and length communicate proximity relative to the unarmed weapon definition's actual melee range `X` (currently 60 world units): at or beyond `5X`, the chirp lasts 180 ms and repeats every 500 ms; between `5X` and `X`, both values ramp linearly toward a 50 ms chirp every 200 ms; at or inside `X`, the slot switches to a constant tone. Shortening the calculated period by another 40 ms immediately starts the next chirp instead of waiting for the prior, longer cycle to expire. The constant tone is entered only at the real `X` boundary, then retained until distance exceeds `1.1X`; this exit-only hysteresis prevents normal character-collision and animation jitter around `X` from flickering the range indication. The cue distance approximates the attack ray's distance to the near body surface by measuring camera-to-body-midpoint distance and subtracting the character radius. It communicates range, not guaranteed impact: aim, animation timing, intervening geometry, and the target's exact shape still govern the punch.

While the player is manually aiming with any weapon, the closest eligible hostile character, automated turret, or targetable security camera to the crosshairs receives precision guidance through its existing combat voice. This follows the game's semantic aim state, so alternate controls and remapped aim inputs behave like the default left trigger. The selected target repeats a 100 ms chirp every 160 ms. Releasing aim immediately restores ordinary distance/elevation cadence or the camera's distinctive scan. The first pass projects centers from the active model's existing collision boxes. Characters retain fixed head, torso, arm, lower-body, and other-region anchors rather than steering toward the potentially empty midpoint of the whole model rectangle; gun and hat boxes are excluded. Fixed object targets use their nearest collision-box center. Character selection applies a small bias toward the body region that supplied the successful semantic visibility sample. If no usable collision box exists, a character uses the engine's native auto-aim position and a fixed object uses the center of its finite projected model bounds.

When the crosshairs enter a 12-logical-pixel expansion of an eligible target's projected rectangle, a second pass refines only one target per frame. It tests the nearest retained collision region first, samples its center and four conservative interior offsets, and reuses the model's detailed polygon test plus an exact-point semantic visibility ray. It then considers the next-nearest regions until it finds a valid visible surface or reaches a fixed 15-query budget. A previously selected target wins the refinement slot while it remains eligible; an existing exact lock skips refinement entirely. A validated geometry point supersedes the coarse anchor for horizontal and vertical guidance. If no sampled point validates, coarse guidance remains available for acquisition and the next animated or articulated frame may try again. All candidate, sample, and diagnostic storage is fixed; no runtime allocation or additional audio voice is used.

Horizontal guidance error, normalized against the current live viewport, drives exaggerated stereo pan; the first half remains at the normal enemy carrier and the second half rises when the selected point is above the crosshairs or falls when it is below. A small center dead zone resolves to the unmodified carrier. Target selection retains the current hostile until another is meaningfully closer to the crosshairs, avoiding rapid switching between adjacent enemies. This guidance never creates a lock: only the native/raw semantic aim result can start the separate solid 660 Hz alignment tone.

Slot phases are deterministically staggered so newly admitted enemies do not all begin their chirps at the same instant. No game sound channel or runtime allocation is used. Existing assignments remain stable while eligible; empty slots are filled in the targeting core's screen-center/distance order. Additional hostiles wait until a slot is released, a bounded policy that requires dense-combat acceptance testing.

Pointing directly at an admitted hostile starts the responsive alignment tone only while the game exposes its native target indicator. The same gate suppresses every centered lock when the aiming point is hidden, notably while unarmed, without suppressing positioned enemy presence or distance feedback. Character locks reuse the exact body part selected by the game's existing non-firing attack query: head geometry raises the ordinary 660 Hz lock to 825 Hz, either hand/forearm/bicep lowers it to 528 Hz, and torso, pelvis, legs, feet, and other character geometry retain 660 Hz. This changes only the feedback tone; it does not alter aim, hit detection, damage, or shot placement. When that exact native shot continues through penetrable glass and hits an admitted combat target, the same body-region carrier gains a 12 Hz amplitude tremolo ranging from 55 to 100 percent gain. Pitch therefore continues to communicate head, torso, or arm while the tremolo communicates that glass remains in the firing path. The modulation disappears immediately when the firing path becomes clear. The lock is otherwise solid for hostile characters, automated turrets, security cameras, threat-detector objects, firing-range targets, and validated device targets when their profile exposes a target indicator. Protected/non-hostile characters retain the body-region relationship but multiply its carrier by 0.60 and interrupt it with an exact 10 ms silent gap in every 100 ms cycle: head is 495 Hz, torso and other regions are 396 Hz, and arms are about 317 Hz. Breakable route-obstruction glass, destructible containers that own a collectable item, and ordinary world objects for which the native reticle has finalized its reactive blue/red state have no character body region and use the interrupted 396 Hz lock. This leaves the ordinary solid hostile lock family as the only alignment feedback in its established 528–825 Hz body-region range. The penetrable-glass tremolo and protected-target interruption are independent pattern flags and can coexist. A hostile seen through penetrable glass retains its ordinary solid-lock carrier and volume while gaining only the obstruction tremolo. The native-reactive path requires the exact retained attack-query hit and the same `OBJFLAG3_REACTTOSIGHT`/`sightIsReactiveToProp` result used by the sight system, so destroyed objects stop qualifying and no stage/model allowlist or enlarged target area is introduced. The 90 ms sound/10 ms gap pattern and lower carrier distinguish a valid target that is not an ordinary hostile without losing the relative head/arm information for a protected character. Loot-container detection follows the engine's parent/child ownership rather than a stage or model allowlist: the outer object must be active, visible to the exact attack query, healthy, mortal, and contain an `OBJFLAG_INSIDEANOTHEROBJ` collectable child. Empty, destroyed, invincible, invisible, and uncollectable containers are excluded, and the equipped attack must be capable of damaging the outer object.

Characters retain the game's finalized character-target selection. A two-frame release grace keeps the tone stable across a transient one- or two-frame loss only while that same living character remains visible, hostile, shootable, and unobstructed; a different target, invalid state, scope transition, or a third lost frame stops or changes it immediately. Security cameras, non-turret threat-detector objects, and breakable route obstructions use the same-frame non-random weapon query ray already used to determine where a shot would land and receive no release grace. Automated turrets use that exact hit when available. Turret and security-camera presence no longer relies on a single ray to the object's setup origin: five bounded screen samples test actual rendered model geometry at the center, upper, lower, upper-left, and upper-right regions, and one sampled surface must pass semantic visual line of sight. This keeps a partly exposed, shootable turret audible when its origin is behind cover while rejecting a fully occluded model. If the attack ray hits no prop, an active hostile turret may also align when the live crosshair lies within three logical screen pixels of its finite projected model bounds. The closest bounds and then closest center win when multiple turrets overlap. This turret-only tolerance still requires the model to be rendered in the current viewport and to have a validated visible geometry sample; an exact hit on a door, character, glass pane, or any other prop suppresses the fallback. It changes only feedback and never pulls, snaps, redirects, or otherwise alters weapon aim, so random spread and visible-model gaps can still make a shot hit or miss independently of the tone. Characters in dying, dead, knockout-fall, or knocked-out actions and cameras that become disabled, deactivated, or destroyed are removed from the audible combat slots; detector objects disappear when the native threat list removes them or the player leaves the detector function. Firing-range targets retain their scoring-center pitch mapping, procedural round robin, and back-facing shootability rule. Switching between firing-range and combat profiles clears prior identities and owned sound state.

### Weapon-function state cues

`Accessibility.WeaponFunctionCues` defaults to `1` and is subordinate to `Accessibility.Enabled`. During first-person gameplay, the accessibility layer observes the same effective primary/secondary value used by the gun-function visual indicator after each player's weapon processing. A successful change to primary plays one centered 1000 Hz beep; a successful change to secondary plays two. Each beep is 35 ms with a 30 ms gap, a 2 ms attack, and a 5 ms release. When the game's Show Gun Function option is enabled, the exact localized function name drawn beside the ammo display is also spoken once at normal, non-interrupting priority under `Accessibility.HudMessages`; this direct-rendered label does not pass through the common HUD-message queue. Initial state, stage entry, and changing weapons establish a new baseline silently, so equipping a weapon does not masquerade as a function-toggle command or repeat its stored firing mode.

The pattern has a dedicated fixed oscillator lane and does not consume a game sound channel, allocate at runtime, or interrupt door/object chirps. Any input path that changes the same semantic state receives the cue, including the dedicated controller command and active-menu selection. Temporary alternate functions are reported when their visual state actually changes. Multiplayer arbitration and usefulness remain to be acceptance-tested.

### Player-stance cues

`Accessibility.StanceCues` defaults to `1` and is subordinate to `Accessibility.Enabled`. During ordinary single-player walking gameplay, the accessibility layer observes the engine's effective crouch position after movement processing. This is the minimum of the player's requested crouch position and automatic crouch position, so collision-rejected commands do not produce a false cue and weapon-driven changes such as the Sniper Rifle's Crouch function are included. A transition to standing plays one centered 880 Hz beep; crouching plays 880 Hz then 1320 Hz; double-crouching plays 880 Hz followed by two 1320 Hz beeps. Beeps use the cane-mode timing of 35 ms with 25 ms gaps. Stage entry establishes a silent baseline, and menus, pause, cutscenes, death, remote views, vehicles, and grabbed-object movement do not emit stance cues.

Stance and scanner/cane confirmations deliberately share the short fixed toggle-confirmation lane, so a newer confirmation replaces an unfinished one. The Sniper Rifle's native secondary function is localized as `Crouch`; the HUD derives its speech from the same effective function label and is intentionally not rewritten by the stance adapter.

### Weapon-change announcements

`Accessibility.WeaponChangeAnnouncements` defaults to `1` and is subordinate to
`Accessibility.Enabled` and speech availability. The accessibility layer waits
for a requested weapon to become the equipped right-hand weapon rather than
speaking the input alone.

Releasing the active weapon menu after selecting a weapon queues its total
ammunition count after the already spoken focus label. Quick forward/back weapon
changes interrupt stale speech with the localized weapon name shown by the gun
HUD followed by the same count. The count uses the ammunition type displayed for
the effective weapon function, falls back to the other function exactly as the
HUD does, and includes reserve plus loaded rounds in both hands. A weapon or
device with no ammunition definition speaks only its name for a quick change and
adds no count after active-menu selection. Pending requests expire if the
requested weapon does not equip, preventing a failed or superseded command from
being announced later.

The same settled observation also checks whether both weapon hands are actually
in use. A transition into dual wielding speaks the existing localized `Double`
prefix followed by the weapon name, such as `Double Falcon 2`; returning to one
hand speaks the ordinary weapon name, such as `Falcon 2`. Initial stage state is
a silent baseline. If a radial or quick-change request causes the same settled
transition, that phrase is folded into the normal weapon-and-ammunition
announcement rather than spoken twice. A requested state does not announce
until the left hand's actual in-use value changes.

### Single-player world scanners

`Accessibility.InteractableBeacons` defaults to `1` for project-owner blind-user acceptance testing and remains the master backend setting in `pd.ini`. The independent F5, F6, and F8 selections are stored as `Accessibility.InteractableScannerActive`, `Accessibility.DoorScannerActive`, and `Accessibility.PickupScannerActive`; each defaults to `1`, changes immediately with its gameplay key, and is saved to `pd.ini` during an orderly exit. The saved selections automatically resume in any supported single-player mission or one-local-player Combat Simulator match.

- F5 independently toggles automatic nearby interactable-object beacons.
- F6 independently toggles automatic nearby door beacons.
- F8 independently toggles automatic nearby collectible-item beacons.
- Each F5/F6/F7/F8 gameplay toggle plays a centered two-beep confirmation. Enabling rises from 880 Hz to 1320 Hz; disabling falls from 880 Hz to 440 Hz. Each beep is 35 ms with a 25 ms gap. F4 shares this timing and lane for its distinct cane-mode patterns.
- Each enabled category tracks up to its three nearest eligible props. Interactables and pickups share one round-robin chirp lane. Doors use three separate preallocated voices on one shared 375 ms clock, with fixed 125 ms start windows and 100 ms chirps. A newly acquired door waits for its next complete window, so nearby computers or pickups cannot delay a door cue and multiple doors cannot overlap one another.
- The interactable/pickup pulse gap is 750 ms divided by the scheduled target count, with a current minimum of 300 ms. Each retained door repeats independently every 375 ms, with deterministic phase offsets preventing simultaneous starts. Each category refreshes twice per second, and a 150-unit membership margin prevents borderline targets from repeatedly entering and leaving the retained set.
- Door and interactable candidates must be visible in the active camera view and have semantic visual line of sight on every scan and again immediately before playback. Interactables require both the renderer's current-view flag and a finite projected model rectangle intersecting the live viewport; this rejects objects whose origin is nearby but behind, above, below, or otherwise outside the screen. Because PC prop rendering converts model matrices in place, a fixed 128-entry cache captures only deliberately interactable object projections at the safe pre-render boundary. Scanner refresh and playback validation consume that cache for at most two logical ticks and verify prop/object identity, rather than recalculating from invalid post-render matrices. Pickups and people retain their existing category-specific visibility policies. Background, closed doors, opaque objects, and opaque path blockers suppress the target; props marked `OBJFLAG_AISEETHROUGH`, including semantically transparent glass, do not. A sibling group representing paired door leaves retains one canonical identity and selects at most one rendered leaf as its sound source. Door visibility excludes every leaf in that same sibling group from the synchronous ray and tests the selected leaf's origin plus five bounded points on its nearest model-box face; this prevents either an embedded anchor or the other half of a doorway from concealing the doorway itself. The scanner uses the same origin and five surface probes for interactable objects. If all ordinary interactable rays fail, an on-screen object whose native interaction does not request its own LOS check receives one final set of rays with those surface endpoints pulled eight world units toward the camera. A monitor carrying the native `OBJFLAG_MONITOR_RENDERPOSTBG` placement semantic uses a bounded 24-unit retry because those pads deliberately place the visible screen plane farther inside the mounting geometry. The larger allowance does not apply to ordinary interactables, and every retry must still pass the semantic collision ray; neither render state nor projection alone can admit a target. Intervening opaque geometry remains authoritative.
- While a menu is open, F5 retains its menu-narration repeat action; gameplay beacons stop and do not process scanner-toggle presses. The selected F5/F6/F7/F8 category states are preserved and automatically resume with a fresh scan when ordinary gameplay returns.
- Door viewport rule: the projection cache described above is now 256 entries and also captures every door. A door sounds only while the finite projected model rectangle of at least one sibling leaf intersects the active viewport; the engine's broader current-screen flag alone is not sufficient. A paired doorway retains one canonical cue and transfers its positioned source between visible leaves without duplicating the sound.
- Interactable objects use one short positioned 880 Hz sine chirp; collectible items use three 35 ms 880 Hz chirps separated by 25 ms; doors use one short positioned 440 Hz chirp through a dedicated voice with a 0.24 mixer level, 50 percent above the former shared chirp lane. Door voices retain independent position and volume but take turns in non-overlapping shared-clock windows. Door distance and stereo direction remain anchored to the closed-position center of the complete doorway rather than following an opening leaf toward the frame. The patterns retain distance attenuation and stereo direction while avoiding game/menu sound meanings.
- Pickups are recognized from the same object-type and collectible/uncollectable flags used by the collection path, including temporarily spawned device-training objects such as the Data Uplink. The player's deployed CamSpy uses a character prop rather than an object prop; after remote control ends, F8 treats that visible, inactive, deployed CamSpy as a pickup until Joanna retrieves it. An active, held, hidden, destroyed, or other character prop is never admitted through this exception. Invisible, inactive, deleted, and temporarily reserved/in-flight projectile items are excluded. The item disappears from the next bounded refresh after collection.
- The implementation supports one-local-player missions and Combat Simulator matches. Interactables, pickups, and people use a 1,200-unit scan radius; rendered doors use an extended 3,000-unit radius. F5, F6, and F8 each retain up to three nearby targets for their independently enabled category; split-screen and cooperative multi-local-player composition remain unsupported.
- Menus, pause, cutscenes, death, unsupported player counts, stage changes, invalid props, and audio allocation failures stop beacon audio. Stage teardown and shutdown discard all prior targets and sound state without changing the four saved selections. Eligible gameplay performs a fresh scan using the retained state, including after restarting the game. Only explicit F5/F6/F7/F8 toggles or direct edits to their `pd.ini` keys change those selections.
- The current stereo positional system primarily conveys left/right and distance. Front/rear and vertical usefulness require runtime and blind-user evidence and are not yet claimed.

When the active camera is a deployed CamSpy, door scanning transfers to the CamSpy prop, rooms, position, and camera heading on the same logical tick as the visible perspective change. Returning to Joanna transfers it back without requiring F6 to be toggled. Interactable and pickup categories deliberately pause while viewing through the CamSpy because those cues describe things Joanna can physically use or collect, then resume from her position after returning. At that point, the inactive CamSpy itself can enter F8 from Joanna's perspective.

### Non-hostile-character beacons

`Accessibility.NonHostileBeacons` defaults to `1` as the people-scanner backend. Its independent F7 selection is stored in `Accessibility.NonHostileScannerActive`, also defaults to `1`, and persists across orderly game restarts. During unobscured single-player gameplay, F7 toggles positioned cues for nearby living friendly or neutral characters. Each retained person uses a two-oscillator drone: a steady 440 Hz fundamental at 72 percent of the voice and a quieter 550 Hz major third at 28 percent. The upper component remains steady for neutral and protected characters. For characters classified as friendly, the 550 Hz component alternates one second on and one second off while the 440 Hz fundamental remains steady; ten-millisecond edges prevent clicks. Distance attenuation is multiplied by two before a 0.10 mixer gain, producing a maximum 0.20 voice level. Smooth 10 ms gain and pan changes avoid clicks as a person moves, appears, or leaves.

The static consonant chord is deliberately unlike pickup and door chirps, hostile pulses, and the player markers' opposing 300–600 Hz sweeps plus numbered 800 Hz chirps. Three dedicated fixed voices allow all retained people to remain spatially trackable at once without consuming the serialized object/pickup chirp lane or three dedicated door lanes. The category uses the engine's team and blue-sight classifications rather than stage or character identifiers: ordinary enemies are excluded, while friendly, neutral, and protected blue-sight characters are eligible. Blue-sight characters are mission-sensitive nonlethal targets such as people who must be incapacitated; they receive the F7 location drone and interrupted valid-aim alignment tone, but never the hostile enemy-presence cadence. The current player, dead, dying, knocked-out, hidden, untargetable, imperceptibly cloaked, inactive, disabled, out-of-range, room-disconnected, and line-of-sight-blocked characters are excluded. Up to three people are retained within the existing 1,200-unit radius and refresh automatically twice per second as characters move or change state. F7 is ignored while menus, pause, cutscenes, death, or unsupported multiplayer contexts suppress beacon output; its selected state resumes automatically with gameplay. The category does not label a person's identity or promise that a neutral character will remain non-hostile after a future scripted state change.

While looking through a CamSpy, the people scan and its range, room, bearing, and line-of-sight decisions use the CamSpy perspective. Remote-view candidates must also have been rendered in that viewport; this prevents room-connected but visually absent character props from producing false cues. The CamSpy's own character prop is excluded. Existing F7 state is preserved across the transition back to Joanna.

### Targeting feedback and Carrington Institute firing-range proof

`Accessibility.TargetingFeedback` defaults to `1` and is subordinate to `Accessibility.Enabled`. It has no provisional key binding. Solid centered lock-on/alignment tones use `Accessibility.TargetingVolume`, which defaults to `0.15`. Interrupted protected, breakable, loot-container, and native-reactive locks instead use the completely independent `Accessibility.InterruptedTargetingVolume`, which defaults to `0.12`. Both controls are bounded from `0` through `0.4`; neither scales nor falls back to the other. They do not change the separately configurable positioned enemy-presence volume. During supported single-player gameplay, the targeting coordinator automatically selects the applicable semantic source: Carrington Institute firing-range targets, admitted hostile characters and security devices, or a validated special-device target profile. It remains silent when the current context has no supported target source.

- Every active, undestroyed firing-range target that the renderer placed in the current viewport becomes eligible after two consecutive observations.
- Eligible targets emit 100 ms positioned pulses using the same configurable 900 Hz carrier, quiet second harmonic, and master gain as enemy-presence feedback. One dedicated procedural one-shot lane rotates through the set: one target repeats every 36 ticks, while larger sets divide that cycle down to a minimum six-tick gap. It consumes no native game-sound channel or combat slot.
- Visible targets keep their positioned presence pulses while they rotate. The firing-range adapter separately reports whether each target is currently shootable, using the range's own facing test rather than duplicating its angle math.
- When the gameâ€™s final aiming ray reports one of those visible targets in `lookingatprop` and the target is facing the player, the separate centered continuous alignment tone reports scoring accuracy from 660 to 1,320 Hz. The tone stops on the first observation that loses the target or reports it as facing away, and resumes when that same target becomes shootable again.
- If the native sight path is expected to have played the same acquisition sound, the accessibility layer suppresses its immediate duplicate and begins only the hold cadence.
- Menu, pause, endscreen, cutscene/non-gameplay camera, death, exercise end, feature disable, and stage teardown clear both targeting audio lanes.
- The observer uses the rangeâ€™s fixed 18 target slots, fixed-capacity core storage, stable prop/object identity, existing projection data, and the already-filtered aiming result. It does not cast a second aim ray, alter aim, select a target, expose off-screen targets, or change range scripts.
- Logs include scope decisions, changed/periodic audits for all 18 slots, projection and identity data, shootability and `facing_away` transitions, aimed-target rejection/acquisition/loss, procedural pulse ownership/frequency/volume/pan, and 30-second sound/memory telemetry. Unchanged high-frequency state is aggregated to protect frame time while retaining periodic complete snapshots.

Outside the firing range, the same alignment lane also identifies destructible
route obstructions under the exact weapon query ray. An object qualifies only
while it is an active, non-hidden, healthy, mortal `OBJFLAG_PATHBLOCKER` and the
equipped attack type can affect its gunfire or explosion resistance. These
objects are aim-only candidates: they never consume an enemy-presence voice or
sound merely because they are nearby. Their valid alignment uses the interrupted
90 ms sound/10 ms gap pattern rather than the solid hostile lock. The exact
weapon query also admits ordinary destroyable cover and scenery when the prop
is active, enabled, visible, healthy, mortal, and the equipped attack can
damage it. This includes the destructible A51 crates used as cover in Pelagic
II without relying on that level or model identifier. Generic destroyable
objects remain aim-only and use the same interrupted lock; they do not gain a
nearby or off-screen presence sound. Invincible, destroyed, hidden, inactive,
ordinary decorative glass, and incompatible attacks remain silent.

The firing-range behavior remains a bounded proof. The separate hostile-combat slice covers basic single-player character relationship, occlusion, cloak/IR, elimination, automated-gun state/team semantics, and the native threat-detector sight list. A shared accessibility relationship adapter preserves the engine's special `TEAM_NONCOMBAT` semantics: those characters cannot become hostile merely because their team mask is disjoint from Joanna's, and eligible living, perceptible examples use the F7 friendly/neutral-character drone instead. Broader vehicle/non-character discovery outside the threat-detector sight, target speech/repeat, multiplayer output, and full Milestone 9 blind-user acceptance remain pending.

### Virtual cane prototype

`Accessibility.VirtualCaneMode` is a bounded integer with `0=off`, `1=slow`, and `2=fast`; it defaults to `1` for blind-user acceptance testing. During ordinary single-player walking gameplay, grabbed-object movement, mounted hoverbike movement, or active CamSpy viewing, F4 cycles Off, Slow, Fast, and back to Off. Each successful mode change plays a centered earcon using the scanner-toggle timing: Slow is 880 Hz then 1320 Hz, Fast is 880 Hz followed by two 1320 Hz beeps, and Off is 880 Hz then 440 Hz. Every beep lasts 35 ms with a 25 ms gap. There is no spoken mode announcement, and F4 is ignored while either Alt key is held. Menus, pause, cutscenes, death, other unsupported movement, stopped simulation, multiplayer, stage teardown, global accessibility disable, and shutdown stop all cane voices. Grab mode retains the ordinary barrier sweep while temporarily excluding exactly the live `grabbedprop` perimeter from each cane collision query, so the carried crate does not mask the route beyond it; all other props and background geometry remain eligible. Stance-dependent terrain traversal and crouch-passage classification resume after the player releases the object.

Each sweep samples current player position, stance-sized movement cylinder, and horizontal camera direction at nine live angles: -80, -45, -25, -10, 0, 10, 25, 45, and 80 degrees. The nonuniform fan concentrates five rays around the forward path, retains rays near a typical widescreen viewport edge, and uses the outer pair to expose peripheral openings. Slow completes the left-to-right sweep plus end pause in 120 logical ticks; Fast completes it in 60. Movement and turning do not restart or freeze the sequence. A delayed frame never catches up with a burst: overdue angles are skipped and no more than one collision query runs in a logical tick.

Each sample tests up to a configurable distance, defaulting to 900 world units, using the movement system's swept-cylinder room traversal against background, solid object, door, and path-blocker collision. It excludes characters and players and follows the current collision-cheat state. A collision plays a 35 ms procedural chirp at the returned surface X/Z position, with the camera height used for horizontal-only spatialization and a configurable master gain before distance attenuation. `Accessibility.VirtualCaneVolume` defaults to `0.184`, 20 percent below the previous `0.23` gain. Its configurable distance curve defaults to full volume through 112.5 units, fading through 750, and silent at 975 so a maximum-range hit remains audible. Pitch redundantly communicates hit distance using perceptually even logarithmic interpolation: the default is 600 Hz at contact, approximately 424 Hz halfway through the reach, and 300 Hz at maximum reach.

While mounted, the same nine-angle schedule becomes travel-relative: it uses
the hoverbike's current world velocity direction, falling back to the bike's
facing direction below one world unit per tick, while every impact remains
spatialized relative to the player's live camera. The swept cylinder uses the
bike's native radius, vertical bounds, position, and rooms and temporarily
excludes both the rider and mounted bike perimeters. Reach grows to one second
of current travel plus the bike radius, clamped between the configured walking
reach and three times that value; fade and silence thresholds expand so the
new far edge remains audible. Vehicle floor sampling extends to at most 1,200
units and reports only refined large drops, not walking stair, ramp, crouch, or
ladder semantics. A rejected native movement command also produces a dedicated
90 ms spatial falling cue from 220 to 140 Hz, rate-limited to once per 30
logical ticks while blocked.

When the selected collision prop is an active, healthy, mortal
`OBJFLAG_PATHBLOCKER`, the wall chirp becomes a distinctive 90 ms falling
glissando. It begins at twice the normal distance pitch and resolves to that
distance pitch, preserving the learned near/far endpoint while identifying
glass or explodable scenery that the game marks as a possible route
obstruction. Ordinary glass and invincible, hidden, destroyed, or inactive
objects retain the normal barrier sound or disappear with their collision.

The same ray also samples the engine's walkable-floor height along a bounded connected path, by default through 450 world units or until an intervening barrier. Room traversal stays at the observer's body height while the separate vertical floor query starts 200 units above the current ground; this prevents a long upward-sloping portal trace from delaying recognition of stairs in an adjacent lower room. The first elevation change of at least 12 units replaces a farther wall cue for that angle. Once terrain is selected, at most ten fixed knots describe one uninterrupted spatial contour. It begins at the player's actual floor elevation, then logarithmically interpolates pitch between absolute sampled elevations: equal heights hold exactly the same pitch, while rises and descents change it proportionally at one octave per 240 world units, bounded to one octave in either direction. Gain interpolates between the configured attenuation at each knot's real distance, so the contour becomes quieter as it travels away. Long profiles are spread over the available knots while preserving a detected final landing. A separately known impassable barrier beyond the floor-query reach reserves the end of the phrase; after a 5 ms separation it uses the ordinary distance-pitched wall carrier with a harder second harmonic. Thus one ray can continuously trace `flat, descending, lower flat`, then report `wall`, without implying that the descent itself leads somewhere useful. The contour reports only the geometry observed along that ray; it does not claim that the route is open beyond the sampled endpoint. If the connected result is unavailable or uncertain, the established single 140 ms rising or falling terrain contour remains the fallback.

A missing floor or a downward change greater than `Accessibility.VirtualCaneDropHeightThreshold`, default 80 units, is a large-drop candidate. Five bounded floor probes refine the last-supported/first-unsupported interval, and at least two unsupported results are required to reject an isolated room-resolution miss. The resulting 260 ms steep falling contour is spatialized at the estimated floor-to-drop transition. It takes priority over a wall beyond the edge, while a nearer wall or railing still masks an unreachable drop. The drop threshold is configurable from 30 through 500 units.

For Joanna's walking perspective, a standing-height collision also receives one conditional sweep using her full-squat height and the same movement radius. This test still runs when a stair or other terrain sample is up to one player radius nearer than the standing barrier, preventing co-located rising terrain from masking an under-stair opening. The connected path additionally confirms that the lower clearance continues for at least two player radii beyond its first low node; short or uncertain paths fall back to the established local test. A confirmed cue is positioned one radius inside the opening. If the standing envelope is blocked but the squat envelope can move through the proven opening, the crouch passage takes priority over that co-located terrain and the ordinary wall chirp becomes two spatialized 55 ms chirps separated by 25 ms. The first uses the ordinary distance pitch and the second is lower by a factor of 1.5. A collision polygon carrying the engine's native ladder or player-only ladder flag instead produces a 220 ms continuous upward sweep at the collision point. It starts at the same lowered carrier used by rising terrain and reaches twice the ordinary distance pitch, making its vertical span and upper endpoint larger than the stair-up contour. When thin ladder geometry is embedded within an ordinary surrounding wall, the cane also tests the player-sized contact cylinder at the barrier using the engine's native ladder-overlap query and its secondary adjacent-room check. This uses the same geometry classification as player climbing and does not depend on stage-specific props or model identifiers. No player or collision state is modified. Large drops retain priority, and the crouch check is suppressed once the current collision envelope is already at full-squat height and for CamSpy or a hoverbike, which cannot crouch. Ten preallocated mixer slots—nine sweep angles plus one context slot used by the hoverbike blocked-movement cue—keep every cane cue independent of game sound channels and the other accessibility oscillator lanes.

Rising terrain receives an additional traversal-aware arbitration before it can mask a farther wall. For a local walking player, the cane tests the live animated collision envelope at each sampled raised floor, so standing, ducking, full squat, and transitions between them use their actual current head clearance. A rise is considered traversable only when every tested position through the selected terrain is volume-clear in that stance. If a traversable rise precedes a farther collision, the wall remains authoritative. If no wall follows, the rise is suppressed only when two floor samples show that it has settled onto a nearby plateau within the first quarter of terrain reach. This makes a short passable ledge before a fence opening resolve as wall cues on the fenced rays and silence through the opening rather than nine misleading terrain cues.

While Joanna is already standing on a steady ramp or hill, one read-only floor-normal query at the start of each sweep establishes the current surface plane in absolute world coordinates. Each later ray compares its sampled world-space floor positions with that plane, so movement during a sweep cannot mix a new player ground height with the old plane. A ray's ordinary slope contour cedes to a farther wall, or to silence in open space, only when all four floor samples continue along that plane within the configured terrain-height tolerance and any upward continuation is verified traversable in the live stance. Native step geometry, slope starts and ends, stairs, ledges, blocked climbs, and large drops retain their spatial ray cues. Continuing along the same surface plane is intentionally silent: the cane describes geometry ahead and no longer plays a separate centered cue for the slope beneath the player. This current-floor query occurs once per completed sweep and is intentionally limited to Joanna's ordinary walking movement.

A refined large-drop edge normally takes priority over a wall farther along the same ray. If the horizontal collision query finds that wall within one live player collision radius beyond the edge, however, the gap is too narrow for the player to reach as open space. The cane suppresses that inaccessible lower-floor result and reports the barrier instead. This prevents floor behind or beneath a wall from masquerading as a nearby ledge while retaining drop cues at open edges and across wider gaps.

The supplementary ladder overlap is accepted only when its face normal is predominantly horizontal, as required for a climbable vertical surface. Floor- and ceiling-facing flagged polygons remain ordinary barriers. This is a geometry invariant rather than a stage, model, room, or coordinate exception.

A crouch-only route that ends at a wall is announced only if it emerges into
standing-height space for at least two player radii before that wall. A route
that remains crouch-clear through the cane's complete terrain reach is also
eligible. A low recess with no recovered standing space remains an ordinary
barrier.

### Speaking and sonified compass

`Accessibility.CompassMode` is a session-sticky bounded integer with `0=off`, `1=speech and sonification`, and `2=sonification only`; it defaults to `1` for blind-user acceptance testing. During supported one-local-player gameplay, Shift+F4 cycles those modes in that order. The mode change itself receives a short spoken confirmation, including when entering sound-only or Off, but ongoing cardinal speech occurs only in mode 1. Plain F4 remains reserved for the virtual cane, Ctrl+F4 is ignored, and either Alt key prevents both accessibility commands so Alt+F4 remains an operating-system command.

The compass projects the active camera look vector onto the horizontal plane and follows the engine's native view-angle convention: world `+Z` is North, `-X` is East, `-Z` is South, and `+X` is West. Crossing a cardinal axis in either direction emits a centered 600 Hz click pattern: one 30 ms click for North, two for East, three for South, or four for West, with 65 ms gaps. Mode 1 requests the matching direction word at the same event as the first click; mode 2 emits only the clicks. A six-degree re-arm distance prevents small input jitter around an axis from repeating it. The active observer makes the reference follow the CamSpy perspective automatically. Menus, pause, cutscenes, death, unsupported player counts, stage transitions, feature disable, and shutdown stop pending clicks and discard the previous heading without changing the saved mode; gameplay resumes from a silent heading baseline.

The cardinal and mode phrases are isolated in the accessibility compass module as prototype accessibility vocabulary. They currently use English because the port has no accessibility-string catalog; localization requires moving that table into such a catalog rather than adding literals to gameplay hooks.

The following `pd.ini` values may be tuned without rebuilding; restart the game after editing them:

```ini
[Accessibility]
CompassMode=1
VirtualCaneReach=900.000000
VirtualCaneFullVolumeDistance=112.500000
VirtualCaneFadeDistance=750.000000
VirtualCaneMaximumAudibleDistance=975.000000
VirtualCaneNearFrequency=600.000000
VirtualCaneFarFrequency=300.000000
VirtualCaneTerrainReach=450.000000
VirtualCaneTerrainHeightThreshold=12.000000
VirtualCaneDropHeightThreshold=80.000000
VirtualCaneVolume=0.184000
EnemyFullVolumeDistance=4000.000000
EnemyFadeDistance=5500.000000
EnemyMaximumDistance=6000.000000
EnemyScopedFullVolumeDistance=6000.000000
EnemyScopedFadeDistance=9000.000000
EnemyScopedMaximumDistance=12000.000000
TargetingVolume=0.150000
InterruptedTargetingVolume=0.120000
EnemyVolume=0.250000
EnemyFrequency=900.000000
AudibleMarkers=1
AuthoredLandmarks=1
MarkerRange=1200.000000
MarkerVolume=1.000000
```

Distances are world units. The effective full/fade/maximum values are normalized into nondecreasing order; the cane maximum-audible distance is also raised to at least its reach. Cane reach is bounded to 100–5,000, cane attenuation values to 0–10,000, and cane, solid-targeting, interrupted-targeting, and enemy volume to 0–0.4. Ordinary enemy distances are bounded to 6,000, scoped enemy distances to 20,000, and enemy frequency to 100–4,000 Hz. Non-finite values fall back to the documented defaults. The effective startup values are recorded in the accessibility session log.

Cane frequencies are bounded to 20–4,000 Hz. If the configured near frequency is lower than the far frequency, the effective endpoints are exchanged so closer obstacles remain higher pitched. Terrain reach is bounded to 50–2,000 units and its height threshold to 1–100 units. Marker range is bounded to 100–10,000 world units and its linear master multiplier to 0–4.

The samples use a shared active-observer pose. Ordinarily that is Joanna's movement cylinder; while the game is actually rendering the CamSpy camera, it is the CamSpy's prop, rooms, 26-unit collision radius, movement-height bounds, position, and look vector. CamSpy rays prepare destination rooms using the native CamSpy floor-room simplification and character-volume room expansion before collision, and temporarily exclude the CamSpy's own perimeter from each query. A perspective change cancels the old partial sweep and starts a fresh one from the new observer so no remaining angle is reported from the prior body.

One allocation-free aggregate `cane/sweep` record captures all nine scheduled results, live pose/direction, player bbox, requested endpoint, collision result, obstacle/geometry metadata, raw and audible position, distance, start/end frequency, terrain direction/height/distance/room/flags/query count, current floor point/normal and forward grade, per-ray plane residual/continuation/safety decisions, every floor probe's position/resolved-room list/floor room/height/delta/flags/rejection reason, distance attenuation, master and effective volume, pan, lateness, and diagnostic query time. Remote samples additionally record the CamSpy floor room, whether it simplified or expanded destination rooms, the observer/initial/final room lists, and whether the CamSpy perimeter was excluded. With `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`, one-second performance records also expose query totals/timing, missed work, requested/active cane masks, tone starts, stops, and mixer counters. Fast mode adds at most 36 bounded look-ahead floor queries plus one current-floor query per one-second sweep; long-session profiling must confirm that this remains within the existing cane timing thresholds. Iterative project-owner blind-user testing accepted the collision sweep, distance pitch, revised stair/ramp cues, and the cane/aim treatment of the captured DataDyne route-blocking pane. Surface-plane arbitration is an unaccepted prototype; broader breakable-path-blocker coverage, geometry coverage, masking, independent-user validation, and long-session stability remain open.

The broad-terrain sensor still groups adjacent audible rise or descent rays and records their transition and midpoint widths in `cane/terrain_width_sensor`, but it is now a diagnostic cross-check rather than the audio decision. Each confirmed connected terrain path performs a bounded lateral corridor query at its entrance, midpoint, and far end. A fourth, informational station samples at most one player radius beyond the verified trace, bounded by the path's stop distance. This short post-transition lookahead does not affect classification, but it can extend a surviving wall edge onto a landing while omitting an open side, giving a subtle indication of which way the route opens. Every side result distinguishes a hard collision barrier, unsupported edge, supported but substantially different-height terrain, open matching floor, and unknown query result. Only hard collision barriers or unsupported edges are audible boundaries; even a close, short, or isolated obstruction is retained rather than treated as open. Supported terrain—including rolling ground at another height—counts as lateral continuation rather than a wall, preventing outdoor contours from becoming false corridors. Classification-station boundaries use three binary refinements; the informational post-transition station uses one coarser refinement to stay comfortably within the hard query cap. Two stations with paired hard boundaries classify a bounded route. Any classification station with one hard boundary and positive continuation opposite it, or with a paired boundary, classifies a guided route so the positively observed sides can be traced independently. Two paired-continuation stations with no such hard evidence classify broad terrain. A swept-cylinder query whose destination-room lists disagree is retried once using the engine's portal-walk room traversal; only a second failure becomes unknown evidence. Missing rooms, repeated collision errors, time/query exhaustion, and evidence without any positively located hard side remain uncertain rather than open. Per-sample and performance records expose retry counts separately from unresolved errors.

Bounded and guided one-sided routes suppress the ordinary per-ray elevation contour. Broad and uncertain terrain audio is held until all nine rays have been classified. Adjacent broad, uncertain, guided, and bounded rays with the same elevation direction form one candidate route family; if any member proves a structured route, every non-structured contour in that family is suppressed. A group without structured evidence retains only its most central spatial contour, at 65% volume for uncertain evidence, so incomplete geometry queries cannot erase unrelated navigation information and broad terrain cannot recreate the obsolete left-to-right elevation sweep. A shallow change of less than one player width that terminates at a wall within four player widths is suppressed when its corridor evidence remains uncertain; this narrow dead-end heuristic is provisional pending blind-user testing. A gap or an up/down change begins another group. After all nine directional samples, one proven route family receives a dedicated sequence of 40 ms spatial boundary clicks. It no longer selects one ray to describe both edges. Instead, it merges the family by choosing the most continuous positively observed left boundary from the outward-left ray and the corresponding right boundary from the outward-right ray. It traces those edges independently: all available left stations near-to-far from a 330 Hz baseline, followed by all available right stations from a 440 Hz baseline. Consequently, small lateral movements cannot flip the whole route between competing left- and right-ray interpretations, and a staircase whose landing continues beside a left wall but opens on the right traces both sides down the stairs, continues along the left landing wall, and stops the right trace where that boundary ends. Even a close boundary remains in the sequence. Open floor and supported different-height terrain are continuation evidence and never play as walls. When a sweep sees multiple route families, successive sweeps round-robin them without allowing intervening single-route sweeps to reset the cursor, so a fork can expose every branch without overlapping boundary sequences. Both edge traces apply the same exponential pitch movement from the measured elevation, so rising and descending contours remain available without the louder original sweep. Each click independently derives its volume and pan from the distance and world position of that boundary sample. The sequence occupies the sweep's ending pause without consuming any of the nine directional cane channels; any unrelated contour tail is temporarily reduced to 20% while the boundary sequence sounds. A connected terrain result also takes precedence over contradictory legacy crouch detection; a path whose semantic result is genuinely crouch remains a crouch cue. Connected descents validated while rejecting a false drop participate in the same corridor classification as ordinary confirmed terrain. Positively broad, shallow terrain is also silent. A maximum path elevation excursion of at least two player widths keeps one representative contour at full volume; on the peripheral `-80` and `80` degree side-passage sentinels the same substantial broad route is capped at 65% volume. Drops remain audible at every angle. `cane/sweep` records boundary kind, route group/count/member count, the selected left/right source angles, every station, whether it is post-transition, side classification, query and retry counts/time, `terrain_audio_held`, applied audio scale, suppression decision, and which route received the boundary sequence. With advanced performance diagnostics enabled, `performance/cane_corridor_window` reports corridor calls, queries, retries, timing, budget stops, unresolved errors, bounded/guided/broad classifications, and suppressions.

### Player-authored audible markers

`Accessibility.AudibleMarkers=1` enables four temporary landmark slots for
single-player missions and one-local-player Combat Simulator sessions. F9
through F12 place slots one through four at the active camera position.
Pressing an occupied slot key moves it; Shift plus the key removes it. Alt- and
Control-modified shortcuts are ignored. The slots are deliberately not saved
to a profile and clear on stage exit or restart.

An audible marker combines two continuous opposed sweeps, 300 to 600 Hz and
600 to 300 Hz over two seconds, with an 800 Hz identity pattern every two
seconds. Slots one through four use one through four chirps. The four bases
have dedicated preallocated voices, while a shared scheduler staggers identity
patterns by at least 500 ms. Identity chirps retain a short 35 ms tone but now
use a 75 ms gap so slots three and four are easier to count. Removal repeats
the slot identity at center and adds a 400 Hz deletion chirp.

Markers sound only within the configured range and when the engine reports an
unobstructed ray from the active observer camera to the stored point. The ray
tests background walls and doors using the normal sight/shoot blocking flags.
A marker behind a wall or closed door is silent, but it may sound behind the
player or outside the viewport when the direct path is clear. Placement,
spatialization, range, and line of sight switch to the CamSpy while that
perspective is active and return to Joanna seamlessly afterward.

Menus, pause, dialogs, cutscenes, death, unsupported multiplayer, stage
teardown, feature disable, and shutdown silence marker voices. Temporary
presentation states preserve marker locations. This engineering
implementation builds successfully but still requires runtime and blind-user
acceptance for sound identity, masking, line-of-sight transitions, preferred
range, CamSpy behavior, and long-session performance.

### Authored navigation landmarks

`Accessibility.AuthoredLandmarks=1` enables semantic navigation destinations
that a stage explicitly registers. These are separate from F5 interactable
objects and from the four F9–F12 player markers. Each uses the same continuous
opposed 300–600 Hz sweeps as a player marker but never plays an 800 Hz identity
chirp: no chirps consistently means a level-authored landmark.

Registry entries currently cover Area 51: Rescue's intact silver-X wall, where
the hovercrate is intended to be positioned, Air Base's baggage conveyor,
where the equipped suitcase is deposited, the three shield-system consoles
on the Attack Ship, and the three shuffled target-amplifier pillars in Skedar
Ruins. They follow the shared active
observer, use `Accessibility.MarkerRange` and `Accessibility.MarkerVolume`,
and require a direct visual line of sight. The wall stops when destroyed; the
conveyor sounds only while the difficulty includes the "Check in equipment"
objective and that objective remains incomplete. The three shield consoles
sound only while "Disable shield system" remains incomplete, stop individually
when destroyed, and enter audibility 250 ms apart so colocated consoles remain
spatially distinguishable. Each Skedar Ruins pillar sounds while "Identify
temple targets" remains incomplete and its corresponding authoritative
per-pillar stage flag is clear. A pillar stops individually as soon as the
level accepts its Target Amplifier. Pillar identity follows the level's live
destination-tag remapping rather than the chosen object's original source tag,
so all random selections use the same behavior. The accessibility R-Tracker also suppresses
that completed pillar even though the solo level script leaves its native
yellow-tracker object flag set. For a tagged object whose setup origin is embedded
in its mounting geometry, the LOS query tests the nearest model-box face and
four inset surface points; render state by itself is never sufficient.
Registry ownership also excludes these props from
F5, so their native activation flags cannot misleadingly describe them as
ordinary interactables. Authored landmarks do not require F5,
consume interactable-scanner results, imply normal activation, or create a
targeting lock. Four dedicated mixer voices are preallocated; adding a landmark
requires a stage, setup tag, expected prop type, optional objective gate,
optional per-object completion flag, and stable semantic name in the registry
rather than new playback logic.

### View orientation recovery

The PC default control scheme maps both the keyboard End key and an Xbox-compatible controller's right thumbstick click (R3) to the configurable `Reset View` action. Pressing either during active gameplay immediately sets the camera's vertical look angle to horizontal without changing the player's compass heading. It also clears residual vertical-look and automatic-centering state so the camera does not continue drifting after the reset. The action is available in the extended controls binding menu under `Reset View`; the N64 default scheme leaves it unbound.

### Fine aiming tone

The firing-range targeting alignment lane uses a centered generated sine wave instead of the former repeated `SFX_0007` cue. It activates automatically only while the game reports that the reticle is over a visible, currently shootable range target. The adapter reuses the exact non-random query-ray hit position produced while calculating `lookingatprop`; it does not include weapon spread or cast an additional ray. The range's existing scoring model measures that point's distance from the target center, with bullseye/ring boundaries at 18, 37, and 56 world units. Accessibility converts the same distance into a continuous quality value across the approximately 75-unit scoring radius. Center produces the highest pitch. A 440 Hz reference and `1.5` to `3.0` pitch range produce 660–1320 Hz. The tone stops on aim loss, a back-facing target, scope loss, pause/menu transitions, feature disable, stage teardown, or shutdown. The backend preserves oscillator phase, interpolates frequency through each audio buffer, applies a 10 ms gain ramp, and uses one fixed buffer with no runtime allocation or game sound handle. A separate 100 ms enveloped chirp voice supplies beacon pulses without interrupting the continuous aiming lane. There is no F9 prototype binding.

### Special-device target tone

During the Carrington Institute Data Uplink, ECM Mine, and Door Decoder exercises, equipping the exercise device enables a dedicated device profile in the existing targeting system. The profile compares the engine's non-shooting aim ray with the exact tagged object used by the exercise success script: terminal tag `0x30` for the Uplink, hub tag `0x32` for the ECM Mine, and door panel tag `0x35` for the Door Decoder. The same registry maps the Data Uplink to terminal tag `0x0a` in DataDyne Research: Investigation; maps the mission-renamed Data Uplink/Reprogrammer to hovercab tag `0x0a` in Chicago; maps the Door Decoder to safe-keypad tag `0x11` in G5 Building; and maps the ECM Mine to internal security hub tag `0x03` and external communications hub tag `0x04` in DataDyne Central: Defection. Pointing at an active, visible registered semantic target while its matching item is equipped produces the same fixed 660 Hz centered alignment tone used for a valid combat target. Wrong terminals, hubs, vehicles, panels, scenery, and other interactable objects remain silent. The profile has no positioned presence cue and does not expose the target while it is merely nearby or off aim.

For the Uplink and Door Decoder, the tone identifies the terminal or panel accepted by the exercise; the player must still satisfy the game's normal interaction requirements. For the ECM Mine it identifies the correct destination surface, not a guaranteed ballistic landing point. The game determines correctness only after the thrown mine embeds, so throw angle, trajectory, intervening geometry, and impact remain authoritative. The data-driven registry supports multiple targets per item and stage-scoped or training-scoped rules, so another special item with a confirmed semantic target can be added without changing the targeting core. This populated set contains only contracts confirmed in gameplay code and does not invent weapon-style aim targets for vision modes, the Tracker, disguises, or cloaking devices whose exercises use discovery, pickup, or passive-state semantics.

The CamSpy uses the same alignment-only device profile while its remote camera and shutter are ready. It discovers targets from the engine's active holograph-objective criteria rather than object names or models. The fixed 660 Hz targeting tone sounds whenever the normal photograph rule can accept a target: the criterion is incomplete, the tagged object is healthy and rendered in front of the CamSpy, its horizontal distance is under 400 units, and its complete projected bounds are inside the viewport. CamSpy photography is frame-based rather than ray-based, so it deliberately does not require the weapon query ray to hit the object. If several photograph criteria are simultaneously valid, the one closest to screen center supplies the stable targeting identity. The tone stops when every target fails those conditions, after criterion completion, or when the player leaves the CamSpy view. It does not identify arbitrary characters or scenery as valid photographs and has no off-frame presence cue.

Perfect Dark is a fast first-person game whose original interface communicates heavily through graphics, spatial audio, motion, timing, and implicit world knowledge. A useful accessibility layer must expose the engine's own semantic state and provide ways to act on it. Merely reading every piece of on-screen text would leave core tasks inaccessible.

## Initial users and scope

The primary users for the first milestones are:

- blind players who use speech and audio cues;
- low-vision players who benefit from explicit, repeatable state announcements;
- screen-reader users who need predictable focus, labels, values, and interruption behaviour;
- testers and developers who need detailed event evidence.

Initial work focuses on Windows because a practical speech path is required for the first end-to-end proof. The semantic core should remain platform-independent so other backends can be added without rewriting game hooks.

The first useful slice is startup through the main menus. Later slices cover HUD messages and objectives, player status, targeting, interaction scanning, navigation experiments, and one bounded training or campaign segment.

## Not in the initial phase

- Claiming that the full campaign, multiplayer, or every menu is accessible.
- Computer vision, OCR, or pixel scraping as the primary interface to known engine state.
- Automated aiming, invulnerability, enemy AI changes, or mission simplification by default.
- Distributing ROMs, extracted assets, generated game data, or copyrighted text/audio.
- Replacing the existing audio, input, menu, localization, or configuration systems.
- Committing to one Windows speech technology before a small backend experiment measures availability, latency, cancellation, deployment, and failure behaviour.
- Treating a successful build or a sighted developer walkthrough as proof of accessibility.

## Design principles

1. **Semantics before pixels.** Read a resolved menu label or objective state, not screen coordinates or glyphs.
2. **Nonvisual interaction, not spoken screenshots.** A user needs reliable focus, commands, state, orientation, and recovery paths—not a stream of visual descriptions.
3. **Small upstream hooks.** Established systems publish narrow facts; accessibility modules decide what, when, and how to announce.
4. **Platform separation.** Gameplay code emits platform-neutral events. A backend owns Windows speech integration.
5. **User control.** Speech, logging, verbosity, categories, repeat commands, and cue volume need explicit settings and useful defaults.
6. **Calm output.** Priorities, replacement, deduplication, throttling, and expiry prevent speech from becoming another barrier.
7. **Deterministic recovery.** Users must be able to repeat current focus/state, learn context, silence speech, and return to a known state.
8. **Preserve game behaviour.** With accessibility disabled, hooks should be inert and game logic should remain unchanged.
9. **Evidence over confidence.** Separate build, runtime, speech-output, task-completion, and independent-user evidence.
10. **Blind testing early.** Short tests after each user-visible increment are more valuable than a large untested feature dump.
11. **Diagnostic-first logging.** Keep logging local and configurable, but enable comprehensive evidence by default during acceptance testing and record all feature-relevant state that can help explain accessibility behaviour.
12. **Legal boundaries.** The project consumes a user-supplied supported ROM and never ships protected game content.

## Capability areas

### Menus and settings

Announce dialog context, focused control, role, value, availability, relevant hints, and explicitly provided summaries for important non-focusable content. Support repeat and predictable handling of lists, sliders, checkboxes, dropdowns, scrollable briefing text, and dynamic labels.

### Messages, dialogue, and objectives

Expose every semantic HUD-message event needed for equal access without inheriting visual duplicate/capacity suppression; keep dialogue and subtitles on their separate path. Announce objective state changes with priority and allow the current objective list and briefing to be queried.

### Player status and inventory

`Accessibility.PlayerStatus` defaults to `1` and is subordinate to
`Accessibility.Enabled`. During unobscured one-local-player gameplay, F1
interrupts stale speech with a compact semantic report. Health is always first;
nonzero shields follow. A Combat Simulator report then adds team identity when
applicable, player-visible scenario state, score, rank, score limit, and match
time. Timed matches report time remaining; unlimited matches report elapsed
time. The query does not repeat the automatic one-minute HUD message or
last-ten-seconds alarm, and it does not report device telemetry.

In a team Combat Simulator game, Shift+F1 reports team name, team score and
rank, aggregate enemy kills and deaths, leader gap, player-visible shared
scenario state, team score limit, and match time. Shift+F1 produces no speech
outside a team scenario. Both shortcuts are ignored in menus, pause, cutscenes,
death, split-screen play, and when Alt or Control is held. Scenario additions
mirror native visibility: private progress/countdowns are only reported to the
player who receives them visually, while Capture the Case carrier details
require the native Show on Radar option.

Later work may expand deliberate inventory queries and add carefully tested
automatic health thresholds. Reserve unsolicited announcements for important
changes and use hysteresis to avoid chatter.

### Targeting and interaction

Describe the current valid target and nearby actionable objects from semantic game objects. Convey friendly/hostile/unknown status only when the game can determine it. Avoid leaking hidden information.

### Navigation and orientation

Experiment with headings, landmark scans, route cues, distance bands, room connectivity, and recovery commands. Navigation must respect what the player could reasonably know and must be evaluated in one controlled space before broad use.

### Presentation and low vision

Later work may add scalable text, contrast controls, reduced motion, cue balancing, and other visual options. These should share semantic settings where appropriate without coupling them to speech availability.

## Development and playtest loop

Each increment should follow this loop:

1. Identify one user task and the semantic state required to complete it.
2. Trace the owning engine systems and record uncertainty.
3. Add the smallest event or query boundary and structured log evidence.
4. Test build, startup, backend failure, output ordering, and regression behaviour.
5. Run a short blind or screen-reader-dependent task with a defined start and success condition.
6. Correlate tester observations with the session log, fix the highest-impact barrier, and repeat.
7. Expand scope only when the bounded task is reliable and recoverable.

Development logs may contain resolved text, names, paths, arguments, positions, input events, identifiers, pointers, and detailed game state when useful. They must never contain ROM contents, extracted copyrighted assets, passwords, authentication tokens, or unrelated operating-system secrets, and they are never uploaded automatically. Testers should be told that the diagnostic log is comprehensive before choosing whether to share it.

During gameplay, press Shift+F2 to save a session-local diagnostic capture to `$S/accessibility.log`. F2 alone and Control- or Alt-modified variants do nothing. The capture includes a fixed, approximately 15-second history sampled four times per second; active-observer position, camera, look direction, room, keyboard/mouse and all four controller states; current stage, player, health, shields, devices, weapons, inventory, objectives and their raw semantic requirements; scanner membership and scheduling state; active world props within 6,000 units; and the first collision blocker directly under the current aim direction. It also audits up to 16 scanner-supported props inside the scanner radius and a bounded forward focus cone. Each `incident/beacon_candidate` record reports the actual category path, semantic and pickup predicates, category state, cached pre-render projection age, capacity/truncation, bounds and viewport decision, range, room relation, LOS result/sample/query count, and final inclusion or rejection reason without changing the live result set or schedule. It uses the CamSpy observer while that perspective is active. A spoken `Diagnostic capture N saved` confirmation supplies the capture number; report that number together with the log's session identifier because numbering restarts with each process. If accessibility logging is unavailable, the command says so instead. The session-start record advertises the shortcut and history duration, and each capture-end record includes the main-thread collection time so an unexpectedly expensive dump is visible.

Temporary performance instrumentation can be compiled in with `-DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=ON`. It records one-second windows with render cadence, logical timing, process-memory totals, active combat/R-Tracker voice counts, and procedural-audio mixer states and counters; environmental-hazard and R-Tracker scan audits also gain rate-limited scan-duration statistics. It additionally times SDL event/dimension work, framebuffer maintenance/setup/composite work, display-list translation, frame limiting, `SDL_GL_SwapWindow`, and renderer completion. A fixed 180-frame history is copied into a fixed 480-frame episode buffer when three 50 ms frames, one 250 ms frame, or a one-second rate below 30 FPS is observed. Detailed records are written only after two recovered windows above 50 FPS or during orderly shutdown, protecting the lead-up without performing per-frame log I/O during a stall. The option defaults off for normal and acceptance builds. When enabled, instrumentation remains active whenever accessibility logging is enabled, including when top-level accessibility is disabled for a diagnostic control and after a training activity ends.

`tools/accessibility/capture_graphics_diagnostics.ps1` is an optional external Windows collector for process, DWM, NVIDIA, Windows GPU-counter, power-plan, configuration, and relevant event-log evidence. It discovers an already launched game, writes incrementally to the ignored `build/diagnostics/` tree, and copies no ROM or extracted asset data. `tools/accessibility/analyze_graphics_diagnostics.ps1` produces a compact Markdown report from either the older aggregate log or the new graphics records. The complete workflow and decision thresholds are in `documentation/GRAPHICS_SLOWDOWN_INVESTIGATION_PLAN.md`.

## Glossary

**Announcement**

A semantic text event offered to speech output, with priority, category, context, and queue policy. It is not necessarily spoken if disabled, superseded, deduplicated, or expired.

**Earcon**

A short non-speech sound with a documented meaning, such as target acquired or route correction. Earcons complement rather than replace queryable speech.

**Speech backend**

A platform-facing implementation that accepts normalized announcements and controls a speech technology. It must report availability and support safe initialization, cancellation, and shutdown.

**Accessibility hook**

A small call placed at an established semantic boundary so the accessibility subsystem can observe an event. Hooks must not contain speech policy or platform API calls.

**Scanner**

A user-triggered query that summarizes relevant nearby or directional entities from game state, subject to range, visibility/knowledge, priority, and rate limits.

**Navigation cue**

A speech or non-speech indication that helps the player orient, follow a route, or recover from deviation without taking control away.

**Playtest log**

A development-focused sequence of accessibility events and decisions used to correlate a tester's experience with detailed internal state. It is enabled by default for the current acceptance-testing phase, remains configurable, is separate from the general engine log, and may intentionally contain raw diagnostics.

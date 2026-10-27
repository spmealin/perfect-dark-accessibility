# Audio glossary: initial version and acceptance checklist

Implemented on 2026-10-15 for v0.92. Project-owner blind-user acceptance covers
the basic glossary workflow and the follow-up selection-beep, friendly-pattern
and Back-navigation changes. Recognition of every entry and independent-user
validation remain open.

## Player workflow

Open Extended Options → Accessibility → Audio glossary. The Accessibility
entry follows Audio, leaving Video as the existing first Extended Options item.
The native default dialog, font, scrolling-list renderer and controller/keyboard
activation are retained. Sound-preview activation suppresses the selection beep;
navigation, Back and other menus retain their normal sounds. Both dialogs include the
existing localized Back control. In the glossary, Back is the final list row so
the native list's wrapping up/down navigation can reach it; the Accessibility
parent retains its usual separate Back button. Cancel/back input also exits.

Focus announces an entry without previewing. Activation plays a single bounded
example; repeat activation restarts it. Continuous cues demonstrate a brief
phrase, at most 2.2 seconds except the friendly-combat-character example, which
plays two full on/off cycles of its upper note over four seconds.
Moving focus, focusing Back, cancel, dialog close,
stage teardown and shutdown stop playback. Samples are synthetic scenarios
rendered by the real procedural mixer, not asset recordings or live world scans.
No scanner mode, feature switch, profile, marker or INI value is modified.

Configured volumes and enemy/cane carrier pitches apply. Feature-specific
scanner switches do not prevent a deliberate preview; global accessibility
disablement and zero configured volume still produce silence. X-Ray has no
unique cue: it exposes objects to their normal scanners. Spoken announcements,
dialogue and ordinary game/menu effects are not separate glossary entries.
Names are English-only accessibility vocabulary pending an accessibility-owned
localization catalog; no ROM/generated language data is edited.

## Sound inventory (93 entries)

- Door
- Interactable object
- Pickup item
- Movable puzzle rock
- Neutral or protected person
- Friendly combat character
- Enemy: distant
- Enemy: nearby
- Enemy: melee range
- Enemy: above your aim
- Enemy: below your aim
- Precise aim: target left
- Precise aim: target right
- Security camera
- Firing-range target
- Enemy targeted: body
- Enemy targeted: head
- Enemy targeted: arm
- Protected person targeted: body
- Protected person targeted: head
- Protected person targeted: arm
- Destructible object targeted
- Enemy targeted through glass
- Protected target through glass
- Valid special-device target
- Range aim: outer scoring area
- Range aim: bullseye
- Threat detector: new contact
- Threat detector: targeted
- Cane: nearby barrier
- Cane: distant barrier
- Cane: uncertain rise
- Cane: uncertain descent
- Cane: ladder
- Cane: crouch passage
- Cane: isolated drop
- Cane: rising floor contour
- Cane: descending floor contour
- Cane: contour ending at wall
- Stair boundaries: ascending
- Stair boundaries: descending
- Dangerous drop-edge warning
- Hoverbike blocked
- Environmental laser hazard
- Incoming rocket: distant
- Incoming rocket: urgent
- R-Tracker: mission object
- R-Tracker: character
- R-Tracker: cheat item
- R-Tracker: above
- R-Tracker: below
- R-Tracker: behind
- IR Scanner highlighted object
- Combat radar: enemy
- Combat radar: ally
- Combat radar: objective
- Combat radar: other contact
- Combat radar: above
- Combat radar: below
- Combat radar: behind
- Combat radar: pulse start
- Combat radar: no contacts
- Combat radar: unavailable
- Hill: center beacon
- Hill: scoring
- Hill: behind
- Hill: radar direction guide
- Player marker 1
- Player marker 2
- Player marker 3
- Player marker 4
- Remove player marker 1
- Remove player marker 2
- Remove player marker 3
- Remove player marker 4
- Mission objective landmark
- Vulnerable boss spike
- Intact Battle Shrine pillar
- Scanner enabled
- Scanner disabled
- Cane slow mode
- Cane fast mode
- Cane off
- Compass: north
- Compass: east
- Compass: south
- Compass: west
- Standing
- Crouching
- Double-crouching
- Weapon primary function
- Weapon secondary function
- X-Ray puzzle: reset switch

## Implementation and boundaries

- Core catalog, simulated inputs and preview timing:
  `src/accessibility/accessibility_glossary.c` and its shared header.
- Native menu definitions and thin list/dialog callbacks:
  `port/src/accessibility/accessibility_options.c` and its port header.
- `port/src/optionsmenu.c`: one link to the new dialog.
- `port/src/accessibility/accessibility_tone.c`: main-thread exclusive preview
  command ownership, existing DSP reuse, clean audio-owned transition state,
  and a sample-clock deadline that fades only the accessibility contribution.
  Ordinary gameplay commands are ignored only during an active preview.
- `src/accessibility/accessibility.c` and `port/src/pdmain.c`: cancel before
  shutdown/stage teardown so preview ownership cannot suppress subsequent play.
- `CMakeLists.txt`: register the new core source; port sources use the existing
  glob. No vendored, generated, ROM or gameplay-policy files are changed.

Storage is fixed. Previews add no world queries, native game sound channels,
dynamic allocations, background logging or speech-backend calls. Outside a
preview the mixer command guards are constant-time branches.

## Evidence collected

- Windows x86-64, ntsc-final, RelWithDebInfo built through MinGW64 with
  `ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=OFF`.
- `tools/tests/accessibility_glossary_test.c` passed all 92 entries with
  performance diagnostics both off and on. It exercises the real native list
  callbacks and actual DSP with synthetic state: non-silent PCM, unique labels,
  menu-time suppression isolation, cancellation, replacement, disabled state,
  and independent sample-clock expiry without menu ticks.
- Existing cane-result and cane-path fixtures passed.
- The adjacent developer `pd.ini` has global accessibility, speech, logging,
  menu narration and every implemented feature switch enabled. Its saved cane
  mode remains Fast; scanner preferences were not changed.
- Existing session `1791737787` confirms those effective enabled feature states
  in `lifecycle/session_start` and `lifecycle/scanner_start_state`. This predates
  the glossary and is configuration evidence only, not glossary runtime evidence.
- Development automation did not launch a new game session or overwrite the
  tester's log. Subsequent owner reports confirmed the working glossary,
  requested the selection-beep fix, and confirmed the two-cycle friendly drone
  and reachable Back entry on 2026-10-15. Those reports are task-based acceptance
  evidence, not a complete 92-entry recognition, visual-layout or long-session
  performance test. No specific acceptance session/capture ID was supplied.

## Reproduce the PCM test

After configuring the game, run in MinGW64:

```sh
cc @build/CMakeFiles/pd.dir/includes_C.rsp \
  -DPLATFORM_64BIT=1 -DVERSION=2 -DPAL=0 -D_LANGUAGE_C=1 \
  -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0 -std=c11 -O2 \
  tools/tests/accessibility_glossary_test.c \
  -o build/accessibility_glossary_test.exe -lSDL2 -lm
build/accessibility_glossary_test.exe
```

Repeat with `-DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=1` for the diagnostic path.
This is a PCM/control regression test, not a listening or FPS benchmark.

## Player acceptance tests

1. From the main menu and a paused mission, find the new menu using the normal
   controller and keyboard paths. Confirm font, sizing, scrolling, full spoken
   names, remembered list position and Back/cancel behavior.
2. Browse without activation: only menu narration should occur. Activate Door,
   Interactable object and Pickup item; confirm one recognizable example each.
3. Replay rapidly, switch between a long drone and a short chirp, move focus,
   press Back during playback and resume gameplay. No stuck, overlapping or
   missing gameplay cues should remain. Saved scanner and cane choices must
   remain unchanged.
4. Compare enemy body/head/arm, interrupted destructible locks, through-glass
   locks and nearby/distant presence against familiar gameplay sounds.
5. Compare marker IDs 1–4 and removal, hill scoring, tracker height/rear patterns,
   stair boundaries, drop warnings and laser movement. Confirm enough time to
   recognize each pattern without needing automatic loops.
6. Browse every entry. Report any silence with nonzero configured volume,
   misleading label, clipped visual text or incorrect pattern. Preview activation
   must not play the native selection beep; other menu actions should still do so.

Logs use `glossary/play` and `glossary/stop`; collect the session ID and entry
name for failures. No new key binding or INI setting is required.

## Rollback and limitations

The feature ships in v0.92. Roll back by running the previous v0.91 package or
reverting only this feature's source/documentation/build registration changes
and rebuilding; no save/config migration is involved. Preserve personal saves
and configuration, and do not discard unrelated working-tree changes.
Main-loop frame cost and long-session listening have not
been measured. Synthetic contours illustrate cue vocabulary, not a promised
safe route. Future sound-policy changes must keep these example inputs and
the catalog inventory synchronized with gameplay.

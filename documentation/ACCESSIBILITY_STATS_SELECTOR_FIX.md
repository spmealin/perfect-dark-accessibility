# Combat Simulator statistics player selector

## Evidence

Owner report: a menu was not reading. Capture 1 in local session `1793402493`
was saved while a Combat Simulator results menu was open. Immediately before
the capture, `menu/snapshot_changed` reported `Stats for Player 1`, type 15
(`MENUITEMTYPE_PLAYERSTATS`), `dimmed=1`, `subindex=0`, `selected=0` and
`count=5`; it continued describing Player 1's statistics. No highlighted-row
changes were announced before the capture.

The native control uses `menuitemDropdownTick` and `menuitemDropdownOverlay`.
The highlighted row lives in `itemdata->dropdown.list.index`; the selected
player remains unchanged until confirmation. Accessibility was observing only
the committed selected index, unlike its ordinary dropdown support.

## Fix

`src/accessibility/accessibility_menu.c` now reads the highlighted player name
and list position while the statistics selector is open, describes it as an
open dropdown and does not request the old player's statistics. Confirmation
or cancellation returns to the authoritative selected player's full stats.
Names still come from the native localized/name handler. This reuses existing
English role/open descriptions and their existing localization limitation.
Both pause and end-of-match statistics controls benefit. No native game file,
hook, setting, shortcut or selection behavior changes.

## Verification and handoff

The synthetic `menu_stats` fixture covers all five rows from the captured
shape, relevant-snapshot changes, composed open-dropdown speech, absence of
stale summaries, confirmation and cancellation. It runs with diagnostics off
and on. This is not runtime or screen-reader acceptance evidence.
All 28 suite tests passed, and the MinGW64 Windows x64 `ntsc-final`
RelWithDebInfo game build succeeded with performance diagnostics off.

Test in Combat Simulator results or pause statistics: open the player name
selector, move through players/simulants, confirm one and then try cancelling
another selection. Each highlighted name should read; closing should restore
the selected player's stats. No game was launched by the agent, preserving
the reported session log. Rollback is the menu-fix commit only; the separate
uncommitted precision-aim experiment is not part of this fix.

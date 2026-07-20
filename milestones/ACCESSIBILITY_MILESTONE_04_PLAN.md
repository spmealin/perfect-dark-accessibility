# Milestone 4 completed record: menu-agnostic focus narration

This document began as the implementation handoff for Milestone 4 of `ACCESSIBILITY_ROADMAP.md` and now records the completed implementation, evidence, acceptance refinements, and remaining regression opportunities.

Plan status: **complete**

Planning baseline: branch `accessibility`, commit `3268e3d7a`

Implementation status: **complete and accepted by the project owner after blind-user testing**

## Outcome

With accessibility, speech, and menu narration explicitly enabled, the game will announce the active dialog and the final focused control. The same observer and semantic resolver must work across startup, file management, main-menu siblings, options, pause menus, multiplayer setup, training menus, and later dialogs. It must not identify a particular menu definition and special-case its navigation route.

The required first end-to-end path is:

1. Start with no selected agent and reach the `Perfect Dark` file-selection dialog.
2. Hear the focused custom list row `New Agent...`.
3. Activate it and hear `Enter Agent Name` plus the keyboard's focused key.
4. Enter a valid agent name, complete the lawful save-location flow when one appears, and reach `Perfect Menu`.
5. Move to the `Options` sibling and navigate Audio, Video, Control, Display, and the PC-only Extended settings route.
6. Change representative checkbox, slider, dropdown, and list values and hear the resulting state.
7. Repeat the current menu announcement and cancel speech without changing game focus.

The architecture supports every focusable control family found in the current source tree. Completion claims remain narrower: this milestone validates the route above and a type-focused test matrix, not every screen or every game mode.

## Owner decisions locked by this plan

1. **Observe final state, not individual input events.** Add one call after `menuProcessInput()` for each menu player. This sees the final dialog, final pre-focus, mouse focus, directional focus, automatic disabled-item skips, sibling swipes, push/pop, and value changes without hooks in every transition function.
2. **Keep narration menu-agnostic.** The core may switch on menu item type, but it must not compare against `g_FilemgrFileSelectMenuDialog`, `g_CiOptionsViaPcMenuDialog`, or any other dialog/item address to choose spoken text or behavior.
3. **Support all current focusable control families.** Selectable, list, scrollable, slider, checkbox, dropdown, keyboard, ranking, player stats, and carousel all receive a defined semantic resolver. Numbered/unknown types are audited and fail safely if later introduced.
4. **Do not announce presentation items as focus controls.** Labels, objectives panels, separators, models, meters, marquees, controller diagrams, color boxes, and the known nonfocusable numbered types contribute no independent focus event. The dialog title supplies basic context. Long-form objectives and richer briefing reading remain Milestone 6.
5. **Extend custom controls through one generic semantic operation.** Custom-rendered list rows and controls whose visible value is otherwise available only to rendering implement `MENUOP_GETACCESSIBILITYTEXT`. The accessibility module never recreates a screen's business rules or reads pixels.
6. **Copy callback text immediately.** Menu text callbacks frequently return language buffers, globals, or mutable scratch storage. Every title, label, option, group, and value must be copied into accessibility-owned UTF-8 storage before a second callback or backend call.
7. **Use replacement, not an unbounded speech queue.** Dialog/focus/value announcements are one replaceable `menu` group. A newer menu state interrupts stale speech. The current normalized snapshot remains available for repeat.
8. **Keep native speech out of the menu engine.** `src/game/menutick.c` publishes a semantic observation only. Resolution, comparison, announcement composition, logging, and speech policy live under `src/accessibility`.
9. **Add one independently configurable feature setting.** Register `Accessibility.MenuNarration`. It has no effect unless `Accessibility.Enabled=1`; speech still separately requires `Accessibility.SpeechEnabled=1`. The original implementation defaulted it off, but the project owner changed all four accessibility defaults to on when blind-user acceptance testing began.
10. **Use provisional PC development commands.** While a menu is open and menu narration is enabled, F5 repeats the current dialog/focus snapshot and F6 cancels speech. They do not activate, close, or move a game menu. Milestone 5 replaces these with discoverable, configurable, conflict-checked accessibility actions. Do not add a controller chord in Milestone 4.
11. **Route speech to one owner initially.** Maintain observation state for every menu slot and log all slots, but speak only menu slot 0 in Milestone 4. This avoids simultaneous local-player chatter. Multi-player ownership policy remains an explicit later decision.
12. **Log all feature-relevant detail.** Logging remains opt-in and local, but there is no privacy-minimization requirement for development diagnostics. Record raw pointers, identifiers, full resolved text, input/source data, state, decisions, and timings when useful. Never record ROM contents, extracted assets, passwords, tokens, or unrelated operating-system secrets.

## Definition of done

Milestone 4 is engineering-complete only when all of the following are true:

1. `Accessibility.MenuNarration` defaults to one for acceptance testing and remains inert unless the coordinator is enabled.
2. No menu observation, handler query, speech request, or F5/F6 consumption occurs when menu narration is disabled.
3. One stable post-`menuProcessInput` observation sees final focus for keyboard, controller, mouse, dialog push/pop, sibling swipe, and automatic focus correction.
4. Dialog title and focused semantics are combined into one initial announcement, so a provisional first item cannot be spoken before `MENUOP_CHECKPREFOCUSED` settles.
5. Every current focusable menu item type has a documented and implemented resolver.
6. Every current focusable custom-rendered list handler supplies semantic row text, or an automated audit fails with the exact uncovered source/handler.
7. The startup list announces `New Agent...` through the generic custom-control contract, not by recognizing the startup dialog.
8. The agent-name keyboard announces its current key, entered string changes, caps state changes, Delete, Caps, Cancel, and OK; unavailable OK is not described as successful.
9. Focus announcements include the best available label, role, value/state, availability, and position/count without inventing missing text.
10. Slider, checkbox, dropdown, list, carousel, ranking, player-stats, keyboard, and scrollable substate changes announce a concise current value and replace stale output.
11. Unchanged frames do not generate speech or repeated semantic handler calls beyond the bounded observation contract.
12. F5 repeats the current combined dialog/focus announcement even when deduplication would suppress it. F6 cancels current speech without mutating focus or dialog state.
13. Empty, null, invalid, overlong, callback-failure, unsupported-type, and backend-unavailable cases are nonfatal and comprehensively logged.
14. The exact MinGW64 build succeeds and produces `build/pd.x86_64.exe` with the required speech DLLs still beside it.
15. Accessibility-disabled behavior and existing menu input are regression-tested.
16. The source audit, automated harness, scripted runtime path, and independent blind-user result are reported separately. If no independent blind tester is available, report “engineering complete, accessibility validation pending.”
17. Documentation and the hook ledger match the implementation. No log, save, configuration, binary, ROM, or generated asset is committed.

## Explicit non-goals

- Do not claim that all Perfect Dark menus are independently validated or that the game is fully accessible.
- Do not hard-code the New Agent route, the main menu, an options dialog, item coordinates, or a sequence of input presses.
- Do not speak every visible label, separator, model, meter, color swatch, controller diagram, or marquee merely because it is rendered.
- Do not implement HUD, subtitle, objective-change, status, targeting, scanner, navigation, or gameplay narration.
- Do not implement rich long-document navigation, heading structure, or objective review; Milestone 6 owns those. Milestone 4 only exposes the current focusable scrollable control and its scroll state.
- Do not create the final accessibility options UI or permanent binding system; Milestone 5 owns discoverability and conflict handling.
- Do not add a background thread. Tolk remains main-thread-only and asynchronous internally.
- Do not add an unbounded FIFO or wait for speech completion before menu input proceeds.
- Do not change how the game chooses focus, enables controls, activates handlers, edits names, saves profiles, or navigates dialogs.
- Do not add new localized game-content strings. Role words and development hints may be English-only for this milestone and must be isolated for later localization.
- Do not refactor unrelated menu rendering or rename decompilation symbols.

## Confirmed repository facts

### Stable observation point

`src/game/menutick.c:menuTick` iterates `MAX_PLAYERS`, assigns `g_MpPlayerNum`, sets the corresponding current player when valid, calls `menuProcessInput()`, and then restores the prior current player. Add the observer immediately after `menuProcessInput()` and before restoring the prior current player. At that point:

- `g_MpPlayerNum` identifies the active menu slot;
- game callbacks see the same current-player context they see during menu processing;
- `g_Menus[g_MpPlayerNum].curdialog` is the final active dialog for that input pass;
- focus and control-local data have already changed;
- an opened dialog that is created later in the same outer tick may be observed on the next menu tick, which is acceptable and deterministic.

Do not place speech calls in `menuOpenDialog`, `dialogChangeItemFocus`, `dialogTick`, `menuSwipe`, or individual menu handlers. Those paths can expose intermediate state or cause duplicate announcements.

### Focus and item data

`menuOpenDialog` initializes item data, chooses `dialogFindFirstItem`, performs the `MENUOP_CHECKPREFOCUSED` scan, calls `MENUOP_FOCUS` on the winner, and opens the dialog. `dialogChangeItemFocus` centralizes directional and PC mouse focus. `dialogTick` also moves away from a disabled focused item.

The focused `struct menuitem` does not directly own its runtime state. `dialogFindItem` maps it to a row, and the row's `blockindex` maps into the current menu's `blocks`. Add a small query helper in `src/game/menu.c` rather than duplicating this private row/block lookup in the accessibility module:

```c
union menuitemdata *menuGetItemData(
		struct menudialog *dialog,
		struct menuitem *item);
```

The helper returns `NULL` for a missing item or `blockindex == -1`, performs no allocation, and does not modify focus or handler state. Declare it in `src/include/game/menu.h`.

### Current control inventory

A planning audit of static menu definitions found the following named types. Counts are source occurrences and are useful as an audit baseline, not a runtime coverage claim:

| Type | Approximate definitions | Focus behavior |
| --- | ---: | --- |
| `SELECTABLE` | 362 | Focusable action |
| `LABEL` | 209 | Presentation only |
| `CHECKBOX` | 189 | Focusable value |
| `SEPARATOR` | 172 | Presentation only |
| `DROPDOWN` | 83 | Focusable selector |
| `SLIDER` | 46 | Focusable value |
| `LIST` | 34 | Focusable collection |
| `SCROLLABLE` | 18 | Focusable only when content can scroll |
| `MARQUEE` | 13 | Presentation only |
| `MODEL` | 11 | Presentation only |
| `KEYBOARD` | 7 | Focusable text entry/grid |
| `OBJECTIVES` | 7 | Presentation/content panel |
| `CAROUSEL` | 4 | Focusable visual selector |
| `CONTROLLER` | 3 | Presentation diagram |
| `RANKING` | 2 | Focusable scrollable table |
| `PLAYERSTATS` | 1 | Focusable player/stat table |
| `COLORBOX` | 1, PC only | Presentation swatch |

The numbered focusable candidates `MENUITEMTYPE_03`, `_0A`, `_10`, `_14`, `_16`, and `_18` were not found in surveyed static definitions. The implementation must repeat this audit across preprocessing variants and retain a runtime unsupported-type log rather than assuming they can never appear.

### Startup and settings paths

`g_FilemgrFileSelectMenuDialog` has title `Perfect Dark`, a presentation label `Choose Your Reality`, and one custom-rendered list handled by `filemgrChooseAgentListMenuHandler`. Its final row is localized `L_OPTIONS_403`, `New Agent...`. The handler does not currently implement `MENUOP_GETOPTIONTEXT`, so the generic custom semantic operation is required.

Selecting that row pushes `g_FilemgrEnterNameMenuDialog`, titled `Enter Agent Name`, whose only focusable item is `MENUITEMTYPE_KEYBOARD`. The keyboard grid includes ordinary characters and localized Delete, Caps, Cancel, and OK cells. Physical keyboard entry also updates the same runtime string.

After an agent is available, `g_CiMenuViaPcMenuDialog` is the `Perfect Menu` route. Its Options sibling uses `g_CiOptionsMenuItems`, including Audio, Video, Control, Display, Cheats, Cinema, and the PC-only Extended dialog. These routes exercise standard selectables, checkboxes, sliders, dropdowns, lists, dynamic labels, and sibling navigation without requiring screen-specific narration.

## Public interfaces to add

Create `src/include/accessibility/accessibility_menu.h` with forward declarations for game menu structs and these main-thread-only calls:

```c
void accessibilityMenuObserve(
		s32 menu_slot,
		s32 player_num,
		s32 menu_root,
		s32 menu_depth,
		struct menu *menu);
void accessibilityMenuReset(void);
```

`accessibilityMenuObserve` must be a fast no-op unless both the accessibility coordinator and menu narration are enabled. `accessibilityMenuReset` clears owned snapshots/pending announcements during shutdown or lifecycle reset; it must not call a menu handler after menu storage becomes invalid.

Add coordinator queries rather than exposing config globals:

```c
s32 accessibilityIsMenuNarrationEnabled(void);
```

Create `src/include/accessibility/accessibility_announcement.h` with the smallest menu-needed dispatcher boundary:

```c
enum accessibility_announcement_reason {
	ACCESSIBILITY_ANNOUNCEMENT_DIALOG,
	ACCESSIBILITY_ANNOUNCEMENT_FOCUS,
	ACCESSIBILITY_ANNOUNCEMENT_VALUE,
	ACCESSIBILITY_ANNOUNCEMENT_REPEAT,
};

s32 accessibilityAnnouncementReplaceMenu(
		const char *utf8,
		enum accessibility_announcement_reason reason);
void accessibilityAnnouncementCancel(void);
void accessibilityAnnouncementReset(void);
```

The dispatcher owns a copy of the last accepted menu utterance and request metadata. For this milestone it may call `accessibilitySpeechOutput(text, 1)` immediately. It must still log whether a request was accepted, suppressed, replaced, repeated, cancelled, unavailable, or disabled. This boundary becomes the seed for later priority policy without pretending that Tolk reports speech completion.

## Generic handler semantic operation

Add an unused operation number after the existing item operations:

```c
#define MENUOP_GETACCESSIBILITYTEXT 26
```

Add this member to `union handlerdata` through a named structure in `src/include/types.h`:

```c
enum menuaccessibilitypart {
	MENUACCESSIBILITYPART_CONTROL = 0,
	MENUACCESSIBILITYPART_OPTION = 1,
	MENUACCESSIBILITYPART_SUMMARY = 2,
};

struct handlerdata_accessibility {
	s32 part;
	s32 index;
	char *buffer;
	u32 bufferlen;
};
```

The caller zero-initializes the entire union, sets part/index/buffer/buffer length, sets `buffer[0] = '\0'`, and calls only a non-null handler. The handler returns nonzero only when it wrote a complete, null-terminated semantic string. The caller rejects a missing terminator, logs the handler result and buffer capacity, and never passes the handler's buffer directly to speech.

Contracts by part:

- `CONTROL`: accessible name or value for the control as a whole when ordinary `param2`/standard operations cannot supply it.
- `OPTION`: semantic text for zero-based `index`, especially a custom-rendered list row or carousel option.
- `SUMMARY`: concise current-table/current-content summary for a compound control when the generic type adapter needs ownership-specific text.

This is a read-only query. A handler must not select, focus, allocate persistent game state, play a sound, push/pop a dialog, or call speech. It may use the same localized semantic sources used to render the requested row. If it needs scratch formatting, it writes into the supplied buffer with bounds checking.

Do not overload `MENUOP_GETOPTIONTEXT` for custom renderers: that operation returns borrowed pointers and cannot express compound summaries safely. Existing standard controls continue to use their established operations.

## Semantic snapshot

Maintain one owned snapshot per `MAX_PLAYERS` menu slot. Do not use pointers as the sole semantic identity, but retain them for diagnostics.

Each snapshot needs at least:

```text
valid
menu slot and mapped player number
menu root and depth
dialog definition pointer and dialog pointer
dialog title: raw copy and normalized copy
focused item pointer, item index, type, param, flags, handler pointer
role
disabled/unavailable state
dialog dimmed/open-control state
label: raw copy and normalized copy
value/state: raw copy and normalized copy
focused sub-index/row/column
selected index
item count and one-based position where known
scroll offset and maximum where known
keyboard string and caps state
semantic-provider success/failure
composed current utterance
fingerprint/hash of all announcement-relevant normalized fields
```

Owned strings may use reusable growable buffers or fixed buffers with explicit truncation state. Prefer reusable growable buffers so long localized and compound text is not silently lost. Allocation failure must preserve game behavior, suppress misleading speech, and log the requested size plus prior capacity.

The raw diagnostic copy is the exact resolved string available at observation time. “Raw” does not mean ROM bytes or extracted asset data; never dump backing language banks. The normalized copy is what composition uses.

## Type-by-type semantic contract

The resolver must use the active `g_MpPlayerNum` and current-player context established by `menuTick`. Zero-initialize `union handlerdata` before every operation. Copy every returned pointer before another callback.

### Selectable

- Label: `menuResolveParam2Text(item)`.
- Optional right-side/dynamic value: resolve `param3` only when the item's flags and existing rendering behavior treat it as text; do not interpret dialog pointers as strings.
- Role: `button` or `action`.
- State: `menuIsItemDisabled`.
- Announcement: label, optional value, then `unavailable` when disabled.
- Never announce “opened” or “activated” based only on the item flags; the following dialog snapshot is the truth.

### Checkbox

- Label: `menuResolveParam2Text`.
- Current state: zero-initialized `MENUOP_GET`; nonzero is checked.
- Role: `checkbox`.
- Focus: “{label}, checkbox, checked/not checked”.
- Same-focus state change: “{label}, checked/not checked”.

### Slider

- Label: `menuResolveParam2Text`.
- Numeric value: `MENUOP_GETSLIDER`.
- Maximum: `item->param3`, matching existing slider behavior.
- Display value: report the raw handler value as a rounded percentage of `item->param3`, clamped to 0–100 percent. This acceptance-testing revision replaces the original formatted-label/raw-unit behavior.
- Role: `slider`.
- Include position/range only when truthful; do not assume a zero minimum if the particular handler formats another semantic scale.
- Same-focus value changes replace prior slider speech.

### Dropdown

- Label: `menuResolveParam2Text`.
- Selected index: `MENUOP_GETSELECTEDINDEX`.
- Count: `MENUOP_GETOPTIONCOUNT`.
- Selected text: `MENUOP_GETOPTIONTEXT` for the selected index.
- When `dialog->dimmed`, the open dropdown row is `menuitemdata_dropdown.list.index`; announce that highlighted option and its position rather than falsely calling it selected.
- Role: `dropdown`, with `open` or `closed` when useful.
- A highlight change replaces earlier dropdown output. Selection is only the handler's selected index after processing.

### List

- Control label: `menuResolveParam2Text`, if present.
- Focused row: `menuitemdata_list.index`.
- Count: `MENUOP_GETOPTIONCOUNT`.
- Standard row text: `MENUOP_GETOPTIONTEXT`.
- Custom-rendered row text: required `MENUOP_GETACCESSIBILITYTEXT` with `OPTION` and the row index.
- Optional group: use `GETOPTGROUPCOUNT`, `GETGROUPSTARTINDEX`, and `GETOPTGROUPTEXT` conservatively; include the current group only when its mapping is valid.
- Optional row checkbox: use `MENUOP_GETLISTITEMCHECKBOX` only where the handler supports it; distinguish unsupported from unchecked.
- Selected index: include only when `MENUOP_GETSELECTEDINDEX` is a defined behavior for that list.
- Role: `list` plus `item X of Y`.
- A row change replaces earlier list speech. Do not announce offscreen rows.

Every focusable `MENUITEMFLAG_LIST_CUSTOMRENDER` definition must be audited. At planning time the known handlers are:

- `amPickTargetMenuList`;
- `filemgrFileToDeleteListMenuHandler`;
- `filemgrFileToCopyListMenuHandler`;
- `pakGameNoteListMenuHandler`;
- `filemgrChooseAgentListMenuHandler` (also reused by the 4 MB file manager);
- `mpChallengesListHandler`;
- `mpChallengesListMenuHandler` (normal and 4 MB definitions);
- `menuhandlerMissionList`;
- `frWeaponListMenuHandler`.

Re-run the audit rather than treating this list as permanently exhaustive. Custom-render flags on nonfocusable labels/models are logged by the source audit but do not require row focus semantics.

For `filemgrChooseAgentListMenuHandler`, the `OPTION` result must match rendered semantics:

- final row: localized `L_OPTIONS_403`, `New Agent...`;
- existing agent row: agent name, current/new-recruit stage text, and mission time when available, composed in the supplied buffer;
- never expose device internals or dump save structures merely because diagnostics are comprehensive.

### Keyboard

- Role: `text entry keyboard`.
- Runtime state: `menuitemdata_keyboard.string`, `row`, `col`, `capslock`, and `capseffective`.
- Ordinary grid cell: derive the same character from `g_KeyboardKeys` and effective caps behavior used by `menuitemKeyboardTick`/rendering.
- Special cells: resolve localized `L_OPTIONS_314` through `L_OPTIONS_317` for Delete, Caps, Cancel, and OK.
- Announce the current cell on grid focus movement.
- Announce concise edits when the string changes: inserted character plus current string, deleted character/current string, or physical-keyboard update/current string.
- Announce caps on/off when it changes.
- Describe OK as unavailable when the existing empty-or-spaces predicate makes acceptance invalid.
- On initial focus, combine dialog title, role, current entered string (or `empty`), and focused cell.
- Never log or speak bytes beyond the bounded keyboard string. Profile names are allowed in the opt-in development log.

Do not modify keyboard behavior to make narration easier. Observe the post-tick state and infer the semantic delta from old/new snapshots.

### Scrollable

- Text source: the same `menuitemScrollableGetText(item->param)` used by rendering.
- Runtime state: scroll offset and maximum from `menuitemdata_scrollable`.
- Role: `scrollable text`.
- On focus: announce a normalized bounded content introduction plus scroll position. Retain the full normalized text in the repeat snapshot when allocation succeeds.
- On scroll: announce the new percentage or beginning/end state and a bounded current-content segment if it can be derived without render-coordinate scraping.
- If `menuIsScrollableUnscrollable` is true, the engine treats it as nonfocusable; do not create synthetic focus.
- This provides control-level operation only. Milestone 6 will add deliberate full briefing/objective reading and richer navigation.

### Carousel

- Count and selected index: existing carousel handler operations.
- Value text: `MENUOP_GETACCESSIBILITYTEXT` with `OPTION` for the selected index.
- Label: `menuResolveParam2Text` when present; otherwise use the provider's `CONTROL` result, or a neutral role without inventing a visual description.
- Role: `carousel`, with `item X of Y`.
- Head/body character selectors must return their localized/name API result through the provider rather than describing the rendered model.

### Ranking

- Role: `ranking table`.
- Runtime scroll state: `menuitemdata_ranking.scrolloffset`.
- The adapter may call a type-specific semantic helper added next to the ranking code in `menuitem.c`, or a handler provider when one exists, but must use the same ranking/player APIs as rendering.
- On focus, announce a concise current ranking summary and position/scroll state. On scroll, replace with the newly current row/summary.
- Do not narrate colors, columns without labels, or visually inferred medals. If a field has no semantic name, log it and omit it.

### Player stats

- Role: `player statistics`.
- The existing handler-selected player is resolved using its dropdown-style selected-index and option-text operations.
- The table summary must use the same player/stat APIs as `menuitemPlayerStatsRender`, exposed through a type-specific helper or `SUMMARY` provider.
- On selected-player or table-position change, announce the selected player plus a concise labeled statistic summary.
- Do not iterate and speak the entire table automatically.

### Numbered or future focusable types

- The implementation's source audit must enumerate all `MENUITEMTYPE_*` values used in definitions and compare them with the resolver table.
- At runtime, an unknown focused type produces no guessed label/value, one state-change log with all raw fields, and a neutral “unknown control” only if there is a safe ordinary label. Repeated unchanged frames do not spam the log.
- Adding a new item type later must fail the development audit until its semantic contract is declared.

## Text ownership and normalization

Implement normalization as a pure, testable function. It must:

- accept UTF-8 and reject or replace invalid sequences deterministically before speech;
- preserve localized non-ASCII characters and meaningful punctuation;
- treat CR, LF, and tabs as word boundaries;
- collapse repeated whitespace and trim ends;
- remove only verified Perfect Dark presentation/control markers, documenting each removed byte sequence;
- avoid concatenating words when stripping markers;
- retain the exact resolved input separately for logs;
- return an explicit empty result rather than a fabricated label;
- report truncation/allocation/invalid-input decisions.

Do not call a rendering text function to obtain glyph output. Do not retain a pointer returned by `langGet`, `menuResolveText`, or a menu handler.

Composition should be deterministic and punctuation-aware. Examples are illustrative, not hard-coded screen rules:

```text
Perfect Dark. New Agent, list item 1 of 1.
Enter Agent Name. Text entry keyboard, empty. Delete.
Options. Audio, button, item 1 of 7.
Music volume, slider, 80 percent.
Subtitles, checkbox, checked.
Resolution, dropdown, 1920 by 1080, closed.
```

Do not speak an empty role/value fragment or literal words such as “null,” “unknown,” or “zero of zero” unless that is the intentional safe fallback.

## Change detection and speech policy

After creating the new snapshot, compare it with the previous snapshot for that menu slot in this order:

1. **No dialog now:** invalidate the snapshot and cancel the replaceable menu group. Do not say “closed” automatically.
2. **New dialog/root/depth:** compose title plus final focused semantics, store it as current, and replace speech with reason `DIALOG`.
3. **New focused item:** compose and store focused semantics without the dialog title, then replace speech with reason `FOCUS`. Returning to a previous dialog is a new dialog context and includes its title again.
4. **Same item, changed subfocus/value/state:** compose the concise control/value delta, update the repeat snapshot, and replace speech with reason `VALUE`.
5. **Only non-announcement diagnostic fields changed:** log when useful but do not speak.
6. **No relevant change:** do nothing.

Dialog identity includes root, depth, definition pointer, and active dialog pointer. Semantic equality uses normalized content plus structured values, not pointer equality alone. Pointer reuse after pop/push must not suppress a real context change.

Every menu state announcement uses `interrupt=1` because stale focus is actively harmful. Do not enqueue a five-item tail during rapid navigation. Store only the newest current menu utterance. Later gameplay events can introduce cross-category priorities without changing this menu adapter.

Repeat speaks the most recently stored utterance even when unchanged and logs `forced=1`. Thus it includes the title immediately after dialog entry, but not after a later within-dialog focus/value announcement. Cancel calls the public cancellation boundary, clears no game state, and retains the current snapshot so a later F5 repeat works.

## Provisional repeat and cancel commands

Implement the commands in the accessibility menu observer or a small port-neutral command function called from it. On PC use:

```c
#define VK_F5 (VK_F1 + 4)
#define VK_F6 (VK_F1 + 5)
```

Prefer named enum constants in `port/include/input.h` if that makes the implementation clearer. Use `inputKeyJustPressed`, and evaluate commands only when all are true:

- accessibility is enabled;
- menu narration is enabled;
- the observed menu slot is zero;
- an active dialog exists.

F5 invokes repeat before normal change suppression. F6 cancels speech. Ensure a held key does not repeat every frame. Record the key, menu context, result, and whether the game input system also had a binding for it. The implementation must verify that default binds do not use these keys and document any collision found.

These are development bindings, not a final user-facing design. Do not add controller chords or a settings-page label in this milestone.

## Logging contract

Use the existing JSONL logger. Because it currently accepts formatted messages rather than structured field objects, use stable `key=value` names and quote/escape text through the logger's existing safe formatting path. Add separate records where one line would become ambiguous.

At minimum log:

- configuration: menu narration requested/effective, speech/logging state;
- observer: menu slot, player, root, depth, dialog/focus pointers, type, param, flags, handler pointer, dimmed state, item index, source input summary, observation duration;
- raw resolution: each operation attempted, return value, indices/counts, returned pointer where useful, exact copied text, copy length, terminator/truncation/allocation result;
- normalized semantics: title, label, role, value, state, position/count, scroll, keyboard row/column/string/caps, provider result;
- diff: old/new fingerprints and exact fields that changed;
- announcement: composed full text, reason, replacement group, interrupt, dedupe/replacement decision, backend availability, acceptance, and duration;
- repeat/cancel: raw key/action, context, stored utterance, result, duration;
- unsupported/error: item type/flags, missing handler/data, invalid count/index, operation failure, invalid UTF-8, missing semantic provider, allocation failure;
- lifecycle/reset: snapshots cleared, pending/current text cleared, counts of observed/spoken/suppressed/provider-failed events.

Log every raw observation during a dedicated high-detail test build only if per-frame volume remains acceptable. For normal opt-in development logging, always log state changes and failures and include counters/timings for unchanged observations. The project owner permits comprehensive logs; performance, not privacy minimization, is the reason to aggregate unchanged-frame records.

Logging or speech failure must never block menu input, change selection, or terminate the process.

## Locked implementation files

Create these project-owned files:

```text
src/accessibility/accessibility_announcement.c
src/accessibility/accessibility_menu.c
src/include/accessibility/accessibility_announcement.h
src/include/accessibility/accessibility_menu.h
```

Modify these core/build files:

```text
CMakeLists.txt
src/accessibility/accessibility.c
src/include/accessibility/accessibility.h
src/include/constants.h
src/include/types.h
src/include/game/menu.h
src/game/menu.c
src/game/menutick.c
port/include/input.h                 # only for F5/F6 names if used
```

Modify semantic owner files only where the custom-provider/type audit proves necessary. Expected files are:

```text
src/game/activemenu.c
src/game/filemgr.c
src/game/fmb.c                       # only if its local handler differs; reused handlers need no duplicate code
src/game/mainmenu.c
src/game/menuitem.c                  # ranking/player-stats semantic helpers if needed
src/game/trainingmenus.c
src/game/mplayer/setup.c
```

If another focusable custom-rendered control appears in the audit, add its semantic owner file and record why. Do not edit a menu definition merely to add an accessibility-only label when its handler can answer the generic query.

Update:

```text
ACCESSIBILITY.md
ACCESSIBILITY_ARCHITECTURE.md
ACCESSIBILITY_ROADMAP.md
ACCESSIBILITY_TESTING.md
milestones/ACCESSIBILITY_MILESTONE_04_PLAN.md
```

Do not modify Tolk or its controller DLL. Do not move native dependencies; Milestone 3 already copies them beside the executable.

## Implementation sequence

### Step 1 — Preflight and audit

1. Confirm branch, commit, and expected dirty documentation patch.
2. Build the unmodified implementation baseline using the required MinGW64 commands.
3. Re-run the menu type inventory across `src/game` and `port/src`.
4. Enumerate every focusable custom-rendered list and every carousel/ranking/player-stats definition.
5. Save the audit result in the implementation record section of this file.
6. Stop if a currently used focusable type has no semantic design above; extend the plan before coding it.

### Step 2 — Add pure semantic data and normalization

1. Add snapshot/string helpers in `accessibility_menu.c`.
2. Implement normalization and deterministic composition without invoking speech.
3. Create a small focused harness for raw/normalized strings, invalid UTF-8, whitespace, punctuation, long content, allocation failure where injectable, and empty fragments.
4. Verify all copied strings survive subsequent handler calls in the harness.

### Step 3 — Add read-only menu queries

1. Add `menuGetItemData`.
2. Add `MENUOP_GETACCESSIBILITYTEXT` and its handler data structure.
3. Implement standard type resolvers using existing menu operations.
4. Implement or expose type-owned ranking/player-stats summaries.
5. Add custom semantic responses one handler at a time.
6. Re-run the coverage audit; it must produce no unhandled current focusable type/custom row.

### Step 4 — Add observation and diffing

1. Add the one post-input call in `menutick.c`.
2. Gate at the top before any handler query.
3. Build a snapshot under the still-active player context.
4. Compare dialog, focus, and value/subfocus changes in the locked order.
5. Store state separately per menu slot; speak only slot zero.
6. Invalidate safely when a dialog disappears or lifecycle shuts down.

### Step 5 — Add replacement announcements and commands

1. Add the replaceable menu dispatcher around the existing speech API.
2. Add F5 repeat and F6 cancel under the strict gate.
3. Verify cancel retains repeat state.
4. Verify unavailable speech produces logs and no game failure.
5. Verify rapid focus/value changes do not form a tail.

### Step 6 — Add comprehensive logs

1. Add stable event names and key/value messages for every contract above.
2. Include observation and backend-call durations.
3. Parse every produced JSONL file.
4. Measure unchanged-frame log volume and observation time before deciding whether unchanged events are individually recorded or aggregated.

### Step 7 — Build and focused tests

Use only the requested MinGW64 environment for builds:

```sh
cmake -G"Unix Makefiles" -Bbuild .
cmake --build build -j4 -- -O
```

The expected executable is `build/pd.x86_64.exe`. Inspect that `Tolk.dll` and the architecture-matching NVDA controller DLL remain beside it. Normal file inspection/editing/Git may use standard tools.

Compile the focused harness with the same compiler environment. If feasible, compile region-sensitive sources for `ntsc-final`, `jpn-final`, and `pal-final`; record unavailable lawful generated inputs rather than making a false claim.

### Step 8 — Runtime truth table

Run and record at least:

| Enabled | Logging | Speech | Menu narration | Expected |
| ---: | ---: | ---: | ---: | --- |
| 0 | any | any | any | No accessibility work or speech |
| 1 | 0 | 0 | 1 | Menu observation allowed, no log or speech, no failure |
| 1 | 1 | 0 | 1 | Semantic/menu decisions logged, no backend output |
| 1 | 1 | 1 | 0 | Backend may initialize, no menu observation/output |
| 1 | 0 | 1 | 1 | Spoken menu behavior, no accessibility log |
| 1 | 1 | 1 | 1 | Spoken behavior and complete diagnostic evidence |

Also run with missing Tolk, missing NVDA controller, no available reader where safely reproducible, and an unwritable log path. All must preserve menu operation.

### Step 9 — Startup/New Agent/settings script

Use a temporary lawful save/config directory so testing does not overwrite the user's profiles.

1. Start with no selected game file.
2. Confirm the first stable announcement contains `Perfect Dark` and the actual focused row. If there are no profiles, it must identify `New Agent...` and truthful position/count.
3. If profiles exist in the test fixture, traverse them and verify every custom row summary before the final New Agent row.
4. Select New Agent and confirm a single combined `Enter Agent Name`/keyboard announcement.
5. Move across ordinary and special keyboard cells; test Caps, Delete, Cancel, empty OK, controller entry, and physical keyboard entry.
6. Enter a unique test name and complete the save-location flow without overwriting an existing file.
7. Reach `Perfect Menu`, swipe to Options, and enter Audio, Video, Control, Display, and Extended where available.
8. Exercise a selectable, checkbox, slider, dropdown, and list. Compare spoken values with handler/game state, not just pixels.
9. Use mouse focus and controller/keyboard navigation on the same controls and confirm the semantic snapshot is equivalent.
10. Test back, dialog push/pop, sibling swipe, disabled auto-skip, rapid five-item navigation, rapid slider/list changes, F5 repeat, and F6 cancel.
11. Exit normally, parse the log, and verify the repository contains no new tracked runtime data.

### Step 10 — Full control-family matrix

In addition to the startup route, find one real instance of every current focusable family:

```text
selectable
checkbox
slider
dropdown (closed and open highlight)
standard list
custom-rendered list
keyboard
scrollable
carousel
ranking
player stats
```

For each record label source, role, current value, position/count, disabled behavior, subfocus/value changes, repeat output, cancel behavior, and exact log event. A type can be engineering-verified outside the startup route without implying that its containing mode is blind-accessible.

### Step 11 — Performance and regression

Measure at least:

- average and worst observer duration over an idle menu interval;
- handler operations per unchanged and changed frame;
- normalization/composition duration for short and longest tested text;
- Tolk request/cancel duration;
- log bytes/events per minute in idle and rapid-navigation tests;
- frame-time comparison with accessibility disabled, menu narration enabled without speech, and full logging/speech enabled.

Pass requirements:

- zero accessibility handler queries when the feature is off;
- zero speech requests on unchanged frames;
- bounded queries for one focused item per observation;
- no observable focus/input lag in the scripted path;
- no unbounded allocation or queue growth;
- no crash or menu behavior change on any semantic/backend/logging failure.

### Step 12 — Independent blind-user check and documentation

Ask a blind screen-reader user to complete the documented startup-to-settings task without live move-by-move coaching. Explain the comprehensive local log and emergency stop first. Record task completion, wrong turns, missing/excessive/late output, repeat/cancel use, recovery, elapsed time, and confidence.

If that session is unavailable, do not block an engineering save point, but label the milestone correctly. Update the roadmap, architecture ledger, testing evidence, and this file with exact completed/remaining evidence.

## Focused automated test matrix

The harness or isolated test target must cover:

- first dialog with first item and with a later pre-focused item;
- dialog pointer reuse with different title/root/depth;
- same focus pointer with dynamic label change;
- null title, null focus, null handler, and null runtime data;
- disabled focus and automatic focus replacement;
- standard/custom list valid, empty, negative, and out-of-range counts/indices;
- dropdown selected versus open highlighted row;
- slider numeric and formatted label paths;
- checkbox checked/unchecked;
- keyboard every row, ordinary/special cells, caps, empty/space-only string, deletion, physical update, and maximum length;
- scrollable beginning/middle/end and nonfocusable unscrollable state;
- carousel provider success/failure;
- ranking/player-stats summary success/failure;
- unknown item type with/without safe label;
- ASCII, localized UTF-8, combining characters, supplementary characters, invalid UTF-8, multiline text, presentation markers, and very long text;
- dedupe, focus replacement, value replacement, repeat bypass, cancel, cancel-then-repeat, backend unavailable, speech disabled, and logging disabled;
- all four menu slots changing in one outer tick while only slot zero speaks;
- lifecycle reset followed by safe re-observation.

## Review checklist

### Architecture

- [ ] Exactly one menu observation hook exists.
- [ ] No native speech call exists under `src/game`.
- [ ] No dialog/item pointer comparison selects screen-specific narration.
- [ ] Type-specific semantics are centralized and custom semantics use one query operation.
- [ ] Handler queries are read-only, zero-initialized, bounded, and made under the correct player context.
- [ ] Callback text is copied immediately.
- [ ] Feature-off paths return before menu work.

### Coverage

- [ ] Every current focusable named type appears in the resolver table.
- [ ] Every focusable custom-rendered list appears in the provider audit.
- [ ] New Agent is obtained through its handler provider.
- [ ] Keyboard grid and physical entry are covered.
- [ ] Settings route covers standard values.
- [ ] Unknown/future types fail safely.
- [ ] Presentation-only types do not create synthetic focus.

### Output policy

- [ ] Initial dialog and final focus are one coherent utterance.
- [ ] Rapid changes replace stale menu output.
- [ ] Unchanged frames are silent.
- [ ] Repeat and cancel do not alter game focus.
- [ ] Disabled/unavailable state is truthful.
- [ ] Missing text is omitted rather than invented.

### Diagnostics and safety

- [ ] Logs contain full feature-relevant raw and normalized data.
- [ ] JSONL parses after invalid/unusual text tests.
- [ ] No ROM content, extracted asset, credential, or unrelated secret is logged.
- [ ] Missing backend/controller/reader and log failure are nonfatal.
- [ ] Runtime data remains ignored and uncommitted.

### Evidence

- [ ] Required MinGW64 build passes.
- [ ] Disabled regression passes.
- [ ] Startup/New Agent/settings script passes.
- [ ] Full control-family engineering matrix is recorded.
- [ ] Performance data is recorded.
- [ ] Human audible behavior and blind task evidence are not inferred from backend return values.
- [ ] Any unavailable blind-user or region evidence is named precisely.

## Handoff report format

The implementation agent must finish by appending an execution result to this file containing:

```text
Implementation commit/dirty patch:
Branch and planning baseline:
Files added/modified:
Any deviation from locked shape and why:

Menu type audit result:
Custom semantic-provider audit result:
Build configurations and commands:
Executable/dependency inspection:
Automated harness result:
Runtime truth-table result:
Startup/New Agent/settings result:
Control-family matrix result:
Backend/log failure result:
Performance measurements:
Accessibility-disabled regression:
Independent blind-user evidence:
Remaining limitations:
Git status and untracked runtime data check:
```

The next milestone after this implementation is Milestone 5, discoverable accessibility settings and permanent collision-aware input actions. Do not begin that UI or binding work as part of Milestone 4.

## Execution result (2026-07-19)

Implementation commit/dirty patch: completed Milestone 4 patch on planning baseline `3268e3d7a`; committed after this record was finalized.

Branch and planning baseline: `accessibility`, `3268e3d7a accessibility: add Tolk speech backend proof`.

Files added/modified: the accessibility menu observer and announcement dispatcher; their public headers; coordinator/configuration, menu/item contracts, one post-input hook, custom semantic-owner handlers, F5/F6 key names, CMake source list, repository guidance, and accessibility architecture/roadmap/testing documents.

Any deviation from locked shape and why: fixed-size bounded owned buffers were used instead of growable strings to keep the frame observer allocation-free; invalid UTF-8 bytes become `?` and incomplete trailing sequences are never passed to speech. Ranking and player-stats currently expose truthful role/scroll/selected-player state rather than reconstructing rendered table cells. No `menuitem.c` hook was required. These choices need usability review in the pending control-family matrix.

Menu type audit result: static definition-line audit found selectable 329, checkbox 125, slider 18, dropdown 66, list 33, keyboard 6, scrollable 18, carousel 4, ranking 2, and player-stats 1. The centralized resolver has an explicit case for each. Presentation-only types remain outside focus narration, and unknown/future focused types log `unsupported_type` without guessing.

Custom semantic-provider audit result: 12 focusable custom-rendered-list definitions resolve through nine unique handlers. Direct providers were added for `amPickTargetMenuList`, `filemgrFileToDeleteListMenuHandler`, `filemgrFileToCopyListMenuHandler`, `pakGameNoteListMenuHandler`, `filemgrChooseAgentListMenuHandler`, `menuhandlerMissionList`, `mpChallengesListHandler`, `mpChallengesListMenuHandler`, and `frWeaponListMenuHandler`. The 4 MB definitions in `fmb.c` reuse audited providers. Custom-render flags on labels/models are presentation-only. Carousel head/body wrappers reach providers in their shared handlers.

Build configurations and commands: x86-64 Windows `ntsc-final`, `RelWithDebInfo`, Unix Makefiles. The exact configure command `cmake -G"Unix Makefiles" -Bbuild .` and build command `cmake --build build -j4 -- -O` passed in the initialized MSYS2 MinGW64 environment; the final incremental rebuild after review also passed.

Executable/dependency inspection: `build/pd.x86_64.exe` exists (20,184,069 bytes), with `tolk.dll` (1,154,914 bytes) and `nvdaControllerClient64.dll` (153,600 bytes) beside it.

Automated harness result: not implemented; the plan's focused semantic test matrix remains pending.

Runtime truth-table result: partial only. An isolated `Enabled=1`, `LoggingEnabled=1`, `SpeechEnabled=0`, `MenuNarration=1` launch used `--savedir ./build/m4-smoke`, loaded successfully, opened the game window, handled a normal window-close event, ran accessibility reset, and exited zero. The executable was launched through the initialized MinGW64 environment as required. The other truth-table combinations remain pending.

Startup/New Agent/settings result: not run. The generic `New Agent...` provider is present and compiled, but an automated attempt did not advance through the title sequence; no interaction result is claimed.

Control-family matrix result: source coverage only; runtime interaction pending.

Backend/log failure result: Milestone 3 backend failure evidence still applies to the unchanged dispatcher backend. Menu-specific failure combinations remain pending.

Performance measurements: not collected. The feature-off gate occurs before dialog or handler queries, unchanged states produce no speech request, and observation storage is fixed-size, but runtime timings remain pending.

Accessibility-disabled regression: build passed. A new disabled runtime regression was not run during this implementation; Milestone 3's disabled lifecycle evidence predates the menu hook.

Independent blind-user evidence: the project owner performed blind-user acceptance testing and reported the resulting spoken-menu behavior was working perfectly. The accepted follow-up behavior reads a dialog title only on entry/return and reports sliders as percentages. No separate session transcript or timing measurements were supplied.

Remaining limitations: no permanent/discoverable bindings; English role/status words; slot zero is the only speech owner; fixed text limits; carousel head names have truthful numeric fallback; ranking/player-stats summaries are intentionally shallow; no focused harness, complete runtime path, other-region build, or human usability evidence yet.

Acceptance-testing default update: after the implementation pass, the project owner requested that `Accessibility.Enabled`, `Accessibility.LoggingEnabled`, `Accessibility.SpeechEnabled`, and `Accessibility.MenuNarration` all default to one. Existing saved `pd.ini` values continue to override these compiled defaults.

Acceptance-testing speech refinements: dialog titles are now spoken only when entering or returning to a dialog, not for each option within it. Slider values are now rounded percentages of their configured maximum instead of raw engine values or custom render labels.

Completion decision: after testing the refinements above, the project owner explicitly marked Milestone 4 complete. The unexecuted exhaustive harness, region matrix, and performance measurements remain documented regression opportunities and do not broaden the user-visible coverage claimed here.

Git status and untracked runtime data check: implementation and documentation remain intentionally uncommitted. Smoke configuration, EEPROM, log, and executable data are under ignored `build/`; no runtime data is staged or tracked.

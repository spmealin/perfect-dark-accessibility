#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "data.h"
#include "input.h"
#include "system.h"
#include "types.h"
#include "game/lang.h"
#include "game/menu.h"
#include "game/menuitem.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_menu.h"

#define ACCESSIBILITY_MENU_TEXT_MAX 2048
#define ACCESSIBILITY_MENU_FIELD_MAX 768

struct accessibilitymenusnapshot {
	s32 valid;
	s32 menuslot;
	s32 playernum;
	s32 menuroot;
	s32 menudepth;
	s32 itemindex;
	s32 itemtype;
	s32 disabled;
	s32 dimmed;
	s32 subindex;
	s32 selectedindex;
	s32 optioncount;
	s32 scrolloffset;
	s32 maxscrolloffset;
	s32 keyboardrow;
	s32 keyboardcol;
	s32 keyboardcaps;
	struct menudialog *dialog;
	struct menudialogdef *dialogdef;
	struct menuitem *item;
	char title[ACCESSIBILITY_MENU_FIELD_MAX];
	char label[ACCESSIBILITY_MENU_FIELD_MAX];
	char role[64];
	char value[ACCESSIBILITY_MENU_FIELD_MAX];
	char keyboardtext[MPSETUP_MAXNAME + 1];
	char utterance[ACCESSIBILITY_MENU_TEXT_MAX];
};

static struct accessibilitymenusnapshot g_AccessibilityMenuSnapshots[MAX_PLAYERS];
static u64 g_AccessibilityMenuObservations;
static u64 g_AccessibilityMenuChanges;
static u64 g_AccessibilityMenuUnsupported;

static void accessibilityMenuCopyNormalized(char *dst, size_t dstlen, const char *src)
{
	size_t out = 0;
	size_t count;
	s32 pending_space = false;
	const unsigned char *ptr = (const unsigned char *)src;

	if (!dstlen) {
		return;
	}

	dst[0] = '\0';

	if (!ptr) {
		return;
	}

	while (*ptr && out + 1 < dstlen) {
		if (*ptr < 0x20 || *ptr == 0x7f) {
			pending_space = out > 0;
			ptr++;
			continue;
		}

		if (*ptr == ' ') {
			pending_space = out > 0;
			ptr++;
			continue;
		}

		count = 1;

		if (*ptr >= 0x80) {
			if (*ptr >= 0xc2 && *ptr <= 0xdf
					&& ptr[1] >= 0x80 && ptr[1] <= 0xbf) {
				count = 2;
			} else if (((*ptr == 0xe0 && ptr[1] >= 0xa0 && ptr[1] <= 0xbf)
						|| (*ptr >= 0xe1 && *ptr <= 0xec && ptr[1] >= 0x80 && ptr[1] <= 0xbf)
						|| (*ptr == 0xed && ptr[1] >= 0x80 && ptr[1] <= 0x9f)
						|| (*ptr >= 0xee && *ptr <= 0xef && ptr[1] >= 0x80 && ptr[1] <= 0xbf))
					&& ptr[2] >= 0x80 && ptr[2] <= 0xbf) {
				count = 3;
			} else if (((*ptr == 0xf0 && ptr[1] >= 0x90 && ptr[1] <= 0xbf)
						|| (*ptr >= 0xf1 && *ptr <= 0xf3 && ptr[1] >= 0x80 && ptr[1] <= 0xbf)
						|| (*ptr == 0xf4 && ptr[1] >= 0x80 && ptr[1] <= 0x8f))
					&& ptr[2] >= 0x80 && ptr[2] <= 0xbf
					&& ptr[3] >= 0x80 && ptr[3] <= 0xbf) {
				count = 4;
			} else {
				if (pending_space && out + 1 < dstlen) {
					dst[out++] = ' ';
					pending_space = false;
				}

				dst[out++] = '?';
				ptr++;
				continue;
			}
		}

		if (pending_space && out + 1 < dstlen) {
			dst[out++] = ' ';
			pending_space = false;
		}

		if (out + count >= dstlen) {
			break;
		}

		while (count-- > 0) {
			dst[out++] = (char)*ptr++;
		}
	}

	while (out > 0 && dst[out - 1] == ' ') {
		out--;
	}

	dst[out] = '\0';
}

static void accessibilityMenuAppend(char *dst, size_t dstlen, const char *text)
{
	size_t used;

	if (!text || !text[0] || !dstlen) {
		return;
	}

	used = strlen(dst);

	if (used >= dstlen - 1) {
		return;
	}

	snprintf(dst + used, dstlen - used, "%s", text);
}

static void accessibilityMenuAppendPart(char *dst, size_t dstlen, const char *text,
		const char *separator)
{
	if (!text || !text[0]) {
		return;
	}

	if (dst[0] && separator) {
		accessibilityMenuAppend(dst, dstlen, separator);
	}

	accessibilityMenuAppend(dst, dstlen, text);
}

static s32 accessibilityMenuGetProviderText(struct menuitem *item, s32 part, s32 index,
		char *dst, size_t dstlen)
{
	union handlerdata data;
	char buffer[ACCESSIBILITY_MENU_FIELD_MAX];
	uintptr_t result;

	if (!item || !item->handler || !dstlen) {
		return false;
	}

	memset(&data, 0, sizeof(data));
	memset(buffer, 0, sizeof(buffer));
	data.accessibility.part = part;
	data.accessibility.index = index;
	data.accessibility.buffer = buffer;
	data.accessibility.bufferlen = sizeof(buffer);
	result = item->handler(MENUOP_GETACCESSIBILITYTEXT, item, &data);
	buffer[sizeof(buffer) - 1] = '\0';

	if (!result || !buffer[0]) {
		return false;
	}

	accessibilityMenuCopyNormalized(dst, dstlen, buffer);
	return dst[0] != '\0';
}

static s32 accessibilityMenuGetHandlerValue(struct menuitem *item, s32 operation)
{
	union handlerdata data;

	if (!item || !item->handler) {
		return -1;
	}

	memset(&data, 0, sizeof(data));
	item->handler(operation, item, &data);
	return (s32)data.list.value;
}

static void accessibilityMenuGetOptionText(struct menuitem *item, s32 index,
		char *dst, size_t dstlen)
{
	union handlerdata data;
	const char *text;

	dst[0] = '\0';

	if (!item || !item->handler || index < 0) {
		return;
	}

	if ((item->flags & MENUITEMFLAG_LIST_CUSTOMRENDER)
			&& accessibilityMenuGetProviderText(item, MENUACCESSIBILITYPART_OPTION,
					index, dst, dstlen)) {
		return;
	}

	memset(&data, 0, sizeof(data));
	data.list.value = index;
	text = (const char *)item->handler(MENUOP_GETOPTIONTEXT, item, &data);
	accessibilityMenuCopyNormalized(dst, dstlen, text);

	if (!dst[0]) {
		accessibilityMenuGetProviderText(item, MENUACCESSIBILITYPART_OPTION,
				index, dst, dstlen);
	}
}

static void accessibilityMenuDescribeKeyboard(struct accessibilitymenusnapshot *snapshot,
		union menuitemdata *itemdata)
{
	struct menuitemdata_keyboard *keyboard;
	char key[8] = "";
	const char *special = NULL;

	strcpy(snapshot->role, "text entry keyboard");

	if (!itemdata) {
		strcpy(snapshot->value, "state unavailable");
		return;
	}

	keyboard = &itemdata->keyboard;
	snapshot->keyboardrow = keyboard->row;
	snapshot->keyboardcol = keyboard->col;
	snapshot->keyboardcaps = keyboard->capseffective;
	accessibilityMenuCopyNormalized(snapshot->keyboardtext,
			sizeof(snapshot->keyboardtext), keyboard->string);

	if (keyboard->row == 4) {
		switch (keyboard->col) {
		case 0: special = langGet(L_OPTIONS_314); break;
		case 2: special = langGet(L_OPTIONS_315); break;
		case 5: special = langGet(L_OPTIONS_316); break;
		case 8: special = langGet(L_OPTIONS_317); break;
		}
	} else if (keyboard->row == 5) {
		special = "type with physical keyboard";
	} else if (keyboard->row >= 0 && keyboard->row < 4
			&& keyboard->col >= 0 && keyboard->col < 10) {
		key[0] = g_KeyboardKeys[keyboard->row][keyboard->col];

		if (!keyboard->capseffective && key[0] >= 'A' && key[0] <= 'Z') {
			key[0] += 'a' - 'A';
		}
	}

	if (snapshot->keyboardtext[0]) {
		snprintf(snapshot->value, sizeof(snapshot->value), "%s, %s%s%s",
				snapshot->keyboardtext,
				special ? special : key,
				keyboard->capseffective ? ", caps on" : "",
				keyboard->row == 4 && keyboard->col == 8
						&& menuitemKeyboardIsStringEmptyOrSpaces(keyboard->string)
					? ", unavailable" : "");
	} else {
		snprintf(snapshot->value, sizeof(snapshot->value), "empty, %s%s%s",
				special ? special : key,
				keyboard->capseffective ? ", caps on" : "",
				keyboard->row == 4 && keyboard->col == 8 ? ", unavailable" : "");
	}
}

static void accessibilityMenuDescribeItem(struct accessibilitymenusnapshot *snapshot,
		union menuitemdata *itemdata)
{
	struct menuitem *item = snapshot->item;
	union handlerdata data;
	char option[ACCESSIBILITY_MENU_FIELD_MAX];
	const char *text;
	s32 value;

	text = menuResolveParam2Text(item);
	accessibilityMenuCopyNormalized(snapshot->label, sizeof(snapshot->label), text);
	snapshot->subindex = -1;
	snapshot->selectedindex = -1;
	snapshot->optioncount = -1;
	snapshot->scrolloffset = -1;
	snapshot->maxscrolloffset = -1;
	snapshot->keyboardrow = -1;
	snapshot->keyboardcol = -1;

	switch (item->type) {
	case MENUITEMTYPE_SELECTABLE:
		strcpy(snapshot->role, "button");
		break;
	case MENUITEMTYPE_CHECKBOX:
		strcpy(snapshot->role, "checkbox");
		memset(&data, 0, sizeof(data));
		value = item->handler ? item->handler(MENUOP_GET, item, &data) : 0;
		strcpy(snapshot->value, value ? "checked" : "not checked");
		break;
	case MENUITEMTYPE_SLIDER:
		strcpy(snapshot->role, "slider");
		memset(&data, 0, sizeof(data));
		if (item->handler) {
			item->handler(MENUOP_GETSLIDER, item, &data);
			value = data.slider.value;

			if ((s32)item->param3 > 0) {
				s64 percentage = ((s64)value * 100 + (s32)item->param3 / 2)
						/ (s32)item->param3;

				if (percentage < 0) {
					percentage = 0;
				} else if (percentage > 100) {
					percentage = 100;
				}

				snprintf(snapshot->value, sizeof(snapshot->value), "%d%%", (s32)percentage);
			} else {
				strcpy(snapshot->value, "0%");
			}

			snapshot->subindex = value;
		}
		break;
	case MENUITEMTYPE_DROPDOWN:
		strcpy(snapshot->role, "dropdown");
		snapshot->optioncount = accessibilityMenuGetHandlerValue(item, MENUOP_GETOPTIONCOUNT);
		snapshot->selectedindex = accessibilityMenuGetHandlerValue(item, MENUOP_GETSELECTEDINDEX);
		snapshot->subindex = snapshot->dimmed && itemdata
				? itemdata->dropdown.list.index : snapshot->selectedindex;
		accessibilityMenuGetOptionText(item, snapshot->subindex, snapshot->value, sizeof(snapshot->value));
		break;
	case MENUITEMTYPE_LIST:
		strcpy(snapshot->role, "list");
		snapshot->optioncount = accessibilityMenuGetHandlerValue(item, MENUOP_GETOPTIONCOUNT);
		snapshot->subindex = itemdata ? itemdata->list.index : -1;
		snapshot->selectedindex = accessibilityMenuGetHandlerValue(item, MENUOP_GETSELECTEDINDEX);
		accessibilityMenuGetOptionText(item, snapshot->subindex, snapshot->value, sizeof(snapshot->value));
		break;
	case MENUITEMTYPE_KEYBOARD:
		accessibilityMenuDescribeKeyboard(snapshot, itemdata);
		break;
	case MENUITEMTYPE_SCROLLABLE:
		strcpy(snapshot->role, "scrollable text");
		accessibilityMenuCopyNormalized(snapshot->value, sizeof(snapshot->value),
				menuitemScrollableGetText(item->param));
		if (itemdata) {
			snapshot->scrolloffset = itemdata->scrollable.scrolloffset;
			snapshot->maxscrolloffset = itemdata->scrollable.maxscrolloffset;
			snapshot->subindex = snapshot->scrolloffset;
		}
		break;
	case MENUITEMTYPE_CAROUSEL:
		strcpy(snapshot->role, "carousel");
		snapshot->optioncount = accessibilityMenuGetHandlerValue(item, MENUOP_GETOPTIONCOUNT);
		snapshot->selectedindex = accessibilityMenuGetHandlerValue(item, MENUOP_GETSELECTEDINDEX);
		snapshot->subindex = snapshot->selectedindex;
		accessibilityMenuGetProviderText(item, MENUACCESSIBILITYPART_OPTION,
				snapshot->selectedindex, snapshot->value, sizeof(snapshot->value));
		break;
	case MENUITEMTYPE_RANKING:
		strcpy(snapshot->role, "ranking table");
		if (itemdata) {
			snapshot->scrolloffset = itemdata->ranking.scrolloffset;
			snapshot->subindex = snapshot->scrolloffset;
			snprintf(snapshot->value, sizeof(snapshot->value), "scroll position %d", snapshot->scrolloffset);
		}
		break;
	case MENUITEMTYPE_PLAYERSTATS:
		strcpy(snapshot->role, "player statistics");
		snapshot->optioncount = accessibilityMenuGetHandlerValue(item, MENUOP_GETOPTIONCOUNT);
		snapshot->selectedindex = accessibilityMenuGetHandlerValue(item, MENUOP_GETSELECTEDINDEX);
		accessibilityMenuGetOptionText(item, snapshot->selectedindex, option, sizeof(option));
		if (itemdata) {
			snapshot->scrolloffset = itemdata->dropdown.scrolloffset;
			snapshot->subindex = snapshot->scrolloffset;
		}
		snprintf(snapshot->value, sizeof(snapshot->value), "%s%s%d",
				option,
				option[0] ? ", scroll position " : "scroll position ",
				snapshot->scrolloffset < 0 ? 0 : snapshot->scrolloffset);
		break;
	default:
		strcpy(snapshot->role, "unknown control");
		g_AccessibilityMenuUnsupported++;
		accessibilityLogEvent("menu", "unsupported_type",
				"slot=%d root=%d depth=%d dialog=%p item=%p item_index=%d type=%d param=%d flags=0x%08x handler=%p label=%s",
				snapshot->menuslot, snapshot->menuroot, snapshot->menudepth,
				(void *)snapshot->dialog, (void *)item, snapshot->itemindex,
				item->type, item->param, item->flags, (void *)item->handler,
				snapshot->label);
		break;
	}

	if (!snapshot->label[0]) {
		accessibilityMenuGetProviderText(item, MENUACCESSIBILITYPART_CONTROL,
				-1, snapshot->label, sizeof(snapshot->label));
	}
}

static void accessibilityMenuCompose(struct accessibilitymenusnapshot *snapshot,
		s32 includetitle)
{
	char position[96];

	snapshot->utterance[0] = '\0';
	position[0] = '\0';
	if (includetitle) {
		accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
				snapshot->title, NULL);
	}
	accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
			snapshot->label, snapshot->utterance[0] ? ". " : NULL);
	accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
			snapshot->role, snapshot->utterance[0] ? ", " : NULL);
	accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
			snapshot->value, snapshot->utterance[0] ? ", " : NULL);

	if (snapshot->subindex >= 0 && snapshot->optioncount > 0
			&& snapshot->subindex < snapshot->optioncount) {
		snprintf(position, sizeof(position), "item %d of %d",
				snapshot->subindex + 1, snapshot->optioncount);
		accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
				position, snapshot->utterance[0] ? ", " : NULL);
	}

	if (snapshot->dimmed
			&& (snapshot->itemtype == MENUITEMTYPE_DROPDOWN
					|| snapshot->itemtype == MENUITEMTYPE_SLIDER)) {
		accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
				"open", snapshot->utterance[0] ? ", " : NULL);
	}

	if (snapshot->disabled) {
		accessibilityMenuAppendPart(snapshot->utterance, sizeof(snapshot->utterance),
				"unavailable", snapshot->utterance[0] ? ", " : NULL);
	}

	accessibilityMenuAppend(snapshot->utterance, sizeof(snapshot->utterance), ".");
}

static s32 accessibilityMenuRelevantEqual(const struct accessibilitymenusnapshot *a,
		const struct accessibilitymenusnapshot *b)
{
	return a->valid == b->valid
		&& a->dialog == b->dialog
		&& a->dialogdef == b->dialogdef
		&& a->item == b->item
		&& a->menuroot == b->menuroot
		&& a->menudepth == b->menudepth
		&& a->disabled == b->disabled
		&& a->dimmed == b->dimmed
		&& a->subindex == b->subindex
		&& a->selectedindex == b->selectedindex
		&& a->optioncount == b->optioncount
		&& a->scrolloffset == b->scrolloffset
		&& a->keyboardrow == b->keyboardrow
		&& a->keyboardcol == b->keyboardcol
		&& a->keyboardcaps == b->keyboardcaps
		&& strcmp(a->title, b->title) == 0
		&& strcmp(a->label, b->label) == 0
		&& strcmp(a->role, b->role) == 0
		&& strcmp(a->value, b->value) == 0
		&& strcmp(a->keyboardtext, b->keyboardtext) == 0;
}

void accessibilityMenuObserve(s32 menuslot, s32 playernum, s32 menuroot,
		s32 menudepth, struct menu *menu)
{
	struct accessibilitymenusnapshot next;
	struct accessibilitymenusnapshot *previous;
	struct menudialog *dialog;
	union menuitemdata *itemdata;
	enum accessibility_announcement_reason reason;
	u64 started;
	u64 elapsed;
	s32 cancelrequested = false;
	s32 repeatrequested = false;
	s32 changed;

	if (!accessibilityIsMenuNarrationEnabled() || menuslot < 0 || menuslot >= MAX_PLAYERS) {
		return;
	}

	started = sysGetMicroseconds();
	g_AccessibilityMenuObservations++;
	previous = &g_AccessibilityMenuSnapshots[menuslot];
	dialog = menu ? menu->curdialog : NULL;

#ifndef PLATFORM_N64
	if (menuslot == 0 && dialog) {
		cancelrequested = inputKeyJustPressed(VK_F6);
		repeatrequested = !cancelrequested && inputKeyJustPressed(VK_F5);
	}
#endif

	if (!dialog || !dialog->definition || !dialog->focuseditem) {
		if (previous->valid) {
			accessibilityLogEvent("menu", "context_cleared",
					"slot=%d player=%d root=%d depth=%d previous_dialog=%p previous_item=%p",
					menuslot, playernum, menuroot, menudepth,
					(void *)previous->dialog, (void *)previous->item);
			memset(previous, 0, sizeof(*previous));
			if (menuslot == 0) {
				accessibilityAnnouncementCancel();
			}
		}
		return;
	}

	memset(&next, 0, sizeof(next));
	next.valid = true;
	next.menuslot = menuslot;
	next.playernum = playernum;
	next.menuroot = menuroot;
	next.menudepth = menudepth;
	next.dialog = dialog;
	next.dialogdef = dialog->definition;
	next.item = dialog->focuseditem;
	next.itemindex = (s32)(next.item - dialog->definition->items);
	next.itemtype = next.item->type;
	next.dimmed = dialog->dimmed;
	accessibilityMenuCopyNormalized(next.title, sizeof(next.title),
			menuResolveDialogTitle(dialog->definition));
	next.disabled = menuIsItemDisabled(next.item, dialog);
	itemdata = menuGetItemData(dialog, next.item);
	accessibilityMenuDescribeItem(&next, itemdata);
	changed = !accessibilityMenuRelevantEqual(previous, &next);

	if (changed) {
		if (!previous->valid || previous->dialog != next.dialog
				|| previous->dialogdef != next.dialogdef
				|| previous->menuroot != next.menuroot
				|| previous->menudepth != next.menudepth) {
			reason = ACCESSIBILITY_ANNOUNCEMENT_DIALOG;
		} else if (previous->item != next.item) {
			reason = ACCESSIBILITY_ANNOUNCEMENT_FOCUS;
		} else {
			reason = ACCESSIBILITY_ANNOUNCEMENT_VALUE;
		}

		accessibilityMenuCompose(&next,
				reason == ACCESSIBILITY_ANNOUNCEMENT_DIALOG);
		g_AccessibilityMenuChanges++;
		elapsed = sysGetMicroseconds() - started;
		accessibilityLogEvent("menu", "snapshot_changed",
				"slot=%d player=%d root=%d depth=%d dialog=%p dialog_def=%p item=%p item_index=%d type=%d param=%d flags=0x%08x handler=%p disabled=%d dimmed=%d subindex=%d selected=%d count=%d scroll=%d max_scroll=%d keyboard_row=%d keyboard_col=%d keyboard_caps=%d title=%s title_spoken=%d label=%s role=%s value=%s text=%s elapsed_us=%llu",
				menuslot, playernum, menuroot, menudepth,
				(void *)dialog, (void *)dialog->definition, (void *)next.item,
				next.itemindex, next.itemtype, next.item->param, next.item->flags,
				(void *)next.item->handler, next.disabled, next.dimmed,
				next.subindex, next.selectedindex, next.optioncount,
				next.scrolloffset, next.maxscrolloffset,
				next.keyboardrow, next.keyboardcol, next.keyboardcaps,
				next.title, reason == ACCESSIBILITY_ANNOUNCEMENT_DIALOG,
				next.label, next.role, next.value, next.utterance,
				(unsigned long long)elapsed);
		*previous = next;
	}

	if (cancelrequested) {
		accessibilityLogEvent("menu", "command",
				"action=cancel key=F6 slot=0 dialog=%p text=%s",
				(void *)dialog, previous->utterance);
		accessibilityAnnouncementCancel();
		return;
	}

	if (repeatrequested && previous->valid) {
		accessibilityLogEvent("menu", "command",
				"action=repeat key=F5 slot=0 dialog=%p text=%s",
				(void *)dialog, previous->utterance);
		accessibilityAnnouncementReplaceMenu(previous->utterance,
				ACCESSIBILITY_ANNOUNCEMENT_REPEAT);
		return;
	}

	if (!changed) {
		return;
	}

	if (menuslot == 0) {
		accessibilityAnnouncementReplaceMenu(previous->utterance, reason);
	} else {
		accessibilityLogEvent("menu", "speech_suppressed",
				"slot=%d reason=non_primary_menu text=%s", menuslot, previous->utterance);
	}
}

void accessibilityMenuReset(void)
{
	accessibilityLogEvent("menu", "reset",
			"observations=%llu changes=%llu unsupported=%llu",
			(unsigned long long)g_AccessibilityMenuObservations,
			(unsigned long long)g_AccessibilityMenuChanges,
			(unsigned long long)g_AccessibilityMenuUnsupported);
	memset(g_AccessibilityMenuSnapshots, 0, sizeof(g_AccessibilityMenuSnapshots));
	g_AccessibilityMenuObservations = 0;
	g_AccessibilityMenuChanges = 0;
	g_AccessibilityMenuUnsupported = 0;
}

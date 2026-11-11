/* Synthetic semantic menu regression, not a screen-reader acceptance test. */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_menu.c"
#include "../../src/accessibility/accessibility_menu_head.c"

static s32 selected;
static s32 summaries;
static s32 checkstate;
u8 g_KeyboardKeys[5][10];
static char *names[] = { "Player 1", "EasySim:1", "EasySim:2", "EasySim:3", "EasySim:4" };

static MenuItemHandlerResult statsHandler(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	switch (operation) {
	case MENUOP_GETOPTIONCOUNT: data->list.value = ARRAYCOUNT(names); break;
	case MENUOP_GETSELECTEDINDEX: data->list.value = selected; break;
	case MENUOP_GETOPTIONTEXT:
		assert(data->list.value < ARRAYCOUNT(names));
		return (uintptr_t)names[data->list.value];
	case MENUOP_GETACCESSIBILITYTEXT:
		if (data->accessibility.part == MENUACCESSIBILITYPART_CONTROL) {
			summaries++;
			snprintf(data->accessibility.buffer, data->accessibility.bufferlen,
					"Stats for %s", names[selected]);
			return 1;
		}
		break;
	}
	return 0;
}

char *menuResolveParam2Text(struct menuitem *item)
{
	switch (item->param2) {
	case 1: return "Kills:";
	case 3: return "Agent file";
	case 4: return "Cannot delete file as it is being used.\n";
	case 5: return "Hidden detail";
	default: return "";
	}
}
char *menuResolveText(uintptr_t thing, void *context) { return thing == 2 ? "42\n" : ""; }
char *menuitemScrollableGetText(u32 type) { return ""; }
char *langGet(s32 textid) { return textid == L_OPTIONS_027 ? "Localized Carrington\n" : ""; }
bool menuitemKeyboardIsStringEmptyOrSpaces(char *text) { return false; }
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...) { }

static MenuItemHandlerResult checkboxHandler(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	switch (operation) {
	case MENUOP_GET: return checkstate;
	case MENUOP_GETOPTIONCOUNT: data->list.value = 2; break;
	case MENUOP_GETSELECTEDINDEX: data->list.value = 0xfffff; break;
	case MENUOP_GETOPTIONTEXT: return (uintptr_t)"Alien Conflict";
	case MENUOP_GETLISTITEMCHECKBOX:
		assert(data->list.value == 0);
		assert(data->list.unk04 == 255);
		if (checkstate != 255) data->list.unk04 = checkstate;
		break;
	}
	return 0;
}

static void testCheckboxNarration(void)
{
	struct menuitem item = {0};
	struct accessibilitymenusnapshot snapshot = {0}, previous;
	union menuitemdata data = {0};
	item.type = MENUITEMTYPE_LIST;
	item.handler = checkboxHandler;
	snapshot.item = &item;
	snapshot.itemtype = item.type;
	checkstate = 1;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(snapshot.selectedindex == 0xfffff && snapshot.checkboxstate == 1);
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "selected Alien Conflict."));
	previous = snapshot;
	checkstate = 0;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(!accessibilityMenuRelevantEqual(&previous, &snapshot));
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "Alien Conflict."));
	strcpy(snapshot.title, "Select Tune");
	accessibilityMenuCompose(&snapshot, 1, 0);
	assert(!strcmp(snapshot.utterance, "Select Tune. Alien Conflict."));
	checkstate = 0x20; /* Import/export selections expose bitmasks, not bools. */
	accessibilityMenuDescribeItem(&snapshot, &data);
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "selected Alien Conflict."));
	checkstate = 255; /* Native row has no checkbox: normal list verbosity. */
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(snapshot.checkboxstate == -1);
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "list, Alien Conflict, item 1 of 2."));
	item.flags = MENUITEMFLAG_LIST_CUSTOMRENDER;
	checkstate = 1;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(snapshot.checkboxstate == -1);
	item.flags = 0;
	item.type = snapshot.itemtype = MENUITEMTYPE_CHECKBOX;
	memset(snapshot.value, 0, sizeof(snapshot.value));
	accessibilityMenuDescribeItem(&snapshot, &data);
	strcpy(snapshot.label, "Music");
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "selected Music."));
	checkstate = 0;
	accessibilityMenuDescribeItem(&snapshot, &data);
	strcpy(snapshot.label, "Music");
	snapshot.disabled = 1;
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "Music, unavailable."));
	snapshot.disabled = 0;
	checkstate = 2; /* Ordinary renderer checks == true, not nonzero. */
	accessibilityMenuDescribeItem(&snapshot, &data);
	strcpy(snapshot.label, "Music");
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "Music."));
	/* Open dropdowns render their options through the same list renderer. */
	memset(&snapshot, 0, sizeof(snapshot));
	item.type = snapshot.itemtype = MENUITEMTYPE_DROPDOWN;
	snapshot.item = &item;
	snapshot.dimmed = 1;
	checkstate = 1;
	accessibilityMenuDescribeItem(&snapshot, &data);
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "selected Alien Conflict."));
	previous = snapshot;
	checkstate = 0;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(!accessibilityMenuRelevantEqual(&previous, &snapshot));
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "Alien Conflict."));
	snapshot.dimmed = 0;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(snapshot.checkboxstate == -1); /* No checkbox in the closed control. */
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(strstr(snapshot.utterance, "dropdown"));
}

static MenuItemHandlerResult hiddenLabel(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	return operation == MENUOP_CHECKHIDDEN;
}

static void testStaticDialogSummary(void)
{
	struct menuitem items[] = {
		{ .type = MENUITEMTYPE_LABEL, .flags = MENUITEMFLAG_ACCESSIBILITYSUMMARY, .param2 = 3 },
		{ .type = MENUITEMTYPE_LABEL, .flags = MENUITEMFLAG_ACCESSIBILITYSUMMARY, .param2 = 5,
			.handler = hiddenLabel },
		{ .type = MENUITEMTYPE_LABEL, .flags = MENUITEMFLAG_ACCESSIBILITYSUMMARY, .param2 = 4 },
		{ .type = MENUITEMTYPE_END },
	};
	struct menudialogdef dialog = { .items = items };
	struct accessibilitymenusnapshot snapshot = {0};

	assert(accessibilityMenuGetDialogSummary(&dialog, snapshot.summary, sizeof(snapshot.summary)));
	assert(!strcmp(snapshot.summary, "Agent file Cannot delete file as it is being used."));
	strcpy(snapshot.title, "Error");
	strcpy(snapshot.label, "Cancel");
	strcpy(snapshot.role, "button");
	accessibilityMenuCompose(&snapshot, true, true);
	assert(strstr(snapshot.utterance, "Cannot delete file as it is being used."));
	accessibilityMenuCompose(&snapshot, false, false);
	assert(!strcmp(snapshot.utterance, "Cancel, button."));
}

static MenuItemHandlerResult fileActionLabel(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	if (operation == MENUOP_GETACCESSIBILITYTEXT) {
		assert(data->accessibility.part == MENUACCESSIBILITYPART_CONTROL);
		snprintf(data->accessibility.buffer, data->accessibility.bufferlen,
				"Delete: Single Player Agent File");
		return 1;
	}
	return 0;
}

static void testFileActionLabel(void)
{
	struct menuitem item = { .type = MENUITEMTYPE_SELECTABLE,
		.flags = MENUITEMFLAG_ACCESSIBILITYOPTION, .handler = fileActionLabel };
	struct accessibilitymenusnapshot snapshot = { .item = &item, .itemtype = MENUITEMTYPE_SELECTABLE };

	accessibilityMenuDescribeItem(&snapshot, NULL);
	accessibilityMenuCompose(&snapshot, false, false);
	assert(!strcmp(snapshot.utterance, "Delete: Single Player Agent File, button."));
}

static s32 deleteListPending;
static MenuItemHandlerResult emptyDeleteList(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	if (operation == MENUOP_GETACCESSIBILITYTEXT) {
		if (data->accessibility.part == MENUACCESSIBILITYPART_PENDING) {
			return deleteListPending;
		}
		if (data->accessibility.part == MENUACCESSIBILITYPART_CONTROL) {
			snprintf(data->accessibility.buffer, data->accessibility.bufferlen,
					"No Combat Simulator player files to delete");
			return 1;
		}
	}
	return 0;
}

static void testEmptyDeleteList(void)
{
	struct menuitem item = { .type = MENUITEMTYPE_LIST,
		.flags = MENUITEMFLAG_LIST_CUSTOMRENDER | MENUITEMFLAG_ACCESSIBILITYOPTION,
		.handler = emptyDeleteList };
	struct accessibilitymenusnapshot snapshot = { .item = &item, .itemtype = MENUITEMTYPE_LIST };
	deleteListPending = true;
	assert(accessibilityMenuIsListPending(&item));
	deleteListPending = false;
	assert(!accessibilityMenuIsListPending(&item));
	strcpy(snapshot.title, "Delete File");
	accessibilityMenuDescribeItem(&snapshot, NULL);
	accessibilityMenuCompose(&snapshot, true, true);
	assert(!strcmp(snapshot.utterance, "Delete File. No Combat Simulator player files to delete."));
}

static MenuItemHandlerResult headCarousel(s32 operation, struct menuitem *item,
		union handlerdata *data)
{
	if (operation == MENUOP_GETOPTIONCOUNT) {
		data->carousel.value = 2;
	} else if (operation == MENUOP_GETSELECTEDINDEX) {
		data->carousel.value = selected;
	} else if (operation == MENUOP_GETACCESSIBILITYTEXT
			&& data->accessibility.part == MENUACCESSIBILITYPART_OPTION) {
		return accessibilityMenuFormatHeadOption(data->accessibility.buffer,
				data->accessibility.bufferlen,
				data->accessibility.index == 0 ? HEAD_CARRINGTON : HEAD_ELVIS_GOGS,
				data->accessibility.index);
	}
	return 0;
}

static void testHeadCarousel(void)
{
	struct menuitem item = { .type = MENUITEMTYPE_CAROUSEL, .handler = headCarousel };
	struct accessibilitymenusnapshot snapshot = { .item = &item, .itemtype = MENUITEMTYPE_CAROUSEL };
	char buffer[64];

	selected = 0;
	accessibilityMenuDescribeItem(&snapshot, NULL);
	assert(!strcmp(snapshot.value, "Localized Carrington"));
	selected = 1;
	accessibilityMenuDescribeItem(&snapshot, NULL);
	accessibilityMenuCompose(&snapshot, false, false);
	assert(!strcmp(snapshot.utterance, "carousel, Elvis with goggles, item 2 of 2."));
	assert(accessibilityMenuFormatHeadOption(buffer, sizeof(buffer), HEAD_ROSS, 1));
	assert(!strcmp(buffer, "Ross"));
	assert(accessibilityMenuFormatHeadOption(buffer, sizeof(buffer), HEAD_DARK_COMBAT, 40));
	assert(!strcmp(buffer, "Joanna Dark, combat"));
	assert(accessibilityMenuFormatHeadOption(buffer, sizeof(buffer), -1, 3));
	assert(!strcmp(buffer, "Head 4"));
	selected = 0;
}

int main(void)
{
	struct menuitem item = {0};
	struct accessibilitymenusnapshot snapshot = {0}, previous;
	union menuitemdata data = {0};
	s32 i;
	testStaticDialogSummary();
	testFileActionLabel();
	testEmptyDeleteList();
	testHeadCarousel();
	item.type = snapshot.itemtype = MENUITEMTYPE_LABEL;
	item.flags = MENUITEMFLAG_ACCESSIBILITYREADONLY;
	item.param2 = 1;
	item.param3 = 2;
	snapshot.item = &item;
	accessibilityMenuDescribeItem(&snapshot, &data);
	accessibilityMenuCompose(&snapshot, 0, 0);
	assert(!strcmp(snapshot.utterance, "Kills: 42."));
	memset(&item, 0, sizeof(item));
	memset(&snapshot, 0, sizeof(snapshot));
	item.type = MENUITEMTYPE_PLAYERSTATS;
	item.handler = statsHandler;
	snapshot.valid = 1;
	snapshot.itemtype = item.type;
	snapshot.item = &item;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(!strcmp(snapshot.label, "Player 1"));
	assert(!strcmp(snapshot.value, "Stats for Player 1"));
	assert(summaries == 1);
	snapshot.dimmed = 1;
	for (i = 0; i < ARRAYCOUNT(names); i++) {
		previous = snapshot;
		memset(snapshot.label, 0, sizeof(snapshot.label));
		memset(snapshot.value, 0, sizeof(snapshot.value));
		data.dropdown.list.index = i;
		accessibilityMenuDescribeItem(&snapshot, &data);
		assert(!strcmp(snapshot.label, names[i]));
		assert(!strcmp(snapshot.role, "dropdown"));
		assert(!snapshot.value[0]);
		assert(snapshot.subindex == i && snapshot.selectedindex == 0);
		assert(snapshot.optioncount == ARRAYCOUNT(names));
		assert(summaries == 1); /* Never request stale selected-player stats. */
		accessibilityMenuCompose(&snapshot, 0, 0);
		assert(strstr(snapshot.utterance, names[i]));
		assert(strstr(snapshot.utterance, "open"));
		if (i > 0) assert(!accessibilityMenuRelevantEqual(&previous, &snapshot));
	}
	/* Commit and cancel both restore the authoritative selected stats. */
	selected = 3;
	snapshot.dimmed = 0;
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(snapshot.subindex == 3);
	assert(!strcmp(snapshot.label, names[3]));
	assert(!strcmp(snapshot.value, "Stats for EasySim:3"));
	assert(summaries == 2);
	snapshot.dimmed = 1;
	data.dropdown.list.index = 1;
	snapshot.value[0] = '\0';
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(!strcmp(snapshot.label, names[1]));
	snapshot.dimmed = 0; /* Cancel leaves selected at 3. */
	accessibilityMenuDescribeItem(&snapshot, &data);
	assert(!strcmp(snapshot.label, names[3]));
	assert(!strcmp(snapshot.value, "Stats for EasySim:3"));
	/* Exact owner-requested punctuation, including native trailing newlines
	 * and the colon that is already part of a simulant's name. */
	memset(&snapshot, 0, sizeof(snapshot));
	snapshot.itemtype = MENUITEMTYPE_RANKING;
	strcpy(snapshot.title, "Player Ranking");
	strcpy(snapshot.role, "table");
	snapshot.subindex = -1;
	accessibilityMenuFormatRankingRow(snapshot.value, sizeof(snapshot.value),
			"Player 1\n", "Deaths\n", 2, "Score\n", 20);
	assert(!strcmp(snapshot.value, "Player 1: Deaths 2, Score 20"));
	accessibilityMenuCompose(&snapshot, 1, 0);
	assert(!strcmp(snapshot.utterance,
			"Player Ranking table. Player 1: Deaths 2, Score 20."));
	accessibilityMenuFormatRankingRow(snapshot.value, sizeof(snapshot.value),
			"EasySim:4 \n", "Deaths", 10, "Score", 13);
	assert(!strcmp(snapshot.value, "EasySim:4: Deaths 10, Score 13"));
	accessibilityMenuFormatRankingRow(snapshot.value, sizeof(snapshot.value),
			"Blue Team\n", NULL, 0, "Score", -1);
	assert(!strcmp(snapshot.value, "Blue Team: Score -1"));
	{
		char shortbuffer[5];
		accessibilityMenuFormatRankingRow(shortbuffer, sizeof(shortbuffer),
				"Long player name", "Deaths", 100, "Score", 200);
		assert(shortbuffer[sizeof(shortbuffer) - 1] == '\0');
		accessibilityMenuFormatRankingRow(NULL, 0, "", NULL, 0, "", 0);
	}
	testCheckboxNarration();
	puts("Stats selector, ranking formatting and native checkbox narration passed.");
	return 0;
}

/* Synthetic semantic menu regression, not a screen-reader acceptance test. */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_menu.c"

static s32 selected;
static s32 summaries;
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

char *menuResolveParam2Text(struct menuitem *item) { return ""; }
char *menuResolveText(uintptr_t thing, void *context) { return ""; }
char *menuitemScrollableGetText(u32 type) { return ""; }
char *langGet(s32 textid) { return ""; }
bool menuitemKeyboardIsStringEmptyOrSpaces(char *text) { return false; }
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...) { }

int main(void)
{
	struct menuitem item = {0};
	struct accessibilitymenusnapshot snapshot = {0}, previous;
	union menuitemdata data = {0};
	s32 i;
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
	puts("Stats selector: highlighted rows, stale-summary suppression and selected stats passed.");
	return 0;
}

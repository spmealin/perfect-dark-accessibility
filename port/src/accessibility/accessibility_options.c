#include <ultra64.h>
#include "constants.h"
#include "types.h"
#include "game/lang.h"
#include "game/menu.h"
#include "accessibility/accessibility_glossary.h"
#include "accessibility/accessibility_options.h"

/* Use native list rendering, fonts, spacing, input, narration and Back behavior.
 * English-only port-owned vocabulary, pending the accessibility locale catalog. */
static s32 g_GlossarySelection;

char *accessibilityOptionsMenuText(void *item)
{
	return "Accessibility\n";
}

static MenuItemHandlerResult glossaryListHandler(s32 operation,
		struct menuitem *item, union handlerdata *data)
{
	switch (operation) {
	case MENUOP_GETOPTIONCOUNT:
		/* A native list consumes up/down and wraps internally. Put Back in
		 * that same focus sequence rather than in an unreachable footer. */
		data->list.value = accessibilityGlossaryGetCount() + 1;
		break;
	case MENUOP_GETOPTGROUPCOUNT:
		data->list.value = 0;
		break;
	case MENUOP_GETOPTIONTEXT:
		if (data->list.value == accessibilityGlossaryGetCount()) {
			return (MenuItemHandlerResult)langGet(L_OPTIONS_213);
		}
		return (MenuItemHandlerResult)accessibilityGlossaryGetName(data->list.value);
	case MENUOP_GETSELECTEDINDEX:
		data->list.value = g_GlossarySelection;
		break;
	case MENUOP_LISTITEMFOCUS:
		if (data->list.value < accessibilityGlossaryGetCount()) {
			g_GlossarySelection = data->list.value;
		}
		accessibilityGlossaryStop();
		break;
	case MENUOP_SET:
		if (data->list.value == accessibilityGlossaryGetCount()) {
			accessibilityGlossaryStop();
			menuPopDialog(); /* Includes the native dialog-close sound. */
		} else {
			accessibilityGlossaryPlay(data->list.value);
		}
		break;
	}
	return 0;
}

static MenuDialogHandlerResult glossaryDialogHandler(s32 operation,
		struct menudialogdef *dialog, union handlerdata *data)
{
	if (operation == MENUOP_TICK) {
		accessibilityGlossaryTick();
	} else if (operation == MENUOP_OPEN || operation == MENUOP_CLOSE) {
		accessibilityGlossaryStop();
	}
	return 0;
}

static struct menuitem g_GlossaryItems[] = {
	{ MENUITEMTYPE_LIST, 0, MENUITEMFLAG_LIST_SILENTSELECT,
		270, 140, glossaryListHandler },
	{ MENUITEMTYPE_END },
};

static struct menudialogdef g_GlossaryDialog = {
	MENUDIALOGTYPE_DEFAULT, (uintptr_t)"Audio glossary", g_GlossaryItems,
	glossaryDialogHandler, MENUDIALOGFLAG_LITERAL_TEXT, NULL,
};

static struct menuitem g_AccessibilityOptionsItems[] = {
	{ MENUITEMTYPE_SELECTABLE, 0,
		MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_OPENSDIALOG,
		(uintptr_t)"Audio glossary\n", 0, (void *)&g_GlossaryDialog },
	{ MENUITEMTYPE_SEPARATOR, 0, 0, 0, 0, NULL },
	{ MENUITEMTYPE_SELECTABLE, 0, MENUITEMFLAG_SELECTABLE_CLOSESDIALOG,
		L_OPTIONS_213, 0, NULL },
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_AccessibilityOptionsMenuDialog = {
	MENUDIALOGTYPE_DEFAULT, (uintptr_t)"Accessibility",
	g_AccessibilityOptionsItems, NULL, MENUDIALOGFLAG_LITERAL_TEXT, NULL,
};

#include <ultra64.h>
#include "constants.h"
#include "types.h"
#include "data.h"
#include "bss.h"
#include "lib/vars.h"
#include "game/objectives.h"
#include "game/propobj.h"
#include "accessibility/accessibility_interaction.h"
#include "accessibility/accessibility_visibility.h"

struct accessibilityinteractionobjectspec {
	s32 stage;
	s32 tag;
	s32 proptype;
	enum accessibilityinteractionkind kind;
};

/* Authored action semantics belong here, not in generic scanner discovery,
 * projection or playback. This does not allocate a mission-landmark voice or
 * change ownership of the separate bridge pressure-pad landmark. */
static const struct accessibilityinteractionobjectspec
		g_AccessibilityInteractionObjects[] = {
	{ STAGE_SKEDARRUINS, 0x4c, PROPTYPE_OBJ, ACCESSIBILITY_INTERACTION_PUSHABLE },
};

s32 accessibilityInteractionGetKind(struct prop *prop)
{
	s32 i;

	if (!prop || !prop->obj) return ACCESSIBILITY_INTERACTION_NATIVE;
	for (i = 0; i < ARRAYCOUNT(g_AccessibilityInteractionObjects); i++) {
		const struct accessibilityinteractionobjectspec *spec
				= &g_AccessibilityInteractionObjects[i];
		if (g_Vars.stagenum == spec->stage && prop->type == spec->proptype
				&& objFindByTagId(spec->tag) == prop->obj) {
			return spec->kind;
		}
	}
	return ACCESSIBILITY_INTERACTION_NATIVE;
}

/* Pelagic's script remaps aliases 08..0f onto eight physical consoles each
 * playthrough. Slot zero's live image is the X-Ray-visible puzzle state.
 * Never classify generic red/green screens or fixed physical switch positions. */
s32 accessibilityInteractionGetCue(struct prop *prop)
{
	struct multimonitorobj *monitor;
	s32 tag;

	if (g_Vars.stagenum != STAGE_PELAGIC || !prop
			|| prop->type != PROPTYPE_OBJ || !prop->obj
			|| prop->obj->prop != prop || prop->obj->type != OBJTYPE_MULTIMONITOR
			|| !accessibilityVisibilityIsXrayExposed(prop)) {
		return ACCESSIBILITY_INTERACTION_DEFAULT;
	}
	for (tag = 0x08; tag <= 0x0f; tag++) {
		if (objFindByTagId(tag) == prop->obj) break;
	}
	if (tag > 0x0f) return ACCESSIBILITY_INTERACTION_DEFAULT;
	monitor = (struct multimonitorobj *)prop->obj;
	if (tvscreenIsShowingImage(&monitor->screens[0], TVCMDLIST_14)) {
		return ACCESSIBILITY_INTERACTION_COMPLETED;
	}
	if (tvscreenIsShowingImage(&monitor->screens[0], TVCMDLIST_13)) {
		return ACCESSIBILITY_INTERACTION_CORRECT;
	}
	if (tvscreenIsShowingImage(&monitor->screens[0], TVCMDLIST_12)
			|| (tag <= 0x0b
				&& tvscreenIsShowingImage(&monitor->screens[0], TVCMDLIST_0F))) {
		return ACCESSIBILITY_INTERACTION_RESET;
	}
	/* Do not invent a color during script initialization/unknown animation. */
	return ACCESSIBILITY_INTERACTION_PENDING;
}

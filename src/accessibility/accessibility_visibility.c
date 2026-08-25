#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "game/prop.h"
#include "game/propobj.h"
#include "lib/collision.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_visibility.h"

bool accessibilityVisibilityIsXrayExposed(struct prop *prop)
{
	f32 distance;

	/*
	 * The Farsight also enters VISIONMODE_XRAY. Require the native scanner's
	 * uninhibited device bit so its weapon sight cannot broaden the semantic
	 * scanners. The retained screen flag is the same preceding-frame render
	 * evidence formerly used by the generic X-Ray audio lane.
	 */
	return prop
			&& accessibilityIsXrayScannerAudioEnabled()
			&& PLAYERCOUNT() == 1
			&& g_Vars.currentplayer
			&& (g_Vars.currentplayer->devicesactive
					& ~g_Vars.currentplayer->devicesinhibit
					& DEVICE_XRAYSCANNER)
			&& (prop->flags & PROPFLAG_ONANYSCREENPREVTICK)
			&& objGetXrayHighlightDistance(prop, &distance);
}

bool accessibilityVisibilityHasVisualLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct coord *targetpos, RoomNum *targetrooms,
		struct prop *targetprop)
{
	struct defaultobj *targetobj = NULL;
	s32 restoreperimeter = false;
	s32 result;

	if (!viewpos || !viewrooms || !targetpos || !targetrooms) {
		return false;
	}

	if (targetprop && (targetprop->type == PROPTYPE_OBJ
			|| targetprop->type == PROPTYPE_WEAPON
			|| targetprop->type == PROPTYPE_DOOR)) {
		targetobj = targetprop->obj;

		/*
		 * The endpoint may be inside the target's own collision volume.
		 * Exclude only that perimeter for this synchronous main-thread query,
		 * preserving any pre-existing disabled state.
		 */
		if (targetobj
				&& (targetobj->hidden & OBJHFLAG_PERIMDISABLED) == 0) {
			propSetPerimEnabled(targetprop, false);
			restoreperimeter = true;
		}
	}

	result = cdTestLos05(viewpos, viewrooms, targetpos, targetrooms,
			CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER
				| CDTYPE_AIOPAQUE,
			GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT);

	if (restoreperimeter) {
		propSetPerimEnabled(targetprop, true);
	}

	return result;
}

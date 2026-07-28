#include <ultra64.h>
#include "constants.h"
#include "types.h"
#include "game/prop.h"
#include "lib/collision.h"
#include "accessibility/accessibility_visibility.h"

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

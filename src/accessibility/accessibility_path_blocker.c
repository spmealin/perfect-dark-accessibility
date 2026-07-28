#include <ultra64.h>
#include "constants.h"
#include "types.h"
#include "game/propobj.h"
#include "accessibility/accessibility_path_blocker.h"

bool accessibilityPathBlockerIsBreakable(const struct prop *prop)
{
	struct defaultobj *obj;

	if (!prop || prop->type != PROPTYPE_OBJ || !prop->obj
			|| !prop->active || (prop->flags & PROPFLAG_ENABLED) == 0) {
		return false;
	}

	obj = prop->obj;

	return (obj->flags & OBJFLAG_PATHBLOCKER)
			&& (obj->flags2 & OBJFLAG2_INVISIBLE) == 0
			&& (obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE)) == 0
			&& objIsHealthy(obj)
			&& objIsMortal(obj);
}

bool accessibilityPathBlockerCanTakeGunfire(const struct prop *prop)
{
	return accessibilityPathBlockerIsBreakable(prop)
			&& (prop->obj->flags2 & OBJFLAG2_IMMUNETOGUNFIRE) == 0;
}

bool accessibilityPathBlockerCanTakeExplosion(const struct prop *prop)
{
	return accessibilityPathBlockerIsBreakable(prop)
			&& (prop->obj->flags2 & OBJFLAG2_IMMUNETOEXPLOSIONS) == 0;
}

#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "game/chraction.h"
#include "game/sight.h"
#include "lib/vars.h"
#include "accessibility/accessibility_relationship.h"

s32 accessibilityRelationshipClassifyCharacter(struct prop *prop)
{
	struct chrdata *chr;

	if (!prop || !prop->chr) {
		return ACCESSIBILITY_RELATIONSHIP_UNKNOWN;
	}

	chr = prop->chr;

	if (chr->hidden2 & CHRH2FLAG_BLUESIGHT) {
		return ACCESSIBILITY_RELATIONSHIP_PROTECTED;
	}

	/*
	 * Native AI acquisition explicitly excludes TEAM_NONCOMBAT even though
	 * the generic disjoint-team comparison can otherwise report it as an
	 * enemy. Preserve that semantic exception for every accessibility user.
	 */
	if (chr->team == TEAM_NONCOMBAT) {
		return ACCESSIBILITY_RELATIONSHIP_NEUTRAL;
	}

	if (sightIsPropFriendly(prop)) {
		return ACCESSIBILITY_RELATIONSHIP_FRIENDLY;
	}

	if (g_Vars.currentplayer && g_Vars.currentplayer->prop
			&& g_Vars.currentplayer->prop->chr
			&& chrCompareTeams(g_Vars.currentplayer->prop->chr, chr,
				COMPARE_ENEMIES)) {
		return ACCESSIBILITY_RELATIONSHIP_HOSTILE;
	}

	return ACCESSIBILITY_RELATIONSHIP_NEUTRAL;
}

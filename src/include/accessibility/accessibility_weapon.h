#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_WEAPON_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_WEAPON_H

#include <ultra64.h>

void accessibilityWeaponFunctionObserve(s32 playernum, s32 stagenum,
		s32 weaponnum, s32 secondary, s32 dual,
		const char *visiblefunctionname);
void accessibilityWeaponActiveMenuObserve(s32 playernum, s32 open,
		s32 selectedweaponnum);
void accessibilityWeaponQuickChangeRequested(s32 playernum,
		s32 selectedweaponnum);
void accessibilityWeaponFunctionReset(const char *reason);

#endif

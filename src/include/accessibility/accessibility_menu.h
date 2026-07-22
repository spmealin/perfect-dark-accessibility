#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_MENU_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_MENU_H

#include <PR/ultratypes.h>

struct menu;

void accessibilityMenuObserve(s32 menuslot, s32 playernum, s32 menuroot,
		s32 menudepth, struct menu *menu);
void accessibilityMenuObserveActive(s32 playernum);
void accessibilityMenuReset(void);

#endif

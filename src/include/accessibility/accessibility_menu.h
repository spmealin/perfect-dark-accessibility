#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_MENU_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_MENU_H

#include <PR/ultratypes.h>

struct menu;

s32 accessibilityMenuFormatHeadOption(char *buffer, u32 bufferlen,
		s32 headid, s32 optionindex);

void accessibilityMenuObserve(s32 menuslot, s32 playernum, s32 menuroot,
		s32 menudepth, struct menu *menu);
void accessibilityMenuObserveActive(s32 playernum);
void accessibilityMenuReset(void);
/* Caller supplies the native name and localized column labels. A NULL deaths
 * label denotes a team row, whose visible table has only a score column. */
void accessibilityMenuFormatRankingRow(char *buffer, u32 bufferlen,
		const char *name, const char *deathslabel, s32 deaths,
		const char *scorelabel, s32 score);

#endif

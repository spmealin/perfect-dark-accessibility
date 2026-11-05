#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_weapon.c"
struct g_vars g_Vars;
static struct player player;
static s32 calls;
static s32 inventory;
static char names[8][80];
s32 accessibilityIsWeaponChangeAnnouncementsEnabled(void) { return 1; }
s32 invGetCount(void) { return inventory; }
s32 invGetWeaponNumByIndex(s32 index) { assert(index == 0); return WEAPON_DRAGON; }
u16 invGetNameIdByIndex(s32 index) { assert(index == 0); return 1000; }
char *bgunGetName(s32 weapon)
{
	return weapon == WEAPON_FALCON2 ? "Falcon 2" : "Dragon";
}
char *langGet(s32 id) { if (id == 1000) return "Mission alias"; assert(id == L_PROPOBJ_001); return "Double "; }
s32 accessibilityAnnouncementWeaponChange(const char *text, const char *source, s32 playernum, s32 interrupt)
{
	assert(!strcmp(source, "quick_selection") && playernum == 0 && interrupt);
	assert(calls < 8); snprintf(names[calls++], sizeof(names[0]), "%s", text); return 1;
}
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...) { }
int main(void)
{
	struct accessibilityweaponfunctionstate *state = &g_AccessibilityWeaponFunctionStates[0];
	g_Vars.currentplayer = &player;
	state->initialized = state->requestedvalid = 1;
	state->weaponnum = state->requestedweapon = WEAPON_FALCON2;
	accessibilityWeaponQuickChangeRequested(0, WEAPON_FALCON2);
	assert(!calls); /* Native cycle did not change selection. */
	accessibilityWeaponQuickChangeRequested(0, WEAPON_DRAGON);
	accessibilityWeaponQuickChangeRequested(0, WEAPON_FALCON2);
	assert(calls == 2 && !strcmp(names[0], "Dragon") && !strcmp(names[1], "Falcon 2"));
	assert(state->weaponnum == WEAPON_FALCON2); /* No equip observation needed. */
	player.gunctrl.dualwielding = 1;
	accessibilityWeaponQuickChangeRequested(0, WEAPON_FALCON2);
	assert(calls == 3 && !strcmp(names[2], "Double Falcon 2"));
	accessibilityWeaponQuickChangeRequested(0, WEAPON_FALCON2);
	assert(calls == 3); /* Duplicate identity, not another inventory selection. */
	player.gunctrl.dualwielding = 0;
	accessibilityWeaponQuickChangeRequested(0, WEAPON_FALCON2);
	assert(calls == 4 && !strcmp(names[3], "Falcon 2"));
	assert(state->pendingweaponnum == WEAPON_FALCON2 && state->pendingsource == ACCESSIBILITY_WEAPON_CHANGE_QUICK);
	inventory = 1;
	accessibilityWeaponQuickChangeRequested(0, WEAPON_DRAGON);
	assert(calls == 5 && !strcmp(names[4], "Mission alias"));
	puts("Quick weapon names: immediate intermediate selections, return-to-equipped and dual identities passed.");
	return 0;
}

/* Exercise the real adapter gate, not rendered pixels or auto-seek identity. */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_targeting_game.c"

struct g_vars g_Vars;
static struct player testplayer;
static struct playerstats teststats;
static s32 sight = SIGHT_MAIAN, weapon = WEAPON_FARSIGHT;
static s32 sightenabled = 1, alwaystarget = 1;
u32 currentPlayerGetSight(void) { return sight; }
s32 bgunGetWeaponNum(s32 hand) { return weapon; }
s32 optionsGetSightOnScreen(s32 player) { return sightenabled; }
s32 optionsGetAlwaysShowTarget(s32 player) { return alwaystarget; }
bool sightHasTargetWhileAiming(s32 value) { return value == SIGHT_DEFAULT || value == SIGHT_ZOOM; }

int main(void)
{
	g_Vars.currentplayer = &testplayer;
	g_Vars.currentplayerstats = &teststats;
	testplayer.activemenumode = AMMODE_CLOSED;
	testplayer.bondhealth = 1;
	for (s32 mode = FUNC_PRIMARY; mode <= FUNC_SECONDARY; mode++) {
		testplayer.hands[HAND_RIGHT].gset.weaponfunc = mode;
		for (alwaystarget = 0; alwaystarget <= 1; alwaystarget++) {
			testplayer.gunsightoff = 0;
			assert(accessibilityTargetingGameTargetIndicatorVisible());
			testplayer.gunsightoff = GUNSIGHTREASON_NOTAIMING;
			assert(accessibilityTargetingGameTargetIndicatorVisible() == alwaystarget);
		}
	}
	alwaystarget = 0;
	testplayer.gunsightoff = 0;
	sightenabled = 0; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	sightenabled = 1;
	testplayer.activemenumode = AMMODE_VIEW; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	testplayer.activemenumode = AMMODE_CLOSED;
	testplayer.gunctrl.passivemode = 1; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	testplayer.gunctrl.passivemode = 0;
	testplayer.bondhealth = 0; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	testplayer.bondhealth = 1;
	sight = SIGHT_NONE; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	weapon = WEAPON_FALCON2;
	sight = SIGHT_DEFAULT; assert(accessibilityTargetingGameTargetIndicatorVisible());
	sight = SIGHT_ZOOM; assert(accessibilityTargetingGameTargetIndicatorVisible());
	sight = SIGHT_MAIAN; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	g_Vars.currentplayerstats = NULL; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	g_Vars.currentplayer = NULL; assert(!accessibilityTargetingGameTargetIndicatorVisible());
	puts("FarSight reticle: both functions, sight preferences, hidden/passive/dead/menu gates and ordinary sights passed.");
	return 0;
}

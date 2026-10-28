/* Semantic regression, not a listening or firing test. Build like the precision
 * mapping fixture, with -O2 -fwhole-program to discard unrelated adapter paths. */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_targeting_game.c"

static struct weaponfunc attack;
static s32 equippedweapon = WEAPON_FALCON2;
struct weaponfunc *currentPlayerGetWeaponFunction(u32 hand) { return &attack; }
s32 bgunGetWeaponNum(s32 hand) { return equippedweapon; }
bool objIsHealthy(struct defaultobj *obj) { return obj->damage < 100; }
bool objIsMortal(struct defaultobj *obj) { return obj->type != OBJTYPE_WEAPON; }

static struct prop prop;
static struct weaponobj weapon;

static void reset(s32 weaponnum)
{
	memset(&prop, 0, sizeof(prop));
	memset(&weapon, 0, sizeof(weapon));
	memset(&attack, 0, sizeof(attack));
	prop.type = PROPTYPE_WEAPON;
	prop.active = true;
	prop.flags = PROPFLAG_ENABLED;
	prop.obj = &weapon.base;
	weapon.base.prop = &prop;
	weapon.base.type = OBJTYPE_WEAPON;
	weapon.weaponnum = weaponnum;
	weapon.timer240 = 1;
	attack.type = INVENTORYFUNCTYPE_SHOOT;
	equippedweapon = WEAPON_FALCON2;
}

static s32 accepted(void)
{
	return accessibilityTargetingGameIsDestroyableObject(&prop)
			&& accessibilityTargetingGameCurrentAttackCanDamageObject(&prop);
}

int main(void)
{
	const s32 explosives[] = { WEAPON_GRENADE, WEAPON_TIMEDMINE,
		WEAPON_REMOTEMINE, WEAPON_PROXIMITYMINE, WEAPON_ROCKET,
		WEAPON_HOMINGROCKET, WEAPON_GRENADEROUND };
	struct accessibilitytargetingcandidate original = {0}, copy;
	struct prop parent = {0};
	s32 i;

	assert(!accessibilityTargetingGameIsDetonatableWeapon(NULL));
	for (i = 0; i < ARRAYCOUNT(explosives); i++) {
		reset(explosives[i]);
		assert(!objIsMortal(&weapon.base));
		assert(accepted());
		prop.type = PROPTYPE_OBJ;
		assert(accepted());
	}
	reset(WEAPON_DRAGON);
	assert(!accepted());
	weapon.gunfunc = FUNC_SECONDARY;
	assert(accepted());
	weapon.gunfunc = FUNC_PRIMARY;
	assert(!accepted());
	reset(WEAPON_FALCON2); assert(!accepted());
	reset(WEAPON_NBOMB); assert(!accepted());
	reset(WEAPON_ECMMINE); assert(!accepted());
	reset(WEAPON_FALCON2);
	weapon.base.flags2 |= OBJFLAG2_AICANNOTUSE;
	assert(accepted()); /* Native special explosive-weapon flag. */

	reset(WEAPON_PROXIMITYMINE);
	weapon.base.flags |= OBJFLAG_INVINCIBLE; assert(!accepted());
	weapon.base.flags = OBJFLAG_DEACTIVATED; assert(!accepted());
	weapon.base.flags = 0;
	weapon.base.flags2 = OBJFLAG2_INVISIBLE; assert(!accepted());
	weapon.base.flags2 = OBJFLAG2_IMMUNETOGUNFIRE; assert(!accepted());
	weapon.base.flags2 = 0;
	weapon.base.hidden = OBJHFLAG_DELETING; assert(!accepted());
	weapon.base.hidden = OBJHFLAG_GONE; assert(!accepted());
	weapon.base.hidden = 0;
	weapon.base.damage = 100; assert(!accepted());
	weapon.base.damage = 0;
	prop.active = false; assert(!accepted());
	prop.active = true;
	prop.flags = 0; assert(!accepted());
	prop.flags = PROPFLAG_ENABLED;
	weapon.base.prop = NULL; assert(!accepted());
	weapon.base.prop = &prop;
	prop.parent = &parent;
	parent.type = PROPTYPE_CHR; assert(!accepted());
	parent.type = PROPTYPE_PLAYER; assert(!accepted());
	parent.type = PROPTYPE_OBJ; assert(accepted()); /* Attached mine. */
	prop.parent = NULL;
	attack.type = INVENTORYFUNCTYPE_DEVICE; assert(!accepted());
	attack.type = INVENTORYFUNCTYPE_MELEE; assert(!accepted());
	attack.type = INVENTORYFUNCTYPE_THROW; assert(accepted());
	weapon.base.flags2 = OBJFLAG2_IMMUNETOEXPLOSIONS; assert(!accepted());
	reset(WEAPON_HOMINGROCKET);
	attack.type = INVENTORYFUNCTYPE_THROW;
	equippedweapon = WEAPON_REMOTEMINE; assert(!accepted());

	reset(WEAPON_DRAGON);
	weapon.gunfunc = FUNC_SECONDARY;
	original.prop = &prop;
	original.category = ACCESSIBILITY_TARGETING_CATEGORY_OBJECT;
	original.relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
	copy = original;
	assert(accessibilityTargetingGamePrepareWeaponThreat(&copy));
	assert(copy.category == ACCESSIBILITY_TARGETING_CATEGORY_DESTROYABLE_OBJECT);
	assert(original.category == ACCESSIBILITY_TARGETING_CATEGORY_OBJECT);
	assert(copy.relationship == original.relationship);
	weapon.base.flags2 = OBJFLAG2_IMMUNETOGUNFIRE;
	assert(!accessibilityTargetingGamePrepareWeaponThreat(&copy));
	reset(WEAPON_NBOMB);
	assert(!accessibilityTargetingGamePrepareWeaponThreat(&copy));
	weapon.base.type = OBJTYPE_AUTOGUN;
	assert(accessibilityTargetingGamePrepareWeaponThreat(&copy));
	puts("Explosive weapon targeting: classification, lifecycle, attack compatibility and detector-copy tests passed.");
	return 0;
}

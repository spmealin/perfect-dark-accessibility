#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_TARGETING_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_TARGETING_H

#include <ultra64.h>
#include <stdint.h>
#include "types.h"

#define ACCESSIBILITY_TARGETING_MAX_CANDIDATES 32

enum accessibilitytargetingsource {
	ACCESSIBILITY_TARGETING_SOURCE_NONE = 0,
	ACCESSIBILITY_TARGETING_SOURCE_FIRING_RANGE = 1,
	ACCESSIBILITY_TARGETING_SOURCE_COMBAT = 2,
};

enum accessibilitytargetingprofile {
	ACCESSIBILITY_TARGETING_PROFILE_NONE = 0,
	ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE = 1,
	ACCESSIBILITY_TARGETING_PROFILE_COMBAT = 2,
};

enum accessibilitytargetingcategory {
	ACCESSIBILITY_TARGETING_CATEGORY_UNKNOWN = 0,
	ACCESSIBILITY_TARGETING_CATEGORY_RANGE_TARGET = 1,
	ACCESSIBILITY_TARGETING_CATEGORY_CHARACTER = 2,
	ACCESSIBILITY_TARGETING_CATEGORY_PLAYER = 3,
	ACCESSIBILITY_TARGETING_CATEGORY_TURRET = 4,
	ACCESSIBILITY_TARGETING_CATEGORY_VEHICLE = 5,
	ACCESSIBILITY_TARGETING_CATEGORY_OBJECT = 6,
};

enum accessibilitytargetingrelationship {
	ACCESSIBILITY_TARGETING_RELATIONSHIP_UNKNOWN = 0,
	ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE = 1,
	ACCESSIBILITY_TARGETING_RELATIONSHIP_FRIENDLY = 2,
	ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL = 3,
};

enum accessibilitytargetingshootability {
	ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN = 0,
	ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE = 1,
	ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY = 2,
};

struct accessibilitytargetingidentity {
	s32 playernum;
	s32 source;
	s32 sourceslot;
	s32 propnum;
	s32 proptype;
	uintptr_t objectidentity;
};

struct accessibilitytargetingcandidate {
	struct accessibilitytargetingidentity identity;
	struct prop *prop;
	s32 category;
	s32 relationship;
	s32 shootability;
	struct coord position;
	f32 distance;
	f32 screenx1;
	f32 screeny1;
	f32 screenx2;
	f32 screeny2;
	f32 horizontalscreenoffset;
	u32 knowledgeflags;
	const char *localizedname;
};

struct accessibilitytargetingobservation {
	s32 playernum;
	s32 source;
	s32 profile;
	s32 stagenum;
	s32 frame60;
	s32 inscope;
	s32 sighton;
	s32 targetindicatorvisible;
	s32 nativealignmentexpected;
	s32 candidatecount;
	struct accessibilitytargetingcandidate candidates[ACCESSIBILITY_TARGETING_MAX_CANDIDATES];
	s32 hasaimedtarget;
	struct accessibilitytargetingidentity aimedidentity;
};

void accessibilityTargetingObserve(const struct accessibilitytargetingobservation *observation);
void accessibilityTargetingCaptureGame(void);
void accessibilityTargetingObserveGame(void);
void accessibilityTargetingReset(const char *reason);

#endif

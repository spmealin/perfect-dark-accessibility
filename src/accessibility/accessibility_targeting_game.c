#include <ultra64.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "system.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/bondgun.h"
#include "game/bondmove.h"
#include "game/camera.h"
#include "game/game_0b0fd0.h"
#include "game/lv.h"
#include "game/objectives.h"
#include "game/propobj.h"
#include "game/sight.h"
#include "game/training.h"
#include "lib/collision.h"
#include "lib/model.h"
#include "lib/mtx.h"
#include "lib/vars.h"
#include "lib/vi.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_path_blocker.h"
#include "accessibility/accessibility_targeting.h"
#include "accessibility/accessibility_visibility.h"

#define ACCESSIBILITY_TARGETING_AUDIT_TICKS TICKS(60)
#define ACCESSIBILITY_TARGETING_RANGE_OUTER_RADIUS 75.0f
#define ACCESSIBILITY_TARGETING_COMBAT_PROJECTION_CAPACITY \
	ACCESSIBILITY_TARGETING_MAX_CANDIDATES
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_NONE 0
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_CENTER 1
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER 2
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_LOWER 3
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_LEFT 4
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_RIGHT 5
#define ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_COUNT 5
#define ACCESSIBILITY_TARGETING_TURRET_AIM_TOLERANCE 3.0f
#define ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT 5
#define ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN 12.0f
#define ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES 5
#define ACCESSIBILITY_TARGETING_PRECISION_FINE_QUERY_BUDGET 15
#define ACCESSIBILITY_TARGETING_PRECISION_FINE_LOG_TICKS TICKS(60)

enum accessibilitytargetingprecisionanchorsource {
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NONE = 0,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_HITBOX = 1,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NATIVE_AUTOAIM = 2,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_GEOMETRY = 3,
};

enum accessibilitytargetingprecisionanchorslot {
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_HEAD = 0,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_TORSO = 1,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_ARM = 2,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_LOWER = 3,
	ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_OTHER = 4,
};

struct accessibilitytargetingprecisionanchor {
	uintptr_t node;
	s32 valid;
	s32 hitpart;
	f32 screenx;
	f32 screeny;
	f32 score;
};

struct accessibilitytargetinggameaudit {
	uintptr_t prop;
	uintptr_t obj;
	s32 propnum;
	s32 proptype;
	s32 modelnum;
	u32 propflags;
	u32 objflags2;
	u8 inuse;
	u8 active;
	u8 destroyed;
	u8 accepted;
	s32 shootability;
	const char *reason;
};

struct accessibilitytargetinggameprojection {
	uintptr_t prop;
	uintptr_t obj;
	s32 propnum;
	s32 projected;
	s32 finite;
	f32 x2;
	f32 x1;
	f32 y2;
	f32 y1;
};

struct accessibilitytargetingcombatprojection {
	uintptr_t prop;
	uintptr_t chr;
	uintptr_t obj;
	s32 propnum;
	s32 category;
	s32 projected;
	s32 finite;
	s32 lineofsight;
	s32 visibilitysample;
	s32 visibilityqueries;
	s32 hasverticalaimerror;
	f32 x2;
	f32 x1;
	f32 y2;
	f32 y1;
	f32 verticalaimerrordegrees;
	f32 aimscreenx;
	f32 aimscreeny;
	struct accessibilitytargetingprecisionanchor
			precisionanchors[ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT];
	s32 precisionanchornodesexamined;
	s32 precisionfallbackvalid;
	f32 precisionfallbackscreenx;
	f32 precisionfallbackscreeny;
	s32 precisionfineattempted;
	s32 precisionfineavailable;
	s32 precisionfinehitpart;
	uintptr_t precisionfinenode;
	s32 precisionfinesample;
	s32 precisionfinequeries;
	s32 precisionfinebudgetexhausted;
	u64 precisionfineelapsedus;
	f32 precisionfinescreenx;
	f32 precisionfinescreeny;
};

struct accessibilitytargetingdevicetarget {
	s32 stagenum;
	s32 weaponnum;
	s32 tagid;
	s32 trainingonly;
};

struct accessibilitytargetingcamspytarget {
	uintptr_t criteria;
	uintptr_t obj;
	uintptr_t prop;
	s32 tagid;
	s32 propnum;
	s32 status;
	s32 eligible;
	const char *reason;
	f32 distance;
	f32 screenx1;
	f32 screeny1;
	f32 screenx2;
	f32 screeny2;
};

static const struct accessibilitytargetingdevicetarget
		g_AccessibilityTargetingDeviceTargets[] = {
	{ STAGE_CITRAINING, WEAPON_DATAUPLINK, 0x30, true },
	{ STAGE_CITRAINING, WEAPON_ECMMINE, 0x32, true },
	{ STAGE_CITRAINING, WEAPON_DOORDECODER, 0x35, true },
	{ STAGE_INVESTIGATION, WEAPON_DATAUPLINK, 0x0a, false },
	{ STAGE_DEFECTION, WEAPON_ECMMINE, 0x03, false },
	{ STAGE_DEFECTION, WEAPON_ECMMINE, 0x04, false },
};

static struct accessibilitytargetinggameaudit
		g_AccessibilityTargetingGameAudit[18];
static s32 g_AccessibilityTargetingGameAuditValid;
static s32 g_AccessibilityTargetingGameNextAudit60;
static s32 g_AccessibilityTargetingGameLastFrame60;
static const char *g_AccessibilityTargetingGameLastScopeReason;
static uintptr_t g_AccessibilityTargetingGameLastRejectedAim;
static struct accessibilitytargetinggameprojection
		g_AccessibilityTargetingGameProjections[18];
static s32 g_AccessibilityTargetingGameProjectionFrame60 = -1;
static s32 g_AccessibilityTargetingGameProjectionPlayer = -1;
static s32 g_AccessibilityTargetingGameProjectionsValid;
static uintptr_t g_AccessibilityTargetingGameAimProp;
static struct coord g_AccessibilityTargetingGameAimHitPos;
static s32 g_AccessibilityTargetingGameAimHitValid;
static uintptr_t g_AccessibilityTargetingGameRawAimProp;
static struct coord g_AccessibilityTargetingGameRawAimHitPos;
static s32 g_AccessibilityTargetingGameRawAimHitValid;
static s32 g_AccessibilityTargetingGameRawAimHitPart;
static s32 g_AccessibilityTargetingGameRawAimHitPartValid;
static struct accessibilitytargetingcombatprojection
		g_AccessibilityTargetingCombatProjections[
			ACCESSIBILITY_TARGETING_COMBAT_PROJECTION_CAPACITY];
static s32 g_AccessibilityTargetingCombatProjectionCount;
static struct accessibilitytargetingcamspytarget
		g_AccessibilityTargetingCamSpyTargets[
			ACCESSIBILITY_TARGETING_MAX_CANDIDATES];
static s32 g_AccessibilityTargetingCamSpyTargetCount;
static s32 g_AccessibilityTargetingGameLastSource;
static s32 g_AccessibilityTargetingPrecisionFineNextLog60;
static s32 g_AccessibilityTargetingPrecisionFineLastStage = -1;
static s32 g_AccessibilityTargetingPrecisionFineLastPropnum = -1;
static s32 g_AccessibilityTargetingPrecisionFineLastResult = -1;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
static struct accessibilitytargetingdiagnostics
		g_AccessibilityTargetingDiagnostics;
#endif

static s32 accessibilityTargetingGameRelationship(struct prop *prop);
static s32 accessibilityTargetingGamePropNum(const struct prop *prop);

static s32 accessibilityTargetingGameAimRegion(s32 hitpart)
{
	if (hitpart == HITPART_HEAD) {
		return ACCESSIBILITY_TARGETING_AIM_REGION_HEAD;
	}

	switch (hitpart) {
	case HITPART_LHAND:
	case HITPART_LFOREARM:
	case HITPART_LBICEP:
	case HITPART_RHAND:
	case HITPART_RFOREARM:
	case HITPART_RBICEP:
		return ACCESSIBILITY_TARGETING_AIM_REGION_ARM;
	}

	return hitpart > 0
			? ACCESSIBILITY_TARGETING_AIM_REGION_STANDARD
			: ACCESSIBILITY_TARGETING_AIM_REGION_NONE;
}

static s32 accessibilityTargetingGamePrecisionAnchorSlot(s32 hitpart)
{
	if (hitpart == HITPART_HEAD) {
		return ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_HEAD;
	}

	switch (hitpart) {
	case HITPART_LHAND:
	case HITPART_LFOREARM:
	case HITPART_LBICEP:
	case HITPART_RHAND:
	case HITPART_RFOREARM:
	case HITPART_RBICEP:
		return ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_ARM;
	case HITPART_LFOOT:
	case HITPART_LSHIN:
	case HITPART_LTHIGH:
	case HITPART_RFOOT:
	case HITPART_RSHIN:
	case HITPART_RTHIGH:
	case HITPART_PELVIS:
		return ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_LOWER;
	case HITPART_TORSO:
		return ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_TORSO;
	case HITPART_TAIL:
	case HITPART_GENERAL:
	case HITPART_GENERALHALF:
		return ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_OTHER;
	}

	return -1;
}

static f32 accessibilityTargetingGamePrecisionAnchorScore(f32 screenx,
		f32 screeny)
{
	f32 halfwidth = camGetScreenWidth() * 0.5f;
	f32 halfheight = camGetScreenHeight() * 0.5f;
	f32 dx;
	f32 dy;

	if (halfwidth <= 0.0f || halfheight <= 0.0f) {
		return MAXFLOAT;
	}

	dx = (screenx - g_Vars.currentplayer->crosspos[0]) / halfwidth;
	dy = (g_Vars.currentplayer->crosspos[1] - screeny) / halfheight;
	return sqrtf(dx * dx + dy * dy);
}

static void accessibilityTargetingGameCapturePrecisionAnchors(
		struct prop *prop, struct chrdata *chr,
		struct accessibilitytargetingcombatprojection *projection)
{
	struct model *model = chr ? chr->model : NULL;
	struct modelnode *node;
	s32 validanchors = 0;
	s32 i;

	if (!prop || !model || !model->definition || !model->matrices
			|| bgunGetWeaponNum(HAND_RIGHT) != WEAPON_SNIPERRIFLE
			|| g_Vars.currentplayer->zoominfovy <= 0.0f
			|| g_Vars.currentplayer->zoominfovy >= PLAYER_DEFAULT_FOV) {
		return;
	}

	node = model->definition->rootnode;

	while (node) {
		if ((node->type & 0xff) == MODELNODETYPE_BBOX) {
			struct modelrodata_bbox *bbox = &node->rodata->bbox;
			s32 slot = accessibilityTargetingGamePrecisionAnchorSlot(
					bbox->hitpart);

			projection->precisionanchornodesexamined++;

			if (slot >= 0) {
				Mtxf *mtx = modelFindNodeMtx(model, node, 0);
				struct coord local;
				struct coord cameracoord;
				f32 screen[2];
				f32 score;

				local.x = (bbox->xmin + bbox->xmax) * 0.5f;
				local.y = (bbox->ymin + bbox->ymax) * 0.5f;
				local.z = (bbox->zmin + bbox->zmax) * 0.5f;

				if (mtx) {
					mtx4TransformVec(mtx, &local, &cameracoord);
				}

				if (mtx && cameracoord.z < 0.0f) {
					cam0f0b4eb8(&cameracoord, screen, viGetFovY(),
							viGetAspect());
					score = accessibilityTargetingGamePrecisionAnchorScore(
							screen[0], screen[1]);

					if (isfinite(screen[0]) && isfinite(screen[1])
							&& isfinite(score)
							&& (!projection->precisionanchors[slot].valid
								|| score < projection->precisionanchors[slot].score)) {
						projection->precisionanchors[slot].node
								= (uintptr_t)node;
						projection->precisionanchors[slot].valid = true;
						projection->precisionanchors[slot].hitpart
								= bbox->hitpart;
						projection->precisionanchors[slot].screenx = screen[0];
						projection->precisionanchors[slot].screeny = screen[1];
						projection->precisionanchors[slot].score = score;
					}
				}
			}
		}

		if (node->child) {
			node = node->child;
		} else {
			while (node) {
				if (node->next) {
					node = node->next;
					break;
				}

				node = node->parent;
			}
		}
	}

	for (i = 0; i < ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT; i++) {
		validanchors += projection->precisionanchors[i].valid;
	}

	if (!validanchors) {
		struct coord nativepos;
		f32 xbounds[2];
		f32 ybounds[2];
		f32 screen[2];

		if (chrCalculateAutoAim(prop, &nativepos, xbounds, ybounds)) {
			cam0f0b4eb8(&nativepos, screen, viGetFovY(), viGetAspect());

			if (isfinite(screen[0]) && isfinite(screen[1])) {
				projection->precisionfallbackvalid = true;
				projection->precisionfallbackscreenx = screen[0];
				projection->precisionfallbackscreeny = screen[1];
			}
		}
	}
}

static const struct accessibilitytargetingprecisionanchor *
accessibilityTargetingGameSelectPrecisionAnchor(
		const struct accessibilitytargetingcombatprojection *projection,
		f32 *selectedscore)
{
	static const f32 centerpenalties[ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT]
			= { 0.04f, 0.00f, 0.05f, 0.03f, 0.05f };
	static const f32 upperpenalties[ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT]
			= { 0.00f, 0.015f, 0.02f, 0.06f, 0.04f };
	static const f32 lowerpenalties[ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT]
			= { 0.08f, 0.02f, 0.06f, 0.00f, 0.04f };
	static const f32 sidepenalties[ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT]
			= { 0.01f, 0.02f, 0.00f, 0.06f, 0.04f };
	const f32 *penalties = centerpenalties;
	const struct accessibilitytargetingprecisionanchor *best = NULL;
	f32 bestscore = 0.0f;
	s32 i;

	if (projection->visibilitysample == ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER) {
		penalties = upperpenalties;
	} else if (projection->visibilitysample
			== ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_LOWER) {
		penalties = lowerpenalties;
	} else if (projection->visibilitysample
			== ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_LEFT
			|| projection->visibilitysample
				== ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_RIGHT) {
		penalties = sidepenalties;
	}

	for (i = 0; i < ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT; i++) {
		const struct accessibilitytargetingprecisionanchor *anchor
				= &projection->precisionanchors[i];
		f32 score;

		if (!anchor->valid) {
			continue;
		}

		score = anchor->score + penalties[i];
		if (!best || score < bestscore) {
			best = anchor;
			bestscore = score;
		}
	}

	if (selectedscore) {
		*selectedscore = best ? bestscore : -1.0f;
	}

	return best;
}

static const char *accessibilityTargetingGamePrecisionAnchorSourceName(
		s32 source)
{
	switch (source) {
	case ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_GEOMETRY:
		return "model_geometry";
	case ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_HITBOX:
		return "model_hitbox";
	case ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NATIVE_AUTOAIM:
		return "native_autoaim_fallback";
	}

	return "none";
}

static s32 accessibilityTargetingGameProjectPrecisionSample(
		struct model *model, struct modelnode *node, s32 sample,
		f32 *screenx, f32 *screeny)
{
	static const f32 xfactors[ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES]
			= { 0.0f, -0.22f, 0.22f, 0.0f, 0.0f };
	static const f32 yfactors[ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES]
			= { 0.0f, 0.0f, 0.0f, -0.22f, 0.22f };
	struct modelrodata_bbox *bbox;
	struct coord local;
	struct coord cameracoord;
	f32 screen[2];
	Mtxf *mtx;

	if (!model || !node || sample < 0
			|| sample >= ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES
			|| (node->type & 0xff) != MODELNODETYPE_BBOX) {
		return false;
	}

	bbox = &node->rodata->bbox;
	mtx = modelFindNodeMtx(model, node, 0);

	if (!mtx) {
		return false;
	}

	local.x = (bbox->xmin + bbox->xmax) * 0.5f
			+ (bbox->xmax - bbox->xmin) * xfactors[sample];
	local.y = (bbox->ymin + bbox->ymax) * 0.5f
			+ (bbox->ymax - bbox->ymin) * yfactors[sample];
	local.z = (bbox->zmin + bbox->zmax) * 0.5f;
	mtx4TransformVec(mtx, &local, &cameracoord);

	if (cameracoord.z >= 0.0f) {
		return false;
	}

	cam0f0b4eb8(&cameracoord, screen, viGetFovY(), viGetAspect());

	if (!isfinite(screen[0]) || !isfinite(screen[1])) {
		return false;
	}

	*screenx = screen[0];
	*screeny = screen[1];
	return true;
}

static s32 accessibilityTargetingGameTestPrecisionGeometry(
		struct prop *prop, struct chrdata *chr, f32 screenx, f32 screeny,
		RoomNum *camrooms, s32 *hitpart, uintptr_t *hitnode)
{
	struct model *model = chr ? chr->model : NULL;
	struct modelnode *bboxnode = NULL;
	struct modelnode *polygonnode = NULL;
	struct modelnode *dlnode = NULL;
	struct coord gunpos = { 0.0f, 0.0f, 0.0f };
	struct coord gundir;
	struct coord polygonhit;
	struct coord worldhit;
	f32 screen[2] = { screenx, screeny };
	f32 distance;
	s32 matrixindex;
	s32 part;

	if (!prop || !chr || !model || !model->definition || !model->matrices) {
		return false;
	}

	cam0f0b4c3c(screen, &gundir, 1);
	part = modelTestForHit(model, &gunpos, &gundir, &bboxnode);

	if (part <= 0) {
		return false;
	}

	if (chrGetShield(chr) <= 0.0f) {
		if (!func0f06bea0(model, model->definition->rootnode,
				model->definition->rootnode, &gunpos, &gundir,
				&polygonhit, &distance, &polygonnode, &part,
				&matrixindex, &dlnode)) {
			return false;
		}

		mtx4TransformVec(camGetProjectionMtxF(), &polygonhit, &worldhit);

		if (!accessibilityVisibilityHasVisualLineOfSight(
				&g_Vars.currentplayer->cam_pos, camrooms,
				&worldhit, prop->rooms, prop)) {
			return false;
		}
	} else {
		polygonnode = bboxnode;
	}

	*hitpart = part;
	*hitnode = (uintptr_t)polygonnode;
	return true;
}

static s32 accessibilityTargetingGamePrecisionFineEnvelope(
		const struct accessibilitytargetingcombatprojection *projection)
{
	f32 aimx = g_Vars.currentplayer->crosspos[0];
	f32 aimy = g_Vars.currentplayer->crosspos[1];
	f32 left = fminf(projection->x1, projection->x2)
			- ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN;
	f32 right = fmaxf(projection->x1, projection->x2)
			+ ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN;
	f32 top = fminf(projection->y1, projection->y2)
			- ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN;
	f32 bottom = fmaxf(projection->y1, projection->y2)
			+ ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN;

	return aimx >= left && aimx <= right && aimy >= top && aimy <= bottom;
}

static void accessibilityTargetingGameRefinePrecisionProjection(
		struct accessibilitytargetingcombatprojection *projection,
		RoomNum *camrooms)
{
	struct prop *prop = (struct prop *)projection->prop;
	struct chrdata *chr = (struct chrdata *)projection->chr;
	struct model *model = chr ? chr->model : NULL;
	const struct accessibilitytargetingprecisionanchor *preferred;
	u32 testedslots = 0;
	s32 querybudget = ACCESSIBILITY_TARGETING_PRECISION_FINE_QUERY_BUDGET;
	s32 result = false;
	s32 group;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	u64 started = sysGetMicroseconds();
#endif

	projection->precisionfineattempted = true;
	projection->precisionfinesample = -1;
	preferred = accessibilityTargetingGameSelectPrecisionAnchor(projection,
			NULL);

	for (group = 0; group < ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT
			&& querybudget > 0 && !result; group++) {
		const struct accessibilitytargetingprecisionanchor *anchor = NULL;
		f32 samplesx[ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES];
		f32 samplesy[ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES];
		f32 samplescore[ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES];
		u32 testedsamples = 0;
		s32 slot = -1;
		s32 sample;

		if (group == 0 && preferred) {
			anchor = preferred;
			slot = (s32)(preferred - projection->precisionanchors);
		} else {
			f32 bestscore = 0.0f;
			s32 i;

			for (i = 0; i < ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_COUNT;
					i++) {
				const struct accessibilitytargetingprecisionanchor *candidate
						= &projection->precisionanchors[i];

				if ((testedslots & (1u << i)) || !candidate->valid) {
					continue;
				}

				if (!anchor || candidate->score < bestscore) {
					anchor = candidate;
					bestscore = candidate->score;
					slot = i;
				}
			}
		}

		if (!anchor || slot < 0) {
			break;
		}

		testedslots |= 1u << slot;

		for (sample = 0;
				sample < ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES;
				sample++) {
			if (accessibilityTargetingGameProjectPrecisionSample(model,
					(struct modelnode *)anchor->node, sample,
					&samplesx[sample], &samplesy[sample])) {
				samplescore[sample]
						= accessibilityTargetingGamePrecisionAnchorScore(
							samplesx[sample], samplesy[sample]);
			} else {
				samplescore[sample] = MAXFLOAT;
			}
		}

		for (sample = 0;
				sample < ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES
					&& querybudget > 0; sample++) {
			f32 bestscore = MAXFLOAT;
			s32 bestsample = -1;
			s32 i;

			for (i = 0; i < ACCESSIBILITY_TARGETING_PRECISION_FINE_SAMPLES;
					i++) {
				if ((testedsamples & (1u << i)) == 0
						&& samplescore[i] < bestscore) {
					bestscore = samplescore[i];
					bestsample = i;
				}
			}

			if (bestsample < 0) {
				break;
			}

			testedsamples |= 1u << bestsample;
			querybudget--;
			projection->precisionfinequeries++;

			if (accessibilityTargetingGameTestPrecisionGeometry(prop, chr,
					samplesx[bestsample], samplesy[bestsample], camrooms,
					&projection->precisionfinehitpart,
					&projection->precisionfinenode)) {
				projection->precisionfineavailable = true;
				projection->precisionfinesample = bestsample;
				projection->precisionfinescreenx = samplesx[bestsample];
				projection->precisionfinescreeny = samplesy[bestsample];
				result = true;
				break;
			}
		}
	}

	projection->precisionfinebudgetexhausted = !result && querybudget == 0;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	projection->precisionfineelapsedus = sysGetMicroseconds() - started;
	g_AccessibilityTargetingDiagnostics.precisionrefinements++;
	g_AccessibilityTargetingDiagnostics.precisionqueries
			+= projection->precisionfinequeries;
	if (result) {
		g_AccessibilityTargetingDiagnostics.precisionhits++;
	} else {
		g_AccessibilityTargetingDiagnostics.precisionmisses++;
	}
	if (projection->precisionfinebudgetexhausted) {
		g_AccessibilityTargetingDiagnostics.precisionbudgetexhaustions++;
	}
	g_AccessibilityTargetingDiagnostics.precisionquerytotalus
			+= projection->precisionfineelapsedus;
	if (projection->precisionfineelapsedus
			> g_AccessibilityTargetingDiagnostics.precisionquerymaxus) {
		g_AccessibilityTargetingDiagnostics.precisionquerymaxus
				= projection->precisionfineelapsedus;
	}
#endif

	if (g_Vars.stagenum != g_AccessibilityTargetingPrecisionFineLastStage
			|| g_Vars.lvframe60
					>= g_AccessibilityTargetingPrecisionFineNextLog60
			|| projection->propnum
					!= g_AccessibilityTargetingPrecisionFineLastPropnum
			|| g_AccessibilityTargetingPrecisionFineLastResult < 0) {
		accessibilityLogEvent("targeting", "precision_refinement",
				"frame=%d propnum=%d result=%s envelope_margin=%.1f queries=%d query_budget=%d budget_exhausted=%d elapsed_us=%llu hitpart=%d hitnode=%p sample=%d target_screen=%.3f,%.3f",
				g_Vars.lvframe60, projection->propnum,
				result ? "geometry_valid" : "no_valid_sample",
				ACCESSIBILITY_TARGETING_PRECISION_FINE_MARGIN,
				projection->precisionfinequeries,
				ACCESSIBILITY_TARGETING_PRECISION_FINE_QUERY_BUDGET,
				projection->precisionfinebudgetexhausted,
				(unsigned long long)projection->precisionfineelapsedus,
				projection->precisionfinehitpart,
				(void *)projection->precisionfinenode,
				projection->precisionfinesample,
				projection->precisionfinescreenx,
				projection->precisionfinescreeny);
		g_AccessibilityTargetingPrecisionFineNextLog60 = g_Vars.lvframe60
				+ ACCESSIBILITY_TARGETING_PRECISION_FINE_LOG_TICKS;
		g_AccessibilityTargetingPrecisionFineLastStage = g_Vars.stagenum;
		g_AccessibilityTargetingPrecisionFineLastPropnum = projection->propnum;
		g_AccessibilityTargetingPrecisionFineLastResult = result;
	}
}

static f32 accessibilityTargetingGamePunchRange(void)
{
	struct gset gset = { WEAPON_UNARMED, 0, 0, FUNC_PRIMARY };
	struct weaponfunc *func = gsetGetWeaponFunction(&gset);

	if (func && (func->type & 0xff) == INVENTORYFUNCTYPE_MELEE) {
		struct weaponfunc_melee *melee = (struct weaponfunc_melee *)func;

		if (melee->range > 0.0f) {
			return melee->range;
		}
	}

	return 60.0f;
}

static s32 accessibilityTargetingGameCharacterCombatCapable(
		struct chrdata *chr)
{
	return chr && !chrIsDead(chr)
			&& chr->actiontype != ACT_DRUGGEDDROP
			&& chr->actiontype != ACT_DRUGGEDKO;
}

static s32 accessibilityTargetingGameAutogunCombatCapable(
		struct autogunobj *autogun)
{
	struct defaultobj *obj;
	struct chrdata *playerchr;

	if (!autogun || !g_Vars.currentplayer
			|| !g_Vars.currentplayer->prop
			|| !g_Vars.currentplayer->prop->chr) {
		return false;
	}

	obj = &autogun->base;
	playerchr = g_Vars.currentplayer->prop->chr;

	/* The villa windmill is scenery implemented with this object type. */
	if (obj->type != OBJTYPE_AUTOGUN
			|| obj->modelnum == MODEL_AIVILLAWINDMILL
			|| (obj->flags & OBJFLAG_DEACTIVATED)
			|| (obj->flags2 & (OBJFLAG2_AICANNOTUSE
				| OBJFLAG2_AUTOGUN_MALFUNCTIONING1
				| OBJFLAG2_AUTOGUN_MALFUNCTIONING2))
			|| autogun->ammoquantity == 0
			|| !objIsHealthy(obj)) {
		return false;
	}

	/*
	 * A zero team mask is the native stationary-autogun fallback: it targets
	 * the player. A deployed laptop gun carries its owner's complement mask,
	 * so this same test excludes the current player's own turret.
	 */
	return autogun->targetteam == 0
			|| (autogun->targetteam & playerchr->team) != 0;
}

static s32 accessibilityTargetingGameCctvCombatCapable(
		struct cctvobj *camera)
{
	struct defaultobj *obj;

	if (!camera) {
		return false;
	}

	obj = &camera->base;

	return obj->type == OBJTYPE_CCTV
			&& (obj->flags & (OBJFLAG_DEACTIVATED
				| OBJFLAG_CAMERA_DISABLED)) == 0
			&& objIsHealthy(obj);
}

static s32 accessibilityTargetingGameCurrentAttackCanDamageObject(
		const struct prop *prop)
{
	struct weaponfunc *func = currentPlayerGetWeaponFunction(HAND_RIGHT);
	s32 type;

	if (!func || !prop || prop->type != PROPTYPE_OBJ || !prop->obj
			|| !objIsHealthy(prop->obj) || !objIsMortal(prop->obj)) {
		return false;
	}

	type = func->type & 0xff;

	if (type == INVENTORYFUNCTYPE_THROW
			|| func->type == INVENTORYFUNCTYPE_SHOOT_PROJECTILE
			|| (func->flags & (FUNCFLAG_EXPLOSIVESHELLS
				| FUNCFLAG_20000000))) {
		return (prop->obj->flags2 & OBJFLAG2_IMMUNETOGUNFIRE) == 0
				|| (prop->obj->flags2 & OBJFLAG2_IMMUNETOEXPLOSIONS) == 0;
	}

	return (type == INVENTORYFUNCTYPE_SHOOT
				|| type == INVENTORYFUNCTYPE_MELEE)
			&& (prop->obj->flags2 & OBJFLAG2_IMMUNETOGUNFIRE) == 0;
}

static s32 accessibilityTargetingGameLootContainerChildType(
		const struct prop *prop, s32 *childpropnum)
{
	struct prop *child;
	s32 guard = 0;

	if (childpropnum) {
		*childpropnum = -1;
	}

	if (!prop || prop->type != PROPTYPE_OBJ || !prop->obj
			|| !prop->active || (prop->flags & PROPFLAG_ENABLED) == 0
			|| (prop->obj->flags2 & OBJFLAG2_INVISIBLE)
			|| (prop->obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))
			|| !objIsHealthy(prop->obj) || !objIsMortal(prop->obj)) {
		return -1;
	}

	for (child = prop->child; child && guard < 32;
			child = child->next, guard++) {
		struct defaultobj *childobj;

		if (child->parent != prop
				|| (child->type != PROPTYPE_OBJ
					&& child->type != PROPTYPE_WEAPON)
				|| !child->obj || child->obj->prop != child) {
			continue;
		}

		childobj = child->obj;

		if ((childobj->flags & OBJFLAG_INSIDEANOTHEROBJ)
				&& (childobj->flags & OBJFLAG_UNCOLLECTABLE) == 0
				&& (childobj->flags2 & OBJFLAG2_INVISIBLE) == 0
				&& (childobj->hidden
					& (OBJHFLAG_DELETING | OBJHFLAG_GONE)) == 0
				&& func0f085194(childobj)
				&& childobj->type != OBJTYPE_HAT
				&& childobj->type != OBJTYPE_ESCASTEP) {
			if (childpropnum) {
				*childpropnum = accessibilityTargetingGamePropNum(child);
			}
			return childobj->type;
		}
	}

	return -1;
}

static s32 accessibilityTargetingGameAuditEqual(
		const struct accessibilitytargetinggameaudit *a,
		const struct accessibilitytargetinggameaudit *b)
{
	return a->prop == b->prop && a->obj == b->obj
			&& a->propnum == b->propnum && a->proptype == b->proptype
			&& a->modelnum == b->modelnum && a->propflags == b->propflags
			&& a->objflags2 == b->objflags2 && a->inuse == b->inuse
			&& a->active == b->active && a->destroyed == b->destroyed
			&& a->accepted == b->accepted && a->shootability == b->shootability
			&& a->reason == b->reason;
}

static s32 accessibilityTargetingGamePropNum(const struct prop *prop)
{
	uintptr_t address;
	uintptr_t first;
	uintptr_t end;

	if (!prop || !g_Vars.props || g_Vars.maxprops <= 0) {
		return -1;
	}

	address = (uintptr_t)prop;
	first = (uintptr_t)g_Vars.props;
	end = first + sizeof(struct prop) * (uintptr_t)g_Vars.maxprops;

	if (address < first || address >= end
			|| (address - first) % sizeof(struct prop) != 0) {
		return -1;
	}

	return (s32)((address - first) / sizeof(struct prop));
}

static const char *accessibilityTargetingGameScopeReason(void)
{
	if (!accessibilityIsTargetingFeedbackEnabled()) {
		return "feature_disabled";
	}

	if (PLAYERCOUNT() != 1) {
		return "unsupported_player_count";
	}

	if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop) {
		return "player_unavailable";
	}

	if (g_MenuData.count > 0 || g_Vars.currentplayer->menuisactive
			|| g_Vars.currentplayer->mpmenuon) {
		return "menu_or_overlay_open";
	}

	if (lvIsPaused()) {
		return "paused";
	}

	if (g_MainIsEndscreen) {
		return "endscreen";
	}

	if (g_Vars.in_cutscene || g_Vars.tickmode != TICKMODE_NORMAL) {
		return "non_gameplay_camera";
	}

	if (g_Vars.currentplayer->isdead) {
		return "player_dead";
	}

	/*
	 * lvupdate60 can be zero on an ordinary PC render/interpolation frame.
	 * The explicit gates above distinguish actual pauses and invalid scopes.
	 */
	return NULL;
}

static s32 accessibilityTargetingGameIsFiringRange(void)
{
	return g_Vars.stagenum == STAGE_CITRAINING && g_FrIsValidWeapon;
}

static s32 accessibilityTargetingGameDeviceTargetInScope(
		const struct accessibilitytargetingdevicetarget *target)
{
	struct trainingdata *data;

	if (!target || g_Vars.stagenum != target->stagenum) {
		return false;
	}

	if (!target->trainingonly) {
		return bgunGetWeaponNum(HAND_RIGHT) == target->weaponnum;
	}

	data = dtGetData();
	if (!data || !data->intraining || data->completed || data->failed
			|| data->finished) {
		return false;
	}

	return dtGetWeaponByDeviceIndex(dtGetIndexBySlot(g_DtSlot))
			== target->weaponnum;
}

static s32 accessibilityTargetingGameCamSpyInScope(void)
{
	struct player *player = g_Vars.currentplayer;

	return player && player->cameramode == CAMERAMODE_EYESPY
			&& player->eyespy && player->eyespy->active
			&& player->eyespy->prop && player->eyespy->prop->active
			&& player->eyespy->mode == EYESPYMODE_CAMSPY
			&& (player->devicesactive & ~player->devicesinhibit
					& DEVICE_EYESPY)
			&& player->eyespy->startuptimer60 >= TICKS(50);
}

static s32 accessibilityTargetingGameHasDeviceTargets(void)
{
	s32 i;

	if (accessibilityTargetingGameCamSpyInScope()) {
		return true;
	}

	for (i = 0; i < ARRAYCOUNT(g_AccessibilityTargetingDeviceTargets); i++) {
		if (accessibilityTargetingGameDeviceTargetInScope(
				&g_AccessibilityTargetingDeviceTargets[i])) {
			return true;
		}
	}

	return false;
}

static s32 accessibilityTargetingGameNativeAlignmentExpected(struct prop *aimedprop)
{
	s32 i;

	if (!aimedprop || !g_Vars.currentplayer->lastsighton) {
		return false;
	}

	if (g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_DEFAULT
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_BETASCANNER
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_ROCKETLAUNCHER
			&& g_Vars.currentplayer->sighttracktype != SIGHTTRACKTYPE_FOLLOWLOCKON) {
		return false;
	}

	for (i = 0; i < ARRAYCOUNT(g_Vars.currentplayer->trackedprops); i++) {
		if (g_Vars.currentplayer->trackedprops[i].prop == aimedprop) {
			return true;
		}
	}

	return false;
}

static s32 accessibilityTargetingGameObservationHasProp(
		const struct accessibilitytargetingobservation *observation,
		const struct prop *prop)
{
	s32 i;

	for (i = 0; i < observation->candidatecount; i++) {
		if (observation->candidates[i].prop == prop) {
			return true;
		}
	}

	return false;
}

static s32 accessibilityTargetingGameThreatDetectorActive(void)
{
	return g_Vars.currentplayer
			&& gsetHasFunctionFlags(
				&g_Vars.currentplayer->hands[HAND_RIGHT].gset,
				FUNCFLAG_THREATDETECTOR);
}

static s32 accessibilityTargetingGameThreatCategory(
		const struct defaultobj *obj)
{
	if (obj && obj->type == OBJTYPE_AUTOGUN) {
		return ACCESSIBILITY_TARGETING_CATEGORY_TURRET;
	}

	if (obj && obj->modelnum == MODEL_SK_SHUTTLE) {
		return ACCESSIBILITY_TARGETING_CATEGORY_VEHICLE;
	}

	return ACCESSIBILITY_TARGETING_CATEGORY_OBJECT;
}

static s32 accessibilityTargetingGameVerticalAimError(
		f32 x1, f32 y1, f32 x2, f32 y2, f32 *degrees)
{
	f32 targetscreen[2];
	struct coord targetdir;
	struct coord aimdir;
	f32 targethorizontal;
	f32 aimhorizontal;
	f32 targetangle;
	f32 aimangle;
	f32 angledelta;

	if (!degrees || !isfinite(x1) || !isfinite(y1)
			|| !isfinite(x2) || !isfinite(y2)
			|| !isfinite(g_Vars.currentplayer->crosspos[0])
			|| !isfinite(g_Vars.currentplayer->crosspos[1])) {
		return false;
	}

	targetscreen[0] = (x1 + x2) * 0.5f;
	targetscreen[1] = (y1 + y2) * 0.5f;
	cam0f0b4c3c(targetscreen, &targetdir, 1.0f);
	cam0f0b4c3c(g_Vars.currentplayer->crosspos, &aimdir, 1.0f);
	targethorizontal = sqrtf(targetdir.x * targetdir.x
			+ targetdir.z * targetdir.z);
	aimhorizontal = sqrtf(aimdir.x * aimdir.x + aimdir.z * aimdir.z);
	targetangle = atan2f(targetdir.y, targethorizontal);
	aimangle = atan2f(aimdir.y, aimhorizontal);
	angledelta = targetangle - aimangle;

	/*
	 * The port's atan2f can return angles on a zero-to-tau interval.
	 * Normalize across that seam before converting to degrees.
	 */
	if (angledelta > M_PI) {
		angledelta -= M_TAU;
	} else if (angledelta < -M_PI) {
		angledelta += M_TAU;
	}

	*degrees = angledelta * 180.0f / M_PI;
	return isfinite(*degrees);
}

static void accessibilityTargetingGameObserveNativeThreats(
		struct accessibilitytargetingobservation *observation, s32 detailed)
{
	f32 viewleft = (f32)viGetViewLeft() / g_ScaleX;
	f32 viewtop = viGetViewTop();
	f32 viewright = viewleft + (f32)viGetViewWidth() / g_ScaleX;
	f32 viewbottom = viewtop + viGetViewHeight();
	f32 viewcenterx = (viewleft + viewright) * 0.5f;
	s32 i;

	observation->threatdetectoractive
			= accessibilityTargetingGameThreatDetectorActive();

	if (!observation->threatdetectoractive) {
		return;
	}

	for (i = 0; i < ARRAYCOUNT(g_Vars.currentplayer->trackedprops)
			&& observation->threatcount
					< ACCESSIBILITY_TARGETING_MAX_NATIVE_THREATS; i++) {
		struct trackedprop *tracked = &g_Vars.currentplayer->trackedprops[i];
		struct prop *prop = tracked->prop;
		s32 objectprop = prop && (prop->type == PROPTYPE_OBJ
				|| prop->type == PROPTYPE_WEAPON);
		struct defaultobj *obj = objectprop ? prop->obj : NULL;
		struct accessibilitytargetingcandidate *threat;
		s32 propnum = accessibilityTargetingGamePropNum(prop);
		s32 eligible = propnum >= 0 && objectprop && obj
				&& prop->active
				&& (prop->flags & PROPFLAG_ENABLED)
				&& (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK)
				&& tracked->x1 <= tracked->x2
				&& tracked->y1 <= tracked->y2;
		const char *reason = eligible ? "native_threat"
				: propnum < 0 ? "invalid_prop"
				: !objectprop ? "wrong_prop_type"
				: !obj ? "object_unavailable"
				: !prop->active || (prop->flags & PROPFLAG_ENABLED) == 0
						? "inactive_or_disabled"
				: (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0
						? "not_rendered_this_tick"
				: "invalid_native_bounds";
		f32 dx;
		f32 dy;
		f32 dz;

		if (detailed) {
			accessibilityLogEvent("targeting",
					"threat_detector_candidate",
					"frame=%d slot=%d accepted=%d reason=%s prop=%p propnum=%d prop_type=%d obj=%p obj_type=%d model=%d screen=%d,%d,%d,%d",
					g_Vars.lvframe60, i, eligible, reason,
					(void *)prop, propnum, prop ? prop->type : -1,
					(void *)obj, obj ? obj->type : -1,
					obj ? obj->modelnum : -1,
					tracked->x1, tracked->y1,
					tracked->x2, tracked->y2);
		}

		if (!eligible) {
			continue;
		}

		threat = &observation->threats[observation->threatcount++];
		memset(threat, 0, sizeof(*threat));
		threat->identity.playernum = g_Vars.currentplayernum;
		threat->identity.source = ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
		threat->identity.sourceslot = propnum;
		threat->identity.propnum = propnum;
		threat->identity.proptype = prop->type;
		threat->identity.objectidentity = (uintptr_t)obj;
		threat->prop = prop;
		threat->category = accessibilityTargetingGameThreatCategory(obj);
		threat->relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
		threat->shootability
				= ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
		threat->position = prop->pos;
		threat->screenx1 = tracked->x1;
		threat->screeny1 = tracked->y1;
		threat->screenx2 = tracked->x2;
		threat->screeny2 = tracked->y2;
		threat->aimscreenx = g_Vars.currentplayer->crosspos[0];
		threat->aimscreeny = g_Vars.currentplayer->crosspos[1];
		if (viewright > viewleft && viewbottom > viewtop) {
			threat->hasscreenaimerror = true;
			threat->horizontalaimerrornormalized
					= (((threat->screenx1 + threat->screenx2) * 0.5f)
							- threat->aimscreenx)
						/ ((viewright - viewleft) * 0.5f);
			threat->verticalaimerrornormalized
					= (threat->aimscreeny
							- ((threat->screeny1 + threat->screeny2) * 0.5f))
						/ ((viewbottom - viewtop) * 0.5f);
		}
		threat->hasverticalaimerror
				= accessibilityTargetingGameVerticalAimError(
					threat->screenx1, threat->screeny1,
					threat->screenx2, threat->screeny2,
					&threat->verticalaimerrordegrees);
		threat->horizontalscreenoffset = fabsf(
				((threat->screenx1 + threat->screenx2) * 0.5f)
					- viewcenterx);
		dx = threat->position.x - g_Vars.currentplayer->prop->pos.x;
		dy = threat->position.y - g_Vars.currentplayer->prop->pos.y;
		dz = threat->position.z - g_Vars.currentplayer->prop->pos.z;
		threat->distance = sqrtf(dx * dx + dy * dy + dz * dz);
		dx = threat->position.x - g_Vars.currentplayer->cam_pos.x;
		dy = threat->position.y - g_Vars.currentplayer->cam_pos.y;
		dz = threat->position.z - g_Vars.currentplayer->cam_pos.z;
		threat->hasdistancecue = true;
		threat->distancecue = sqrtf(dx * dx + dy * dy + dz * dz);
	}
}

static void accessibilityTargetingGameClearProjections(void)
{
	memset(g_AccessibilityTargetingGameProjections, 0,
			sizeof(g_AccessibilityTargetingGameProjections));
	g_AccessibilityTargetingGameProjectionFrame60 = -1;
	g_AccessibilityTargetingGameProjectionPlayer = -1;
	g_AccessibilityTargetingGameProjectionsValid = false;
	g_AccessibilityTargetingGameAimProp = 0;
	memset(&g_AccessibilityTargetingGameAimHitPos, 0,
			sizeof(g_AccessibilityTargetingGameAimHitPos));
	g_AccessibilityTargetingGameAimHitValid = false;
	g_AccessibilityTargetingGameRawAimProp = 0;
	memset(&g_AccessibilityTargetingGameRawAimHitPos, 0,
			sizeof(g_AccessibilityTargetingGameRawAimHitPos));
	g_AccessibilityTargetingGameRawAimHitValid = false;
	g_AccessibilityTargetingGameRawAimHitPart = 0;
	g_AccessibilityTargetingGameRawAimHitPartValid = false;
	memset(g_AccessibilityTargetingCombatProjections, 0,
			sizeof(g_AccessibilityTargetingCombatProjections));
	g_AccessibilityTargetingCombatProjectionCount = 0;
	memset(g_AccessibilityTargetingCamSpyTargets, 0,
			sizeof(g_AccessibilityTargetingCamSpyTargets));
	g_AccessibilityTargetingCamSpyTargetCount = 0;
}

static const char *accessibilityTargetingGameVisibilitySampleName(s32 sample)
{
	switch (sample) {
	case ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_CENTER:
		return "center";
	case ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER:
		return "upper";
	case ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_LOWER:
		return "lower";
	case ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_LEFT:
		return "upper_left";
	case ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_UPPER_RIGHT:
		return "upper_right";
	default:
		return "none";
	}
}

static s32 accessibilityTargetingGameCharacterLineOfSight(
		struct prop *prop, struct chrdata *chr, RoomNum *camrooms,
		s32 *sample, s32 *queries)
{
	struct coord positions[ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_COUNT];
	f32 dx = prop->pos.x - g_Vars.currentplayer->cam_pos.x;
	f32 dz = prop->pos.z - g_Vars.currentplayer->cam_pos.z;
	f32 horizontal = sqrtf(dx * dx + dz * dz);
	f32 sideamount = chr->radius * 0.75f;
	f32 sidex = 0.0f;
	f32 sidez = 0.0f;
	s32 i;

	*sample = ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_NONE;
	*queries = 0;

	if (horizontal > 0.001f) {
		sidex = dz / horizontal;
		sidez = -dx / horizontal;
	}

	positions[0] = prop->pos;
	positions[0].y = chr->manground + chr->height * 0.50f;
	positions[1] = prop->pos;
	positions[1].y = chr->manground + chr->height * 0.82f;
	positions[2] = prop->pos;
	positions[2].y = chr->manground + chr->height * 0.20f;
	positions[3] = prop->pos;
	positions[3].x += sidex * sideamount;
	positions[3].z += sidez * sideamount;
	positions[3].y = chr->manground + chr->height * 0.65f;
	positions[4] = prop->pos;
	positions[4].x -= sidex * sideamount;
	positions[4].z -= sidez * sideamount;
	positions[4].y = chr->manground + chr->height * 0.65f;

	for (i = 0; i < ARRAYCOUNT(positions); i++) {
		(*queries)++;

		if (accessibilityVisibilityHasVisualLineOfSight(
				&g_Vars.currentplayer->cam_pos, camrooms,
				&positions[i], prop->rooms, prop)) {
			*sample = ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_CENTER + i;
			return true;
		}
	}

	return false;
}

static void accessibilityTargetingCaptureCombat(void)
{
	struct prop **propptr;
	RoomNum camrooms[2];
	struct accessibilitytargetingcombatprojection *fineprojection = NULL;
	f32 fineprojectionscore = 0.0f;
	s32 fineprojectionpreferred = false;
	s32 preferredpropnum
			= accessibilityTargetingGetPrecisionGuidancePropnum();

	if (!g_Vars.onscreenprops || !g_Vars.endonscreenprops) {
		return;
	}

	camrooms[0] = g_Vars.currentplayer->cam_room;
	camrooms[1] = -1;

	for (propptr = g_Vars.onscreenprops;
			propptr < g_Vars.endonscreenprops
			&& g_AccessibilityTargetingCombatProjectionCount
					< ARRAYCOUNT(g_AccessibilityTargetingCombatProjections);
			propptr++) {
		struct prop *prop = *propptr;
		struct chrdata *chr = NULL;
		struct defaultobj *obj = NULL;
		struct model *model;
		struct accessibilitytargetingcombatprojection *projection;
		struct coord targetpos;
		s32 category;
		s32 propnum;

		if (!prop || prop == g_Vars.currentplayer->prop) {
			continue;
		}

		if ((prop->type == PROPTYPE_CHR
				|| prop->type == PROPTYPE_PLAYER) && prop->chr) {
			chr = prop->chr;
			model = chr->model;
			category = prop->type == PROPTYPE_PLAYER
					? ACCESSIBILITY_TARGETING_CATEGORY_PLAYER
					: ACCESSIBILITY_TARGETING_CATEGORY_CHARACTER;
		} else if (prop->type == PROPTYPE_OBJ && prop->obj
				&& (prop->obj->type == OBJTYPE_AUTOGUN
					|| prop->obj->type == OBJTYPE_CCTV)) {
			obj = prop->obj;
			model = obj->model;
			category = obj->type == OBJTYPE_AUTOGUN
					? ACCESSIBILITY_TARGETING_CATEGORY_TURRET
					: ACCESSIBILITY_TARGETING_CATEGORY_SECURITY_CAMERA;
		} else {
			continue;
		}

		propnum = accessibilityTargetingGamePropNum(prop);
		projection = &g_AccessibilityTargetingCombatProjections[
				g_AccessibilityTargetingCombatProjectionCount++];
		projection->prop = (uintptr_t)prop;
		projection->chr = (uintptr_t)chr;
		projection->obj = (uintptr_t)obj;
		projection->propnum = propnum;
		projection->category = category;

		if (propnum < 0 || !model || !model->matrices
				|| !model->definition
				|| (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			continue;
		}

		projection->projected = modelGetScreenCoords(model,
				&projection->x2, &projection->x1,
				&projection->y2, &projection->y1);
		projection->finite = projection->projected
				&& isfinite(projection->x2) && isfinite(projection->x1)
				&& isfinite(projection->y2) && isfinite(projection->y1);
		if (chr) {
			accessibilityTargetingGameCapturePrecisionAnchors(prop, chr,
					projection);
		}

		if (projection->finite
				&& accessibilityTargetingGameVerticalAimError(
					projection->x1, projection->y1,
					projection->x2, projection->y2,
					&projection->verticalaimerrordegrees)) {
			projection->aimscreenx = g_Vars.currentplayer->crosspos[0];
			projection->aimscreeny = g_Vars.currentplayer->crosspos[1];
			projection->hasverticalaimerror = true;
		}

		targetpos = prop->pos;
		if (chr) {
			targetpos.y = chr->manground + chr->height * 0.5f;
		}

		if (prop->active && (prop->flags & PROPFLAG_ENABLED)
				&& ((chr
					&& accessibilityTargetingGameCharacterCombatCapable(chr)
					&& (chr->chrflags & CHRCFLAG_HIDDEN) == 0
					&& (chr->hidden & CHRHFLAG_UNTARGETABLE) == 0
					&& ((chr->hidden & CHRHFLAG_CLOAKED) == 0
						|| USINGDEVICE(DEVICE_IRSCANNER))
					&& accessibilityTargetingGameRelationship(prop)
						== ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE)
				|| (obj && ((obj->type == OBJTYPE_AUTOGUN
						&& accessibilityTargetingGameAutogunCombatCapable(
							(struct autogunobj *)obj))
					|| (obj->type == OBJTYPE_CCTV
						&& accessibilityTargetingGameCctvCombatCapable(
							(struct cctvobj *)obj)))))) {
			if (chr) {
				projection->lineofsight
						= accessibilityTargetingGameCharacterLineOfSight(
							prop, chr, camrooms,
							&projection->visibilitysample,
							&projection->visibilityqueries);
			} else {
				projection->visibilityqueries = 1;
				projection->lineofsight
						= accessibilityVisibilityHasVisualLineOfSight(
							&g_Vars.currentplayer->cam_pos, camrooms,
							&targetpos, prop->rooms, prop);
				projection->visibilitysample = projection->lineofsight
						? ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_CENTER
						: ACCESSIBILITY_TARGETING_VISIBILITY_SAMPLE_NONE;
			}
		}

		if (chr && projection->finite && projection->lineofsight
				&& accessibilityTargetingGamePrecisionFineEnvelope(projection)) {
			const struct accessibilitytargetingprecisionanchor *anchor
					= accessibilityTargetingGameSelectPrecisionAnchor(
						projection, NULL);
			s32 preferred = projection->propnum == preferredpropnum;

			if (anchor && (!fineprojection
					|| (preferred && !fineprojectionpreferred)
					|| (preferred == fineprojectionpreferred
						&& anchor->score < fineprojectionscore))) {
				fineprojection = projection;
				fineprojectionscore = anchor->score;
				fineprojectionpreferred = preferred;
			}
		}
	}

	if (fineprojection
			&& g_AccessibilityTargetingGameRawAimProp
					!= fineprojection->prop) {
		accessibilityTargetingGameRefinePrecisionProjection(fineprojection,
				camrooms);
	}
}

static void accessibilityTargetingCaptureCamSpy(void)
{
	struct criteria_holograph *criteria = g_HolographCriterias;
	f32 screenleft = camGetScreenLeft();
	f32 screentop = camGetScreenTop();
	f32 screenright = screenleft + camGetScreenWidth();
	f32 screenbottom = screentop + camGetScreenHeight();

	while (criteria && g_AccessibilityTargetingCamSpyTargetCount
			< ARRAYCOUNT(g_AccessibilityTargetingCamSpyTargets)) {
		struct accessibilitytargetingcamspytarget *capture
				= &g_AccessibilityTargetingCamSpyTargets[
					g_AccessibilityTargetingCamSpyTargetCount++];
		struct defaultobj *obj = objFindByTagId(criteria->obj);
		struct prop *prop = obj ? obj->prop : NULL;
		struct coord projected;
		f32 bounds1[2];
		f32 bounds2[2];
		f32 screen1[2];
		f32 screen2[2];
		f32 xdiff;
		f32 zdiff;

		capture->criteria = (uintptr_t)criteria;
		capture->obj = (uintptr_t)obj;
		capture->prop = (uintptr_t)prop;
		capture->tagid = criteria->obj;
		capture->propnum = accessibilityTargetingGamePropNum(prop);
		capture->status = criteria->status;
		capture->reason = "eligible";

		if (criteria->status != OBJECTIVE_INCOMPLETE) {
			capture->reason = "criterion_complete";
		} else if (!obj || !prop || capture->propnum < 0) {
			capture->reason = "target_unavailable";
		} else if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			capture->reason = "not_rendered_this_tick";
		} else if (prop->z < 0.0f) {
			capture->reason = "behind_camera";
		} else if (!objIsHealthy(obj)) {
			capture->reason = "object_destroyed";
		} else {
			xdiff = prop->pos.x - g_Vars.currentplayer->cam_pos.x;
			zdiff = prop->pos.z - g_Vars.currentplayer->cam_pos.z;
			capture->distance = sqrtf(xdiff * xdiff + zdiff * zdiff);

			if (capture->distance >= 400.0f) {
				capture->reason = "outside_photo_range";
			} else if (!func0f0899dc(prop, &projected, bounds1, bounds2)) {
				capture->reason = "projection_failed";
			} else {
				func0f06803c(&projected, bounds1, bounds2, screen1, screen2);
				capture->screenx1 = screen1[0];
				capture->screeny1 = screen1[1];
				capture->screenx2 = screen2[0];
				capture->screeny2 = screen2[1];

				if (!isfinite(capture->screenx1)
						|| !isfinite(capture->screeny1)
						|| !isfinite(capture->screenx2)
						|| !isfinite(capture->screeny2)) {
					capture->reason = "projection_non_finite";
				} else if (capture->screenx1 <= screenleft
						|| capture->screenx1 >= screenright
						|| capture->screenx2 <= screenleft
						|| capture->screenx2 >= screenright
						|| capture->screeny1 <= screentop
						|| capture->screeny1 >= screenbottom
						|| capture->screeny2 <= screentop
						|| capture->screeny2 >= screenbottom) {
					capture->reason = "not_fully_in_photo";
				} else {
					capture->eligible = true;
				}
			}
		}

		criteria = criteria->next;
	}
}

void accessibilityTargetingCaptureGame(struct prop *queryaimedprop,
		const struct coord *queryhitpos, s32 queryhitpart)
{
	struct frdata *frdata;
	s32 i;

	accessibilityTargetingGameClearProjections();
	g_AccessibilityTargetingGameProjectionFrame60 = g_Vars.lvframe60;
	g_AccessibilityTargetingGameProjectionPlayer = g_Vars.currentplayernum;

	if (accessibilityTargetingGameScopeReason()) {
		return;
	}

	if (queryaimedprop && queryhitpos
			&& accessibilityTargetingGamePropNum(queryaimedprop) >= 0
			&& isfinite(queryhitpos->x) && isfinite(queryhitpos->y)
			&& isfinite(queryhitpos->z)) {
		g_AccessibilityTargetingGameRawAimProp = (uintptr_t)queryaimedprop;
		g_AccessibilityTargetingGameRawAimHitPos = *queryhitpos;
		g_AccessibilityTargetingGameRawAimHitValid = true;
		if ((queryaimedprop->type == PROPTYPE_CHR
				|| queryaimedprop->type == PROPTYPE_PLAYER)
				&& queryhitpart > 0) {
			g_AccessibilityTargetingGameRawAimHitPart = queryhitpart;
			g_AccessibilityTargetingGameRawAimHitPartValid = true;
		}
	}

	if (queryaimedprop && queryhitpos
			&& queryaimedprop == g_Vars.currentplayer->lookingatprop.prop
			&& accessibilityTargetingGamePropNum(queryaimedprop) >= 0
			&& isfinite(queryhitpos->x) && isfinite(queryhitpos->y)
			&& isfinite(queryhitpos->z)) {
		g_AccessibilityTargetingGameAimProp = (uintptr_t)queryaimedprop;
		g_AccessibilityTargetingGameAimHitPos = *queryhitpos;
		g_AccessibilityTargetingGameAimHitValid = true;
	}

	if (accessibilityTargetingGameHasDeviceTargets()) {
		if (accessibilityTargetingGameCamSpyInScope()) {
			accessibilityTargetingCaptureCamSpy();
		}
		g_AccessibilityTargetingGameProjectionsValid = true;
		return;
	}

	if (!accessibilityTargetingGameIsFiringRange()) {
		accessibilityTargetingCaptureCombat();
		g_AccessibilityTargetingGameProjectionsValid = true;
		return;
	}

	frdata = frGetData();

	for (i = 0; frdata && i < ARRAYCOUNT(frdata->targets)
			&& i < ARRAYCOUNT(g_AccessibilityTargetingGameProjections); i++) {
		struct frtarget *target = &frdata->targets[i];
		struct prop *prop = target->prop;
		struct defaultobj *obj = NULL;
		struct accessibilitytargetinggameprojection *projection
				= &g_AccessibilityTargetingGameProjections[i];
		s32 propnum = accessibilityTargetingGamePropNum(prop);

		if (propnum >= 0) {
			obj = prop->obj;
		}

		projection->prop = (uintptr_t)prop;
		projection->obj = (uintptr_t)obj;
		projection->propnum = propnum;

		if (propnum < 0 || !obj || !obj->model || !obj->model->matrices
				|| !obj->model->definition
				|| (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			continue;
		}

		projection->projected = modelGetScreenCoords(obj->model,
				&projection->x2, &projection->x1,
				&projection->y2, &projection->y1);
		projection->finite = projection->projected
				&& isfinite(projection->x2) && isfinite(projection->x1)
				&& isfinite(projection->y2) && isfinite(projection->y1);
	}

	g_AccessibilityTargetingGameProjectionsValid = true;
}

static s32 accessibilityTargetingGameRelationship(struct prop *prop)
{
	if (prop && prop->chr
			&& (prop->chr->hidden2 & CHRH2FLAG_BLUESIGHT)) {
		return ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED;
	}

	if (sightIsPropFriendly(prop)) {
		return ACCESSIBILITY_TARGETING_RELATIONSHIP_FRIENDLY;
	}

	if (prop && prop->chr && g_Vars.currentplayer
			&& g_Vars.currentplayer->prop
			&& chrCompareTeams(g_Vars.currentplayer->prop->chr,
				prop->chr, COMPARE_ENEMIES)) {
		return ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
	}

	return ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
}

static struct prop *accessibilityTargetingGameFindTolerantTurretAim(
		struct prop *rawaimedprop, f32 viewleft, f32 viewtop,
		f32 viewright, f32 viewbottom, f32 *distancefrombounds)
{
	struct prop *bestprop = NULL;
	f32 bestdistance2 = ACCESSIBILITY_TARGETING_TURRET_AIM_TOLERANCE
			* ACCESSIBILITY_TARGETING_TURRET_AIM_TOLERANCE;
	f32 bestcenterdistance2 = 0.0f;
	f32 aimx = g_Vars.currentplayer->crosspos[0];
	f32 aimy = g_Vars.currentplayer->crosspos[1];
	s32 bestpropnum = -1;
	s32 i;

	*distancefrombounds = -1.0f;

	/*
	 * Never substitute a tolerant target for an exact hit on another prop.
	 * This preserves doors and other foreground geometry as authoritative.
	 */
	if (rawaimedprop) {
		return NULL;
	}

	for (i = 0; i < g_AccessibilityTargetingCombatProjectionCount; i++) {
		struct accessibilitytargetingcombatprojection *projection
				= &g_AccessibilityTargetingCombatProjections[i];
		struct prop *prop = (struct prop *)projection->prop;
		struct defaultobj *obj = (struct defaultobj *)projection->obj;
		f32 dx = 0.0f;
		f32 dy = 0.0f;
		f32 distance2;
		f32 centerx;
		f32 centery;
		f32 centerdistance2;

		if (projection->category != ACCESSIBILITY_TARGETING_CATEGORY_TURRET
				|| projection->propnum < 0 || !prop || !obj
				|| accessibilityTargetingGamePropNum(prop)
						!= projection->propnum
				|| prop->type != PROPTYPE_OBJ || prop->obj != obj
				|| obj->type != OBJTYPE_AUTOGUN
				|| !prop->active || (prop->flags & PROPFLAG_ENABLED) == 0
				|| !accessibilityTargetingGameAutogunCombatCapable(
					(struct autogunobj *)obj)
				|| !projection->projected || !projection->finite
				|| !projection->lineofsight
				|| projection->x2 < viewleft || projection->x1 > viewright
				|| projection->y2 < viewtop || projection->y1 > viewbottom) {
			continue;
		}

		if (aimx < projection->x1) {
			dx = projection->x1 - aimx;
		} else if (aimx > projection->x2) {
			dx = aimx - projection->x2;
		}

		if (aimy < projection->y1) {
			dy = projection->y1 - aimy;
		} else if (aimy > projection->y2) {
			dy = aimy - projection->y2;
		}

		distance2 = dx * dx + dy * dy;
		centerx = (projection->x1 + projection->x2) * 0.5f;
		centery = (projection->y1 + projection->y2) * 0.5f;
		centerdistance2 = (centerx - aimx) * (centerx - aimx)
				+ (centery - aimy) * (centery - aimy);

		if (distance2 <= bestdistance2
				&& (!bestprop || distance2 < bestdistance2
					|| (distance2 == bestdistance2
						&& (centerdistance2 < bestcenterdistance2
							|| (centerdistance2 == bestcenterdistance2
								&& projection->propnum < bestpropnum))))) {
			bestprop = prop;
			bestpropnum = projection->propnum;
			bestdistance2 = distance2;
			bestcenterdistance2 = centerdistance2;
		}
	}

	if (bestprop) {
		*distancefrombounds = sqrtf(bestdistance2);
	}

	return bestprop;
}

static void accessibilityTargetingObserveCombat(
		struct accessibilitytargetingobservation *observation,
		s32 detailed, s32 scopechanged)
{
	struct prop *aimedprop = g_Vars.currentplayer->lookingatprop.prop;
	struct prop *rawaimedprop = g_AccessibilityTargetingGameRawAimHitValid
			? (struct prop *)g_AccessibilityTargetingGameRawAimProp : NULL;
	f32 viewleft = (f32)viGetViewLeft() / g_ScaleX;
	f32 viewtop = viGetViewTop();
	f32 viewright = viewleft + (f32)viGetViewWidth() / g_ScaleX;
	f32 viewbottom = viewtop + viGetViewHeight();
	f32 viewcenterx = (viewleft + viewright) * 0.5f;
	f32 tolerantturretdistance = -1.0f;
	struct prop *tolerantaimedturret
			= accessibilityTargetingGameFindTolerantTurretAim(rawaimedprop,
				viewleft, viewtop, viewright, viewbottom,
				&tolerantturretdistance);
	s32 aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
	s32 alignmentusesraw = false;
	const char *alignmentsource = "none";
	s32 i;

	observation->inscope = true;
	observation->distancecuereference = accessibilityTargetingGamePunchRange();
	observation->sighton = g_Vars.currentplayer->lastsighton;
	observation->targetindicatorvisible = !g_Vars.currentplayer->gunsightoff;
	observation->viewfovy = g_Vars.currentplayer->zoominfovy;
	observation->defaultfovy = PLAYER_DEFAULT_FOV;
	if (observation->defaultfovy > 0.0f && observation->viewfovy > 0.0f
			&& observation->viewfovy < observation->defaultfovy) {
		observation->zoomblend = (observation->defaultfovy
				- observation->viewfovy) / (observation->defaultfovy * 0.25f);
		if (observation->zoomblend > 1.0f) {
			observation->zoomblend = 1.0f;
		}
	}
	observation->precisionguidanceactive
			= bgunGetWeaponNum(HAND_RIGHT) == WEAPON_SNIPERRIFLE
			&& observation->zoomblend > 0.0f;

	/*
	 * Native threats are admitted before the generic combat scan so a
	 * crowded candidate list cannot hide an object already boxed by the
	 * detector. The observation also carries this list independently so the
	 * new-threat alert remains available in the firing-range profile.
	 */
	for (i = 0; i < observation->threatcount
			&& observation->candidatecount
					< ACCESSIBILITY_TARGETING_MAX_CANDIDATES; i++) {
		struct accessibilitytargetingcandidate *threat
				= &observation->threats[i];
		struct accessibilitytargetingcandidate *candidate;
		s32 aimedbyraw = threat->prop == rawaimedprop;
		s32 aimedbytolerance = threat->prop == tolerantaimedturret;
		s32 aimed = aimedbyraw || aimedbytolerance;
		f32 dx;
		f32 dy;
		f32 dz;

		if (accessibilityTargetingGameObservationHasProp(
				observation, threat->prop)) {
			continue;
		}

		candidate = &observation->candidates[observation->candidatecount++];
		*candidate = *threat;

		if (aimed) {
			observation->hasaimedtarget = true;
			observation->aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;
			alignmentusesraw = true;
			alignmentsource = aimedbyraw ? "raw_query" : "turret_tolerance";
			dx = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.x
					: candidate->position.x) - g_Vars.currentplayer->cam_pos.x;
			dy = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.y
					: candidate->position.y) - g_Vars.currentplayer->cam_pos.y;
			dz = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.z
					: candidate->position.z) - g_Vars.currentplayer->cam_pos.z;
			candidate->aimdistance = sqrtf(dx * dx + dy * dy + dz * dz);
		}
	}

	for (i = 0; i < g_AccessibilityTargetingCombatProjectionCount; i++) {
		struct accessibilitytargetingcombatprojection *projection
				= &g_AccessibilityTargetingCombatProjections[i];
		struct prop *prop = (struct prop *)projection->prop;
		struct chrdata *chr = (struct chrdata *)projection->chr;
		struct defaultobj *obj = (struct defaultobj *)projection->obj;
		struct accessibilitytargetingcandidate *candidate;
		const char *reason = "eligible";
		s32 relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_UNKNOWN;
		s32 turret = projection->category
				== ACCESSIBILITY_TARGETING_CATEGORY_TURRET;
		s32 camera = projection->category
				== ACCESSIBILITY_TARGETING_CATEGORY_SECURITY_CAMERA;
		s32 objecttarget = turret || camera;
		s32 eligible = true;
		s32 aimedbyraw = prop && objecttarget && prop == rawaimedprop;
		s32 aimedbytolerance = prop && turret
				&& prop == tolerantaimedturret;
		s32 aimed = prop && (objecttarget
				? aimedbyraw || aimedbytolerance : prop == aimedprop);
		const char *aimsource = aimed
				? objecttarget
					? aimedbyraw ? "raw_query" : "turret_tolerance"
					: "native_filtered"
				: "none";
		f32 dx;
		f32 dy;
		f32 dz;
		const struct accessibilitytargetingprecisionanchor *precisionanchor = NULL;
		s32 precisionaimsource = ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NONE;
		s32 precisionaimhitpart = 0;
		uintptr_t precisionaimnode = 0;
		f32 precisionaimscore = -1.0f;
		f32 targetscreenx = (projection->x1 + projection->x2) * 0.5f;
		f32 targetscreeny = (projection->y1 + projection->y2) * 0.5f;
		f32 precisionverticalaimerror = projection->verticalaimerrordegrees;
		s32 hasprecisionverticalaimerror = projection->hasverticalaimerror;

		if (accessibilityTargetingGameObservationHasProp(observation, prop)) {
			if (detailed) {
				accessibilityLogEvent("targeting", "combat_candidate",
						"frame=%d slot=%d accepted=0 reason=native_threat_duplicate prop=%p propnum=%d",
						g_Vars.lvframe60, i, (void *)prop,
						projection->propnum);
			}
			continue;
		}

		if (projection->propnum < 0 || !prop
				|| accessibilityTargetingGamePropNum(prop) != projection->propnum
				|| (objecttarget ? prop->obj != obj : prop->chr != chr)
				|| (objecttarget ? !obj : !chr)) {
			eligible = false;
			reason = "stale_or_invalid_identity";
		} else if (objecttarget
				? prop->type != PROPTYPE_OBJ
					|| (turret
						? obj->type != OBJTYPE_AUTOGUN
						: obj->type != OBJTYPE_CCTV)
				: prop->type != PROPTYPE_CHR
					&& prop->type != PROPTYPE_PLAYER) {
			eligible = false;
			reason = "wrong_prop_type";
		} else if (!prop->active || (prop->flags & PROPFLAG_ENABLED) == 0) {
			eligible = false;
			reason = "inactive_or_disabled";
		} else if (turret
				&& !accessibilityTargetingGameAutogunCombatCapable(
					(struct autogunobj *)obj)) {
			eligible = false;
			reason = "autogun_inactive_or_non_hostile";
		} else if (camera
				&& !accessibilityTargetingGameCctvCombatCapable(
					(struct cctvobj *)obj)) {
			eligible = false;
			reason = "camera_inactive_disabled_or_destroyed";
		} else if (!objecttarget
				&& !accessibilityTargetingGameCharacterCombatCapable(chr)) {
			eligible = false;
			reason = "dead_dying_or_knocked_out";
		} else if (!objecttarget && (chr->chrflags & CHRCFLAG_HIDDEN)) {
			eligible = false;
			reason = "character_hidden";
		} else if (!objecttarget && (chr->hidden & CHRHFLAG_UNTARGETABLE)) {
			eligible = false;
			reason = "character_untargetable";
		} else if (!objecttarget && (chr->hidden & CHRHFLAG_CLOAKED)
				&& !USINGDEVICE(DEVICE_IRSCANNER)) {
			eligible = false;
			reason = "character_cloaked";
		} else if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			eligible = false;
			reason = "not_rendered_this_tick";
		} else if (objecttarget
				? !obj->model || !obj->model->matrices
					|| !obj->model->definition
				: !chr->model || !chr->model->matrices
					|| !chr->model->definition) {
			eligible = false;
			reason = "model_unavailable";
		} else if (!g_AccessibilityTargetingGameProjectionsValid
				|| g_AccessibilityTargetingGameProjectionFrame60
						!= g_Vars.lvframe60
				|| g_AccessibilityTargetingGameProjectionPlayer
						!= g_Vars.currentplayernum) {
			eligible = false;
			reason = "projection_capture_unavailable";
		} else if (!projection->projected) {
			eligible = false;
			reason = "projection_failed";
		} else if (!projection->finite) {
			eligible = false;
			reason = "projection_non_finite";
		} else if (projection->x2 < viewleft || projection->x1 > viewright
				|| projection->y2 < viewtop || projection->y1 > viewbottom) {
			eligible = false;
			reason = "outside_viewport";
		} else if (!projection->lineofsight && !aimed) {
			eligible = false;
			reason = "line_of_sight_blocked";
		} else {
			relationship = objecttarget
					? ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE
					: accessibilityTargetingGameRelationship(prop);

			if (relationship == ACCESSIBILITY_TARGETING_RELATIONSHIP_FRIENDLY) {
				eligible = false;
				reason = "friendly";
			} else if (relationship != ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE
					&& relationship
						!= ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED) {
				eligible = false;
				reason = "not_hostile";
			} else if (relationship
					== ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED) {
				reason = "protected_nonlethal_target";
			}
		}

		if (eligible && chr && observation->precisionguidanceactive) {
			if (projection->precisionfineavailable) {
				precisionaimsource
						= ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_GEOMETRY;
				precisionaimhitpart = projection->precisionfinehitpart;
				precisionaimnode = projection->precisionfinenode;
				targetscreenx = projection->precisionfinescreenx;
				targetscreeny = projection->precisionfinescreeny;
				precisionaimscore
						= accessibilityTargetingGamePrecisionAnchorScore(
							targetscreenx, targetscreeny);
			} else {
				precisionanchor = accessibilityTargetingGameSelectPrecisionAnchor(
						projection, &precisionaimscore);
			}

			if (precisionaimsource
					!= ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_GEOMETRY) {
				if (precisionanchor) {
					precisionaimsource
							= ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_MODEL_HITBOX;
					precisionaimhitpart = precisionanchor->hitpart;
					precisionaimnode = precisionanchor->node;
					targetscreenx = precisionanchor->screenx;
					targetscreeny = precisionanchor->screeny;
				} else if (projection->precisionfallbackvalid) {
					precisionaimsource
							= ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NATIVE_AUTOAIM;
					targetscreenx = projection->precisionfallbackscreenx;
					targetscreeny = projection->precisionfallbackscreeny;
					precisionaimscore
							= accessibilityTargetingGamePrecisionAnchorScore(
								targetscreenx, targetscreeny);
				}
			}

			if (precisionaimsource != ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NONE) {
				hasprecisionverticalaimerror
						= accessibilityTargetingGameVerticalAimError(
							targetscreenx, targetscreeny,
							targetscreenx, targetscreeny,
							&precisionverticalaimerror);
			}
		}

		if (detailed) {
			accessibilityLogEvent("targeting", "combat_candidate",
					"frame=%d slot=%d accepted=%d reason=%s aimed=%d aim_source=%s category=%d relationship=%d aimonly=%d prop=%p propnum=%d chr=%p obj=%p obj_type=%d model=%d prop_type=%d prop_flags=0x%02x obj_flags=0x%08x obj_flags2=0x%08x chr_flags=0x%08x chr_hidden=0x%08x action=%d capture_valid=%d projected=%d finite=%d line_of_sight=%d visibility_sample=%s visibility_queries=%d screen=%.3f,%.3f,%.3f,%.3f precision_anchor_source=%s precision_anchor_hitpart=%d precision_anchor_node=%p precision_anchor_nodes_examined=%d precision_anchor_score=%.4f precision_fine_attempted=%d precision_fine_queries=%d precision_fine_budget_exhausted=%d precision_fine_elapsed_us=%llu target_screen=%.3f,%.3f aim_screen=%.3f,%.3f vertical_aim_error_available=%d raw_elevation_degrees=%.3f normalized_screen_error=%.4f,%.4f",
					g_Vars.lvframe60, i, eligible, reason, aimed,
					aimsource,
					projection->category,
					relationship,
					relationship
						== ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED,
					(void *)prop, projection->propnum,
					(void *)chr, (void *)obj, obj ? obj->type : -1,
					obj ? obj->modelnum : -1,
					prop ? prop->type : -1, prop ? prop->flags : 0,
					obj ? obj->flags : 0, obj ? obj->flags2 : 0,
					chr ? chr->chrflags : 0,
					chr ? chr->hidden : 0, chr ? chr->actiontype : -1,
					g_AccessibilityTargetingGameProjectionsValid
						&& g_AccessibilityTargetingGameProjectionFrame60
								== g_Vars.lvframe60
						&& g_AccessibilityTargetingGameProjectionPlayer
								== g_Vars.currentplayernum,
					projection->projected, projection->finite,
					projection->lineofsight,
					accessibilityTargetingGameVisibilitySampleName(
						projection->visibilitysample),
					projection->visibilityqueries,
					projection->x1, projection->y1,
					projection->x2, projection->y2,
					accessibilityTargetingGamePrecisionAnchorSourceName(
						precisionaimsource),
					precisionaimhitpart, (void *)precisionaimnode,
					projection->precisionanchornodesexamined,
					precisionaimscore,
					projection->precisionfineattempted,
					projection->precisionfinequeries,
					projection->precisionfinebudgetexhausted,
					(unsigned long long)projection->precisionfineelapsedus,
					targetscreenx, targetscreeny,
					projection->aimscreenx, projection->aimscreeny,
					hasprecisionverticalaimerror,
					precisionverticalaimerror,
					viewright > viewleft
							? (targetscreenx
									- projection->aimscreenx)
								/ ((viewright - viewleft) * 0.5f) : 0.0f,
					viewbottom > viewtop
							? (projection->aimscreeny
									- targetscreeny)
								/ ((viewbottom - viewtop) * 0.5f) : 0.0f);
		}

		if (!eligible || observation->candidatecount
				>= ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			continue;
		}

		candidate = &observation->candidates[observation->candidatecount++];
		memset(candidate, 0, sizeof(*candidate));
		candidate->identity.playernum = g_Vars.currentplayernum;
		candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
		candidate->identity.sourceslot = projection->propnum;
		candidate->identity.propnum = projection->propnum;
		candidate->identity.proptype = prop->type;
		candidate->identity.objectidentity = objecttarget
				? (uintptr_t)obj : (uintptr_t)chr;
		candidate->prop = prop;
		candidate->category = projection->category;
		candidate->relationship = relationship;
		candidate->aimonly = relationship
				== ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED;
		candidate->shootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
		candidate->position = prop->pos;
		if (chr) {
			candidate->position.y = chr->manground + chr->height * 0.5f;
		}
		candidate->screenx1 = projection->x1;
		candidate->screeny1 = projection->y1;
		candidate->screenx2 = projection->x2;
		candidate->screeny2 = projection->y2;
		candidate->hasverticalaimerror = hasprecisionverticalaimerror;
		candidate->verticalaimerrordegrees
				= precisionverticalaimerror;
		candidate->aimscreenx = projection->aimscreenx;
		candidate->aimscreeny = projection->aimscreeny;
		if (viewright > viewleft && viewbottom > viewtop) {
			candidate->hasscreenaimerror = true;
			candidate->horizontalaimerrornormalized
					= (targetscreenx
							- projection->aimscreenx)
						/ ((viewright - viewleft) * 0.5f);
			candidate->verticalaimerrornormalized
					= (projection->aimscreeny
							- targetscreeny)
						/ ((viewbottom - viewtop) * 0.5f);
		}
		candidate->horizontalscreenoffset = fabsf(
				targetscreenx - viewcenterx);
		candidate->precisionaimavailable
				= precisionaimsource != ACCESSIBILITY_TARGETING_PRECISION_ANCHOR_NONE;
		candidate->precisionaimsource = precisionaimsource;
		candidate->precisionaimhitpart = precisionaimhitpart;
		candidate->precisionaimnode = precisionaimnode;
		candidate->precisionaimnodesexamined
				= projection->precisionanchornodesexamined;
		candidate->precisionaimscreenx = targetscreenx;
		candidate->precisionaimscreeny = targetscreeny;
		candidate->precisionfineattempted = projection->precisionfineattempted;
		candidate->precisionfinequeries = projection->precisionfinequeries;
		candidate->precisionfinebudgetexhausted
				= projection->precisionfinebudgetexhausted;
		candidate->precisionfineelapsedus = projection->precisionfineelapsedus;
		dx = candidate->position.x - g_Vars.currentplayer->prop->pos.x;
		dy = candidate->position.y - g_Vars.currentplayer->prop->pos.y;
		dz = candidate->position.z - g_Vars.currentplayer->prop->pos.z;
		candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);
		dx = candidate->position.x - g_Vars.currentplayer->cam_pos.x;
		dy = candidate->position.y - g_Vars.currentplayer->cam_pos.y;
		dz = candidate->position.z - g_Vars.currentplayer->cam_pos.z;
		candidate->hasdistancecue = !camera;
		candidate->distancecue = sqrtf(dx * dx + dy * dy + dz * dz);
		if (chr) {
			candidate->distancecue -= chr->radius;
		}
		if (candidate->distancecue < 0.0f) {
			candidate->distancecue = 0.0f;
		}
		if (chr && prop == rawaimedprop
				&& g_AccessibilityTargetingGameRawAimHitPartValid) {
			candidate->aimregion = accessibilityTargetingGameAimRegion(
					g_AccessibilityTargetingGameRawAimHitPart);
		}

		if (aimed) {
			observation->hasaimedtarget = true;
			observation->aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;
			alignmentusesraw = objecttarget;
			alignmentsource = aimsource;

			if (objecttarget) {
				dx = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.x
						: candidate->position.x) - g_Vars.currentplayer->cam_pos.x;
				dy = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.y
						: candidate->position.y) - g_Vars.currentplayer->cam_pos.y;
				dz = (aimedbyraw ? g_AccessibilityTargetingGameRawAimHitPos.z
						: candidate->position.z) - g_Vars.currentplayer->cam_pos.z;
				candidate->aimdistance = sqrtf(dx * dx + dy * dy + dz * dz);
			}
		}
	}

	if (rawaimedprop && g_AccessibilityTargetingGameRawAimHitValid) {
		struct defaultobj *obj = rawaimedprop->obj;
		struct weaponfunc *func = currentPlayerGetWeaponFunction(HAND_RIGHT);
		s32 propnum = accessibilityTargetingGamePropNum(rawaimedprop);
		s32 breakable = accessibilityPathBlockerIsBreakable(rawaimedprop);
		s32 lootchildpropnum = -1;
		s32 lootchildtype = accessibilityTargetingGameLootContainerChildType(
				rawaimedprop, &lootchildpropnum);
		s32 lootcontainer = lootchildtype >= 0;
		s32 attackcompatible
				= accessibilityTargetingGameCurrentAttackCanDamageObject(
					rawaimedprop);

		if (propnum >= 0 && (breakable || lootcontainer) && attackcompatible
				&& !accessibilityTargetingGameObservationHasProp(
					observation, rawaimedprop)) {
			struct accessibilitytargetingcandidate *candidate;
			f32 dx;
			f32 dy;
			f32 dz;

			if (observation->candidatecount
					>= ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
				observation->candidatecount--;
			}
			candidate = &observation->candidates[observation->candidatecount++];
			memset(candidate, 0, sizeof(*candidate));
			candidate->identity.playernum = g_Vars.currentplayernum;
			candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
			candidate->identity.sourceslot = propnum;
			candidate->identity.propnum = propnum;
			candidate->identity.proptype = rawaimedprop->type;
			candidate->identity.objectidentity = (uintptr_t)obj;
			candidate->prop = rawaimedprop;
			candidate->category
					= lootcontainer
						? ACCESSIBILITY_TARGETING_CATEGORY_LOOT_CONTAINER
						: ACCESSIBILITY_TARGETING_CATEGORY_BREAKABLE_PATH_BLOCKER;
			candidate->relationship
					= ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
			candidate->shootability
					= ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
			candidate->position = g_AccessibilityTargetingGameRawAimHitPos;
			candidate->aimonly = true;
			dx = candidate->position.x - g_Vars.currentplayer->prop->pos.x;
			dy = candidate->position.y - g_Vars.currentplayer->prop->pos.y;
			dz = candidate->position.z - g_Vars.currentplayer->prop->pos.z;
			candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);
			dx = candidate->position.x - g_Vars.currentplayer->cam_pos.x;
			dy = candidate->position.y - g_Vars.currentplayer->cam_pos.y;
			dz = candidate->position.z - g_Vars.currentplayer->cam_pos.z;
			candidate->aimdistance = sqrtf(dx * dx + dy * dy + dz * dz);
			observation->hasaimedtarget = true;
			observation->aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;
			alignmentusesraw = true;
			alignmentsource = lootcontainer
					? "loot_container_raw_query"
					: "path_blocker_raw_query";
		}

		if (detailed || (breakable
				&& (uintptr_t)rawaimedprop
						!= g_AccessibilityTargetingGameLastRejectedAim)) {
			accessibilityLogEvent("targeting", "path_blocker_aim",
					"frame=%d stage=%d player=%d accepted=%d reason=%s prop=%p propnum=%d obj=%p obj_type=%d model=%d obj_flags=0x%08x obj_flags2=0x%08x obj_hidden=0x%08x healthy=%d mortal=%d attack_type=%d hit=%.3f,%.3f,%.3f",
					g_Vars.lvframe60, g_Vars.stagenum,
					g_Vars.currentplayernum,
					propnum >= 0 && breakable && attackcompatible,
					propnum < 0 ? "invalid_prop"
						: !breakable ? "not_breakable_path_blocker"
						: !attackcompatible ? "current_attack_incompatible"
						: "eligible",
					(void *)rawaimedprop, propnum, (void *)obj,
					obj ? obj->type : -1, obj ? obj->modelnum : -1,
					obj ? obj->flags : 0, obj ? obj->flags2 : 0,
					obj ? obj->hidden : 0,
					obj ? objIsHealthy(obj) : false,
					obj ? objIsMortal(obj) : false,
					func ? func->type : INVENTORYFUNCTYPE_NONE,
					g_AccessibilityTargetingGameRawAimHitPos.x,
					g_AccessibilityTargetingGameRawAimHitPos.y,
					g_AccessibilityTargetingGameRawAimHitPos.z);
		}

		if (detailed) {
			accessibilityLogEvent("targeting", "loot_container_aim",
					"frame=%d stage=%d player=%d accepted=%d reason=%s prop=%p propnum=%d obj=%p obj_type=%d model=%d obj_flags=0x%08x obj_flags2=0x%08x obj_hidden=0x%08x healthy=%d mortal=%d child_propnum=%d child_obj_type=%d attack_type=%d hit=%.3f,%.3f,%.3f",
					g_Vars.lvframe60, g_Vars.stagenum,
					g_Vars.currentplayernum,
					propnum >= 0 && lootcontainer && attackcompatible,
					propnum < 0 ? "invalid_prop"
						: !lootcontainer ? "no_collectable_child"
						: !attackcompatible ? "current_attack_incompatible"
						: "eligible",
					(void *)rawaimedprop, propnum, (void *)obj,
					obj ? obj->type : -1, obj ? obj->modelnum : -1,
					obj ? obj->flags : 0, obj ? obj->flags2 : 0,
					obj ? obj->hidden : 0,
					obj ? objIsHealthy(obj) : false,
					obj ? objIsMortal(obj) : false,
					lootchildpropnum, lootchildtype,
					func ? func->type : INVENTORYFUNCTYPE_NONE,
					g_AccessibilityTargetingGameRawAimHitPos.x,
					g_AccessibilityTargetingGameRawAimHitPos.y,
					g_AccessibilityTargetingGameRawAimHitPos.z);
		}
	}

	if (aimedprop && !observation->hasaimedtarget
			&& (detailed || (uintptr_t)aimedprop
					!= g_AccessibilityTargetingGameLastRejectedAim)) {
		s32 aimedpropnum = accessibilityTargetingGamePropNum(aimedprop);

		accessibilityLogEvent("targeting", "aimed_candidate_rejected",
				"frame=%d aimed_prop=%p propnum=%d prop_type=%d reason=not_in_visible_hostile_character_set",
				g_Vars.lvframe60, (void *)aimedprop,
				aimedpropnum, aimedpropnum >= 0 ? aimedprop->type : -1);
	}
	g_AccessibilityTargetingGameLastRejectedAim = observation->hasaimedtarget
			? 0 : (uintptr_t)aimedprop;
	observation->nativealignmentexpected = observation->hasaimedtarget
			&& aimedshootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
			&& !alignmentusesraw
			&& accessibilityTargetingGameNativeAlignmentExpected(aimedprop);

	if (detailed || scopechanged) {
		accessibilityLogEvent("targeting", "scope_gate",
				"frame=%d stage=%d player=%d accepted=1 reason=in_scope mode=combat weapon=%d function=%d threat_detector=%d autoaim_x_enabled=%d autoaim_y_enabled=%d autoaim_x_prop=%p autoaim_y_prop=%p candidates=%d captured=%d aimed=%d aimed_prop=%p raw_aim_prop=%p raw_aim_valid=%d raw_aim_hitpart=%d raw_aim_region=%d tolerant_turret_prop=%p tolerant_distance_px=%.3f tolerance_px=%.3f alignment_source=%s aimed_shootability=%d native_alignment_expected=%d viewport=%.3f,%.3f,%.3f,%.3f",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
				bgunGetWeaponNum(HAND_RIGHT),
				g_Vars.currentplayer->hands[HAND_RIGHT].gset.weaponfunc,
				accessibilityTargetingGameThreatDetectorActive(),
				bmoveIsAutoAimXEnabledForCurrentWeapon(),
				bmoveIsAutoAimYEnabledForCurrentWeapon(),
				(void *)g_Vars.currentplayer->autoxaimprop,
				(void *)g_Vars.currentplayer->autoyaimprop,
				observation->candidatecount,
				g_AccessibilityTargetingCombatProjectionCount,
				observation->hasaimedtarget, (void *)aimedprop,
				(void *)rawaimedprop,
				g_AccessibilityTargetingGameRawAimHitValid,
				g_AccessibilityTargetingGameRawAimHitPartValid
						? g_AccessibilityTargetingGameRawAimHitPart : 0,
				g_AccessibilityTargetingGameRawAimHitPartValid
						? accessibilityTargetingGameAimRegion(
								g_AccessibilityTargetingGameRawAimHitPart)
						: ACCESSIBILITY_TARGETING_AIM_REGION_NONE,
				(void *)tolerantaimedturret, tolerantturretdistance,
				ACCESSIBILITY_TARGETING_TURRET_AIM_TOLERANCE,
				alignmentsource,
				aimedshootability, observation->nativealignmentexpected,
				viewleft, viewtop, viewright, viewbottom);
	}

	accessibilityTargetingObserve(observation);
}

static void accessibilityTargetingObserveDevice(
		struct accessibilitytargetingobservation *observation,
		s32 detailed, s32 scopechanged)
{
	s32 camspyscope = accessibilityTargetingGameCamSpyInScope();
	s32 i;

	observation->inscope = true;
	observation->sighton = g_Vars.currentplayer->lastsighton;
	observation->targetindicatorvisible = camspyscope
			|| !g_Vars.currentplayer->gunsightoff;

	for (i = 0; i < ARRAYCOUNT(g_AccessibilityTargetingDeviceTargets); i++) {
		const struct accessibilitytargetingdevicetarget *targetspec
				= &g_AccessibilityTargetingDeviceTargets[i];
		struct defaultobj *obj;
		struct prop *prop;
		struct accessibilitytargetingcandidate *candidate;
		const char *reason = "eligible";
		s32 propnum;
		s32 equipped;
		s32 eligible = true;
		f32 dx;
		f32 dy;
		f32 dz;

		if (!accessibilityTargetingGameDeviceTargetInScope(targetspec)) {
			continue;
		}

		obj = objFindByTagId(targetspec->tagid);
		prop = obj ? obj->prop : NULL;
		propnum = accessibilityTargetingGamePropNum(prop);
		equipped = bgunGetWeaponNum(HAND_RIGHT) == targetspec->weaponnum;

		if (!equipped) {
			eligible = false;
			reason = "training_device_not_equipped";
		} else if (propnum < 0 || !obj || !prop) {
			eligible = false;
			reason = "target_unavailable";
		} else if (prop->type != PROPTYPE_OBJ) {
			eligible = false;
			reason = "wrong_prop_type";
		} else if (!prop->active || (prop->flags & PROPFLAG_ENABLED) == 0) {
			eligible = false;
			reason = "inactive_or_disabled";
		} else if (obj->flags2 & OBJFLAG2_INVISIBLE) {
			eligible = false;
			reason = "object_invisible";
		} else if (obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE)) {
			eligible = false;
			reason = "object_deleting_or_gone";
		}

		if (eligible && observation->candidatecount
				< ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			candidate = &observation->candidates[observation->candidatecount++];
			memset(candidate, 0, sizeof(*candidate));
			candidate->identity.playernum = g_Vars.currentplayernum;
			candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_DEVICE;
			candidate->identity.sourceslot = targetspec->tagid;
			candidate->identity.propnum = propnum;
			candidate->identity.proptype = prop->type;
			candidate->identity.objectidentity = (uintptr_t)obj;
			candidate->prop = prop;
			candidate->category = ACCESSIBILITY_TARGETING_CATEGORY_OBJECT;
			candidate->relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
			candidate->shootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
			candidate->position = prop->pos;
			dx = prop->pos.x - g_Vars.currentplayer->prop->pos.x;
			dy = prop->pos.y - g_Vars.currentplayer->prop->pos.y;
			dz = prop->pos.z - g_Vars.currentplayer->prop->pos.z;
			candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);

			if (g_AccessibilityTargetingGameRawAimHitValid
					&& g_AccessibilityTargetingGameRawAimProp
							== (uintptr_t)prop) {
				observation->hasaimedtarget = true;
				observation->aimedidentity = candidate->identity;
				dx = g_AccessibilityTargetingGameRawAimHitPos.x
						- g_Vars.currentplayer->cam_pos.x;
				dy = g_AccessibilityTargetingGameRawAimHitPos.y
						- g_Vars.currentplayer->cam_pos.y;
				dz = g_AccessibilityTargetingGameRawAimHitPos.z
						- g_Vars.currentplayer->cam_pos.z;
				candidate->aimdistance = sqrtf(dx * dx + dy * dy + dz * dz);
			}
		}

		if (detailed || scopechanged) {
			accessibilityLogEvent("targeting", "device_candidate",
					"frame=%d stage=%d player=%d accepted=%d reason=%s weapon=%d equipped=%d target_tag=%d prop=%p propnum=%d obj=%p prop_type=%d prop_flags=0x%02x obj_flags2=0x%08x obj_hidden=0x%08x raw_aim_prop=%p raw_aim_valid=%d aimed=%d",
					g_Vars.lvframe60, g_Vars.stagenum,
					g_Vars.currentplayernum, eligible, reason,
					targetspec->weaponnum, equipped, targetspec->tagid,
					(void *)prop, propnum, (void *)obj,
					propnum >= 0 ? prop->type : -1,
					propnum >= 0 ? prop->flags : 0,
					obj ? obj->flags2 : 0, obj ? obj->hidden : 0,
					(void *)g_AccessibilityTargetingGameRawAimProp,
					g_AccessibilityTargetingGameRawAimHitValid,
					observation->hasaimedtarget);
		}
	}

	if (camspyscope) {
		s32 bestindex = -1;
		f32 bestoffset = 0.0f;
		f32 screencenterx = camGetScreenLeft() + camGetScreenWidth() * 0.5f;
		f32 screencentery = camGetScreenTop() + camGetScreenHeight() * 0.5f;

		for (i = 0; i < g_AccessibilityTargetingCamSpyTargetCount; i++) {
			struct accessibilitytargetingcamspytarget *capture
					= &g_AccessibilityTargetingCamSpyTargets[i];

			if (capture->eligible) {
				f32 xoffset = (capture->screenx1 + capture->screenx2) * 0.5f
						- screencenterx;
				f32 yoffset = (capture->screeny1 + capture->screeny2) * 0.5f
						- screencentery;
				f32 offset = xoffset * xoffset + yoffset * yoffset;

				if (bestindex < 0 || offset < bestoffset) {
					bestindex = i;
					bestoffset = offset;
				}
			}
		}

		for (i = 0; i < g_AccessibilityTargetingCamSpyTargetCount; i++) {
			struct accessibilitytargetingcamspytarget *capture
					= &g_AccessibilityTargetingCamSpyTargets[i];
			struct prop *prop = (struct prop *)capture->prop;
			struct defaultobj *obj = (struct defaultobj *)capture->obj;
			struct accessibilitytargetingcandidate *candidate = NULL;
			s32 aimed = false;

			if (capture->eligible && observation->candidatecount
					< ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
				candidate = &observation->candidates[
						observation->candidatecount++];
				memset(candidate, 0, sizeof(*candidate));
				candidate->identity.playernum = g_Vars.currentplayernum;
				candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_DEVICE;
				candidate->identity.sourceslot = capture->tagid;
				candidate->identity.propnum = capture->propnum;
				candidate->identity.proptype = prop->type;
				candidate->identity.objectidentity = capture->criteria;
				candidate->prop = prop;
				candidate->category = ACCESSIBILITY_TARGETING_CATEGORY_OBJECT;
				candidate->relationship
						= ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
				candidate->shootability
						= ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
				candidate->position = prop->pos;
				candidate->distance = capture->distance;
				candidate->screenx1 = capture->screenx1;
				candidate->screeny1 = capture->screeny1;
				candidate->screenx2 = capture->screenx2;
				candidate->screeny2 = capture->screeny2;

				if (i == bestindex) {
					aimed = true;
					observation->hasaimedtarget = true;
					observation->aimedidentity = candidate->identity;
					candidate->aimdistance = capture->distance;
				}
			}

			if (detailed || scopechanged) {
				accessibilityLogEvent("targeting", "camspy_candidate",
						"frame=%d stage=%d player=%d accepted=%d reason=%s criterion=%p status=%d tag=%d prop=%p propnum=%d obj=%p prop_type=%d healthy=%d rendered=%d z=%.3f distance=%.3f screen=%.3f,%.3f,%.3f,%.3f raw_aim_prop=%p raw_aim_valid=%d aimed=%d",
						g_Vars.lvframe60, g_Vars.stagenum,
						g_Vars.currentplayernum, capture->eligible,
						capture->reason, (void *)capture->criteria,
						capture->status, capture->tagid, (void *)prop,
						capture->propnum, (void *)obj,
						capture->propnum >= 0 ? prop->type : -1,
						obj ? objIsHealthy(obj) : false,
						capture->propnum >= 0
								&& (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK),
						capture->propnum >= 0 ? prop->z : 0.0f,
						capture->distance, capture->screenx1,
						capture->screeny1, capture->screenx2,
						capture->screeny2,
						(void *)g_AccessibilityTargetingGameRawAimProp,
						g_AccessibilityTargetingGameRawAimHitValid, aimed);
			}
		}
	}

	accessibilityTargetingObserve(observation);
}

void accessibilityTargetingObserveGame(void)
{
	struct accessibilitytargetingobservation observation;
	s32 hasdevicetargets = accessibilityTargetingGameHasDeviceTargets();
	struct frdata *frdata;
	struct prop *aimedprop;
	const char *scopereason = accessibilityTargetingGameScopeReason();
	f32 viewleft;
	f32 viewtop;
	f32 viewright;
	f32 viewbottom;
	f32 viewcenterx;
	s32 i;
	s32 detailed;
	s32 scopechanged;
	s32 aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
	const char *modename = hasdevicetargets
			? accessibilityTargetingGameCamSpyInScope() ? "camspy" : "device"
			: accessibilityTargetingGameIsFiringRange()
				? "firing_range" : "combat";

	memset(&observation, 0, sizeof(observation));
	observation.playernum = g_Vars.currentplayernum;
	observation.source = hasdevicetargets
			? ACCESSIBILITY_TARGETING_SOURCE_DEVICE
			: accessibilityTargetingGameIsFiringRange()
				? ACCESSIBILITY_TARGETING_SOURCE_FIRING_RANGE
				: ACCESSIBILITY_TARGETING_SOURCE_COMBAT;
	observation.profile = hasdevicetargets
			? ACCESSIBILITY_TARGETING_PROFILE_DEVICE
			: accessibilityTargetingGameIsFiringRange()
				? ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE
				: ACCESSIBILITY_TARGETING_PROFILE_COMBAT;
	observation.stagenum = g_Vars.stagenum;
	observation.frame60 = g_Vars.lvframe60;

	if (g_Vars.lvframe60 < g_AccessibilityTargetingGameLastFrame60
			|| observation.source != g_AccessibilityTargetingGameLastSource) {
		g_AccessibilityTargetingGameNextAudit60 = 0;
		g_AccessibilityTargetingGameAuditValid = false;
	}
	g_AccessibilityTargetingGameLastFrame60 = g_Vars.lvframe60;
	g_AccessibilityTargetingGameLastSource = observation.source;
	detailed = g_AccessibilityTargetingGameNextAudit60 == 0
			|| g_Vars.lvframe60 >= g_AccessibilityTargetingGameNextAudit60;

	if (scopereason) {
		if (detailed || scopereason != g_AccessibilityTargetingGameLastScopeReason) {
			accessibilityLogEvent("targeting", "scope_gate",
				"frame=%d stage=%d player=%d accepted=0 reason=%s mode=%s valid_weapon=%d player_count=%d menu_count=%d tickmode=%d lvupdate60=%d",
				g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
				scopereason, modename,
				g_FrIsValidWeapon, PLAYERCOUNT(), g_MenuData.count,
				g_Vars.tickmode, g_Vars.lvupdate60);
		}
		g_AccessibilityTargetingGameLastScopeReason = scopereason;
		if (detailed) {
			g_AccessibilityTargetingGameNextAudit60
					= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
		}
		accessibilityTargetingObserve(&observation);
		accessibilityTargetingGameClearProjections();
		return;
	}
	scopechanged = g_AccessibilityTargetingGameLastScopeReason != NULL;
	g_AccessibilityTargetingGameLastScopeReason = NULL;
	accessibilityTargetingGameObserveNativeThreats(&observation, detailed);

	if (hasdevicetargets) {
		accessibilityTargetingObserveDevice(&observation, detailed, scopechanged);
		if (detailed) {
			g_AccessibilityTargetingGameNextAudit60
					= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
		}
		accessibilityTargetingGameClearProjections();
		return;
	}

	if (!accessibilityTargetingGameIsFiringRange()) {
		accessibilityTargetingObserveCombat(&observation, detailed, scopechanged);
		if (detailed) {
			g_AccessibilityTargetingGameNextAudit60
					= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
		}
		accessibilityTargetingGameClearProjections();
		return;
	}

	observation.inscope = true;
	observation.sighton = g_Vars.currentplayer->lastsighton;
	observation.targetindicatorvisible = !g_Vars.currentplayer->gunsightoff;
	frdata = frGetData();
	aimedprop = g_Vars.currentplayer->lookingatprop.prop;

	viewleft = (f32)viGetViewLeft() / g_ScaleX;
	viewtop = viGetViewTop();
	viewright = viewleft + (f32)viGetViewWidth() / g_ScaleX;
	viewbottom = viewtop + viGetViewHeight();
	viewcenterx = (viewleft + viewright) * 0.5f;

	for (i = 0; frdata && i < ARRAYCOUNT(frdata->targets); i++) {
		struct frtarget *target = &frdata->targets[i];
		struct prop *prop = target->prop;
		struct defaultobj *obj = NULL;
		struct accessibilitytargetinggameprojection *projection
				= &g_AccessibilityTargetingGameProjections[i];
		struct accessibilitytargetingcandidate *candidate;
		const char *reason = "eligible";
		f32 x2;
		f32 x1;
		f32 y2;
		f32 y1;
		f32 dx;
		f32 dy;
		f32 dz;
		s32 propnum = accessibilityTargetingGamePropNum(prop);
		s32 eligible = true;
		s32 shootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
		struct accessibilitytargetinggameaudit audit;

		if (propnum >= 0) {
			obj = prop->obj;
		}

		if (!target->inuse) {
			eligible = false;
			reason = "not_in_use";
		} else if (!target->active) {
			eligible = false;
			reason = "not_active";
		} else if (target->destroyed) {
			eligible = false;
			reason = "destroyed";
		} else if (propnum < 0 || !obj) {
			eligible = false;
			reason = "invalid_prop_or_object";
		} else if (prop->type != PROPTYPE_OBJ) {
			eligible = false;
			reason = "wrong_prop_type";
		} else if (obj->modelnum != MODEL_TARGET) {
			eligible = false;
			reason = "wrong_model";
		} else if ((prop->flags & PROPFLAG_ENABLED) == 0) {
			eligible = false;
			reason = "prop_disabled";
		} else if (obj->flags2 & OBJFLAG2_INVISIBLE) {
			eligible = false;
			reason = "object_invisible";
		} else if ((prop->flags & PROPFLAG_ONTHISSCREENTHISTICK) == 0) {
			eligible = false;
			reason = "not_rendered_this_tick";
		} else if (!obj->model || !obj->model->matrices || !obj->model->definition) {
			eligible = false;
			reason = "model_unavailable";
		} else if (!g_AccessibilityTargetingGameProjectionsValid
				|| g_AccessibilityTargetingGameProjectionFrame60 != g_Vars.lvframe60
				|| g_AccessibilityTargetingGameProjectionPlayer
						!= g_Vars.currentplayernum
				|| projection->prop != (uintptr_t)prop
				|| projection->obj != (uintptr_t)obj
				|| projection->propnum != propnum) {
			eligible = false;
			reason = "projection_capture_unavailable";
		} else if (!projection->projected) {
			eligible = false;
			reason = "projection_failed";
		} else if (!projection->finite) {
			eligible = false;
			reason = "projection_non_finite";
		} else {
			x2 = projection->x2;
			x1 = projection->x1;
			y2 = projection->y2;
			y1 = projection->y1;

			if (x2 < viewleft || x1 > viewright
					|| y2 < viewtop || y1 > viewbottom) {
				eligible = false;
				reason = "outside_viewport";
			}
		}

		if (eligible) {
			shootability = frIsTargetFacingPos(prop,
					&g_Vars.currentplayer->prop->pos)
				? ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
				: ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY;
		}

		memset(&audit, 0, sizeof(audit));
		audit.prop = (uintptr_t)prop;
		audit.obj = (uintptr_t)obj;
		audit.propnum = propnum;
		audit.proptype = propnum >= 0 ? prop->type : -1;
		audit.modelnum = obj ? obj->modelnum : -1;
		audit.propflags = propnum >= 0 ? prop->flags : 0;
		audit.objflags2 = obj ? obj->flags2 : 0;
		audit.inuse = target->inuse;
		audit.active = target->active;
		audit.destroyed = target->destroyed;
		audit.accepted = eligible;
		audit.shootability = shootability;
		audit.reason = reason;

		if (detailed || !g_AccessibilityTargetingGameAuditValid
				|| !accessibilityTargetingGameAuditEqual(
					&audit, &g_AccessibilityTargetingGameAudit[i])) {
			accessibilityLogEvent("targeting", "range_candidate",
				"frame=%d slot=%d accepted=%d reason=%s shootability=%d shootability_reason=%s inuse=%d active=%d destroyed=%d prop=%p propnum=%d obj=%p prop_type=%d model=%d prop_flags=0x%02x obj_flags2=0x%08x capture_valid=%d projected=%d finite=%d screen=%.3f,%.3f,%.3f,%.3f",
				g_Vars.lvframe60, i, eligible, reason, shootability,
				shootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
						? "shootable" : shootability
								== ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY
							? "facing_away" : "unknown",
				target->inuse,
				target->active, target->destroyed, (void *)prop, propnum,
				(void *)obj, propnum >= 0 ? prop->type : -1,
				obj ? obj->modelnum : -1, propnum >= 0 ? prop->flags : 0,
				obj ? obj->flags2 : 0,
				g_AccessibilityTargetingGameProjectionsValid
						&& g_AccessibilityTargetingGameProjectionFrame60
								== g_Vars.lvframe60
						&& g_AccessibilityTargetingGameProjectionPlayer
								== g_Vars.currentplayernum,
				projection->projected, projection->finite,
				projection->x1, projection->y1,
				projection->x2, projection->y2);
		}
		g_AccessibilityTargetingGameAudit[i] = audit;

		if (!eligible || observation.candidatecount
				>= ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			continue;
		}

		candidate = &observation.candidates[observation.candidatecount++];
		memset(candidate, 0, sizeof(*candidate));
		candidate->identity.playernum = g_Vars.currentplayernum;
		candidate->identity.source = ACCESSIBILITY_TARGETING_SOURCE_FIRING_RANGE;
		candidate->identity.sourceslot = i;
		candidate->identity.propnum = propnum;
		candidate->identity.proptype = prop->type;
		candidate->identity.objectidentity = (uintptr_t)obj;
		candidate->prop = prop;
		candidate->category = ACCESSIBILITY_TARGETING_CATEGORY_RANGE_TARGET;
		candidate->relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_NEUTRAL;
		candidate->shootability = shootability;
		candidate->position = prop->pos;
		candidate->screenx1 = x1;
		candidate->screeny1 = y1;
		candidate->screenx2 = x2;
		candidate->screeny2 = y2;
		candidate->horizontalscreenoffset
				= fabsf(((x1 + x2) * 0.5f) - viewcenterx);
		dx = prop->pos.x - g_Vars.currentplayer->prop->pos.x;
		dy = prop->pos.y - g_Vars.currentplayer->prop->pos.y;
		dz = prop->pos.z - g_Vars.currentplayer->prop->pos.z;
		candidate->distance = sqrtf(dx * dx + dy * dy + dz * dz);
		candidate->localizedname = "Firing range target";

		if (prop == aimedprop) {
			f32 aimdx;
			f32 aimdy;
			f32 aimdz;

			observation.hasaimedtarget = true;
			observation.aimedidentity = candidate->identity;
			aimedshootability = candidate->shootability;

			if (g_AccessibilityTargetingGameAimHitValid
					&& g_AccessibilityTargetingGameAimProp == (uintptr_t)prop) {
				aimdx = g_AccessibilityTargetingGameAimHitPos.x - prop->pos.x;
				aimdy = g_AccessibilityTargetingGameAimHitPos.y - prop->pos.y;
				aimdz = g_AccessibilityTargetingGameAimHitPos.z - prop->pos.z;
				candidate->aimdistance = sqrtf(aimdx * aimdx + aimdy * aimdy
						+ aimdz * aimdz);
				candidate->aimquality = 1.0f - candidate->aimdistance
						/ ACCESSIBILITY_TARGETING_RANGE_OUTER_RADIUS;

				if (candidate->aimquality < 0.0f) {
					candidate->aimquality = 0.0f;
				} else if (candidate->aimquality > 1.0f) {
					candidate->aimquality = 1.0f;
				}

				candidate->hasaimquality = true;
			}
		}
	}

	if (aimedprop && !observation.hasaimedtarget
			&& (detailed
				|| (uintptr_t)aimedprop != g_AccessibilityTargetingGameLastRejectedAim)) {
		s32 aimedpropnum = accessibilityTargetingGamePropNum(aimedprop);
		struct defaultobj *aimedobj
				= aimedpropnum >= 0 ? aimedprop->obj : NULL;

		accessibilityLogEvent("targeting", "aimed_candidate_rejected",
				"frame=%d aimed_prop=%p propnum=%d prop_type=%d obj=%p model=%d reason=not_in_visible_range_candidate_set",
				g_Vars.lvframe60, (void *)aimedprop, aimedpropnum,
				aimedpropnum >= 0 ? aimedprop->type : -1, (void *)aimedobj,
				aimedobj ? aimedobj->modelnum : -1);
	}
	g_AccessibilityTargetingGameLastRejectedAim
			= observation.hasaimedtarget ? 0 : (uintptr_t)aimedprop;

	observation.nativealignmentexpected
			= observation.hasaimedtarget
			&& aimedshootability == ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE
			&& accessibilityTargetingGameNativeAlignmentExpected(aimedprop);

	if (detailed || scopechanged) {
		accessibilityLogEvent("targeting", "scope_gate",
			"frame=%d stage=%d player=%d accepted=1 reason=in_scope candidates=%d aimed=%d aimed_prop=%p aimed_shootability=%d native_alignment_expected=%d viewport=%.3f,%.3f,%.3f,%.3f",
			g_Vars.lvframe60, g_Vars.stagenum, g_Vars.currentplayernum,
			observation.candidatecount, observation.hasaimedtarget,
			(void *)aimedprop, aimedshootability,
			observation.nativealignmentexpected,
			viewleft, viewtop, viewright, viewbottom);
	}
	g_AccessibilityTargetingGameAuditValid = true;
	if (detailed) {
		g_AccessibilityTargetingGameNextAudit60
				= g_Vars.lvframe60 + ACCESSIBILITY_TARGETING_AUDIT_TICKS;
	}

	accessibilityTargetingObserve(&observation);
	accessibilityTargetingGameClearProjections();
}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityTargetingGetDiagnostics(
		struct accessibilitytargetingdiagnostics *diagnostics)
{
	if (diagnostics) {
		*diagnostics = g_AccessibilityTargetingDiagnostics;
	}
}
#endif

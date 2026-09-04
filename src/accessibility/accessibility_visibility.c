#include <math.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "game/bondgun.h"
#include "game/prop.h"
#include "game/propobj.h"
#include "lib/collision.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_visibility.h"

#define ACCESSIBILITY_VISIBILITY_SURFACE_INSET 0.35f
#define ACCESSIBILITY_VISIBILITY_SURFACE_PULL_FORWARD 0.25f
#define ACCESSIBILITY_VISIBILITY_EMBEDDED_SURFACE_TOLERANCE 8.0f
#define ACCESSIBILITY_VISIBILITY_RENDERPOSTBG_SURFACE_TOLERANCE 24.0f

bool accessibilityVisibilityIsXrayExposed(struct prop *prop)
{
	f32 distance;

	/*
	 * The Farsight also enters VISIONMODE_XRAY. Require the native scanner's
	 * uninhibited device bit so its weapon sight cannot broaden the semantic
	 * scanners. The retained screen flag is the same preceding-frame render
	 * evidence formerly used by the generic X-Ray audio lane.
	 */
	return prop
			&& accessibilityIsXrayScannerAudioEnabled()
			&& PLAYERCOUNT() == 1
			&& g_Vars.currentplayer
			&& (g_Vars.currentplayer->devicesactive
					& ~g_Vars.currentplayer->devicesinhibit
					& DEVICE_XRAYSCANNER)
			&& (prop->flags & PROPFLAG_ONANYSCREENPREVTICK)
			&& objGetXrayHighlightDistance(prop, &distance);
}

bool accessibilityVisibilityIsFarsightExposed(struct prop *prop)
{
	f32 distance;

	/*
	 * FarSight aiming uses the native X-Ray renderer in both manual-depth and
	 * Target Locator modes. Keep this separate from X-Ray Scanner semantic
	 * visibility: only combat targeting should use this evidence.
	 */
	return prop
			&& PLAYERCOUNT() == 1
			&& g_Vars.currentplayer
			&& bgunGetWeaponNum(HAND_RIGHT) == WEAPON_FARSIGHT
			&& g_Vars.currentplayer->gunsightoff == 0
			&& g_Vars.currentplayer->visionmode == VISIONMODE_XRAY
			&& (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK)
			&& objGetXrayHighlightDistance(prop, &distance);
}

bool accessibilityVisibilityHasVisualLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct coord *targetpos, RoomNum *targetrooms,
		struct prop *targetprop)
{
	struct defaultobj *targetobj = NULL;
	s32 restoreperimeter = false;
	s32 result;

	if (!viewpos || !viewrooms || !targetpos || !targetrooms) {
		return false;
	}

	if (targetprop && (targetprop->type == PROPTYPE_OBJ
			|| targetprop->type == PROPTYPE_WEAPON
			|| targetprop->type == PROPTYPE_DOOR)) {
		targetobj = targetprop->obj;

		/*
		 * The endpoint may be inside the target's own collision volume.
		 * Exclude only that perimeter for this synchronous main-thread query,
		 * preserving any pre-existing disabled state.
		 */
		if (targetobj
				&& (targetobj->hidden & OBJHFLAG_PERIMDISABLED) == 0) {
			propSetPerimEnabled(targetprop, false);
			restoreperimeter = true;
		}
	}

	result = cdTestLos05(viewpos, viewrooms, targetpos, targetrooms,
			CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER
				| CDTYPE_AIOPAQUE,
			GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT);

	if (restoreperimeter) {
		propSetPerimEnabled(targetprop, true);
	}

	return result;
}

static void accessibilityVisibilityTransformObjectPoint(
		struct defaultobj *obj, struct coord *local, struct coord *world)
{
	world->x = obj->prop->pos.x
			+ obj->realrot[0][0] * local->x
			+ obj->realrot[1][0] * local->y
			+ obj->realrot[2][0] * local->z;
	world->y = obj->prop->pos.y
			+ obj->realrot[0][1] * local->x
			+ obj->realrot[1][1] * local->y
			+ obj->realrot[2][1] * local->z;
	world->z = obj->prop->pos.z
			+ obj->realrot[0][2] * local->x
			+ obj->realrot[1][2] * local->y
			+ obj->realrot[2][2] * local->z;
}

static void accessibilityVisibilityPullPointTowardCamera(
		struct coord *point, const struct coord *camera, f32 amount)
{
	f32 x = camera->x - point->x;
	f32 y = camera->y - point->y;
	f32 z = camera->z - point->z;
	f32 distance = sqrtf(x * x + y * y + z * z);

	if (distance > amount) {
		f32 scale = amount / distance;

		point->x += x * scale;
		point->y += y * scale;
		point->z += z * scale;
	}
}

bool accessibilityVisibilityHasObjectSurfaceLineOfSight(
		struct coord *viewpos, RoomNum *viewrooms,
		struct prop *targetprop, bool allowembedded,
		s32 *sample, s32 *queries)
{
	struct defaultobj *obj = NULL;
	struct modelrodata_bbox *bbox;
	struct coord local;
	struct coord facecenters[6];
	struct coord targets[5];
	f32 mins[3];
	f32 maxs[3];
	f32 mids[3];
	f32 halfspans[3];
	f32 bestdist = 0.0f;
	s32 bestface = -1;
	s32 faceaxis;
	s32 otheraxis1;
	s32 otheraxis2;
	s32 i;
	s32 localsample = 0;
	s32 localqueries = 1;

	if (!sample) {
		sample = &localsample;
	}

	if (!queries) {
		queries = &localqueries;
	}

	*sample = 0;
	*queries = 1;

	if (!targetprop) {
		return false;
	}

	if (targetprop->type == PROPTYPE_OBJ
			|| targetprop->type == PROPTYPE_WEAPON
			|| targetprop->type == PROPTYPE_DOOR) {
		obj = targetprop->obj;
	}

	if (accessibilityVisibilityHasVisualLineOfSight(
			viewpos, viewrooms, &targetprop->pos,
			targetprop->rooms, targetprop)) {
		return true;
	}

	if (!obj || !obj->model || obj->prop != targetprop) {
		return false;
	}

	bbox = objFindBboxRodata(obj);

	if (!bbox) {
		return false;
	}

	mins[0] = objGetLocalXMin(bbox);
	mins[1] = objGetLocalYMin(bbox);
	mins[2] = objGetLocalZMin(bbox);
	maxs[0] = objGetLocalXMax(bbox);
	maxs[1] = objGetLocalYMax(bbox);
	maxs[2] = objGetLocalZMax(bbox);

	for (i = 0; i < 3; i++) {
		mids[i] = (mins[i] + maxs[i]) * 0.5f;
		halfspans[i] = (maxs[i] - mins[i]) * 0.5f;
	}

	for (i = 0; i < 6; i++) {
		f32 dx;
		f32 dy;
		f32 dz;
		f32 distsq;
		s32 axis = i / 2;

		local.x = mids[0];
		local.y = mids[1];
		local.z = mids[2];
		local.f[axis] = (i & 1) ? maxs[axis] : mins[axis];
		accessibilityVisibilityTransformObjectPoint(
				obj, &local, &facecenters[i]);

		dx = facecenters[i].x - viewpos->x;
		dy = facecenters[i].y - viewpos->y;
		dz = facecenters[i].z - viewpos->z;
		distsq = dx * dx + dy * dy + dz * dz;

		if (bestface < 0 || distsq < bestdist) {
			bestface = i;
			bestdist = distsq;
		}
	}

	faceaxis = bestface / 2;
	otheraxis1 = (faceaxis + 1) % 3;
	otheraxis2 = (faceaxis + 2) % 3;
	local.x = mids[0];
	local.y = mids[1];
	local.z = mids[2];
	local.f[faceaxis] = (bestface & 1) ? maxs[faceaxis] : mins[faceaxis];

	for (i = 0; i < ARRAYCOUNT(targets); i++) {
		struct coord samplelocal = local;

		if (i > 0) {
			samplelocal.f[otheraxis1] += halfspans[otheraxis1]
					* ACCESSIBILITY_VISIBILITY_SURFACE_INSET
					* ((i & 1) ? 1.0f : -1.0f);
			samplelocal.f[otheraxis2] += halfspans[otheraxis2]
					* ACCESSIBILITY_VISIBILITY_SURFACE_INSET
					* ((i & 2) ? 1.0f : -1.0f);
		}

		accessibilityVisibilityTransformObjectPoint(
				obj, &samplelocal, &targets[i]);
		accessibilityVisibilityPullPointTowardCamera(&targets[i], viewpos,
				ACCESSIBILITY_VISIBILITY_SURFACE_PULL_FORWARD);
		(*queries)++;

		if (accessibilityVisibilityHasVisualLineOfSight(
				viewpos, viewrooms, &targets[i],
				targetprop->rooms, targetprop)) {
			*sample = i + 1;
			return true;
		}
	}

	if (allowembedded
			&& (targetprop->flags & PROPFLAG_ONTHISSCREENTHISTICK)
			&& (obj->flags2 & OBJFLAG2_INTERACTCHECKLOS) == 0) {
		f32 tolerance = ACCESSIBILITY_VISIBILITY_EMBEDDED_SURFACE_TOLERANCE;

		if (obj->flags & OBJFLAG_MONITOR_RENDERPOSTBG) {
			tolerance =
					ACCESSIBILITY_VISIBILITY_RENDERPOSTBG_SURFACE_TOLERANCE;
		}

		for (i = 0; i < ARRAYCOUNT(targets); i++) {
			accessibilityVisibilityPullPointTowardCamera(
					&targets[i], viewpos, tolerance);
			(*queries)++;

			if (accessibilityVisibilityHasVisualLineOfSight(
					viewpos, viewrooms, &targets[i],
					targetprop->rooms, targetprop)) {
				*sample = ARRAYCOUNT(targets) + i + 1;
				return true;
			}
		}
	}

	return false;
}

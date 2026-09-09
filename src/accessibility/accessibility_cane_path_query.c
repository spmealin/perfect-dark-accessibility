#include <math.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "types.h"
#include "data.h"
#include "game/bondmove.h"
#include "game/prop.h"
#include "lib/collision.h"
#include "system.h"
#include "accessibility/accessibility_cane_path_query.h"

struct accessibilitycanepathcontext {
	const struct accessibilityobserver *observer;
	struct coord direction;
	struct accessibilitycanepathdiagnostic *diagnostic;
	u64 started;
	f32 originheight;
	f32 bottom;
	s32 types;
};

static s32 accessibilityCanePathReserve(struct accessibilitycanepathcontext *context)
{
	if (context->diagnostic->queries >= ACCESSIBILITY_CANE_PATH_QUERY_LIMIT) {
		context->diagnostic->budget = 1;
		return false;
	}
	if (sysGetMicroseconds() - context->started >= ACCESSIBILITY_CANE_PATH_TIME_LIMIT_US) {
		context->diagnostic->budget = 2;
		return false;
	}
	context->diagnostic->queries++;
	return true;
}

static struct coord accessibilityCanePathPosition(struct accessibilitycanepathcontext *context,
		float distance, float ground)
{
	struct coord pos = context->observer->origin;
	pos.x += context->direction.x * distance;
	pos.z += context->direction.z * distance;
	pos.y = ground + context->originheight;
	return pos;
}

static void accessibilityCanePathRooms(struct coord *from, RoomNum *fromrooms,
		struct coord *to, RoomNum *rooms)
{
	RoomNum extra[20];
	func0f065dfc(from, fromrooms, to, rooms, extra, 20);
	bmoveFindEnteredRoomsByPos(g_Vars.currentplayer, to, rooms);
}

static int accessibilityCanePathFloor(void *opaque,
		const struct accessibilitycanepathnode *from, float distance,
		struct accessibilitycanepathfloor *floor)
{
	struct accessibilitycanepathcontext *context = opaque;
	struct coord start = context->observer->origin;
	RoomNum rooms[8];
	RoomNum *startrooms = context->observer->prop->rooms;
	struct coord pos;
	struct prop *lift = NULL;
	struct prop *pointprop = NULL;
	struct coord normal;
	u16 colour = 0, flags = 0;
	u8 type = 0;
	RoomNum room = -1;
	s32 inlift = false;
	float ground = g_Vars.currentplayer->vv_manground;
	if (from) {
		ground = from->floor.ground;
		start = accessibilityCanePathPosition(context, from->distance, ground);
		memcpy(rooms, from->floor.rooms, sizeof(rooms));
		startrooms = rooms;
	}
	pos = accessibilityCanePathPosition(context, distance, ground);
	context->diagnostic->lastdistance = distance;
	memset(floor, 0, sizeof(*floor));
	floor->room = -1;
	floor->rooms[0] = -1;
	if (!accessibilityCanePathReserve(context)) return false;
	accessibilityCanePathRooms(&start, startrooms, &pos, floor->rooms);
	if (floor->rooms[0] < 0) {
		context->diagnostic->errors++;
		return false;
	}
	if (!accessibilityCanePathReserve(context)) return false;
	context->diagnostic->floorqueries++;
	floor->ground = cdFindGroundInfoAtCyl(&pos, context->observer->radius,
			floor->rooms, &colour, &type, &flags, &room, &inlift, &lift);
	floor->room = room;
	floor->flags = flags;
	floor->supported = isfinite(floor->ground) && floor->ground > -30000 && room >= 0;
	floor->platform = (uintptr_t)lift;
	if (!accessibilityCanePathReserve(context)) return false;
	context->diagnostic->floorqueries++;
	floor->pointground = -30000;
	room = cdFindFloorRoomYColourNormalPropAtPos(&pos, floor->rooms,
			&floor->pointground, NULL, &normal, &pointprop);
	floor->pointsupported = room >= 0 && isfinite(floor->pointground)
			&& floor->pointground > -30000;
	if (!floor->platform) floor->platform = (uintptr_t)pointprop;
	context->diagnostic->lastground = floor->ground;
	context->diagnostic->lastpointground = floor->pointground;
	return true;
}

static enum accessibilitycaneevidence accessibilityCanePathSegment(
		struct accessibilitycanepathcontext *context, struct coord *start,
		RoomNum *startrooms, struct coord *end, float height)
{
	RoomNum rooms[8];
	s32 result;
	float top = height - context->originheight;
	float bottom = context->bottom - context->originheight;
	context->diagnostic->lastclearance = ACCESSIBILITY_CANE_UNKNOWN;
	context->diagnostic->blocker = 0;
	if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
	accessibilityCanePathRooms(start, startrooms, end, rooms);
	if (rooms[0] < 0) {
		context->diagnostic->errors++;
		return ACCESSIBILITY_CANE_UNKNOWN;
	}
	/* Native horizontal sweep helpers are not vertical swept-cylinder queries.
	 * For a vertical adjustment, test the complete swept envelope directly. */
	if (fabsf(end->x - start->x) < 0.001f && fabsf(end->z - start->z) < 0.001f) {
		float delta = end->y - start->y;
		if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
		context->diagnostic->movequeries++;
		result = cdTestVolume(end, context->observer->radius, rooms,
				context->types, true, fmaxf(top, top - delta), fminf(bottom, bottom - delta));
		context->diagnostic->lastclearance = result ? ACCESSIBILITY_CANE_CLEAR : ACCESSIBILITY_CANE_BLOCKED;
		if (!result) context->diagnostic->blocker = (uintptr_t)cdGetObstacleProp();
		return context->diagnostic->lastclearance;
	}
	if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
	context->diagnostic->movequeries++;
	result = cdExamCylMove06(start, startrooms, end, rooms,
			context->observer->radius, context->types, true, top, bottom);
	if (result == CDRESULT_NOCOLLISION) {
		if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
		context->diagnostic->movequeries++;
		result = cdExamCylMove02(start, end, context->observer->radius,
				rooms, context->types, true, top, bottom);
	}
	if (result == CDRESULT_NOCOLLISION) {
		if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
		context->diagnostic->movequeries++;
		result = cdTestVolume(end, context->observer->radius, rooms,
				context->types, true, top, bottom);
	}
	if (result == CDRESULT_NOCOLLISION) {
		context->diagnostic->lastclearance = ACCESSIBILITY_CANE_CLEAR;
		return ACCESSIBILITY_CANE_CLEAR;
	}
	if (result == CDRESULT_COLLISION) {
		context->diagnostic->lastclearance = ACCESSIBILITY_CANE_BLOCKED;
		context->diagnostic->blocker = (uintptr_t)cdGetObstacleProp();
		return ACCESSIBILITY_CANE_BLOCKED;
	}
	context->diagnostic->errors++;
	return ACCESSIBILITY_CANE_UNKNOWN;
}

static enum accessibilitycaneevidence accessibilityCanePathMove(void *opaque,
		const struct accessibilitycanepathnode *from,
		const struct accessibilitycanepathnode *to, float height)
{
	struct accessibilitycanepathcontext *context = opaque;
	struct coord start = accessibilityCanePathPosition(context, from->distance, from->floor.ground);
	struct coord end = accessibilityCanePathPosition(context, to->distance, to->floor.ground);
	struct coord corner;
	RoomNum rooms[8];
	RoomNum cornerrooms[8];
	enum accessibilitycaneevidence result;
	memcpy(rooms, from->floor.rooms, sizeof(rooms));
	g_Vars.enableslopes = !(from->floor.flags & GEOFLAG_SLOPE);
	/* Rise before advancing; descend after advancing. Check both segments and
	 * their endpoints instead of calling the stateful player movement solver. */
	if (fabsf(end.y - start.y) > 0.01f) {
		corner = end.y > start.y ? start : end;
		corner.y = end.y > start.y ? end.y : start.y;
		result = accessibilityCanePathSegment(context, &start, rooms, &corner, height);
		if (result != ACCESSIBILITY_CANE_CLEAR) return result;
		if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
		accessibilityCanePathRooms(&start, rooms, &corner, cornerrooms);
		memcpy(rooms, cornerrooms, sizeof(rooms));
		start = corner;
	}
	return accessibilityCanePathSegment(context, &start, rooms, &end, height);
}

void accessibilityCaneQueryPath(const struct accessibilityobserver *observer,
		const struct coord *direction, f32 reach, f32 terrainthreshold,
		f32 dropthreshold, struct accessibilitycanepathdiagnostic *result)
{
	struct accessibilitycanepathcontext context = {0};
	struct accessibilitycanepathinput input = {0};
	struct accessibilitycanepathqueries queries = {0};
	s32 slopes = g_Vars.enableslopes;
	float scale;
	memset(result, 0, sizeof(*result));
	if (observer->isremote || observer->isvehicle || g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK) {
		result->path.stop = ACCESSIBILITY_CANE_PATH_UNSUPPORTED;
		return;
	}
	context.started = sysGetMicroseconds();
	context.observer = observer;
	context.direction = *direction;
	context.diagnostic = result;
	context.originheight = observer->origin.y - g_Vars.currentplayer->vv_manground;
	context.bottom = observer->ymin - g_Vars.currentplayer->vv_manground;
	context.types = g_Vars.bondcollisions
			? CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER : CDTYPE_BG;
	input.reach = reach;
	input.radius = observer->radius;
	input.height = observer->ymax - g_Vars.currentplayer->vv_manground;
	scale = g_Vars.currentplayer->vv_eyeheight / 159.0f;
	if (g_Vars.currentplayer->vv_eyeheight - 90 * scale < 69)
		scale = (g_Vars.currentplayer->vv_eyeheight - 69) / 90.0f;
	input.lowerheights[0] = fmaxf(80, g_Vars.currentplayer->vv_headheight - 45 * scale);
	input.lowerheights[1] = fmaxf(80, g_Vars.currentplayer->vv_headheight - 90 * scale);
	input.maxrise = context.bottom;
	input.terrainthreshold = terrainthreshold;
	input.dropthreshold = dropthreshold;
	queries.context = &context;
	queries.floor = accessibilityCanePathFloor;
	queries.move = accessibilityCanePathMove;
	accessibilityCaneTracePath(&input, &queries, &result->path);
	g_Vars.enableslopes = slopes;
	result->elapsedus = sysGetMicroseconds() - context.started;
}

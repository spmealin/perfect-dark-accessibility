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

static void accessibilityCaneCorridorAccumulate(
		struct accessibilitycanecorridordiagnostic *result,
		const struct accessibilitycanepathdiagnostic *diagnostic)
{
	result->queries = diagnostic->queries;
	result->floorqueries = diagnostic->floorqueries;
	result->movequeries = diagnostic->movequeries;
	result->retries = diagnostic->retries;
	result->errors = diagnostic->errors;
	result->budget = diagnostic->budget;
}

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
	if (result == CDRESULT_ERROR) {
		/* The 06 helper rejects an otherwise valid query when its independently
		 * derived destination-room lists do not intersect. Retry with the
		 * engine's portal-walk variant while preserving the swept cylinder. */
		if (!accessibilityCanePathReserve(context)) {
			return ACCESSIBILITY_CANE_UNKNOWN;
		}
		context->diagnostic->movequeries++;
		context->diagnostic->retries++;
		result = cdExamCylMove08(start, startrooms, end, rooms,
				context->observer->radius, context->types, true, top, bottom);
	}
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

static enum accessibilitycaneevidence accessibilityCanePathClearance(void *opaque,
		const struct accessibilitycanepathnode *at, float height)
{
	struct accessibilitycanepathcontext *context = opaque;
	struct coord pos = accessibilityCanePathPosition(context, at->distance,
			at->floor.ground);
	RoomNum rooms[8];
	float top = height - context->originheight;
	float bottom = context->bottom - context->originheight;
	s32 result;

	memcpy(rooms, at->floor.rooms, sizeof(rooms));
	if (!accessibilityCanePathReserve(context)) return ACCESSIBILITY_CANE_UNKNOWN;
	context->diagnostic->clearancequeries++;
	result = cdTestVolume(&pos, context->observer->radius, rooms,
			context->types, true, top, bottom);
	return result ? ACCESSIBILITY_CANE_CLEAR : ACCESSIBILITY_CANE_BLOCKED;
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
	if (observer->isremote || observer->isvehicle
			|| g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK
			|| g_Vars.currentplayer->grabbedprop) {
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
	queries.clearance = accessibilityCanePathClearance;
	accessibilityCaneTracePath(&input, &queries, &result->path);
	g_Vars.enableslopes = slopes;
	result->elapsedus = sysGetMicroseconds() - context.started;
}

static void accessibilityCaneCorridorCenterNode(
		const struct accessibilityobserver *observer,
		const struct coord *direction,
		const struct accessibilitycanepathresult *path, f32 distance,
		struct accessibilitycanepathnode *node, struct coord *position)
{
	s32 index = 0;
	f32 fraction = 0.0f;

	while (index + 1 < path->count
			&& path->nodes[index + 1].distance < distance) {
		index++;
	}

	*node = path->nodes[index];
	node->distance = 0.0f;
	if (index + 1 < path->count
			&& path->nodes[index + 1].distance > path->nodes[index].distance) {
		fraction = (distance - path->nodes[index].distance)
				/ (path->nodes[index + 1].distance
						- path->nodes[index].distance);
		if (fraction < 0.0f) fraction = 0.0f;
		if (fraction > 1.0f) fraction = 1.0f;
		node->floor.ground += (path->nodes[index + 1].floor.ground
				- node->floor.ground) * fraction;
		node->floor.pointground += (path->nodes[index + 1].floor.pointground
				- node->floor.pointground) * fraction;
	}

	position->x = observer->origin.x + direction->x * distance;
	position->y = node->floor.ground
			+ observer->origin.y - g_Vars.currentplayer->vv_manground;
	position->z = observer->origin.z + direction->z * distance;
}

static enum accessibilitycanecorridorsideevidence
accessibilityCaneCorridorTestDistance(
		struct accessibilitycanepathcontext *context,
		const struct accessibilitycanepathnode *center, f32 distance,
		struct accessibilitycanepathnode *destination,
		enum accessibilitycanecorridorsidekind *kind)
{
	enum accessibilitycaneevidence move;

	*kind = ACCESSIBILITY_CANE_CORRIDOR_KIND_UNKNOWN;
	memset(destination, 0, sizeof(*destination));
	destination->distance = distance;
	destination->height = center->height;
	if (!accessibilityCanePathFloor(context, center, distance,
			&destination->floor)) {
		return ACCESSIBILITY_CANE_CORRIDOR_SIDE_UNKNOWN;
	}
	if (!destination->floor.supported || !destination->floor.pointsupported) {
		*kind = ACCESSIBILITY_CANE_CORRIDOR_KIND_EDGE;
		return ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY;
	}
	if (fabsf(destination->floor.ground - center->floor.ground)
			> context->observer->radius) {
		*kind = ACCESSIBILITY_CANE_CORRIDOR_KIND_TERRAIN;
		return ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY;
	}
	move = accessibilityCanePathMove(context, center, destination,
			center->height);
	if (move == ACCESSIBILITY_CANE_CLEAR) {
		*kind = ACCESSIBILITY_CANE_CORRIDOR_KIND_NONE;
		return ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN;
	}
	if (move == ACCESSIBILITY_CANE_BLOCKED) {
		*kind = ACCESSIBILITY_CANE_CORRIDOR_KIND_BARRIER;
		return ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY;
	}
	return ACCESSIBILITY_CANE_CORRIDOR_SIDE_UNKNOWN;
}

static void accessibilityCaneCorridorProbeSide(
		struct accessibilitycanepathcontext *context,
		const struct accessibilitycanepathnode *center, f32 extent,
		s32 refinements,
		struct accessibilitycanecorridorside *side)
{
	struct accessibilitycanepathnode destination;
	enum accessibilitycanecorridorsideevidence evidence;
	enum accessibilitycanecorridorsidekind kind;
	enum accessibilitycanecorridorsidekind boundarykind;
	f32 safe = 0.0f;
	f32 unsafe = extent;
	s32 i;

	evidence = accessibilityCaneCorridorTestDistance(context, center, extent,
			&destination, &kind);
	if (evidence == ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN) {
		side->evidence = evidence;
		side->kind = kind;
		side->distance = extent;
		side->point = accessibilityCanePathPosition(context, extent,
				destination.floor.ground);
		return;
	}
	if (evidence == ACCESSIBILITY_CANE_CORRIDOR_SIDE_UNKNOWN) {
		side->evidence = evidence;
		side->kind = kind;
		return;
	}
	boundarykind = kind;

	for (i = 0; i < refinements; i++) {
		f32 middle = (safe + unsafe) * 0.5f;
		evidence = accessibilityCaneCorridorTestDistance(context, center,
				middle, &destination, &kind);
		if (evidence == ACCESSIBILITY_CANE_CORRIDOR_SIDE_UNKNOWN) {
			side->evidence = evidence;
			side->kind = kind;
			return;
		}
		if (evidence == ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN) {
			safe = middle;
		} else {
			unsafe = middle;
			boundarykind = kind;
		}
	}

	side->evidence = ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY;
	side->kind = boundarykind;
	side->distance = (safe + unsafe) * 0.5f;
	side->point = accessibilityCanePathPosition(context, side->distance,
			center->floor.ground);
}

void accessibilityCaneQueryCorridor(const struct accessibilityobserver *observer,
		const struct coord *direction,
		const struct accessibilitycanepathresult *path, f32 maxwidth,
		struct accessibilitycanecorridordiagnostic *result)
{
	static const f32 fractions[ACCESSIBILITY_CANE_CORRIDOR_STATIONS]
			= {0.0f, 0.5f, 1.0f, 1.0f};
	struct accessibilitycanepathdiagnostic querydiagnostic = {0};
	struct accessibilitycanepathcontext context = {0};
	struct accessibilityobserver stationobserver;
	f32 start;
	f32 end;
	f32 extent;
	f32 rightx;
	f32 rightz;
	u64 started;
	s32 slopes = g_Vars.enableslopes;
	s32 i;

	memset(result, 0, sizeof(*result));
	result->classification = ACCESSIBILITY_CANE_CORRIDOR_UNCERTAIN;
	result->maxwidth = maxwidth;
	if (!observer || !direction || !path || path->count < 2
			|| path->cue != ACCESSIBILITY_CANE_CUE_TERRAIN
			|| observer->isremote || observer->isvehicle
			|| g_Vars.currentplayer->bondmovemode != MOVEMODE_WALK
			|| g_Vars.currentplayer->grabbedprop
			|| !isfinite(maxwidth) || maxwidth <= observer->radius * 2.0f) {
		result->classification = ACCESSIBILITY_CANE_CORRIDOR_NOT_RUN;
		return;
	}

	started = sysGetMicroseconds();
	start = path->cuedistance;
	end = path->reached - observer->radius * 0.5f;
	if (end < start) end = path->reached;
	if (end < start) end = start;
	extent = maxwidth * 0.5f;
	rightx = -direction->z;
	rightz = direction->x;
	stationobserver = *observer;

	for (i = 0; i < ACCESSIBILITY_CANE_CORRIDOR_STATIONS; i++) {
		struct accessibilitycanepathnode center;
		struct accessibilitycanecorridorstation *station
				= &result->stations[i];
		f32 distance;
		s32 lefthard;
		s32 righthard;
		s32 leftcontinuation;
		s32 rightcontinuation;

		if (i < ACCESSIBILITY_CANE_CORRIDOR_CLASSIFICATION_STATIONS) {
			distance = start + (end - start) * fractions[i];
		} else {
			struct accessibilitycanepathcontext forwardcontext = {0};
			const struct accessibilitycanepathnode *last
					= &path->nodes[path->count - 1];

			/* One short lookahead beyond the verified elevation trace lets the
			 * runway expose a side opening on the landing. It is informational:
			 * only the three verified stations decide corridor classification. */
			if (path->stopdistance <= path->reached + 0.01f) break;
			distance = path->reached + observer->radius;
			if (distance > path->stopdistance) {
				distance = path->stopdistance;
			}
			if (distance <= end + 0.01f) break;

			forwardcontext.started = started;
			forwardcontext.observer = observer;
			forwardcontext.direction = *direction;
			forwardcontext.diagnostic = &querydiagnostic;
			forwardcontext.originheight = observer->origin.y
					- g_Vars.currentplayer->vv_manground;
			forwardcontext.bottom = observer->ymin
					- g_Vars.currentplayer->vv_manground;
			forwardcontext.types = g_Vars.bondcollisions
					? CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS
							| CDTYPE_PATHBLOCKER
					: CDTYPE_BG;
			center = *last;
			center.distance = distance;
			station->pathdistance = distance;
			station->posttransition = true;
			if (!accessibilityCanePathFloor(&forwardcontext, last, distance,
						&center.floor)
					|| !center.floor.supported || !center.floor.pointsupported
					|| fabsf(center.floor.ground - last->floor.ground)
							> observer->radius * 2.0f) {
				result->stationcount++;
				result->uncertainstations++;
				break;
			}
			station->center.x = observer->origin.x + direction->x * distance;
			station->center.y = center.floor.ground + observer->origin.y
					- g_Vars.currentplayer->vv_manground;
			station->center.z = observer->origin.z + direction->z * distance;
		}

		if (!station->posttransition) {
			station->pathdistance = distance;
			accessibilityCaneCorridorCenterNode(observer, direction, path,
					distance, &center, &station->center);
		}
		stationobserver.origin = station->center;
		stationobserver.ymin = center.floor.ground
				+ observer->ymin - g_Vars.currentplayer->vv_manground;
		stationobserver.ymax = center.floor.ground
				+ observer->ymax - g_Vars.currentplayer->vv_manground;

		memset(&context, 0, sizeof(context));
		context.started = started;
		context.observer = &stationobserver;
		context.diagnostic = &querydiagnostic;
		context.originheight = observer->origin.y
				- g_Vars.currentplayer->vv_manground;
		context.bottom = observer->ymin
				- g_Vars.currentplayer->vv_manground;
		context.types = g_Vars.bondcollisions
				? CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PATHBLOCKER
				: CDTYPE_BG;

		context.direction.x = -rightx;
		context.direction.y = 0.0f;
		context.direction.z = -rightz;
		accessibilityCaneCorridorProbeSide(&context, &center, extent,
				station->posttransition ? 1
						: ACCESSIBILITY_CANE_CORRIDOR_REFINEMENTS,
				&station->left);
		context.direction.x = rightx;
		context.direction.z = rightz;
		accessibilityCaneCorridorProbeSide(&context, &center, extent,
				station->posttransition ? 1
						: ACCESSIBILITY_CANE_CORRIDOR_REFINEMENTS,
				&station->right);

		result->stationcount++;
		if (station->posttransition) {
			/* This station enriches playback but cannot promote or demote the
			 * classification established by the verified path. */
		} else {
			lefthard = station->left.evidence
					== ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY
				&& (station->left.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_BARRIER
					|| station->left.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_EDGE);
			righthard = station->right.evidence
					== ACCESSIBILITY_CANE_CORRIDOR_SIDE_BOUNDARY
				&& (station->right.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_BARRIER
					|| station->right.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_EDGE);
			leftcontinuation = station->left.evidence
					== ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN
				|| station->left.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_TERRAIN;
			rightcontinuation = station->right.evidence
					== ACCESSIBILITY_CANE_CORRIDOR_SIDE_OPEN
				|| station->right.kind
						== ACCESSIBILITY_CANE_CORRIDOR_KIND_TERRAIN;

			if (leftcontinuation && rightcontinuation) {
				result->broadstations++;
			} else if (lefthard && righthard
					&& station->left.distance + station->right.distance
							<= maxwidth) {
				result->boundedstations++;
			} else {
				result->uncertainstations++;
			}

			/* Track each positively observed guide edge independently. A wall
			 * on one side and traversable continuation on the other is useful
			 * route evidence even when the passage is not a paired corridor. */
			if (lefthard && rightcontinuation) {
				result->leftguidedstations++;
			}
			if (righthard && leftcontinuation) {
				result->rightguidedstations++;
			}
		}

		if (querydiagnostic.budget) break;
	}

	accessibilityCaneCorridorAccumulate(result, &querydiagnostic);
	result->elapsedus = sysGetMicroseconds() - started;
	if (result->boundedstations >= 2) {
		result->classification = ACCESSIBILITY_CANE_CORRIDOR_BOUNDED;
	} else if (result->boundedstations >= 1
			|| result->leftguidedstations >= 1
			|| result->rightguidedstations >= 1) {
		result->classification = ACCESSIBILITY_CANE_CORRIDOR_GUIDED;
	} else if (result->broadstations >= 2) {
		result->classification = ACCESSIBILITY_CANE_CORRIDOR_BROAD;
	} else {
		result->classification = ACCESSIBILITY_CANE_CORRIDOR_UNCERTAIN;
	}
	g_Vars.enableslopes = slopes;
}

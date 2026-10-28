/* Synthetic query-trace regression, not an engine collision or listening test.
 * Exercise both the shared helper and real scanner dispatch/door ownership. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../src/accessibility/accessibility_visibility.c"
#include "../../src/accessibility/accessibility_beacon.c"

static struct model model;
static struct modelrodata_bbox box;
static struct doorobj object, sibling;
static struct prop target, other;
static struct coord camera;
static RoomNum rooms[2] = { 1, -1 };
static struct coord endpoints[11];
static s32 calls, successcall, bboxavailable, doorquery, perimchanges;

void propSetPerimEnabled(struct prop *prop, bool enabled)
{
	perimchanges++;
	if (enabled) prop->obj->hidden &= ~OBJHFLAG_PERIMDISABLED;
	else prop->obj->hidden |= OBJHFLAG_PERIMDISABLED;
}

bool cdTestLos05(struct coord *from, RoomNum *fromrooms,
		struct coord *to, RoomNum *torooms, s32 types, u16 flags)
{
	assert(from && fromrooms && to && torooms);
	assert(types == (CDTYPE_BG | CDTYPE_OBJS | CDTYPE_DOORS
			| CDTYPE_PATHBLOCKER | CDTYPE_AIOPAQUE));
	assert(flags == (GEOFLAG_WALL | GEOFLAG_BLOCK_SIGHT));
	assert(object.base.hidden & OBJHFLAG_PERIMDISABLED);
	if (doorquery) assert(sibling.base.hidden & OBJHFLAG_PERIMDISABLED);
	assert(calls < ARRAYCOUNT(endpoints));
	endpoints[calls] = *to;
	return calls++ == successcall;
}

struct modelrodata_bbox *objFindBboxRodata(struct defaultobj *obj)
{ return bboxavailable ? &box : NULL; }
f32 objGetLocalXMin(struct modelrodata_bbox *b) { return b->xmin; }
f32 objGetLocalXMax(struct modelrodata_bbox *b) { return b->xmax; }
f32 objGetLocalYMin(struct modelrodata_bbox *b) { return b->ymin; }
f32 objGetLocalYMax(struct modelrodata_bbox *b) { return b->ymax; }
f32 objGetLocalZMin(struct modelrodata_bbox *b) { return b->zmin; }
f32 objGetLocalZMax(struct modelrodata_bbox *b) { return b->zmax; }

static void reset(void)
{
	memset(&object, 0, sizeof(object));
	memset(&sibling, 0, sizeof(sibling));
	memset(&target, 0, sizeof(target));
	memset(&other, 0, sizeof(other));
	memset(endpoints, 0, sizeof(endpoints));
	target.type = PROPTYPE_OBJ;
	target.obj = &object.base;
	target.pos = (struct coord){ .x = 100, .y = 200, .z = 300 };
	target.rooms[0] = 1;
	target.rooms[1] = -1;
	object.base.prop = &target;
	object.base.model = &model;
	object.base.realrot[0][0] = 1;
	object.base.realrot[1][1] = 1;
	object.base.realrot[2][2] = 1;
	box = (struct modelrodata_bbox){ .xmin = -10, .xmax = 10,
		.ymin = -20, .ymax = 20, .zmin = -30, .zmax = 30 };
	camera = (struct coord){ .x = 100, .y = 200, .z = 100 };
	calls = perimchanges = doorquery = 0;
	successcall = -1;
	bboxavailable = 1;
}

/* 0: strict shared surface; 1: embedded shared surface; 2: F5 scanner;
 * 3: F6 door scanner. Callers keep category/viewport policy outside the helper. */
static s32 query(s32 route, s32 *sample, s32 *queries)
{
	if (route < 2) {
		return accessibilityVisibilityHasObjectSurfaceLineOfSight(
				&camera, rooms, &target, route == 1, sample, queries);
	} else {
		struct accessibilityobserver observer = {0};
		struct accessibilitybeaconresult result = {0};
		s32 clear;
		observer.camera = camera;
		observer.room = 1;
		result.kind = route == 2 ? ACCESSIBILITY_BEACON_KIND_OBJECT
				: ACCESSIBILITY_BEACON_KIND_DOOR;
		clear = accessibilityBeaconHasLineOfSight(&result, &observer, &target);
		*sample = result.lossample;
		*queries = result.losqueries;
		return clear;
	}
}

static void closePoint(struct coord actual, struct coord expected)
{
	assert(fabsf(actual.x - expected.x) < .001f);
	assert(fabsf(actual.y - expected.y) < .001f);
	assert(fabsf(actual.z - expected.z) < .001f);
}

static struct coord pulled(struct coord point, f32 amount)
{
	f32 dx = camera.x - point.x, dy = camera.y - point.y;
	f32 dz = camera.z - point.z;
	f32 length = sqrtf(dx * dx + dy * dy + dz * dz);
	if (length > amount) {
		point.x += amount * dx / length;
		point.y += amount * dy / length;
		point.z += amount * dz / length;
	}
	return point;
}

static void surfaceCases(void)
{
	s32 route, axis, sign, winner, sample, queries, i;
	const f32 spans[3] = { 10, 20, 30 };
	for (route = 0; route < 3; route++) {
		for (axis = 0; axis < 3; axis++) {
			for (sign = -1; sign <= 1; sign += 2) {
				for (winner = -1; winner <= 5; winner++) {
					reset();
					camera = target.pos;
					camera.f[axis] += sign * 200;
					successcall = winner;
					assert(query(route, &sample, &queries) == (winner >= 0));
					assert(queries == calls && calls == (winner < 0 ? 6 : winner + 1));
					assert(sample == (winner < 0 ? 0 : winner));
					closePoint(endpoints[0], target.pos);
					for (i = 1; i < calls; i++) {
						struct coord expected = target.pos;
						s32 corner = i - 1, a = (axis + 1) % 3, b = (axis + 2) % 3;
						expected.f[axis] += sign * spans[axis];
						if (corner) {
							expected.f[a] += spans[a] * .35f * ((corner & 1) ? 1 : -1);
							expected.f[b] += spans[b] * .35f * ((corner & 2) ? 1 : -1);
						}
						closePoint(endpoints[i], pulled(expected, .25f));
					}
					assert(!(object.base.hidden & OBJHFLAG_PERIMDISABLED));
					assert(perimchanges == calls * 2);
				}
			}
		}
	}
}

static void embeddedCases(void)
{
	s32 route, bits, winner, sample, queries, i;
	for (route = 0; route < 3; route++) {
		for (bits = 0; bits < 8; bits++) {
			for (winner = -1; winner <= 10; winner++) {
				s32 allowed = route != 0 && (bits & 1) && !(bits & 2);
				s32 expectedcalls, accepted;
				reset();
				target.flags = (bits & 1) ? PROPFLAG_ONTHISSCREENTHISTICK : 0;
				object.base.flags2 = (bits & 2) ? OBJFLAG2_INTERACTCHECKLOS : 0;
				object.base.flags = (bits & 4) ? OBJFLAG_MONITOR_RENDERPOSTBG : 0;
				successcall = winner;
				accepted = winner >= 0 && (winner <= 5 || allowed);
				expectedcalls = accepted ? winner + 1 : allowed ? 11 : 6;
				assert(query(route, &sample, &queries) == accepted);
				assert(queries == expectedcalls && calls == expectedcalls);
				assert(sample == (accepted ? winner : 0));
				for (i = 6; i < calls; i++) {
					closePoint(endpoints[i], pulled(endpoints[i - 5], (bits & 4) ? 24 : 8));
				}
				assert(!(object.base.hidden & OBJHFLAG_PERIMDISABLED));
			}
		}
	}
}

static void rotatedAndMissingCases(void)
{
	s32 route, sample, queries, missing;
	for (route = 0; route < 3; route++) {
		reset();
		/* Local X maps to +world Z at 4x scale, Y to +Y at 3x,
		 * and Z to -X at 2x. Nearest face is local +X. */
		memset(object.base.realrot, 0, sizeof(object.base.realrot));
		object.base.realrot[0][2] = 4;
		object.base.realrot[1][1] = 3;
		object.base.realrot[2][0] = -2;
		camera.z = 500;
		assert(!query(route, &sample, &queries));
		assert(queries == 6);
		closePoint(endpoints[1], (struct coord){ .x = 100, .y = 200, .z = 340.25f });
		closePoint(endpoints[2], pulled((struct coord){ .x = 121, .y = 221, .z = 340 }, .25f));
		for (missing = 0; missing < 3; missing++) {
			reset();
			if (missing == 0) object.base.model = NULL;
			if (missing == 1) object.base.prop = NULL;
			if (missing == 2) bboxavailable = 0;
			assert(!query(route, &sample, &queries));
			assert(calls == 1 && queries == 1 && sample == 0);
		}
		reset();
		object.base.hidden |= OBJHFLAG_PERIMDISABLED;
		assert(!query(route, &sample, &queries));
		assert(perimchanges == 0);
		assert(object.base.hidden & OBJHFLAG_PERIMDISABLED);
	}
	reset();
	assert(!accessibilityVisibilityHasObjectSurfaceLineOfSight(
			&camera, rooms, NULL, false, NULL, NULL));
	successcall = 0;
	assert(accessibilityVisibilityHasObjectSurfaceLineOfSight(
			&camera, rooms, &target, false, NULL, NULL));
}

static void doorCases(void)
{
	s32 bits, winner, sample, queries;
	for (bits = 0; bits < 4; bits++) {
		for (winner = -1; winner <= 5; winner++) {
			reset();
			doorquery = 1;
			target.type = PROPTYPE_DOOR;
			other.type = PROPTYPE_DOOR;
			other.obj = &sibling.base;
			sibling.base.prop = &other;
			object.sibling = &sibling;
			sibling.sibling = &object;
			object.base.hidden = (bits & 1) ? OBJHFLAG_PERIMDISABLED : 0;
			sibling.base.hidden = (bits & 2) ? OBJHFLAG_PERIMDISABLED : 0;
			target.flags = PROPFLAG_ONTHISSCREENTHISTICK;
			object.base.flags = OBJFLAG_MONITOR_RENDERPOSTBG;
			successcall = winner;
			assert(query(3, &sample, &queries) == (winner >= 0));
			assert(queries == calls && calls == (winner < 0 ? 6 : winner + 1));
			assert(sample == (winner < 0 ? 0 : winner));
			assert(!!(object.base.hidden & OBJHFLAG_PERIMDISABLED) == !!(bits & 1));
			assert(!!(sibling.base.hidden & OBJHFLAG_PERIMDISABLED) == !!(bits & 2));
			assert(perimchanges == 2 * ((!(bits & 1)) + (!(bits & 2))));
		}
	}
}

int main(void)
{
	surfaceCases();
	embeddedCases();
	rotatedAndMissingCases();
	doorCases();
	puts("Visibility: surface coordinates/order/budgets, embedded policy, transforms and door perimeter ownership passed.");
	return 0;
}

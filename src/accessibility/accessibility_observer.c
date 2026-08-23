#include <math.h>
#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "lib/vars.h"
#include "game/player.h"
#include "game/propobj.h"
#include "accessibility/accessibility_observer.h"

s32 accessibilityObserverGet(struct accessibilityobserver *observer)
{
	struct player *player = g_Vars.currentplayer;

	if (!observer) {
		return false;
	}

	memset(observer, 0, sizeof(*observer));

	if (!player || !player->prop) {
		return false;
	}

	if (player->cameramode == CAMERAMODE_EYESPY) {
		struct eyespy *eyespy = player->eyespy;
		struct chrdata *chr;
		f32 yminoffset;

		if (!eyespy || !eyespy->active || !eyespy->prop
				|| !eyespy->prop->active || !eyespy->prop->chr) {
			return false;
		}

		chr = eyespy->prop->chr;
		observer->prop = eyespy->prop;
		observer->origin = eyespy->prop->pos;
		observer->camera = player->cam_pos;
		observer->look = player->cam_look;
		observer->room = player->cam_room;
		observer->ground = eyespy->oldground;
		observer->radius = 26.0f;
		observer->ymax = observer->origin.y + 15.0f;
		yminoffset = eyespy->oldground <= chr->manground + 30.0f
				? chr->manground - observer->origin.y + 30.0f
				: eyespy->oldground - observer->origin.y;
		observer->ymin = observer->origin.y + yminoffset;
		observer->isremote = true;
	} else {
		observer->prop = player->prop;
		observer->origin = player->prop->pos;
		observer->camera = player->cam_pos;
		observer->look = player->cam_look;
		observer->room = player->cam_room;
		observer->ground = player->vv_ground;
		playerGetBbox(player->prop, &observer->radius,
				&observer->ymax, &observer->ymin);
	}

	return true;
}

s32 accessibilityObserverGetVehicle(struct accessibilityvehicle *vehicle)
{
	struct player *player = g_Vars.currentplayer;
	struct hoverbikeobj *bike;
	f32 angle;

	if (!vehicle) {
		return false;
	}

	memset(vehicle, 0, sizeof(*vehicle));
	vehicle->room = -1;
	vehicle->movementmode = player ? player->bondmovemode : -1;
	vehicle->vehiclemode = player ? player->bondvehiclemode : -1;

	if (!player || player->bondmovemode != MOVEMODE_BIKE
			|| !player->hoverbike || !player->hoverbike->active
			|| !player->hoverbike->obj
			|| player->hoverbike->obj->type != OBJTYPE_HOVERBIKE) {
		return false;
	}

	bike = (struct hoverbikeobj *)player->hoverbike->obj;
	vehicle->prop = player->hoverbike;
	vehicle->origin = player->hoverbike->pos;
	vehicle->room = player->hoverbike->rooms[0];
	vehicle->velocity.x = bike->speed[0];
	vehicle->velocity.y = 0.0f;
	vehicle->velocity.z = bike->speed[1];
	vehicle->speed = sqrtf(vehicle->velocity.x * vehicle->velocity.x
			+ vehicle->velocity.z * vehicle->velocity.z);
	vehicle->turnspeed = bike->w;
	angle = hoverpropGetTurnAngle(&bike->base);
	vehicle->heading.x = sinf(angle);
	vehicle->heading.y = 0.0f;
	vehicle->heading.z = cosf(angle);

	if (vehicle->speed > 0.01f) {
		vehicle->travel.x = vehicle->velocity.x / vehicle->speed;
		vehicle->travel.z = vehicle->velocity.z / vehicle->speed;
	} else {
		vehicle->travel = vehicle->heading;
	}

	objGetBbox(vehicle->prop, &vehicle->radius,
			&vehicle->ymax, &vehicle->ymin);

	return true;
}

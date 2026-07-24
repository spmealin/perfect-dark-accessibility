#include <string.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "lib/vars.h"
#include "game/player.h"
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

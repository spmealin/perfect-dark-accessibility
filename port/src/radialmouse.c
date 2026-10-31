#include <math.h>
#include <string.h>
#include "constants.h"
#include "radialmouse.h"

static struct {
	double x, y;
	int active;
	int anchored;
} g_RadialMouse[MAX_PLAYERS];

void radialMouseReset(int player)
{
	if (player >= 0 && player < MAX_PLAYERS) {
		memset(&g_RadialMouse[player], 0, sizeof(g_RadialMouse[player]));
	}
}

void radialMouseUpdate(int player, float dx, float dy, float speed)
{
	double x, y;
	if (player < 0 || player >= MAX_PLAYERS) return;
	/* Opening-frame motion preceded the anchor. Do not select from it. */
	if (!g_RadialMouse[player].anchored) {
		g_RadialMouse[player].anchored = 1;
		return;
	}
	if (!isfinite(dx) || !isfinite(dy) || !isfinite(speed) || speed <= 0) return;
	x = g_RadialMouse[player].x + (double)dx * speed;
	y = g_RadialMouse[player].y + (double)dy * speed;
	if (!isfinite(x) || !isfinite(y)) return;
	/* Never clip the displacement: reversing the physical movement must return
	 * to the same anchor even after moving beyond full virtual-stick travel. */
	g_RadialMouse[player].x = x;
	g_RadialMouse[player].y = y;
	g_RadialMouse[player].active |= dx != 0 || dy != 0;
}

int radialMouseGetStick(int player, int *x, int *y)
{
	double scale;
	*x = *y = 0;
	if (player < 0 || player >= MAX_PLAYERS) return 0;
	/* Scale only the output, preserving the direction at extreme displacement. */
	scale = fmax(fabs(g_RadialMouse[player].x), fabs(g_RadialMouse[player].y));
	scale = scale > 127.0 ? 127.0 / scale : 1.0;
	*x = (int)(g_RadialMouse[player].x * scale);
	*y = (int)(-g_RadialMouse[player].y * scale);
	return g_RadialMouse[player].active;
}

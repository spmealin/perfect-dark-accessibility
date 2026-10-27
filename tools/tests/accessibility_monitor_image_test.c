/* Native image-query parity regression. Whole-program optimization discards
 * unrelated engine code; no game assets, rendering or ROM are loaded. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/game/propobj.c"

int main(void)
{
	struct tvscreen screen = {0};
	struct tvscreen before;
	s32 image;
	for (image = 0; image <= 0x36; image++) {
		tvscreenSetImageByNum(&screen, image);
		assert(screen.offset == 0);
		screen.offset = 7; /* Advancing a program doesn't change its identity. */
		before = screen;
		assert(tvscreenIsShowingImage(&screen, image));
		assert(!memcmp(&before, &screen, sizeof(screen)));
	}
	tvscreenSetImageByNum(&screen, TVCMDLIST_12);
	assert(screen.cmdlist == g_TvCmdlist12);
	assert(!tvscreenIsShowingImage(&screen, TVCMDLIST_13));
	tvscreenSetImageByNum(&screen, TVCMDLIST_13);
	assert(screen.cmdlist == g_TvCmdlist13);
	tvscreenSetImageByNum(&screen, TVCMDLIST_14);
	assert(screen.cmdlist == g_TvCmdlist14);
	tvscreenSetImageByNum(&screen, -1);
	assert(screen.cmdlist == g_TvCmdlist00);
	assert(!tvscreenIsShowingImage(NULL, TVCMDLIST_12));
	puts("Native monitor image query: setter parity, puzzle programs and read-only state passed.");
	return 0;
}

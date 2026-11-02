/* The real Extended Audio checkbox handler, isolated from the game. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../port/src/optionsmenu.c"
static s32 setting = 1, available = 1;
s32 g_MusicDisableMpDeath;
int accessibilitySpatialGetEnabled(void) { return setting; }
int accessibilitySpatialAvailable(void) { return available; }
void accessibilitySpatialSetEnabled(int value) { setting = value != 0; }
int main(void)
{
	union handlerdata data = {0};
	assert(g_ExtendedAudioMenuItems[0].type == MENUITEMTYPE_CHECKBOX);
	assert(!strcmp((const char *)g_ExtendedAudioMenuItems[0].param2, "3D Accessibility Audio"));
	assert(g_ExtendedAudioMenuItems[0].handler == menuhandlerBinauralAudio);
	assert(menuhandlerBinauralAudio(MENUOP_GET, NULL, &data) == 1);
	assert(menuhandlerBinauralAudio(MENUOP_CHECKDISABLED, NULL, &data) == 0);
	data.checkbox.value = 0;
	menuhandlerBinauralAudio(MENUOP_SET, NULL, &data);
	assert(!menuhandlerBinauralAudio(MENUOP_GET, NULL, &data));
	data.checkbox.value = 1;
	menuhandlerBinauralAudio(MENUOP_SET, NULL, &data);
	assert(menuhandlerBinauralAudio(MENUOP_GET, NULL, &data));
	available = 0;
	assert(menuhandlerBinauralAudio(MENUOP_CHECKDISABLED, NULL, &data));
	puts("Extended Audio: 3D checkbox default-on, live get/set and unavailable state passed.");
	return 0;
}

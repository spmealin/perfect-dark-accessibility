#define main glossaryFixtureMain
#include "accessibility_glossary_test.c"
#undef main
#include <float.h>

int main(void)
{
	struct accessibilitymixstate state = {0}, partitioned = {0};
	struct accessibilitymixsettings settings = {1, 1, 1, 0};
	s16 l, r, l2, r2, game[512];
	double previous;
	assert(glossaryFixtureMain() == 0); /* Also retains the legacy PCM fixture. */
	accessibilityMixConfigure(&state, &settings, 22020);
	accessibilityMixFrame(&state, .1f, .05f, .05f, .025f, .025f, .0125f, &l, &r);
	assert(abs(l - (s32)lrintf(.175f * 32768)) <= 1);
	assert(abs(l - 2*r) <= 1 && state.limitergain == 1);
	for (s32 i = 0; i < 10000; i++) {
		accessibilityMixFrame(&state, .5f, .25f, .9f, .45f, .5f, .25f, &l, &r);
		assert(abs(l) <= 32113 && abs(r) <= 32113);
		assert(abs(l - 2*r) <= 1);
	}
	assert(state.limitedframes > 0 && state.minimumgain < .6f);
	previous = state.limitergain;
	for (s32 i = 0; i < 44100; i++) {
		accessibilityMixFrame(&state, .01f, .005f, 0, 0, 0, 0, &l, &r);
		assert(state.limitergain >= previous);
		previous = state.limitergain;
	}
	assert(!accessibilityMixNeedsProcessing(&state));
	assert(state.limitergain == 1 && state.gamegain == 1);
	settings.headroomdb = 3;
	accessibilityMixConfigure(&state, &settings, 22020);
	for (s32 i = 0; i < 300; i++)
		accessibilityMixFrame(&state, .5f, .5f, .001f, .001f, 0, 0, &l, &r);
	assert(fabsf(state.gamegain - powf(10, -3.0f/20)) < .00001f);
	for (s32 i = 0; i < 44100; i++)
		accessibilityMixFrame(&state, .5f, .5f, 0, 0, 0, 0, &l, &r);
	assert(!accessibilityMixNeedsProcessing(&state));
	assert(l == 16384 && r == 16384);
	accessibilityMixConfigure(&partitioned, &settings, 22020);
	memset(&state, 0, sizeof(state));
	accessibilityMixConfigure(&state, &settings, 22020);
	for (s32 i = 0; i < 44100; i++) {
		float w = sinf(i * .12f) * 1.4f;
		if (i % 37 == 0) accessibilityMixConfigure(&partitioned, &settings, 22020);
		accessibilityMixFrame(&state, .25f, -.25f, w, w*.3f, 0, 0, &l, &r);
		accessibilityMixFrame(&partitioned, .25f, -.25f, w, w*.3f, 0, 0, &l2, &r2);
		assert(l == l2 && r == r2);
	}
	settings.worldgain = 0;
	settings.headroomdb = 0;
	accessibilityMixConfigure(&state, &settings, 22020);
	accessibilityMixReset(&state);
	accessibilityMixFrame(&state, 0, 0, 100, 100, .25f, .25f, &l, &r);
	assert(l == 8192 && r == 8192); /* World mute never mutes feedback. */
	accessibilityMixFrame(&state, NAN, INFINITY, FLT_MAX, FLT_MAX, INFINITY, 0, &l, &r);
	assert(state.nonfiniteinputs >= 3 && isfinite(state.limitergain));
	settings.worldgain = settings.feedbackgain = NAN;
	settings.headroomdb = NAN;
	accessibilityMixConfigure(&state, &settings, NAN);
	assert(state.settings.worldgain == 1 && state.settings.headroomdb == 3);

	/* Actual production mixer, crowded cues over loud original game audio. */
	testmixenabled = 1;
	simulateWorldSuppression();
	for (s32 i = 0; i < 512; i++) game[i] = 24000;
	for (s32 slot = 0; slot < 10; slot++)
		accessibilityToneSetCombatSlot(slot, 1, 900, 900, 1, 0, 340, 180, 0, 1, 1, 1);
	for (s32 slot = 0; slot < 4; slot++) accessibilityToneSetMarkerSlot(slot, 1, 1, 0, 1);
	accessibilityToneSetAlignment(1, 660, 0);
	for (s32 block = 0; block < 100; block++) {
		const s16 *pcm = accessibilityToneMix(game, sizeof(game));
		for (s32 i = 0; i < 512; i++) assert(abs(pcm[i]) <= 32113);
	}
	assert(g_AccessibilityMixState.limitedframes > 0);
	assert(g_AccessibilityMixState.peak > 1 && g_AccessibilityMixState.outputpeak <= .98001f);
	simulateWorldSuppression();
	for (s32 i = 0; i < 200; i++) accessibilityToneMix(game, sizeof(game));
	assert(accessibilityToneMix(game, sizeof(game)) == game);
	testmixenabled = 0;
	assert(accessibilityToneMix(game, sizeof(game)) == game);
	{
		u64 started = SDL_GetPerformanceCounter();
		for (s32 i = 0; i < 22020; i++)
			accessibilityMixFrame(&state, .7f, .35f, .8f, .4f, .2f, .1f, &l, &r);
		printf("Mix protection, one second of stereo audio: %.3f ms wall; no buffering or allocation.\n",
				(SDL_GetPerformanceCounter() - started) * 1000.0 / SDL_GetPerformanceFrequency());
	}
	puts("Accessibility mix: headroom, linked overload limiting, release, finite guards, partition invariance, buses, crowded mixer and exact bypass passed.");
	return 0;
}

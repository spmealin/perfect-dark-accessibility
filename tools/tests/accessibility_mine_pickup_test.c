#define main glossaryFixtureMain
#include "accessibility_glossary_test.c"
#undef main

static void capture(s32 mine, s16 *output)
{
	s16 silence[512] = {0};
	simulateWorldSuppression();
	for (s32 i = 0; i < 100; i++) accessibilityToneMix(silence, sizeof(silence));
	if (mine) accessibilityTonePlayMinePickupChirp(880, 1, 0, NULL);
	else accessibilityTonePlayPickupChirp(880, 1, 0, NULL);
	for (s32 i = 0; i < 4096; i++) {
		const s16 *pcm = accessibilityToneMix(silence, sizeof(s16) * 2);
		output[i] = pcm[0];
		assert(pcm[0] == pcm[1]);
	}
}

int main(void)
{
	s16 ordinary[4096], mine[4096], silence[512] = {0};
	s32 total = ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES * 3 + ACCESSIBILITY_CHIRP_PATTERN_GAP_SAMPLES * 2;
	f32 phase = 0;
	assert(glossaryFixtureMain() == 0);
	capture(0, ordinary);
	capture(1, mine);
	for (s32 i = 0; i < total; i++) {
		f32 envelope = fminf(1, fminf((f32)i / ACCESSIBILITY_MINE_DRONE_ATTACK_SAMPLES,
				(f32)(total - i) / ACCESSIBILITY_MINE_DRONE_RELEASE_SAMPLES));
		f32 expected = sinf(phase) * envelope * ACCESSIBILITY_CHIRP_VOLUME * ACCESSIBILITY_MINE_DRONE_GAIN * 32767;
		assert(fabsf((mine[i] - ordinary[i]) - expected) < 2.1f);
		phase += TWO_PI * ACCESSIBILITY_MINE_DRONE_FREQUENCY_HZ / ACCESSIBILITY_TONE_SAMPLE_RATE;
		if (phase >= TWO_PI) phase -= TWO_PI;
	}
	for (s32 i = total; i < 4096; i++) assert(ordinary[i] == 0 && mine[i] == 0);
	assert(ordinary[900] == 0 && mine[900] != 0); /* Drone spans the chirp gap. */
	accessibilityTonePlayMinePickupChirp(880, 1, 0, NULL);
	accessibilityToneMix(silence, sizeof(silence));
	accessibilityToneStopChirp();
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	accessibilityTonePreviewBegin();
	accessibilityTonePreviewCommit(500);
	accessibilityTonePlayMinePickupChirp(880, 1, 0, NULL);
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	accessibilityTonePreviewStop();
	puts("Mine pickup: unchanged chirps plus 220 Hz layer, continuous gap tone, bounded tail, stop and ownership passed.");
	return 0;
}

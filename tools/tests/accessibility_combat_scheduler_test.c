/* Production mixer scheduling regression; no ROM, SDK, game or personal logs. */
#define main glossaryFixtureMain
#include "accessibility_glossary_test.c"
#undef main

static void stop(void)
{
	s16 silence[2] = {0};
	simulateWorldSuppression();
	for (s32 i = 0; i < 300; i++) accessibilityToneMix(silence, sizeof(silence));
}

static void request(s32 slot, s32 period, s32 continuous)
{
	accessibilityToneSetCombatSlot(slot, 1, 900, 900, .3f, 0,
			period, period / 2, 0, continuous, 1, 1);
}

static void run(s32 count, s32 period, s32 samples, s32 churn)
{
	s16 silence[2] = {0};
	s32 starts[10] = {0}, previous[10];
	s32 last = -100000, total = 0;
	s32 gap = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * .050f);
	s32 periodframes = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * period / 1000);
	if (periodframes / count < gap) gap = periodframes / count;
	stop();
	for (s32 slot = 0; slot < count; slot++) request(slot, period, 0);
	for (s32 sample = 0; sample < samples; sample++) {
		if (churn && sample % 199 == 0)
			for (s32 slot = 0; slot < count; slot++) request(slot, period, 0);
		for (s32 slot = 0; slot < count; slot++) previous[slot] = g_AccessibilityCombatCycleSample[slot];
		accessibilityToneMix(silence, sizeof(silence));
		for (s32 slot = 0; slot < count; slot++) {
			if (g_AccessibilityCombatCycleSample[slot] == 1
					&& previous[slot] != 0) {
				assert(sample - last >= gap);
				last = sample;
				starts[slot]++;
				total++;
			}
		}
	}
	for (s32 slot = 0; slot < count; slot++) assert(starts[slot] >= 2);
	assert(total >= count * 2);
}

int main(void)
{
	s16 silence[2] = {0};
	assert(glossaryFixtureMain() == 0);
	run(1, 340, 22020, 0);
	run(3, 340, 22020, 0);
	run(10, 340, 22020, 0);
	run(10, 80, 22020, 0);
	run(3, 340, 22020, 1); /* Simultaneous restarts cannot re-synchronize. */
	stop();
	request(0, 340, 1); /* Continuous proximity tone bypasses the scheduler. */
	for (s32 i = 0; i < 300; i++) accessibilityToneMix(silence, sizeof(silence));
	assert(g_AccessibilityCombatGain[0] > 0);
	assert(g_AccessibilityCombatLastStarted == -1);
	stop();
	request(7, 340, 0);
	accessibilityToneMix(silence, sizeof(silence));
	assert(g_AccessibilityCombatLastStarted == 7);
	assert(g_AccessibilityCombatCycleSample[7] == 1);
	stop();
	puts("Combat scheduler: separated onsets, dense fairness, restarts, continuous bypass and stop/resume passed.");
	return 0;
}

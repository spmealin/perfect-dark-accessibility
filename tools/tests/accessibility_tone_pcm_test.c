/* Exact PCM baseline recorded before the ownership/mixer refactor.
 * Reuse the existing deterministic getter/menu stubs and cleanup checks. */
#define main glossaryFixtureMain
#include "accessibility_glossary_test.c"
#undef main
#include <stdint.h>

static void conflictingWorldCommands(void)
{
	accessibilityToneSet(1, 1234);
	accessibilityToneSetAlignment(1, 777, ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED);
	accessibilityTonePlayChirp(555, .2f, -.5f);
	accessibilityTonePlayChirpPattern(888, .2f, .5f, 3, 1);
	accessibilityTonePlayMovableLandmark(.2f, .2f);
	accessibilityTonePlayTargetPresence(900, .2f, -.2f);
	accessibilityTonePlayThreatAlert(.2f, .3f);
	accessibilityTonePlayToggleConfirmation(1);
	accessibilityTonePlayCaneModeConfirmation(2);
	accessibilityTonePlayCompass(4);
	accessibilityTonePlayStanceConfirmation(2);
	accessibilityTonePlayWeaponFunction(1);
	accessibilityToneSetHazard(1, 220, .2f, .4f);
	accessibilityToneSetCaneEdge(1, .2f, -.4f);
	accessibilityToneSetCombatSlot(0, 1, 900, 1200, .2f, .3f, 200, 50, 1, 0, 1, 1);
	accessibilityToneSetProjectileSlot(0, 1, 999, .2f, .4f, 150, 30, 1);
	accessibilityToneSetTrackerSlot(0, 1, 1111, .2f, -.4f, 200, 1, 1, 1);
	accessibilityToneSetFriendlySlot(0, 1, .2f, .5f, 1, 1);
	accessibilityToneSetDoorSlot(0, 1, .2f, -.5f, 1);
	accessibilityTonePlayRadarPing(1000, .2f, .3f, 1, 1, ACCESSIBILITY_TONE_RADAR_ENEMY);
	accessibilityToneSetHillBeacon(1, .2f, .5f, 1, 1, 1, 1);
	accessibilityTonePlayCaneSlot(0, 300, 600, .2f, .4f, 200, 0, 0, 0, NULL, NULL, NULL);
	accessibilityToneSetMarkerSlot(0, 1, .2f, .5f, 1);
	accessibilityTonePlayMarkerRemoval(0);
	accessibilityToneSetLandmarkSlot(0, 1, .2f, .4f, 1, 1);
	simulateWorldSuppression();
}

int main(int argc, char **argv)
{
	const s32 chunks[] = { 1, 17, 128, 512 };
	s16 gameaudio[1024];
	uint64_t hash = UINT64_C(14695981039346656037);
	s32 entry, block, i;
	assert(glossaryFixtureMain() == 0);
	testenabled = 1;
	for (i = 0; i < ARRAYCOUNT(gameaudio); i++) gameaudio[i] = (i % 7 - 3) * 7000;
	for (entry = 0; entry < accessibilityGlossaryGetCount(); entry++) {
		u64 frames = 0, started = testclock;
		accessibilityGlossaryPlay(entry);
		for (block = 0; frames < (u64)(g_Glossary[entry].durationms + 300) * 22020 / 1000; block++) {
			s32 samples = chunks[block % ARRAYCOUNT(chunks)] * 2;
			const s16 *pcm;
			if (accessibilityToneOwnershipIsPreview()) conflictingWorldCommands();
			pcm = accessibilityToneMix(gameaudio, samples * sizeof(s16));
			for (i = 0; i < samples; i++) {
				u16 value = (u16)pcm[i];
				hash = (hash ^ (value & 255)) * UINT64_C(1099511628211);
				hash = (hash ^ (value >> 8)) * UINT64_C(1099511628211);
			}
			frames += samples / 2;
			testclock = started + frames * UINT64_C(1000000) / 22020;
			accessibilityGlossaryTick();
		}
		accessibilityGlossaryStop();
	}
	printf("Tone PCM fingerprint: %016llx\n", (unsigned long long)hash);
	if (argc == 2 && !strcmp(argv[1], "--print-golden")) return 0;
	/* Filled from the pre-refactor backend, not from the implementation under test. */
	assert(hash == UINT64_C(0xd9543198abbfbb3b));
	/* Main-thread ownership transitions and idempotent cleanup. */
	assert(!accessibilityToneOwnershipIsPreview());
	assert(accessibilityToneOwnershipCanPublish());
	accessibilityTonePreviewCommands(1);
	accessibilityTonePreviewCommit(100);
	assert(!accessibilityToneOwnershipIsPreview());
	accessibilityTonePreviewBegin();
	assert(accessibilityToneOwnershipCanCommit());
	accessibilityTonePreviewCommit(100);
	assert(!accessibilityToneOwnershipCanPublish());
	assert(!accessibilityToneOwnershipCanCommit());
	accessibilityTonePreviewCommands(1);
	assert(accessibilityToneOwnershipCanPublish());
	accessibilityTonePreviewCommands(0);
	assert(!accessibilityToneOwnershipCanPublish());
	accessibilityTonePreviewBegin(); /* Replay acquires a fresh command transaction. */
	assert(accessibilityToneOwnershipCanCommit());
	accessibilityTonePreviewCommit(1);
	accessibilityTonePreviewStop();
	accessibilityTonePreviewStop();
	assert(accessibilityToneOwnershipCanPublish());
	assert(!accessibilityToneOwnershipIsPreview());
	accessibilityToneSetAlignment(1, 660, 0);
	assert(SDL_AtomicGet(&g_AccessibilityToneEnabled));
	assert(memcmp(accessibilityToneMix(gameaudio, sizeof(gameaudio)),
			gameaudio, sizeof(gameaudio)) != 0);
	accessibilityToneSetAlignment(0, 0, 0);
	assert(!SDL_AtomicGet(&g_AccessibilityToneEnabled));
	assert(accessibilityToneMix(NULL, 0) == NULL);
	assert(accessibilityToneMix(gameaudio, 3) == gameaudio);
	assert(accessibilityToneMix(gameaudio, sizeof(gameaudio) * 3) == gameaudio);
	/* Range layering is isolated from ordinary locks and the golden glossary.
	 * Verify actual mixer gains, independent phases, summed PCM and cleanup. */
	memset(gameaudio, 0, sizeof(gameaudio));
	accessibilityToneSetAlignment(1, 1320, ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_RANGE);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(fabsf(g_AccessibilityToneGain - .15f * .8f) < .00001f);
	assert(g_AccessibilityBullseyeGain == 0);
	accessibilityToneSetAlignment(1, 1320,
			ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_RANGE
			| ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_BULLSEYE);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(fabsf(g_AccessibilityToneGain - .12f) < .00001f);
	assert(fabsf(g_AccessibilityBullseyeGain - .15f) < .00001f);
	assert(fabsf(g_AccessibilityToneFrequencyHz - 1320) < .01f);
	g_AccessibilityTonePhase = .5f;
	g_AccessibilityBullseyePhase = 1.0f;
	{
		s32 expected = (s32)(sinf(.5f) * .12f * 32767.0f)
				+ (s32)(sinf(1.0f) * .15f * 32767.0f);
		const s16 *pcm = accessibilityToneMix(gameaudio, 2 * sizeof(s16));
		assert(abs(pcm[0] - expected) <= 1);
		assert(pcm[0] == pcm[1]);
	}
	accessibilityToneSetAlignment(1, 1200, ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_RANGE);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(g_AccessibilityBullseyeGain == 0);
	assert(fabsf(g_AccessibilityToneGain - .12f) < .00001f);
	accessibilityToneSetAlignment(1, 660, 0);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(fabsf(g_AccessibilityToneGain - .15f) < .00001f);
	accessibilityToneSetAlignment(1, 1320,
			ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_RANGE
			| ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_BULLSEYE);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	accessibilityToneSetAlignment(0, 0, 0);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(g_AccessibilityToneGain == 0 && g_AccessibilityBullseyeGain == 0);
	assert(accessibilityToneMix(gameaudio, sizeof(gameaudio)) == gameaudio);
	accessibilityToneSetAlignment(1, 1320,
			ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_RANGE
			| ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_BULLSEYE);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	accessibilityTonePreviewBegin();
	accessibilityTonePreviewCommit(100);
	accessibilityToneMix(gameaudio, sizeof(gameaudio));
	assert(g_AccessibilityBullseyeGain == 0);
	accessibilityTonePreviewStop();
	return 0;
}

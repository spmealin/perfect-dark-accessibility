/* Actual HRTF + production cue mixer; no game or personal log is opened. */
#define ACCESSIBILITY_SPATIAL_CAPTURE_TEST 1
#define main glossaryFixtureMain
#include "accessibility_glossary_test.c"
#undef main
#include "../../port/src/accessibility/accessibility_spatial.c"

void configRegisterInt(const char *name, s32 *value, s32 minimum, s32 maximum) { }
static struct coord observerlook = { 0, 0, 1 };
s32 accessibilityObserverGet(struct accessibilityobserver *observer)
{
	memset(observer, 0, sizeof(*observer));
	observer->look = observerlook;
	return 1;
}

static void renderEnergy(double *left, double *right)
{
	s16 silence[512] = {0};
	int block, i;
	*left = *right = 0;
	for (block = 0; block < 100; block++) {
		const s16 *pcm = accessibilityToneMix(silence, sizeof(silence));
		for (i = 0; i < 512; i += 2) {
			*left += (double)pcm[i] * pcm[i];
			*right += (double)pcm[i+1] * pcm[i+1];
		}
	}
}

int main(void)
{
	struct coord position = { -100, 0, 100 }; /* Right in native +Z-forward world. */
	double left, right;
	s16 silence[512] = {0};
	assert(glossaryFixtureMain() == 0);
	accessibilitySpatialInit();
	assert(accessibilitySpatialEnabled());
	/* Capture snapshots retain world positions and publication-time listener
	 * state without changing DSP direction, command state, or allocations. */
	{
		struct coord path[2] = {{100, -20, 100}, {100, -80, 300}};
		f32 direction[3], after[3];
		unsigned long long before = allocations;
		testclock = 1000000;
		accessibilityToneSetSpatialPath(ACCESSIBILITY_SPATIAL_CANE, 2, path);
		assert(spatialdiagnostics[ACCESSIBILITY_SPATIAL_CANE].count == 2);
		assert(spatialdiagnostics[ACCESSIBILITY_SPATIAL_CANE].points[1].y == -80);
		assert(spatialdiagnostics[ACCESSIBILITY_SPATIAL_CANE].publishedus == testclock);
		assert(accessibilitySpatialGetDirection(ACCESSIBILITY_SPATIAL_CANE, direction));
		assert(direction[0] < 0 && direction[1] < 0 && direction[2] < 0);
		testclock += 500000;
		observerlook = (struct coord){1, 0, 0};
		accessibilityToneDumpSpatialDiagnostics(1);
		assert(spatialcapturerecordcount == 1 + ACCESSIBILITY_SPATIAL_VOICES + 2);
		assert(strstr(spatialcapturerecords[0], "coordinate_frame=right_up_back"));
		assert(strstr(spatialcapturerecords[1 + ACCESSIBILITY_SPATIAL_CANE], "age_ms=500.000 points=2"));
		assert(strstr(spatialcapturerecords[2 + ACCESSIBILITY_SPATIAL_CANE], "point=0 world=100.000,-20.000,100.000"));
		assert(strstr(spatialcapturerecords[2 + ACCESSIBILITY_SPATIAL_CANE], "current_relative=100.000,-20.000,-100.000"));
		assert(strstr(spatialcapturerecords[3 + ACCESSIBILITY_SPATIAL_CANE], "point=1 world=100.000,-80.000,300.000"));
		assert(accessibilitySpatialGetDirection(ACCESSIBILITY_SPATIAL_CANE, after));
		assert(memcmp(direction, after, sizeof(direction)) == 0);
		observerlook = (struct coord){0, 0, 1};
		assert(before == allocations);
		assert(!accessibilitySpatialGetDirection(-1, after));
		assert(!accessibilitySpatialGetDirection(ACCESSIBILITY_SPATIAL_VOICES, after));
		accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_CANE, NULL);
		assert(spatialdiagnostics[ACCESSIBILITY_SPATIAL_CANE].count == 0);
		assert(!SDL_AtomicGet(&g_AccessibilitySpatialValid[ACCESSIBILITY_SPATIAL_CANE]));
	}
	/* Pitch rotates the source basis, not just the listener's world position. */
	observerlook = (struct coord){ 0, .70710678f, .70710678f };
	position = (struct coord){ 0, 100, 100 };
	accessibilityToneSetSpatialPosition(0, &position);
	assert(abs(SDL_AtomicGet(&voices[0].direction[1])) < 2);
	assert(SDL_AtomicGet(&voices[0].direction[2]) < -999000);
	position.y = 0;
	accessibilityToneSetSpatialPosition(0, &position);
	assert(SDL_AtomicGet(&voices[0].direction[1]) < -700000);
	observerlook = (struct coord){ 0, 0, 1 };
	position = (struct coord){ -100, 0, 100 };
	accessibilityToneSetSpatialPosition(0, &position);
	accessibilityToneSetCombatSlot(0, 1, 900, 900, .3f, 0, 340, 180, 0, 0, 1, 1);
	renderEnergy(&left, &right);
	assert(right > left && left > 0);
	printf("Enemy mixer: left=%g right=%g\n", left, right);
	accessibilityToneStopCombat();
	accessibilityToneMix(silence, sizeof(silence));
	position.x = 100;
	accessibilityTonePlayPickupChirp(880, .5f, 0, &position);
	renderEnergy(&left, &right);
	assert(left > right && right > 0);
	printf("Pickup mixer: left=%g right=%g\n", left, right);
	accessibilityToneStopChirp();
	accessibilityToneMix(silence, sizeof(silence));
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	accessibilityTonePlayPickupChirp(880, .5f, 0, &position);
	accessibilityToneMix(silence, sizeof(silence));
	accessibilityTonePreviewBegin();
	accessibilityTonePreviewCommit(100);
	assert(!nonzero(accessibilityToneMix(silence, sizeof(silence)), 512));
	accessibilityTonePreviewStop();
	/* All scanner/cane families use independent mono buses. */
	position = (struct coord){ 100, 0, 100 };
	accessibilityTonePlayObjectChirp(880, .5f, 0, 1, 1, &position);
	renderEnergy(&left, &right); assert(left > right && right > 0);
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_FRIENDLY, &position);
	accessibilityToneSetFriendlySlot(0, 1, .3f, 0, 1, 1);
	renderEnergy(&left, &right); assert(left > right && right > 0);
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	position.x = -100;
	accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_DOOR, &position);
	accessibilityToneSetDoorSlot(0, 1, .3f, 0, 1);
	renderEnergy(&left, &right); assert(right > left && left > 0);
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_CANE, &position);
	accessibilityTonePlayCaneSlot(0, 300, 600, .3f, 0, 200, 0, 0, 0, NULL, NULL, NULL);
	renderEnergy(&left, &right); assert(right > left && left > 0);
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	accessibilityToneSetSpatialPosition(ACCESSIBILITY_SPATIAL_EDGE, &position);
	accessibilityToneSetCaneEdge(1, .3f, 0);
	renderEnergy(&left, &right); assert(right > left && left > 0);
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	{
		struct coord path[2] = {{100, 0, 100}, {-100, 0, 100}};
		f32 frequencies[2] = {600, 800}, volumes[2] = {.3f, .3f}, pans[2] = {0, 0};
		accessibilityToneSetSpatialPath(ACCESSIBILITY_SPATIAL_CANE, 2, path);
		accessibilityToneSpatialPathDirection(0, 0, 1, .5f);
		assert(abs(SDL_AtomicGet(&voices[ACCESSIBILITY_SPATIAL_CANE].direction[0])) < 2);
		accessibilityTonePlayCaneSlot(0, 600, 800, .3f, 0, 300,
				ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY, 2, 2, frequencies, volumes, pans);
		renderEnergy(&left, &right); assert(left > 0 && right > 0);
		assert(SDL_AtomicGet(&voices[ACCESSIBILITY_SPATIAL_CANE].direction[0]) > 0);
	}
	simulateWorldSuppression(); accessibilityToneMix(silence, sizeof(silence));
	{
		unsigned long long before = allocations;
		accessibilitySpatialSetEnabled(0);
		assert(accessibilitySpatialAvailable() && !accessibilitySpatialGetEnabled());
		accessibilityTonePlayObjectChirp(880, .5f, 0, 1, 1, &position);
		renderEnergy(&left, &right); assert(left == right && left > 0);
		accessibilitySpatialSetEnabled(1);
		assert(accessibilitySpatialGetEnabled());
		accessibilityTonePlayObjectChirp(880, .5f, 0, 1, 1, &position);
		renderEnergy(&left, &right); assert(right > left);
		assert(allocations == before);
	}
	accessibilitySpatialShutdown();
	assert(allocations == frees);
	puts("Spatial mixer: enemy, pickup, object, person, door, cane, edge, moving path, live toggle, stop and preview checks passed.");
	return 0;
}

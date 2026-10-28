/* Value-only precision-audio regression; not a hit-test or listening test.
 * Include the owning core to exercise private policy without exporting it.
 * Build in MinGW64, after configuration:
 * cc @build/CMakeFiles/pd.dir/includes_C.rsp -DPLATFORM_64BIT=1 -DVERSION=2
 *   -DPAL=0 -D_LANGUAGE_C=1 -DACCESSIBILITY_PERFORMANCE_DIAGNOSTICS=0
 *   -std=c11 -O2 -fwhole-program tools/tests/accessibility_precision_mapping_test.c
 *   -o build/accessibility_precision_mapping_test.exe -lm
 */
#include <assert.h>
#include <stdio.h>
#include "../../src/accessibility/accessibility_targeting.c"

static f32 capturedfrequency;
static s32 capturedflags;
static s32 capturedcalls;

void accessibilityToneSetAlignment(s32 enabled, f32 frequency, s32 flags)
{
	assert(enabled);
	capturedfrequency = frequency;
	capturedflags = flags;
	capturedcalls++;
}

f32 accessibilityGetTargetingVolume(void) { return .15f; }
f32 accessibilityGetInterruptedTargetingVolume(void) { return .12f; }
void accessibilityLogEvent(const char *category, const char *event,
		const char *fmt, ...) { }

static void closeTo(f32 actual, f32 expected)
{
	assert(fabsf(actual - expected) < .002f);
}

static f32 frequency(f32 offset, s32 *zone)
{
	struct accessibilitytargetingcandidate candidate = {0};
	candidate.verticalaimerrornormalized = offset;
	return accessibilityTargetingPrecisionFrequency(&candidate, 900, zone);
}

int main(void)
{
	f32 lastpan = 0, lastabove = 900, lastbelow = 900;
	s32 i, zone;
	struct accessibilitytargetingcombatslot voice = {0};
	struct accessibilitytargetingcandidate candidate = {0};

	closeTo(accessibilityTargetingPrecisionPan(0), 0);
	closeTo(accessibilityTargetingPrecisionPan(.0025f), 0);
	closeTo(accessibilityTargetingPrecisionPan(-.0025f), 0);
	assert(accessibilityTargetingPrecisionPan(.0026f) >= .16f);
	assert(accessibilityTargetingPrecisionPan(-.0026f) <= -.16f);
	closeTo(accessibilityTargetingPrecisionPan(1), 1);
	closeTo(accessibilityTargetingPrecisionPan(-1), -1);
	closeTo(accessibilityTargetingPrecisionPan(NAN), 0);
	closeTo(accessibilityTargetingPrecisionPan(INFINITY), 0);
	closeTo(frequency(0, &zone), 900); assert(zone == 0);
	closeTo(frequency(.0025f, &zone), 900); assert(zone == 0);
	closeTo(frequency(-.0025f, &zone), 900); assert(zone == 0);
	assert(frequency(.0026f, &zone) > 960 && zone == 1);
	assert(frequency(-.0026f, &zone) < 851 && zone == -1);
	closeTo(frequency(.5f, &zone), 1500);
	closeTo(frequency(-.5f, &zone), 600);
	closeTo(frequency(1, &zone), 1500);
	closeTo(frequency(-1, &zone), 600);
	closeTo(frequency(NAN, &zone), 900); assert(zone == 0);

	/* The reported near misses should no longer collapse to centered/level. */
	assert(accessibilityTargetingPrecisionPan(20.0f / 960.0f) > .30f);
	assert(frequency(5.0f / 540.0f, &zone) > 1000 && zone == 1);
	assert(frequency(-5.0f / 540.0f, &zone) < 820 && zone == -1);
	/* Existing viewport normalization still scales uniformly with resolution. */
	closeTo(accessibilityTargetingPrecisionPan(20.0f / 960.0f),
			accessibilityTargetingPrecisionPan(40.0f / 1920.0f));
	closeTo(frequency(5.0f / 540.0f, &zone), frequency(10.0f / 1080.0f, &zone));

	for (i = 1; i <= 1000; i++) {
		f32 offset = (f32)i / 1000.0f;
		f32 pan = accessibilityTargetingPrecisionPan(offset);
		f32 above = frequency(offset, &zone);
		f32 below = frequency(-offset, &zone);
		assert(pan >= lastpan && pan <= 1);
		assert(above >= lastabove && above <= 1500.01f);
		assert(below <= lastbelow && below >= 599.99f);
		closeTo(accessibilityTargetingPrecisionPan(-offset), -pan);
		lastpan = pan; lastabove = above; lastbelow = below;
	}

	voice.precisionguidance = 1;
	voice.elevationzone = 1;
	voice.frequencyhz = 1200;
	closeTo(accessibilityTargetingPrecisionSmoothFrequency(&voice, 750, -1), 750);
	closeTo(accessibilityTargetingPrecisionSmoothFrequency(&voice, 900, 0), 900);
	closeTo(accessibilityTargetingPrecisionSmoothFrequency(&voice, 1000, 1), 1070);
	voice.elevationzone = -1;
	voice.frequencyhz = 750;
	closeTo(accessibilityTargetingPrecisionSmoothFrequency(&voice, 1000, 1), 1000);
	voice.precisionguidance = 0;
	closeTo(accessibilityTargetingPrecisionSmoothFrequency(&voice, 800, -1), 800);
	assert(capturedcalls == 0); /* Guidance helpers cannot create a lock. */

	/* Preserve exact-lock body-region, protected and glass semantics. */
	g_AccessibilityTargetingCurrentState = &g_AccessibilityTargetingStates[0];
	candidate.relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
	accessibilityTargetingUpdateAlignment(1, &candidate, "fixture");
	closeTo(capturedfrequency, 660); assert(capturedflags == 0);
	candidate.aimregion = ACCESSIBILITY_TARGETING_AIM_REGION_HEAD;
	accessibilityTargetingUpdateAlignment(2, &candidate, "fixture");
	closeTo(capturedfrequency, 825);
	candidate.aimregion = ACCESSIBILITY_TARGETING_AIM_REGION_ARM;
	accessibilityTargetingUpdateAlignment(3, &candidate, "fixture");
	closeTo(capturedfrequency, 528);
	candidate.relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED;
	accessibilityTargetingUpdateAlignment(4, &candidate, "fixture");
	closeTo(capturedfrequency, 316.8f);
	assert(capturedflags == ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED);
	candidate.relationship = ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
	candidate.aimregion = ACCESSIBILITY_TARGETING_AIM_REGION_STANDARD;
	candidate.obstruction = ACCESSIBILITY_TARGETING_OBSTRUCTION_PENETRABLE_GLASS;
	accessibilityTargetingUpdateAlignment(5, &candidate, "fixture");
	closeTo(capturedfrequency, 660);
	assert(capturedflags == ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_PENETRABLE);
	/* A hostile detector relationship must not turn an explosive weapon's
	 * destructible category into the ordinary solid hostile lock. */
	candidate.category = ACCESSIBILITY_TARGETING_CATEGORY_DESTROYABLE_OBJECT;
	candidate.obstruction = ACCESSIBILITY_TARGETING_OBSTRUCTION_NONE;
	accessibilityTargetingUpdateAlignment(6, &candidate, "weapon_fixture");
	closeTo(capturedfrequency, 396);
	assert(capturedflags == ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED);
	puts("Precision mapping: direction, close-in contrast, bounds, smoothing and unchanged locks passed.");
	return 0;
}

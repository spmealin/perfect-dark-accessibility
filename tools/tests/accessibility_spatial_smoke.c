/* Explicit SDK smoke/benchmark. Does not launch the game or touch its logs. */
#include <assert.h>
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdarg.h>
#include "../../port/src/accessibility/accessibility_spatial.c"

void configRegisterInt(const char *name, s32 *value, s32 minimum, s32 maximum) { }
u64 sysGetMicroseconds(void)
{
	return SDL_GetPerformanceCounter() * 1000000 / SDL_GetPerformanceFrequency();
}
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...)
{
	va_list args;
	printf("%s/%s ", category, event);
	va_start(args, format); vprintf(format, args); va_end(args);
	puts("");
}

static void energy(float x, float y, float z, double *left, double *right)
{
	int i;
	unsigned noise = 12345;
	float l, r;
	*left = *right = 0;
	accessibilitySpatialReset(0);
	accessibilitySpatialDirection(0, x, y, z);
	for (i = 0; i < NATIVE_RATE; i++) {
		noise = noise * 1664525u + 1013904223u;
		accessibilitySpatialSample(0, ((noise >> 16) / 32768.0f - 1) * .1f, &l, &r);
		assert(isfinite(l) && isfinite(r));
		*left += l*l; *right += r*r;
	}
}

int main(int argc, char **argv)
{
	double left, right, above, below;
	u64 start;
	int i, slot;
	float l, r;
	accessibilitySpatialInit();
	if (argc > 1 && !strcmp(argv[1], "--expect-fallback")) {
		assert(!accessibilitySpatialEnabled());
		return 0;
	}
	assert(accessibilitySpatialEnabled());
	energy(-1, 0, 0, &left, &right); assert(left > right);
	printf("Left source energy: L=%.6f R=%.6f\n", left, right);
	energy(1, 0, 0, &left, &right); assert(right > left);
	energy(0, 1, -1, &left, &right); above = left + right;
	energy(0, -1, -1, &left, &right); below = left + right;
	assert(fabs(above - below) > .00001);
	printf("Elevation filter energies: above=%.6f below=%.6f\n", above, below);
	for (slot = 0; slot < ACCESSIBILITY_SPATIAL_VOICES; slot++) accessibilitySpatialReset(slot);
	blocks = processUs = maxUs = 0;
	start = sysGetMicroseconds();
	for (i = 0; i < NATIVE_RATE; i++) {
		for (slot = 0; slot < ACCESSIBILITY_SPATIAL_VOICES; slot++) {
			accessibilitySpatialSample(slot, sinf(i * .2f) * .1f, &l, &r);
		}
	}
	printf("%d continuously active voices, 1 second of audio: %llu us wall, %llu us DSP; allocations after warmup=%llu\n",
		ACCESSIBILITY_SPATIAL_VOICES, (unsigned long long)(sysGetMicroseconds() - start), processUs, allocations - warmAllocations);
	assert(allocations == warmAllocations);
	accessibilitySpatialShutdown();
	printf("Shutdown allocations=%llu frees=%llu\n", allocations, frees);
	assert(allocations == frees);
	assert(!accessibilitySpatialEnabled());
	accessibilitySpatialShutdown();
	/* Saved-off startup still preallocates, so turning on from the menu is
	 * allocation-free and never performs SDK enumeration/context creation. */
	accessibilitySpatialSetEnabled(0);
	accessibilitySpatialInit();
	assert(accessibilitySpatialAvailable() && !accessibilitySpatialEnabled());
	{
		unsigned long long before = allocations;
		accessibilitySpatialInit();
		accessibilitySpatialSetEnabled(1);
		assert(accessibilitySpatialEnabled() && allocations == before);
	}
	accessibilitySpatialShutdown();
	assert(allocations == frees);
	return 0;
}

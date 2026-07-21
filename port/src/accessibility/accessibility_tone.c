#include <math.h>
#include <string.h>
#include <SDL.h>
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_TONE_SAMPLE_RATE 22020.0f
#define ACCESSIBILITY_TONE_VOLUME 0.12f
#define ACCESSIBILITY_TONE_GAIN_STEP (1.0f / (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f))
#define ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES 2048
#define TWO_PI 6.28318530717958647692f

static SDL_atomic_t g_AccessibilityToneEnabled;
static SDL_atomic_t g_AccessibilityToneFrequencyMilliHz;
static s16 g_AccessibilityToneMixBuffer[ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES];
static f32 g_AccessibilityTonePhase;
static f32 g_AccessibilityToneFrequencyHz;
static f32 g_AccessibilityToneGain;

static s16 accessibilityToneClamp(s32 value)
{
	if (value < -32768) {
		return -32768;
	}

	if (value > 32767) {
		return 32767;
	}

	return value;
}

void accessibilityToneSet(s32 enabled, f32 frequencyhz)
{
	if (enabled) {
		if (frequencyhz < 1.0f) {
			frequencyhz = 1.0f;
		}

		SDL_AtomicSet(&g_AccessibilityToneFrequencyMilliHz,
				(s32)(frequencyhz * 1000.0f));
	}

	SDL_AtomicSet(&g_AccessibilityToneEnabled, enabled != 0);
}

const s16 *accessibilityToneMix(const s16 *input, u32 len)
{
	s32 enabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	f32 targetfrequency = (f32)SDL_AtomicGet(
			&g_AccessibilityToneFrequencyMilliHz) / 1000.0f;
	f32 targetgain = enabled ? ACCESSIBILITY_TONE_VOLUME : 0.0f;
	u32 frames;
	f32 frequencystep;
	u32 i;

	if (!input || len == 0 || len % (sizeof(s16) * 2) != 0
			|| len > sizeof(g_AccessibilityToneMixBuffer)) {
		return input;
	}

	if (!enabled && g_AccessibilityToneGain <= 0.0f) {
		return input;
	}

	memcpy(g_AccessibilityToneMixBuffer, input, len);
	frames = len / (sizeof(s16) * 2);

	if (g_AccessibilityToneFrequencyHz < 1.0f) {
		g_AccessibilityToneFrequencyHz = targetfrequency;
	}

	frequencystep = (targetfrequency - g_AccessibilityToneFrequencyHz)
			/ (f32)frames;

	for (i = 0; i < frames; i++) {
		s32 tone;
		u32 index = i * 2;

		g_AccessibilityToneFrequencyHz += frequencystep;

		if (g_AccessibilityToneGain < targetgain) {
			g_AccessibilityToneGain += ACCESSIBILITY_TONE_GAIN_STEP;

			if (g_AccessibilityToneGain > targetgain) {
				g_AccessibilityToneGain = targetgain;
			}
		} else if (g_AccessibilityToneGain > targetgain) {
			g_AccessibilityToneGain -= ACCESSIBILITY_TONE_GAIN_STEP;

			if (g_AccessibilityToneGain < targetgain) {
				g_AccessibilityToneGain = targetgain;
			}
		}

		tone = (s32)(sinf(g_AccessibilityTonePhase)
				* g_AccessibilityToneGain * 32767.0f);
		g_AccessibilityToneMixBuffer[index] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index] + tone);
		g_AccessibilityToneMixBuffer[index + 1] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index + 1] + tone);

		g_AccessibilityTonePhase += TWO_PI
				* g_AccessibilityToneFrequencyHz / ACCESSIBILITY_TONE_SAMPLE_RATE;

		if (g_AccessibilityTonePhase >= TWO_PI) {
			g_AccessibilityTonePhase -= TWO_PI;
		}
	}

	return g_AccessibilityToneMixBuffer;
}

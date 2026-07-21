#include <math.h>
#include <string.h>
#include <SDL.h>
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_TONE_SAMPLE_RATE 22020.0f
#define ACCESSIBILITY_TONE_VOLUME 0.12f
#define ACCESSIBILITY_TONE_GAIN_STEP (1.0f / (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f))
#define ACCESSIBILITY_CHIRP_VOLUME 0.16f
#define ACCESSIBILITY_CHIRP_DURATION_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.10f))
#define ACCESSIBILITY_CHIRP_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_CHIRP_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.02f))
#define ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES 2048
#define TWO_PI 6.28318530717958647692f

static SDL_atomic_t g_AccessibilityToneEnabled;
static SDL_atomic_t g_AccessibilityToneFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityChirpSequence;
static SDL_atomic_t g_AccessibilityChirpEnabled;
static SDL_atomic_t g_AccessibilityChirpFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityChirpVolumeMillionths;
static SDL_atomic_t g_AccessibilityChirpPanMillionths;
static s16 g_AccessibilityToneMixBuffer[ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES];
static f32 g_AccessibilityTonePhase;
static f32 g_AccessibilityToneFrequencyHz;
static f32 g_AccessibilityToneGain;
static s32 g_AccessibilityChirpObservedSequence;
static s32 g_AccessibilityChirpSamplesRemaining;
static s32 g_AccessibilityChirpSample;
static f32 g_AccessibilityChirpPhase;
static f32 g_AccessibilityChirpFrequencyHz;
static f32 g_AccessibilityChirpVolume;
static f32 g_AccessibilityChirpPan;

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

void accessibilityTonePlayChirp(f32 frequencyhz, f32 volume, f32 pan)
{
	if (frequencyhz < 1.0f) {
		frequencyhz = 1.0f;
	}

	if (volume < 0.0f) {
		volume = 0.0f;
	} else if (volume > 1.0f) {
		volume = 1.0f;
	}

	if (pan < -1.0f) {
		pan = -1.0f;
	} else if (pan > 1.0f) {
		pan = 1.0f;
	}

	SDL_AtomicSet(&g_AccessibilityChirpFrequencyMilliHz,
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityToneStopChirp(void)
{
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

const s16 *accessibilityToneMix(const s16 *input, u32 len)
{
	s32 enabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	s32 chirpsequence = SDL_AtomicGet(&g_AccessibilityChirpSequence);
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

	if (chirpsequence != g_AccessibilityChirpObservedSequence) {
		g_AccessibilityChirpObservedSequence = chirpsequence;

		if (SDL_AtomicGet(&g_AccessibilityChirpEnabled)) {
			g_AccessibilityChirpFrequencyHz = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpFrequencyMilliHz) / 1000.0f;
			g_AccessibilityChirpVolume = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpVolumeMillionths) / 1000000.0f;
			g_AccessibilityChirpPan = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpPanMillionths) / 1000000.0f;
			g_AccessibilityChirpSamplesRemaining = ACCESSIBILITY_CHIRP_DURATION_SAMPLES;
			g_AccessibilityChirpSample = 0;
			g_AccessibilityChirpPhase = 0.0f;
		} else {
			g_AccessibilityChirpSamplesRemaining = 0;
		}
	}

	if (!enabled && g_AccessibilityToneGain <= 0.0f
			&& g_AccessibilityChirpSamplesRemaining <= 0) {
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
		s32 chirpleft = 0;
		s32 chirpright = 0;
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

		if (g_AccessibilityChirpSamplesRemaining > 0) {
			f32 envelope = 1.0f;
			f32 leftpan = g_AccessibilityChirpPan > 0.0f
					? 1.0f - g_AccessibilityChirpPan : 1.0f;
			f32 rightpan = g_AccessibilityChirpPan < 0.0f
					? 1.0f + g_AccessibilityChirpPan : 1.0f;
			f32 chirp;

			if (g_AccessibilityChirpSample < ACCESSIBILITY_CHIRP_ATTACK_SAMPLES) {
				envelope = (f32)g_AccessibilityChirpSample
						/ (f32)ACCESSIBILITY_CHIRP_ATTACK_SAMPLES;
			} else if (g_AccessibilityChirpSamplesRemaining
					< ACCESSIBILITY_CHIRP_RELEASE_SAMPLES) {
				envelope = (f32)g_AccessibilityChirpSamplesRemaining
						/ (f32)ACCESSIBILITY_CHIRP_RELEASE_SAMPLES;
			}

			chirp = sinf(g_AccessibilityChirpPhase) * envelope
					* g_AccessibilityChirpVolume * ACCESSIBILITY_CHIRP_VOLUME
					* 32767.0f;
			chirpleft = (s32)(chirp * leftpan);
			chirpright = (s32)(chirp * rightpan);

			g_AccessibilityChirpPhase += TWO_PI
					* g_AccessibilityChirpFrequencyHz / ACCESSIBILITY_TONE_SAMPLE_RATE;

			if (g_AccessibilityChirpPhase >= TWO_PI) {
				g_AccessibilityChirpPhase -= TWO_PI;
			}

			g_AccessibilityChirpSample++;
			g_AccessibilityChirpSamplesRemaining--;
		}

		g_AccessibilityToneMixBuffer[index] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index] + tone + chirpleft);
		g_AccessibilityToneMixBuffer[index + 1] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index + 1] + tone + chirpright);

		g_AccessibilityTonePhase += TWO_PI
				* g_AccessibilityToneFrequencyHz / ACCESSIBILITY_TONE_SAMPLE_RATE;

		if (g_AccessibilityTonePhase >= TWO_PI) {
			g_AccessibilityTonePhase -= TWO_PI;
		}
	}

	return g_AccessibilityToneMixBuffer;
}

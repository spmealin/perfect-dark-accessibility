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
#define ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_CHIRP_PATTERN_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_CHIRP_PATTERN_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_CHIRP_PATTERN_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.007f))
#define ACCESSIBILITY_WEAPON_FUNCTION_FREQUENCY_HZ 1000.0f
#define ACCESSIBILITY_WEAPON_FUNCTION_VOLUME 0.10f
#define ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_WEAPON_FUNCTION_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.030f))
#define ACCESSIBILITY_WEAPON_FUNCTION_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.002f))
#define ACCESSIBILITY_WEAPON_FUNCTION_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_HAZARD_VOLUME 0.14f
#define ACCESSIBILITY_COMBAT_CHIRP_VOLUME 0.07f
#define ACCESSIBILITY_COMBAT_GAIN_STEP (ACCESSIBILITY_COMBAT_CHIRP_VOLUME / (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f))
#define ACCESSIBILITY_COMBAT_CHIRP_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.004f))
#define ACCESSIBILITY_COMBAT_CHIRP_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.012f))
#define ACCESSIBILITY_TRACKER_VOLUME 0.065f
#define ACCESSIBILITY_TRACKER_LEVEL_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.045f))
#define ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_TRACKER_DOUBLE_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_TRACKER_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_TRACKER_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.008f))
#define ACCESSIBILITY_TRACKER_REAR_MODULATION_HZ 30.0f
#define ACCESSIBILITY_CANE_VOLUME 0.115f
#define ACCESSIBILITY_CANE_DURATION_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_CANE_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_CANE_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.008f))
#define ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES 2048
#define TWO_PI 6.28318530717958647692f

static SDL_atomic_t g_AccessibilityToneEnabled;
static SDL_atomic_t g_AccessibilityToneFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityChirpSequence;
static SDL_atomic_t g_AccessibilityChirpEnabled;
static SDL_atomic_t g_AccessibilityChirpFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityChirpVolumeMillionths;
static SDL_atomic_t g_AccessibilityChirpPanMillionths;
static SDL_atomic_t g_AccessibilityChirpPulses;
static SDL_atomic_t g_AccessibilityWeaponFunctionSequence;
static SDL_atomic_t g_AccessibilityWeaponFunctionPulses;
static SDL_atomic_t g_AccessibilityHazardEnabled;
static SDL_atomic_t g_AccessibilityHazardFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityHazardVolumeMillionths;
static SDL_atomic_t g_AccessibilityHazardPanMillionths;
static SDL_atomic_t g_AccessibilityCombatEnabled[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatTriggerSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatFrequencyMilliHz[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatVolumeMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatPanMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatPeriodMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatDurationMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatContinuous[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerEnabled[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerSequence[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerFrequencyMilliHz[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerVolumeMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerPanMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerPeriodMs[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerHeight[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerRear[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneEnabled[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneSequence[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneVolumeMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePanMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
static SDL_atomic_t g_AccessibilityToneMixCalls;
static SDL_atomic_t g_AccessibilityTonePassthroughCalls;
static SDL_atomic_t g_AccessibilityToneActiveCalls;
static SDL_atomic_t g_AccessibilityToneMixedFrames;
static SDL_atomic_t g_AccessibilityCaneActiveMask;
static SDL_atomic_t g_AccessibilityCaneCommands;
static SDL_atomic_t g_AccessibilityCaneTonesStarted;
static SDL_atomic_t g_AccessibilityCaneStops;
#endif
static s16 g_AccessibilityToneMixBuffer[ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES];
static f32 g_AccessibilityTonePhase;
static f32 g_AccessibilityToneFrequencyHz;
static f32 g_AccessibilityToneGain;
static s32 g_AccessibilityChirpObservedSequence;
static s32 g_AccessibilityChirpSamplesRemaining;
static s32 g_AccessibilityChirpSample;
static s32 g_AccessibilityChirpPulseCount;
static f32 g_AccessibilityChirpPhase;
static f32 g_AccessibilityChirpFrequencyHz;
static f32 g_AccessibilityChirpVolume;
static f32 g_AccessibilityChirpPan;
static s32 g_AccessibilityWeaponFunctionObservedSequence;
static s32 g_AccessibilityWeaponFunctionSamplesRemaining;
static s32 g_AccessibilityWeaponFunctionSample;
static f32 g_AccessibilityWeaponFunctionPhase;
static f32 g_AccessibilityHazardPhase;
static f32 g_AccessibilityHazardFrequencyHz;
static f32 g_AccessibilityHazardGain;
static f32 g_AccessibilityHazardPan;
static s32 g_AccessibilityCombatObservedSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static s32 g_AccessibilityCombatObservedTriggerSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static s32 g_AccessibilityCombatCycleSample[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static f32 g_AccessibilityCombatPhase[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static f32 g_AccessibilityCombatPan[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static f32 g_AccessibilityCombatGain[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static s32 g_AccessibilityTrackerObservedSequence[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static s32 g_AccessibilityTrackerCycleSample[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static f32 g_AccessibilityTrackerPhase[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static f32 g_AccessibilityTrackerModulationPhase[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static f32 g_AccessibilityTrackerPan[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static s32 g_AccessibilityCaneObservedSequence[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCaneSamplesRemaining[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCaneSample[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCanePhase[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCaneFrequencyHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCaneVolume[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCanePan[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];

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
	accessibilityTonePlayChirpPattern(frequencyhz, volume, pan, 1);
}

void accessibilityTonePlayChirpPattern(f32 frequencyhz, f32 volume, f32 pan,
		s32 pulses)
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

	if (pulses < 1) {
		pulses = 1;
	} else if (pulses > 8) {
		pulses = 8;
	}

	SDL_AtomicSet(&g_AccessibilityChirpFrequencyMilliHz,
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPulses, pulses);
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityToneStopChirp(void)
{
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityTonePlayWeaponFunction(s32 secondary)
{
	SDL_AtomicSet(&g_AccessibilityWeaponFunctionPulses, secondary ? 2 : 1);
	SDL_AtomicAdd(&g_AccessibilityWeaponFunctionSequence, 1);
}

void accessibilityToneStopWeaponFunction(void)
{
	SDL_AtomicSet(&g_AccessibilityWeaponFunctionPulses, 0);
	SDL_AtomicAdd(&g_AccessibilityWeaponFunctionSequence, 1);
}

void accessibilityToneSetHazard(s32 enabled, f32 frequencyhz, f32 volume, f32 pan)
{
	if (enabled) {
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

		SDL_AtomicSet(&g_AccessibilityHazardFrequencyMilliHz,
				(s32)(frequencyhz * 1000.0f));
		SDL_AtomicSet(&g_AccessibilityHazardVolumeMillionths,
				(s32)(volume * 1000000.0f));
		SDL_AtomicSet(&g_AccessibilityHazardPanMillionths,
				(s32)(pan * 1000000.0f));
	}

	SDL_AtomicSet(&g_AccessibilityHazardEnabled, enabled != 0 && volume > 0.0f);
}

void accessibilityToneSetCombatSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 durationms, s32 continuous,
		s32 restart, s32 triggernow)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT) {
		return;
	}

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

	if (periodms < 1) {
		periodms = 1;
	}

	if (durationms < 1) {
		durationms = 1;
	} else if (durationms > periodms) {
		durationms = periodms;
	}

	SDL_AtomicSet(&g_AccessibilityCombatFrequencyMilliHz[slot],
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatPeriodMs[slot], periodms);
	SDL_AtomicSet(&g_AccessibilityCombatDurationMs[slot], durationms);
	SDL_AtomicSet(&g_AccessibilityCombatContinuous[slot], continuous != 0);
	SDL_AtomicSet(&g_AccessibilityCombatEnabled[slot], enabled && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityCombatSequence[slot], 1);
	}

	if (triggernow) {
		SDL_AtomicAdd(&g_AccessibilityCombatTriggerSequence[slot], 1);
	}
}

void accessibilityToneStopCombat(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityCombatEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityCombatSequence[slot], 1);
	}
}

void accessibilityToneSetTrackerSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 height, s32 rear,
		s32 restart)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT) {
		return;
	}

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

	if (periodms < 100) {
		periodms = 100;
	}

	if (height < 0 || height > 2) {
		height = 0;
	}

	SDL_AtomicSet(&g_AccessibilityTrackerFrequencyMilliHz[slot],
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityTrackerVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityTrackerPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityTrackerPeriodMs[slot], periodms);
	SDL_AtomicSet(&g_AccessibilityTrackerHeight[slot], height);
	SDL_AtomicSet(&g_AccessibilityTrackerRear[slot], rear != 0);
	SDL_AtomicSet(&g_AccessibilityTrackerEnabled[slot],
			enabled != 0 && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityTrackerSequence[slot], 1);
	}
}

void accessibilityToneStopTracker(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityTrackerEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityTrackerSequence[slot], 1);
	}
}

void accessibilityTonePlayCaneSlot(s32 slot, f32 frequencyhz,
		f32 volume, f32 pan)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_CANE_SLOT_COUNT) {
		return;
	}

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

	SDL_AtomicSet(&g_AccessibilityCaneFrequencyMilliHz[slot],
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCanePanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneEnabled[slot], volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityCaneSequence[slot], 1);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityCaneCommands, 1);
#endif
}

void accessibilityToneStopCane(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityCaneEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityCaneSequence[slot], 1);
	}
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityCaneStops, 1);
#endif
}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
void accessibilityToneGetDiagnostics(struct accessibilitytonediagnostics *diagnostics)
{
	s32 slot;

	if (!diagnostics) {
		return;
	}

	diagnostics->toneenabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	diagnostics->chirpenabled = SDL_AtomicGet(&g_AccessibilityChirpEnabled);
	diagnostics->chirpsequence = SDL_AtomicGet(&g_AccessibilityChirpSequence);
	diagnostics->weaponfunctionsequence = SDL_AtomicGet(
			&g_AccessibilityWeaponFunctionSequence);
	diagnostics->weaponfunctionpulses = SDL_AtomicGet(
			&g_AccessibilityWeaponFunctionPulses);
	diagnostics->hazardenabled = SDL_AtomicGet(&g_AccessibilityHazardEnabled);
	diagnostics->combatenabledslots = 0;
	diagnostics->trackerenabledslots = 0;
	diagnostics->canerequestedmask = 0;
	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		diagnostics->combatenabledslots += SDL_AtomicGet(
				&g_AccessibilityCombatEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
		diagnostics->trackerenabledslots += SDL_AtomicGet(
				&g_AccessibilityTrackerEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		if (SDL_AtomicGet(&g_AccessibilityCaneEnabled[slot])) {
			diagnostics->canerequestedmask |= 1 << slot;
		}
	}
	diagnostics->caneactivemask = SDL_AtomicGet(
			&g_AccessibilityCaneActiveMask);
	diagnostics->canecommands = SDL_AtomicGet(&g_AccessibilityCaneCommands);
	diagnostics->canetonesstarted = SDL_AtomicGet(
			&g_AccessibilityCaneTonesStarted);
	diagnostics->canestops = SDL_AtomicGet(&g_AccessibilityCaneStops);
	diagnostics->hazardfrequencymillihz = SDL_AtomicGet(
			&g_AccessibilityHazardFrequencyMilliHz);
	diagnostics->hazardvolumemillionths = SDL_AtomicGet(
			&g_AccessibilityHazardVolumeMillionths);
	diagnostics->hazardpanmillionths = SDL_AtomicGet(
			&g_AccessibilityHazardPanMillionths);
	diagnostics->mixcalls = SDL_AtomicGet(&g_AccessibilityToneMixCalls);
	diagnostics->passthroughcalls = SDL_AtomicGet(
			&g_AccessibilityTonePassthroughCalls);
	diagnostics->activecalls = SDL_AtomicGet(&g_AccessibilityToneActiveCalls);
	diagnostics->mixedframes = SDL_AtomicGet(&g_AccessibilityToneMixedFrames);
}
#endif

const s16 *accessibilityToneMix(const s16 *input, u32 len)
{
	s32 enabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	s32 chirpsequence = SDL_AtomicGet(&g_AccessibilityChirpSequence);
	s32 weaponfunctionsequence = SDL_AtomicGet(
			&g_AccessibilityWeaponFunctionSequence);
	s32 hazardenabled = SDL_AtomicGet(&g_AccessibilityHazardEnabled);
	s32 combatenabled[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatfrequency[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatvolume[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatpan[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatperiodsamples[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatdurationsamples[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatcontinuous[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 trackerenabled[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackerfrequency[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackervolume[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackerpan[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerperiodsamples[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerheight[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerrear[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 anycombatenabled = 0;
	s32 anytrackerenabled = 0;
	s32 anycaneactive = 0;
	f32 targetfrequency = (f32)SDL_AtomicGet(
			&g_AccessibilityToneFrequencyMilliHz) / 1000.0f;
	f32 targetgain = enabled ? ACCESSIBILITY_TONE_VOLUME : 0.0f;
	f32 hazardtargetfrequency = (f32)SDL_AtomicGet(
			&g_AccessibilityHazardFrequencyMilliHz) / 1000.0f;
	f32 hazardtargetgain = hazardenabled
			? (f32)SDL_AtomicGet(&g_AccessibilityHazardVolumeMillionths)
					/ 1000000.0f * ACCESSIBILITY_HAZARD_VOLUME
			: 0.0f;
	f32 hazardtargetpan = (f32)SDL_AtomicGet(
			&g_AccessibilityHazardPanMillionths) / 1000000.0f;
	u32 frames;
	f32 frequencystep;
	f32 hazardfrequencystep;
	f32 hazardpanstep;
	u32 i;
	s32 slot;

	if (!input || len == 0 || len % (sizeof(s16) * 2) != 0
			|| len > sizeof(g_AccessibilityToneMixBuffer)) {
		return input;
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityCombatSequence[slot]);
		s32 triggersequence = SDL_AtomicGet(
				&g_AccessibilityCombatTriggerSequence[slot]);
		s32 offset;

		combatenabled[slot] = SDL_AtomicGet(&g_AccessibilityCombatEnabled[slot]);
		combatfrequency[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityCombatFrequencyMilliHz[slot]) / 1000.0f;
		combatvolume[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityCombatVolumeMillionths[slot]) / 1000000.0f;
		combatpan[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityCombatPanMillionths[slot]) / 1000000.0f;
		combatperiodsamples[slot] = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE
				* (f32)SDL_AtomicGet(&g_AccessibilityCombatPeriodMs[slot])
				/ 1000.0f);
		combatdurationsamples[slot] = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE
				* (f32)SDL_AtomicGet(&g_AccessibilityCombatDurationMs[slot])
				/ 1000.0f);
		if (combatperiodsamples[slot] < 1) {
			combatperiodsamples[slot] = 1;
		}
		if (combatdurationsamples[slot] < 1) {
			combatdurationsamples[slot] = 1;
		} else if (combatdurationsamples[slot] > combatperiodsamples[slot]) {
			combatdurationsamples[slot] = combatperiodsamples[slot];
		}
		combatcontinuous[slot] = SDL_AtomicGet(
				&g_AccessibilityCombatContinuous[slot]);
		anycombatenabled |= combatenabled[slot]
				|| g_AccessibilityCombatGain[slot] > 0.0f;

		if (sequence != g_AccessibilityCombatObservedSequence[slot]) {
			g_AccessibilityCombatObservedSequence[slot] = sequence;
			/* Golden-ratio offsets keep independently repeating voices from
			 * starting together even when the first few slots are occupied. */
			offset = ((slot * 618) % 1000)
					* combatperiodsamples[slot] / 1000;
			g_AccessibilityCombatCycleSample[slot] = offset == 0 ? 0
					: combatperiodsamples[slot] - offset;
			g_AccessibilityCombatPhase[slot] = 0.0f;
			g_AccessibilityCombatPan[slot] = combatpan[slot];
		}

		if (triggersequence
				!= g_AccessibilityCombatObservedTriggerSequence[slot]) {
			g_AccessibilityCombatObservedTriggerSequence[slot] = triggersequence;
			g_AccessibilityCombatCycleSample[slot] = 0;
			g_AccessibilityCombatPhase[slot] = 0.0f;
			g_AccessibilityCombatPan[slot] = combatpan[slot];
		}
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityTrackerSequence[slot]);
		s32 offset;

		trackerenabled[slot] = SDL_AtomicGet(
				&g_AccessibilityTrackerEnabled[slot]);
		trackerfrequency[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityTrackerFrequencyMilliHz[slot]) / 1000.0f;
		trackervolume[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityTrackerVolumeMillionths[slot]) / 1000000.0f;
		trackerpan[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityTrackerPanMillionths[slot]) / 1000000.0f;
		trackerperiodsamples[slot] = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE
				* (f32)SDL_AtomicGet(&g_AccessibilityTrackerPeriodMs[slot])
				/ 1000.0f);
		if (trackerperiodsamples[slot] < 1) {
			trackerperiodsamples[slot] = 1;
		}
		trackerheight[slot] = SDL_AtomicGet(
				&g_AccessibilityTrackerHeight[slot]);
		trackerrear[slot] = SDL_AtomicGet(
				&g_AccessibilityTrackerRear[slot]);
		anytrackerenabled |= trackerenabled[slot];

		if (sequence != g_AccessibilityTrackerObservedSequence[slot]) {
			g_AccessibilityTrackerObservedSequence[slot] = sequence;
			offset = ((slot * 618) % 1000)
					* trackerperiodsamples[slot] / 1000;
			g_AccessibilityTrackerCycleSample[slot] = offset == 0 ? 0
					: trackerperiodsamples[slot] - offset;
			g_AccessibilityTrackerPhase[slot] = 0.0f;
			g_AccessibilityTrackerModulationPhase[slot] = 0.0f;
			g_AccessibilityTrackerPan[slot] = trackerpan[slot];
		} else if (g_AccessibilityTrackerCycleSample[slot]
				>= trackerperiodsamples[slot]) {
			g_AccessibilityTrackerCycleSample[slot] = 0;
			g_AccessibilityTrackerPhase[slot] = 0.0f;
		}
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityCaneSequence[slot]);

		if (sequence != g_AccessibilityCaneObservedSequence[slot]) {
			g_AccessibilityCaneObservedSequence[slot] = sequence;

			if (SDL_AtomicGet(&g_AccessibilityCaneEnabled[slot])) {
				g_AccessibilityCaneFrequencyHz[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCaneFrequencyMilliHz[slot]) / 1000.0f;
				g_AccessibilityCaneVolume[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCaneVolumeMillionths[slot]) / 1000000.0f;
				g_AccessibilityCanePan[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCanePanMillionths[slot]) / 1000000.0f;
				g_AccessibilityCaneSamplesRemaining[slot]
						= ACCESSIBILITY_CANE_DURATION_SAMPLES;
				g_AccessibilityCaneSample[slot] = 0;
				g_AccessibilityCanePhase[slot] = 0.0f;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
				SDL_AtomicAdd(&g_AccessibilityCaneTonesStarted, 1);
#endif
			} else {
				g_AccessibilityCaneSamplesRemaining[slot] = 0;
			}
		}

		anycaneactive |= g_AccessibilityCaneSamplesRemaining[slot] > 0;
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityToneMixCalls, 1);
#endif

	if (chirpsequence != g_AccessibilityChirpObservedSequence) {
		g_AccessibilityChirpObservedSequence = chirpsequence;

		if (SDL_AtomicGet(&g_AccessibilityChirpEnabled)) {
			g_AccessibilityChirpFrequencyHz = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpFrequencyMilliHz) / 1000.0f;
			g_AccessibilityChirpVolume = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpVolumeMillionths) / 1000000.0f;
			g_AccessibilityChirpPan = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpPanMillionths) / 1000000.0f;
			g_AccessibilityChirpPulseCount = SDL_AtomicGet(
					&g_AccessibilityChirpPulses);
			g_AccessibilityChirpSamplesRemaining
					= g_AccessibilityChirpPulseCount > 1
						? ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES
								* g_AccessibilityChirpPulseCount
							+ ACCESSIBILITY_CHIRP_PATTERN_GAP_SAMPLES
								* (g_AccessibilityChirpPulseCount - 1)
						: ACCESSIBILITY_CHIRP_DURATION_SAMPLES;
			g_AccessibilityChirpSample = 0;
			g_AccessibilityChirpPhase = 0.0f;
		} else {
			g_AccessibilityChirpSamplesRemaining = 0;
		}
	}

	if (weaponfunctionsequence
			!= g_AccessibilityWeaponFunctionObservedSequence) {
		s32 pulses = SDL_AtomicGet(&g_AccessibilityWeaponFunctionPulses);

		g_AccessibilityWeaponFunctionObservedSequence = weaponfunctionsequence;
		g_AccessibilityWeaponFunctionSample = 0;
		g_AccessibilityWeaponFunctionPhase = 0.0f;
		g_AccessibilityWeaponFunctionSamplesRemaining = pulses > 0
				? ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES * pulses
						+ ACCESSIBILITY_WEAPON_FUNCTION_GAP_SAMPLES
								* (pulses - 1)
				: 0;
	}

	if (!enabled && g_AccessibilityToneGain <= 0.0f
			&& !hazardenabled && g_AccessibilityHazardGain <= 0.0f
			&& g_AccessibilityChirpSamplesRemaining <= 0
			&& g_AccessibilityWeaponFunctionSamplesRemaining <= 0
			&& !anycombatenabled && !anytrackerenabled && !anycaneactive) {
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
		SDL_AtomicSet(&g_AccessibilityCaneActiveMask, 0);
		SDL_AtomicAdd(&g_AccessibilityTonePassthroughCalls, 1);
#endif
		return input;
	}

	memcpy(g_AccessibilityToneMixBuffer, input, len);
	frames = len / (sizeof(s16) * 2);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityToneActiveCalls, 1);
	SDL_AtomicAdd(&g_AccessibilityToneMixedFrames, frames);
#endif

	if (g_AccessibilityToneFrequencyHz < 1.0f) {
		g_AccessibilityToneFrequencyHz = targetfrequency;
	}

	frequencystep = (targetfrequency - g_AccessibilityToneFrequencyHz)
			/ (f32)frames;

	if (g_AccessibilityHazardFrequencyHz < 1.0f) {
		g_AccessibilityHazardFrequencyHz = hazardtargetfrequency;
	}

	hazardfrequencystep = (hazardtargetfrequency
			- g_AccessibilityHazardFrequencyHz) / (f32)frames;
	hazardpanstep = (hazardtargetpan - g_AccessibilityHazardPan) / (f32)frames;

	for (i = 0; i < frames; i++) {
		s32 tone;
		s32 chirpleft = 0;
		s32 chirpright = 0;
		s32 hazardleft = 0;
		s32 hazardright = 0;
		s32 weaponfunctiontone = 0;
		s32 combatleft = 0;
		s32 combatright = 0;
		s32 trackerleft = 0;
		s32 trackerright = 0;
		s32 caneleft = 0;
		s32 caneright = 0;
		u32 index = i * 2;

		g_AccessibilityToneFrequencyHz += frequencystep;
		g_AccessibilityHazardFrequencyHz += hazardfrequencystep;
		g_AccessibilityHazardPan += hazardpanstep;

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

		if (g_AccessibilityHazardGain < hazardtargetgain) {
			g_AccessibilityHazardGain += ACCESSIBILITY_TONE_GAIN_STEP;

			if (g_AccessibilityHazardGain > hazardtargetgain) {
				g_AccessibilityHazardGain = hazardtargetgain;
			}
		} else if (g_AccessibilityHazardGain > hazardtargetgain) {
			g_AccessibilityHazardGain -= ACCESSIBILITY_TONE_GAIN_STEP;

			if (g_AccessibilityHazardGain < hazardtargetgain) {
				g_AccessibilityHazardGain = hazardtargetgain;
			}
		}

		if (g_AccessibilityHazardGain > 0.0f) {
			f32 leftpan = g_AccessibilityHazardPan > 0.0f
					? 1.0f - g_AccessibilityHazardPan : 1.0f;
			f32 rightpan = g_AccessibilityHazardPan < 0.0f
					? 1.0f + g_AccessibilityHazardPan : 1.0f;
			f32 hazard = sinf(g_AccessibilityHazardPhase)
					* g_AccessibilityHazardGain * 32767.0f;

			hazardleft = (s32)(hazard * leftpan);
			hazardright = (s32)(hazard * rightpan);
		}

		if (g_AccessibilityChirpSamplesRemaining > 0) {
			f32 envelope = 1.0f;
			s32 chirpactive = 1;
			f32 leftpan = g_AccessibilityChirpPan > 0.0f
					? 1.0f - g_AccessibilityChirpPan : 1.0f;
			f32 rightpan = g_AccessibilityChirpPan < 0.0f
					? 1.0f + g_AccessibilityChirpPan : 1.0f;
			f32 chirp;

			if (g_AccessibilityChirpPulseCount > 1) {
				s32 cycle = ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES
						+ ACCESSIBILITY_CHIRP_PATTERN_GAP_SAMPLES;
				s32 sample = g_AccessibilityChirpSample % cycle;

				if (sample >= ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES) {
					chirpactive = 0;
				} else if (sample < ACCESSIBILITY_CHIRP_PATTERN_ATTACK_SAMPLES) {
					envelope = (f32)sample
							/ (f32)ACCESSIBILITY_CHIRP_PATTERN_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES - sample
						< ACCESSIBILITY_CHIRP_PATTERN_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES - sample)
							/ (f32)ACCESSIBILITY_CHIRP_PATTERN_RELEASE_SAMPLES;
				}
			} else if (g_AccessibilityChirpSample < ACCESSIBILITY_CHIRP_ATTACK_SAMPLES) {
				envelope = (f32)g_AccessibilityChirpSample
						/ (f32)ACCESSIBILITY_CHIRP_ATTACK_SAMPLES;
			} else if (g_AccessibilityChirpSamplesRemaining
					< ACCESSIBILITY_CHIRP_RELEASE_SAMPLES) {
				envelope = (f32)g_AccessibilityChirpSamplesRemaining
						/ (f32)ACCESSIBILITY_CHIRP_RELEASE_SAMPLES;
			}

			chirp = chirpactive ? sinf(g_AccessibilityChirpPhase) * envelope
					* g_AccessibilityChirpVolume * ACCESSIBILITY_CHIRP_VOLUME
					* 32767.0f : 0.0f;
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

		if (g_AccessibilityWeaponFunctionSamplesRemaining > 0) {
			s32 cyclelength = ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES
					+ ACCESSIBILITY_WEAPON_FUNCTION_GAP_SAMPLES;
			s32 beepsample = g_AccessibilityWeaponFunctionSample % cyclelength;

			if (beepsample < ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES) {
				f32 envelope = 1.0f;

				if (beepsample < ACCESSIBILITY_WEAPON_FUNCTION_ATTACK_SAMPLES) {
					envelope = (f32)beepsample
							/ (f32)ACCESSIBILITY_WEAPON_FUNCTION_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES
						- beepsample < ACCESSIBILITY_WEAPON_FUNCTION_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES
							- beepsample)
							/ (f32)ACCESSIBILITY_WEAPON_FUNCTION_RELEASE_SAMPLES;
				}

				weaponfunctiontone = (s32)(sinf(
						g_AccessibilityWeaponFunctionPhase) * envelope
						* ACCESSIBILITY_WEAPON_FUNCTION_VOLUME * 32767.0f);
				g_AccessibilityWeaponFunctionPhase += TWO_PI
						* ACCESSIBILITY_WEAPON_FUNCTION_FREQUENCY_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityWeaponFunctionPhase >= TWO_PI) {
					g_AccessibilityWeaponFunctionPhase -= TWO_PI;
				}
			} else {
				g_AccessibilityWeaponFunctionPhase = 0.0f;
			}

			g_AccessibilityWeaponFunctionSample++;
			g_AccessibilityWeaponFunctionSamplesRemaining--;
		}

		for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
			f32 targetcombatgain = combatenabled[slot] && combatcontinuous[slot]
					? combatvolume[slot] * ACCESSIBILITY_COMBAT_CHIRP_VOLUME
					: 0.0f;

			g_AccessibilityCombatPan[slot] += (combatpan[slot]
					- g_AccessibilityCombatPan[slot]) / (f32)(frames - i);

			if (g_AccessibilityCombatGain[slot] < targetcombatgain) {
				g_AccessibilityCombatGain[slot] += ACCESSIBILITY_COMBAT_GAIN_STEP;
				if (g_AccessibilityCombatGain[slot] > targetcombatgain) {
					g_AccessibilityCombatGain[slot] = targetcombatgain;
				}
			} else if (g_AccessibilityCombatGain[slot] > targetcombatgain) {
				g_AccessibilityCombatGain[slot] -= ACCESSIBILITY_COMBAT_GAIN_STEP;
				if (g_AccessibilityCombatGain[slot] < targetcombatgain) {
					g_AccessibilityCombatGain[slot] = targetcombatgain;
				}
			}

			if (g_AccessibilityCombatGain[slot] > 0.0f) {
				f32 leftpan = g_AccessibilityCombatPan[slot] > 0.0f
						? 1.0f - g_AccessibilityCombatPan[slot] : 1.0f;
				f32 rightpan = g_AccessibilityCombatPan[slot] < 0.0f
						? 1.0f + g_AccessibilityCombatPan[slot] : 1.0f;
				f32 combat = sinf(g_AccessibilityCombatPhase[slot])
						* g_AccessibilityCombatGain[slot] * 32767.0f;

				combatleft += (s32)(combat * leftpan);
				combatright += (s32)(combat * rightpan);
				g_AccessibilityCombatPhase[slot] += TWO_PI
						* combatfrequency[slot] / ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityCombatPhase[slot] >= TWO_PI) {
					g_AccessibilityCombatPhase[slot] -= TWO_PI;
				}
			}

			if (combatenabled[slot] && !combatcontinuous[slot]
					&& g_AccessibilityCombatGain[slot] <= 0.0f) {
				s32 sample = g_AccessibilityCombatCycleSample[slot];

				if (sample < combatdurationsamples[slot]) {
					f32 envelope = 1.0f;
					f32 leftpan = g_AccessibilityCombatPan[slot] > 0.0f
							? 1.0f - g_AccessibilityCombatPan[slot] : 1.0f;
					f32 rightpan = g_AccessibilityCombatPan[slot] < 0.0f
							? 1.0f + g_AccessibilityCombatPan[slot] : 1.0f;
					f32 combat;

					if (sample < ACCESSIBILITY_COMBAT_CHIRP_ATTACK_SAMPLES) {
						envelope = (f32)sample
								/ (f32)ACCESSIBILITY_COMBAT_CHIRP_ATTACK_SAMPLES;
					} else if (combatdurationsamples[slot] - sample
							< ACCESSIBILITY_COMBAT_CHIRP_RELEASE_SAMPLES) {
						envelope = (f32)(combatdurationsamples[slot]
								- sample)
								/ (f32)ACCESSIBILITY_COMBAT_CHIRP_RELEASE_SAMPLES;
					}

					combat = sinf(g_AccessibilityCombatPhase[slot]) * envelope
							* combatvolume[slot] * ACCESSIBILITY_COMBAT_CHIRP_VOLUME
							* 32767.0f;
					combatleft += (s32)(combat * leftpan);
					combatright += (s32)(combat * rightpan);
					g_AccessibilityCombatPhase[slot] += TWO_PI
							* combatfrequency[slot] / ACCESSIBILITY_TONE_SAMPLE_RATE;

					if (g_AccessibilityCombatPhase[slot] >= TWO_PI) {
						g_AccessibilityCombatPhase[slot] -= TWO_PI;
					}
				}

				g_AccessibilityCombatCycleSample[slot]++;
				if (g_AccessibilityCombatCycleSample[slot]
						>= combatperiodsamples[slot]) {
					g_AccessibilityCombatCycleSample[slot] = 0;
					g_AccessibilityCombatPhase[slot] = 0.0f;
				}
			}
		}

		for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
			s32 sample = g_AccessibilityTrackerCycleSample[slot];
			s32 beepsample = -1;
			s32 beeplength = ACCESSIBILITY_TRACKER_LEVEL_BEEP_SAMPLES;
			f32 frequencymultiplier = 1.0f;

			g_AccessibilityTrackerPan[slot] += (trackerpan[slot]
					- g_AccessibilityTrackerPan[slot]) / (f32)(frames - i);

			if (trackerenabled[slot]) {
				if (trackerheight[slot] == 0) {
					if (sample < ACCESSIBILITY_TRACKER_LEVEL_BEEP_SAMPLES) {
						beepsample = sample;
					}
				} else {
					beeplength = ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES;

					if (sample < ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES) {
						beepsample = sample;
						frequencymultiplier = trackerheight[slot] == 1
								? 0.9f : 1.1f;
					} else if (sample >= ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES
							+ ACCESSIBILITY_TRACKER_DOUBLE_GAP_SAMPLES
							&& sample < ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES
									* 2
								+ ACCESSIBILITY_TRACKER_DOUBLE_GAP_SAMPLES) {
						beepsample = sample
								- ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES
								- ACCESSIBILITY_TRACKER_DOUBLE_GAP_SAMPLES;
						frequencymultiplier = trackerheight[slot] == 1
								? 1.1f : 0.9f;
					}
				}
			}

			if (beepsample >= 0) {
				f32 envelope = 1.0f;
				f32 leftpan = g_AccessibilityTrackerPan[slot] > 0.0f
						? 1.0f - g_AccessibilityTrackerPan[slot] : 1.0f;
				f32 rightpan = g_AccessibilityTrackerPan[slot] < 0.0f
						? 1.0f + g_AccessibilityTrackerPan[slot] : 1.0f;
				f32 modulation = trackerrear[slot]
						? 0.75f + 0.25f * sinf(
								g_AccessibilityTrackerModulationPhase[slot])
						: 1.0f;
				f32 tracker;

				if (beepsample == 0) {
					g_AccessibilityTrackerPhase[slot] = 0.0f;
				}

				if (beepsample < ACCESSIBILITY_TRACKER_ATTACK_SAMPLES) {
					envelope = (f32)beepsample
							/ (f32)ACCESSIBILITY_TRACKER_ATTACK_SAMPLES;
				} else if (beeplength - beepsample
						< ACCESSIBILITY_TRACKER_RELEASE_SAMPLES) {
					envelope = (f32)(beeplength - beepsample)
							/ (f32)ACCESSIBILITY_TRACKER_RELEASE_SAMPLES;
				}

				tracker = sinf(g_AccessibilityTrackerPhase[slot]) * envelope
						* modulation * trackervolume[slot]
						* ACCESSIBILITY_TRACKER_VOLUME * 32767.0f;
				trackerleft += (s32)(tracker * leftpan);
				trackerright += (s32)(tracker * rightpan);
				g_AccessibilityTrackerPhase[slot] += TWO_PI
						* trackerfrequency[slot] * frequencymultiplier
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				g_AccessibilityTrackerModulationPhase[slot] += TWO_PI
						* ACCESSIBILITY_TRACKER_REAR_MODULATION_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;

				if (g_AccessibilityTrackerPhase[slot] >= TWO_PI) {
					g_AccessibilityTrackerPhase[slot] -= TWO_PI;
				}
				if (g_AccessibilityTrackerModulationPhase[slot] >= TWO_PI) {
					g_AccessibilityTrackerModulationPhase[slot] -= TWO_PI;
				}
			}

			if (trackerenabled[slot]) {
				g_AccessibilityTrackerCycleSample[slot]++;
				if (g_AccessibilityTrackerCycleSample[slot]
						>= trackerperiodsamples[slot]) {
					g_AccessibilityTrackerCycleSample[slot] = 0;
					g_AccessibilityTrackerPhase[slot] = 0.0f;
				}
			}
		}

		if (anycaneactive) {
			for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
				if (g_AccessibilityCaneSamplesRemaining[slot] > 0) {
					f32 envelope = 1.0f;
					f32 leftpan = g_AccessibilityCanePan[slot] > 0.0f
							? 1.0f - g_AccessibilityCanePan[slot] : 1.0f;
					f32 rightpan = g_AccessibilityCanePan[slot] < 0.0f
							? 1.0f + g_AccessibilityCanePan[slot] : 1.0f;
					f32 cane;

					if (g_AccessibilityCaneSample[slot]
							< ACCESSIBILITY_CANE_ATTACK_SAMPLES) {
						envelope = (f32)g_AccessibilityCaneSample[slot]
								/ (f32)ACCESSIBILITY_CANE_ATTACK_SAMPLES;
					} else if (g_AccessibilityCaneSamplesRemaining[slot]
							< ACCESSIBILITY_CANE_RELEASE_SAMPLES) {
						envelope = (f32)g_AccessibilityCaneSamplesRemaining[slot]
								/ (f32)ACCESSIBILITY_CANE_RELEASE_SAMPLES;
					}

					cane = sinf(g_AccessibilityCanePhase[slot]) * envelope
							* g_AccessibilityCaneVolume[slot]
							* ACCESSIBILITY_CANE_VOLUME * 32767.0f;
					caneleft += (s32)(cane * leftpan);
					caneright += (s32)(cane * rightpan);
					g_AccessibilityCanePhase[slot] += TWO_PI
							* g_AccessibilityCaneFrequencyHz[slot]
							/ ACCESSIBILITY_TONE_SAMPLE_RATE;

					if (g_AccessibilityCanePhase[slot] >= TWO_PI) {
						g_AccessibilityCanePhase[slot] -= TWO_PI;
					}

					g_AccessibilityCaneSample[slot]++;
					g_AccessibilityCaneSamplesRemaining[slot]--;
				}
			}
		}

		g_AccessibilityToneMixBuffer[index] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index] + tone + chirpleft
						+ hazardleft + combatleft + trackerleft + caneleft
						+ weaponfunctiontone);
		g_AccessibilityToneMixBuffer[index + 1] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index + 1] + tone + chirpright
						+ hazardright + combatright + trackerright + caneright
						+ weaponfunctiontone);

		g_AccessibilityTonePhase += TWO_PI
				* g_AccessibilityToneFrequencyHz / ACCESSIBILITY_TONE_SAMPLE_RATE;

		if (g_AccessibilityTonePhase >= TWO_PI) {
			g_AccessibilityTonePhase -= TWO_PI;
		}

		g_AccessibilityHazardPhase += TWO_PI
				* g_AccessibilityHazardFrequencyHz / ACCESSIBILITY_TONE_SAMPLE_RATE;

		if (g_AccessibilityHazardPhase >= TWO_PI) {
			g_AccessibilityHazardPhase -= TWO_PI;
		}
	}

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	{
		s32 activemask = 0;

		for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
			if (g_AccessibilityCaneSamplesRemaining[slot] > 0) {
				activemask |= 1 << slot;
			}
		}

		SDL_AtomicSet(&g_AccessibilityCaneActiveMask, activemask);
	}
#endif

	return g_AccessibilityToneMixBuffer;
}

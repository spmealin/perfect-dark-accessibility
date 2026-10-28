#include <math.h>
#include <string.h>
#include <SDL.h>
#include "constants.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_tone.h"
#include "accessibility_tone_ownership.h"

#include "accessibility_tone_internal.h"

/* Main-thread publication only. The mixer owns all non-atomic playback state. */
SDL_atomic_t g_AccessibilityPreviewActive;
SDL_atomic_t g_AccessibilityPreviewSequence;
SDL_atomic_t g_AccessibilityPreviewDurationMs;

SDL_atomic_t g_AccessibilityToneEnabled;
SDL_atomic_t g_AccessibilityToneFrequencyMilliHz;
SDL_atomic_t g_AccessibilityTonePatternFlags;
SDL_atomic_t g_AccessibilityTonePatternSequence;
SDL_atomic_t g_AccessibilityChirpSequence;
SDL_atomic_t g_AccessibilityChirpEnabled;
SDL_atomic_t g_AccessibilityChirpFrequencyMilliHz;
SDL_atomic_t g_AccessibilityChirpVolumeMillionths;
SDL_atomic_t g_AccessibilityChirpGainMillionths;
SDL_atomic_t g_AccessibilityChirpPanMillionths;
SDL_atomic_t g_AccessibilityChirpPulses;
SDL_atomic_t g_AccessibilityChirpStyle;
SDL_atomic_t g_AccessibilityTargetPresenceSequence;
SDL_atomic_t g_AccessibilityTargetPresenceEnabled;
SDL_atomic_t g_AccessibilityTargetPresenceFrequencyMilliHz;
SDL_atomic_t g_AccessibilityTargetPresenceVolumeMillionths;
SDL_atomic_t g_AccessibilityTargetPresencePanMillionths;
SDL_atomic_t g_AccessibilityThreatAlertSequence;
SDL_atomic_t g_AccessibilityThreatAlertEnabled;
SDL_atomic_t g_AccessibilityThreatAlertVolumeMillionths;
SDL_atomic_t g_AccessibilityThreatAlertPanMillionths;
SDL_atomic_t g_AccessibilityToggleSequence;
SDL_atomic_t g_AccessibilityTogglePattern;
SDL_atomic_t g_AccessibilityTogglePulses;
SDL_atomic_t g_AccessibilityCompassSequence;
SDL_atomic_t g_AccessibilityCompassEnabled;
SDL_atomic_t g_AccessibilityCompassPulses;
SDL_atomic_t g_AccessibilityWeaponFunctionSequence;
SDL_atomic_t g_AccessibilityWeaponFunctionPulses;
SDL_atomic_t g_AccessibilityHazardEnabled;
SDL_atomic_t g_AccessibilityHazardFrequencyMilliHz;
SDL_atomic_t g_AccessibilityHazardVolumeMillionths;
SDL_atomic_t g_AccessibilityHazardPanMillionths;
SDL_atomic_t g_AccessibilityCaneEdgeEnabled;
SDL_atomic_t g_AccessibilityCaneEdgeVolumeMillionths;
SDL_atomic_t g_AccessibilityCaneEdgePanMillionths;
SDL_atomic_t g_AccessibilityCombatEnabled[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatTriggerSequence[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatFrequencyMilliHz[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatEndFrequencyMilliHz[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatVolumeMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatPanMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatPeriodMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatDurationMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatFrequencyContour[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCombatContinuous[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectileEnabled[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectileSequence[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectileFrequencyMilliHz[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectileVolumeMillionths[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectilePanMillionths[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectilePeriodMs[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityProjectileDurationMs[ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerEnabled[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerSequence[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerFrequencyMilliHz[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerVolumeMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerPanMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerPeriodMs[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerHeight[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityTrackerRear[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityFriendlyEnabled[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
SDL_atomic_t g_AccessibilityFriendlySequence[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
SDL_atomic_t g_AccessibilityFriendlyVolumeMillionths[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
SDL_atomic_t g_AccessibilityFriendlyPanMillionths[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
SDL_atomic_t g_AccessibilityFriendlyPulseThird[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
SDL_atomic_t g_AccessibilityDoorEnabled[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
SDL_atomic_t g_AccessibilityDoorSequence[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
SDL_atomic_t g_AccessibilityDoorVolumeMillionths[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
SDL_atomic_t g_AccessibilityDoorPanMillionths[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
SDL_atomic_t g_AccessibilityRadarSequence;
SDL_atomic_t g_AccessibilityRadarEnabled;
SDL_atomic_t g_AccessibilityRadarFrequencyMilliHz;
SDL_atomic_t g_AccessibilityRadarVolumeMillionths;
SDL_atomic_t g_AccessibilityRadarPanMillionths;
SDL_atomic_t g_AccessibilityRadarHeight;
SDL_atomic_t g_AccessibilityRadarRear;
SDL_atomic_t g_AccessibilityRadarKind;
SDL_atomic_t g_AccessibilityHillSequence;
SDL_atomic_t g_AccessibilityHillEnabled;
SDL_atomic_t g_AccessibilityHillVolumeMillionths;
SDL_atomic_t g_AccessibilityHillPanMillionths;
SDL_atomic_t g_AccessibilityHillRear;
SDL_atomic_t g_AccessibilityHillIdentityChirp;
SDL_atomic_t g_AccessibilityHillScoring;
SDL_atomic_t g_AccessibilityCaneEnabled[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCaneSequence[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCaneFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCaneEndFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCaneVolumeMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCanePanMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCaneDurationMs[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCanePattern[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCanePathFloorCount[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCanePathCount[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
SDL_atomic_t g_AccessibilityCanePathFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
SDL_atomic_t g_AccessibilityCanePathVolumeMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
SDL_atomic_t g_AccessibilityCanePathPanMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
SDL_atomic_t g_AccessibilityMarkerEnabled[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
SDL_atomic_t g_AccessibilityMarkerSequence[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
SDL_atomic_t g_AccessibilityMarkerVolumeMillionths[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
SDL_atomic_t g_AccessibilityMarkerPanMillionths[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
SDL_atomic_t g_AccessibilityMarkerProfile[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
SDL_atomic_t g_AccessibilityMarkerRemovalSequence[ACCESSIBILITY_TONE_MARKER_SLOT_COUNT];
SDL_atomic_t g_AccessibilityMarkerResetSequence;
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
SDL_atomic_t g_AccessibilityToneMixCalls;
SDL_atomic_t g_AccessibilityTonePassthroughCalls;
SDL_atomic_t g_AccessibilityToneActiveCalls;
SDL_atomic_t g_AccessibilityToneMixedFrames;
SDL_atomic_t g_AccessibilityCaneActiveMask;
SDL_atomic_t g_AccessibilityCaneCommands;
SDL_atomic_t g_AccessibilityCaneTonesStarted;
SDL_atomic_t g_AccessibilityCaneStops;
#endif

void accessibilityToneSet(s32 enabled, f32 frequencyhz)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	accessibilityToneSetAlignment(enabled, frequencyhz, 0);
}

void accessibilityToneSetAlignment(s32 enabled, f32 frequencyhz,
		s32 patternflags)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 previousenabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	s32 previouspatternflags = SDL_AtomicGet(
			&g_AccessibilityTonePatternFlags);

	if (enabled) {
		if (frequencyhz < 1.0f) {
			frequencyhz = 1.0f;
		}

		SDL_AtomicSet(&g_AccessibilityToneFrequencyMilliHz,
				(s32)(frequencyhz * 1000.0f));
	}

	patternflags = enabled ? patternflags : 0;
	SDL_AtomicSet(&g_AccessibilityTonePatternFlags, patternflags);
	SDL_AtomicSet(&g_AccessibilityToneEnabled, enabled != 0);

	if (previousenabled != (enabled != 0)
			|| previouspatternflags != patternflags) {
		SDL_AtomicAdd(&g_AccessibilityTonePatternSequence, 1);
	}
}

void accessibilityTonePlayChirp(f32 frequencyhz, f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	accessibilityTonePlayChirpPattern(frequencyhz, volume, pan, 1, 1.0f);
}

void accessibilityTonePlayChirpPattern(f32 frequencyhz, f32 volume, f32 pan,
		s32 pulses, f32 gain)
{
	if (!accessibilityToneOwnershipCanPublish()) {
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

	if (pulses < 1) {
		pulses = 1;
	} else if (pulses > 8) {
		pulses = 8;
	}

	if (gain < 0.0f) {
		gain = 0.0f;
	} else if (gain > 2.0f) {
		gain = 2.0f;
	}

	SDL_AtomicSet(&g_AccessibilityChirpFrequencyMilliHz,
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpGainMillionths,
			(s32)(gain * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPulses, pulses);
	SDL_AtomicSet(&g_AccessibilityChirpStyle,
			ACCESSIBILITY_CHIRP_STYLE_STANDARD);
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityTonePlayMovableLandmark(f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	/* Reuse the serialized object-scanner lane: the distinctive pattern must
	 * take its normal round-robin turn rather than overlap another F5 cue. */
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

	SDL_AtomicSet(&g_AccessibilityChirpVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpGainMillionths, 1000000);
	SDL_AtomicSet(&g_AccessibilityChirpPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityChirpPulses, 1);
	SDL_AtomicSet(&g_AccessibilityChirpStyle,
			ACCESSIBILITY_CHIRP_STYLE_MOVABLE_LANDMARK);
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityToneStopChirp(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityTonePlayTargetPresence(f32 frequencyhz,
		f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
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

	SDL_AtomicSet(&g_AccessibilityTargetPresenceFrequencyMilliHz,
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityTargetPresenceVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityTargetPresencePanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityTargetPresenceEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityTargetPresenceSequence, 1);
}

void accessibilityToneStopTargetPresence(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityTargetPresenceEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityTargetPresenceSequence, 1);
}

void accessibilityTonePlayThreatAlert(f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
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

	SDL_AtomicSet(&g_AccessibilityThreatAlertVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityThreatAlertPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityThreatAlertEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityThreatAlertSequence, 1);
}

void accessibilityToneStopThreatAlert(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityThreatAlertEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityThreatAlertSequence, 1);
}

void accessibilityTonePlayToggleConfirmation(s32 enabled)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityTogglePattern,
			enabled ? ACCESSIBILITY_TOGGLE_PATTERN_ON
					: ACCESSIBILITY_TOGGLE_PATTERN_OFF);
	SDL_AtomicSet(&g_AccessibilityTogglePulses, 2);
	SDL_AtomicAdd(&g_AccessibilityToggleSequence, 1);
}

void accessibilityTonePlayCaneModeConfirmation(s32 mode)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 pattern = mode;

	if (pattern < ACCESSIBILITY_TOGGLE_PATTERN_OFF) {
		pattern = ACCESSIBILITY_TOGGLE_PATTERN_OFF;
	} else if (pattern > ACCESSIBILITY_TOGGLE_PATTERN_FAST) {
		pattern = ACCESSIBILITY_TOGGLE_PATTERN_FAST;
	}

	SDL_AtomicSet(&g_AccessibilityTogglePattern, pattern);
	SDL_AtomicSet(&g_AccessibilityTogglePulses,
			pattern == ACCESSIBILITY_TOGGLE_PATTERN_FAST ? 3 : 2);
	SDL_AtomicAdd(&g_AccessibilityToggleSequence, 1);
}

void accessibilityTonePlayCompass(s32 pulses)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (pulses < 1) {
		pulses = 1;
	} else if (pulses > 4) {
		pulses = 4;
	}

	SDL_AtomicSet(&g_AccessibilityCompassPulses, pulses);
	SDL_AtomicSet(&g_AccessibilityCompassEnabled, 1);
	SDL_AtomicAdd(&g_AccessibilityCompassSequence, 1);
}

void accessibilityToneStopCompass(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityCompassEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityCompassSequence, 1);
}

void accessibilityTonePlayStanceConfirmation(s32 crouchpos)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 pulses = CROUCHPOS_STAND - crouchpos + 1;

	if (pulses < 1) {
		pulses = 1;
	} else if (pulses > 3) {
		pulses = 3;
	}

	SDL_AtomicSet(&g_AccessibilityTogglePattern,
			ACCESSIBILITY_TOGGLE_PATTERN_ON);
	SDL_AtomicSet(&g_AccessibilityTogglePulses, pulses);
	SDL_AtomicAdd(&g_AccessibilityToggleSequence, 1);
}

void accessibilityTonePlayWeaponFunction(s32 secondary)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityWeaponFunctionPulses, secondary ? 2 : 1);
	SDL_AtomicAdd(&g_AccessibilityWeaponFunctionSequence, 1);
}

void accessibilityToneStopWeaponFunction(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityWeaponFunctionPulses, 0);
	SDL_AtomicAdd(&g_AccessibilityWeaponFunctionSequence, 1);
}

void accessibilityToneSetHazard(s32 enabled, f32 frequencyhz, f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
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

void accessibilityToneSetCaneEdge(s32 enabled, f32 volume, f32 pan)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	if (pan < -1.0f) pan = -1.0f;
	if (pan > 1.0f) pan = 1.0f;
	SDL_AtomicSet(&g_AccessibilityCaneEdgeVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneEdgePanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneEdgeEnabled,
			enabled != 0 && volume > 0.0f);
}

void accessibilityToneSetCombatSlot(s32 slot, s32 enabled,
		f32 startfrequencyhz, f32 endfrequencyhz, f32 volume, f32 pan,
		s32 periodms, s32 durationms, s32 frequencycontour, s32 continuous,
		s32 restart, s32 triggernow)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT) {
		return;
	}

	if (startfrequencyhz < 1.0f) {
		startfrequencyhz = 1.0f;
	}

	if (endfrequencyhz < 1.0f) {
		endfrequencyhz = 1.0f;
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
			(s32)(startfrequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatEndFrequencyMilliHz[slot],
			(s32)(endfrequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCombatPeriodMs[slot], periodms);
	SDL_AtomicSet(&g_AccessibilityCombatDurationMs[slot], durationms);
	SDL_AtomicSet(&g_AccessibilityCombatFrequencyContour[slot],
			frequencycontour);
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
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityCombatEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityCombatSequence[slot], 1);
	}
}

void accessibilityToneSetProjectileSlot(s32 slot, s32 enabled,
		f32 frequencyhz, f32 volume, f32 pan, s32 periodms,
		s32 durationms, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT) {
		return;
	}

	if (frequencyhz < 1.0f) frequencyhz = 1.0f;
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	if (pan < -1.0f) pan = -1.0f;
	if (pan > 1.0f) pan = 1.0f;
	if (periodms < 1) periodms = 1;
	if (durationms < 1) durationms = 1;
	if (durationms > periodms) durationms = periodms;

	SDL_AtomicSet(&g_AccessibilityProjectileFrequencyMilliHz[slot],
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityProjectileVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityProjectilePanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityProjectilePeriodMs[slot], periodms);
	SDL_AtomicSet(&g_AccessibilityProjectileDurationMs[slot], durationms);
	SDL_AtomicSet(&g_AccessibilityProjectileEnabled[slot],
			enabled && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityProjectileSequence[slot], 1);
	}
}

void accessibilityToneStopProjectiles(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityProjectileEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityProjectileSequence[slot], 1);
	}
}

void accessibilityToneSetTrackerSlot(s32 slot, s32 enabled, f32 frequencyhz,
		f32 volume, f32 pan, s32 periodms, s32 height, s32 rear,
		s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
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
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityTrackerEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityTrackerSequence[slot], 1);
	}
}

void accessibilityToneSetFriendlySlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 pulsethird, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT) {
		return;
	}

	if (volume < 0.0f) {
		volume = 0.0f;
	} else if (volume > 2.0f) {
		volume = 2.0f;
	}
	if (pan < -1.0f) {
		pan = -1.0f;
	} else if (pan > 1.0f) {
		pan = 1.0f;
	}

	SDL_AtomicSet(&g_AccessibilityFriendlyVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityFriendlyPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityFriendlyPulseThird[slot], pulsethird != 0);
	SDL_AtomicSet(&g_AccessibilityFriendlyEnabled[slot],
			enabled != 0 && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityFriendlySequence[slot], 1);
	}
}

void accessibilityToneStopFriendly(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityFriendlyEnabled[slot], 0);
	}
}

void accessibilityToneSetDoorSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_DOOR_SLOT_COUNT) {
		return;
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

	SDL_AtomicSet(&g_AccessibilityDoorVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityDoorPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityDoorEnabled[slot],
			enabled != 0 && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityDoorSequence[slot], 1);
	}
}

void accessibilityToneStopDoors(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_DOOR_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityDoorEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityDoorSequence[slot], 1);
	}
}

void accessibilityTonePlayRadarPing(f32 frequencyhz, f32 volume, f32 pan,
		s32 height, s32 rear, s32 kind)
{
	if (!accessibilityToneOwnershipCanPublish()) {
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
	if (height < 0 || height > 2) {
		height = 0;
	}
	if (kind < ACCESSIBILITY_TONE_RADAR_ENEMY
			|| kind > ACCESSIBILITY_TONE_RADAR_UNAVAILABLE) {
		kind = ACCESSIBILITY_TONE_RADAR_OTHER;
	}

	SDL_AtomicSet(&g_AccessibilityRadarFrequencyMilliHz,
			(s32)(frequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityRadarVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityRadarPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityRadarHeight, height);
	SDL_AtomicSet(&g_AccessibilityRadarRear, rear != 0);
	SDL_AtomicSet(&g_AccessibilityRadarKind, kind);
	SDL_AtomicSet(&g_AccessibilityRadarEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityRadarSequence, 1);
}

void accessibilityToneStopRadar(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityRadarEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityRadarSequence, 1);
}

void accessibilityToneSetHillBeacon(s32 enabled, f32 volume, f32 pan,
		s32 rear, s32 identitychirp, s32 scoring, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (volume < 0.0f) {
		volume = 0.0f;
	} else if (volume > 4.0f) {
		volume = 4.0f;
	}
	if (pan < -1.0f) {
		pan = -1.0f;
	} else if (pan > 1.0f) {
		pan = 1.0f;
	}

	SDL_AtomicSet(&g_AccessibilityHillVolumeMillionths,
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityHillPanMillionths,
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityHillRear, rear != 0);
	SDL_AtomicSet(&g_AccessibilityHillIdentityChirp, identitychirp != 0);
	SDL_AtomicSet(&g_AccessibilityHillScoring, scoring != 0);
	SDL_AtomicSet(&g_AccessibilityHillEnabled, enabled && volume > 0.0f);
	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityHillSequence, 1);
	}
}

void accessibilityToneStopHillBeacon(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	SDL_AtomicSet(&g_AccessibilityHillEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityHillSequence, 1);
}

void accessibilityTonePlayCaneSlot(s32 slot, f32 startfrequencyhz,
		f32 endfrequencyhz, f32 volume, f32 pan, s32 durationms,
		s32 pattern, s32 pathfloorcount, s32 pathcount,
		const f32 *pathfrequencieshz, const f32 *pathvolumes,
		const f32 *pathpans)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 i;

	if (slot < 0 || slot >= ACCESSIBILITY_TONE_CANE_SLOT_COUNT) {
		return;
	}

	if (startfrequencyhz < 1.0f) {
		startfrequencyhz = 1.0f;
	}
	if (endfrequencyhz < 1.0f) {
		endfrequencyhz = 1.0f;
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
	if (durationms < 10) {
		durationms = 10;
	} else if (durationms > 500) {
		durationms = 500;
	}
	if (pattern != ACCESSIBILITY_TONE_CANE_PATTERN_CROUCH_DOUBLE
			&& pattern != ACCESSIBILITY_TONE_CANE_PATTERN_PATH
			&& pattern != ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY) {
		pattern = ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR;
	}
	if (pathcount < 1 || pathcount > ACCESSIBILITY_CANE_PATH_MAX_STEPS
			|| pathfloorcount < 1 || pathfloorcount > pathcount
			|| !pathfrequencieshz || !pathvolumes) {
		pathcount = 0;
		pathfloorcount = 0;
		if (pattern == ACCESSIBILITY_TONE_CANE_PATTERN_PATH
				|| pattern == ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY) {
			pattern = ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR;
		}
	}

	SDL_AtomicSet(&g_AccessibilityCaneFrequencyMilliHz[slot],
			(s32)(startfrequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneEndFrequencyMilliHz[slot],
			(s32)(endfrequencyhz * 1000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCanePanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityCaneDurationMs[slot], durationms);
	SDL_AtomicSet(&g_AccessibilityCanePattern[slot], pattern);
	SDL_AtomicSet(&g_AccessibilityCanePathFloorCount[slot], pathfloorcount);
	SDL_AtomicSet(&g_AccessibilityCanePathCount[slot], pathcount);
	for (i = 0; i < pathcount; i++) {
		f32 pathfrequency = pathfrequencieshz[i];
		f32 pathvolume = pathvolumes[i];
		f32 pathpan = pathpans ? pathpans[i] : pan;
		if (pathfrequency < 1.0f) pathfrequency = 1.0f;
		if (pathvolume < 0.0f) pathvolume = 0.0f;
		if (pathvolume > 1.0f) pathvolume = 1.0f;
		if (pathpan < -1.0f) pathpan = -1.0f;
		if (pathpan > 1.0f) pathpan = 1.0f;
		SDL_AtomicSet(&g_AccessibilityCanePathFrequencyMilliHz[slot][i],
				(s32)(pathfrequency * 1000.0f));
		SDL_AtomicSet(&g_AccessibilityCanePathVolumeMillionths[slot][i],
				(s32)(pathvolume * 1000000.0f));
		SDL_AtomicSet(&g_AccessibilityCanePathPanMillionths[slot][i],
				(s32)(pathpan * 1000000.0f));
	}
	SDL_AtomicSet(&g_AccessibilityCaneEnabled[slot], volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityCaneSequence[slot], 1);
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityCaneCommands, 1);
#endif
}

void accessibilityToneStopCane(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityCaneEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityCaneSequence[slot], 1);
	}
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityCaneStops, 1);
#endif
}

static void accessibilityToneSetMarkerSlotProfile(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 profile, s32 restart)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_MARKER_VOICE_COUNT) {
		return;
	}

	if (volume < 0.0f) {
		volume = 0.0f;
	} else if (volume > 4.0f) {
		volume = 4.0f;
	}

	if (pan < -1.0f) {
		pan = -1.0f;
	} else if (pan > 1.0f) {
		pan = 1.0f;
	}

	SDL_AtomicSet(&g_AccessibilityMarkerVolumeMillionths[slot],
			(s32)(volume * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityMarkerPanMillionths[slot],
			(s32)(pan * 1000000.0f));
	SDL_AtomicSet(&g_AccessibilityMarkerProfile[slot], profile);
	SDL_AtomicSet(&g_AccessibilityMarkerEnabled[slot],
			enabled != 0 && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[slot], 1);
	}
}

void accessibilityToneSetMarkerSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	accessibilityToneSetMarkerSlotProfile(slot, enabled, volume, pan,
			ACCESSIBILITY_TONE_LANDMARK_STANDARD, restart);
}

void accessibilityTonePlayMarkerRemoval(s32 slot)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_MARKER_SLOT_COUNT) {
		return;
	}

	SDL_AtomicAdd(&g_AccessibilityMarkerRemovalSequence[slot], 1);
}

void accessibilityToneStopMarkers(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityMarkerEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[slot], 1);
	}
	SDL_AtomicAdd(&g_AccessibilityMarkerResetSequence, 1);
}

void accessibilityToneSetLandmarkSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 profile, s32 restart)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT) {
		return;
	}

	if (profile != ACCESSIBILITY_TONE_LANDMARK_VULNERABLE_TARGET
			&& profile != ACCESSIBILITY_TONE_LANDMARK_PILLAR) {
		profile = ACCESSIBILITY_TONE_LANDMARK_STANDARD;
	}

	accessibilityToneSetMarkerSlotProfile(
			ACCESSIBILITY_TONE_MARKER_SLOT_COUNT + slot,
			enabled, volume, pan, profile, restart);
}

void accessibilityToneStopLandmarks(void)
{
	if (!accessibilityToneOwnershipCanPublish()) {
		return;
	}
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		s32 voice = ACCESSIBILITY_TONE_MARKER_SLOT_COUNT + slot;

		SDL_AtomicSet(&g_AccessibilityMarkerEnabled[voice], 0);
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[voice], 1);
	}
}

static void accessibilityTonePreviewClear(void)
{
	accessibilityToneSet(0, 0.0f);
	accessibilityToneStopChirp();
	accessibilityToneStopTargetPresence();
	accessibilityToneStopThreatAlert();
	accessibilityToneStopCompass();
	accessibilityToneStopWeaponFunction();
	accessibilityToneSetHazard(0, 0.0f, 0.0f, 0.0f);
	accessibilityToneSetCaneEdge(0, 0.0f, 0.0f);
	accessibilityToneStopCombat();
	accessibilityToneStopProjectiles();
	accessibilityToneStopTracker();
	accessibilityToneStopFriendly();
	accessibilityToneStopDoors();
	accessibilityToneStopRadar();
	accessibilityToneStopHillBeacon();
	accessibilityToneStopCane();
	accessibilityToneStopMarkers();
	accessibilityToneStopLandmarks();
	SDL_AtomicSet(&g_AccessibilityTogglePulses, 0);
	SDL_AtomicAdd(&g_AccessibilityToggleSequence, 1);
}

void accessibilityTonePreviewBegin(void)
{
	accessibilityToneOwnershipBegin();
	accessibilityTonePreviewClear();
}

void accessibilityTonePreviewCommit(s32 durationms)
{
	if (!accessibilityToneOwnershipCanCommit()) {
		return;
	}
	if (durationms < 1) {
		durationms = 1;
	} else if (durationms > 5000) {
		durationms = 5000;
	}
	SDL_AtomicSet(&g_AccessibilityPreviewDurationMs, durationms);
	SDL_AtomicSet(&g_AccessibilityPreviewActive, 1);
	SDL_AtomicAdd(&g_AccessibilityPreviewSequence, 1);
	accessibilityToneOwnershipCommands(0);
}

void accessibilityTonePreviewStop(void)
{
	if (!accessibilityToneOwnershipIsPreview()) {
		return;
	}
	accessibilityToneOwnershipCommands(1);
	accessibilityTonePreviewClear();
	SDL_AtomicSet(&g_AccessibilityPreviewActive, 0);
	SDL_AtomicSet(&g_AccessibilityPreviewDurationMs, 0);
	SDL_AtomicAdd(&g_AccessibilityPreviewSequence, 1);
	accessibilityToneOwnershipCommands(0);
	accessibilityToneOwnershipRelease();
}

void accessibilityTonePreviewCommands(s32 enabled)
{
	accessibilityToneOwnershipCommands(enabled);
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
	diagnostics->radarenabled = SDL_AtomicGet(&g_AccessibilityRadarEnabled);
	diagnostics->radarsequence = SDL_AtomicGet(&g_AccessibilityRadarSequence);
	diagnostics->hillenabled = SDL_AtomicGet(&g_AccessibilityHillEnabled);
	diagnostics->combatenabledslots = 0;
	diagnostics->trackerenabledslots = 0;
	diagnostics->friendlyenabledslots = 0;
	diagnostics->doorenabledslots = 0;
	diagnostics->projectileenabledslots = 0;
	diagnostics->markerenabledslots = 0;
	diagnostics->landmarkenabledslots = 0;
	diagnostics->canerequestedmask = 0;
	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		diagnostics->combatenabledslots += SDL_AtomicGet(
				&g_AccessibilityCombatEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT; slot++) {
		diagnostics->trackerenabledslots += SDL_AtomicGet(
				&g_AccessibilityTrackerEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT; slot++) {
		diagnostics->friendlyenabledslots += SDL_AtomicGet(
				&g_AccessibilityFriendlyEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_DOOR_SLOT_COUNT; slot++) {
		diagnostics->doorenabledslots += SDL_AtomicGet(
				&g_AccessibilityDoorEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_PROJECTILE_SLOT_COUNT; slot++) {
		diagnostics->projectileenabledslots += SDL_AtomicGet(
				&g_AccessibilityProjectileEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT; slot++) {
		diagnostics->markerenabledslots += SDL_AtomicGet(
				&g_AccessibilityMarkerEnabled[slot]) != 0;
	}
	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		diagnostics->landmarkenabledslots += SDL_AtomicGet(
				&g_AccessibilityMarkerEnabled[
						ACCESSIBILITY_TONE_MARKER_SLOT_COUNT + slot]) != 0;
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

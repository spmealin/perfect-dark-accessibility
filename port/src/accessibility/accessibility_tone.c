#include <math.h>
#include <string.h>
#include <SDL.h>
#include "constants.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_TONE_SAMPLE_RATE 22020.0f
#define ACCESSIBILITY_TONE_GAIN_STEP (1.0f / (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f))
#define ACCESSIBILITY_ALIGNMENT_CYCLE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.100f))
#define ACCESSIBILITY_ALIGNMENT_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.010f))
#define ACCESSIBILITY_ALIGNMENT_ON_SAMPLES (ACCESSIBILITY_ALIGNMENT_CYCLE_SAMPLES - ACCESSIBILITY_ALIGNMENT_GAP_SAMPLES)
#define ACCESSIBILITY_ALIGNMENT_EDGE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.002f))
#define ACCESSIBILITY_ALIGNMENT_PENETRABLE_MODULATION_HZ 12.0f
#define ACCESSIBILITY_ALIGNMENT_PENETRABLE_MIN_GAIN 0.55f
#define ACCESSIBILITY_CHIRP_VOLUME 0.16f
#define ACCESSIBILITY_CHIRP_DURATION_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.10f))
#define ACCESSIBILITY_CHIRP_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_CHIRP_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.02f))
#define ACCESSIBILITY_CHIRP_PATTERN_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_CHIRP_PATTERN_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_CHIRP_PATTERN_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_CHIRP_PATTERN_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.007f))
#define ACCESSIBILITY_TOGGLE_BASE_FREQUENCY_HZ 880.0f
#define ACCESSIBILITY_TOGGLE_ON_FREQUENCY_HZ 1320.0f
#define ACCESSIBILITY_TOGGLE_OFF_FREQUENCY_HZ 440.0f
#define ACCESSIBILITY_TOGGLE_VOLUME 0.10f
#define ACCESSIBILITY_TOGGLE_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_TOGGLE_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_TOGGLE_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.002f))
#define ACCESSIBILITY_TOGGLE_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_TOGGLE_PATTERN_OFF 0
#define ACCESSIBILITY_TOGGLE_PATTERN_ON 1
#define ACCESSIBILITY_TOGGLE_PATTERN_FAST 2
#define ACCESSIBILITY_COMPASS_FREQUENCY_HZ 600.0f
#define ACCESSIBILITY_COMPASS_VOLUME 0.11f
#define ACCESSIBILITY_COMPASS_CLICK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.030f))
#define ACCESSIBILITY_COMPASS_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.065f))
#define ACCESSIBILITY_COMPASS_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.001f))
#define ACCESSIBILITY_COMPASS_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.010f))
#define ACCESSIBILITY_WEAPON_FUNCTION_FREQUENCY_HZ 1000.0f
#define ACCESSIBILITY_WEAPON_FUNCTION_VOLUME 0.10f
#define ACCESSIBILITY_WEAPON_FUNCTION_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_WEAPON_FUNCTION_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.030f))
#define ACCESSIBILITY_WEAPON_FUNCTION_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.002f))
#define ACCESSIBILITY_WEAPON_FUNCTION_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_HAZARD_VOLUME 0.14f
#define ACCESSIBILITY_COMBAT_CHIRP_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.004f))
#define ACCESSIBILITY_COMBAT_CHIRP_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.012f))
#define ACCESSIBILITY_COMBAT_FUNDAMENTAL_GAIN 0.78f
#define ACCESSIBILITY_COMBAT_OCTAVE_GAIN 0.22f
#define ACCESSIBILITY_TARGET_PRESENCE_DURATION_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.10f))
#define ACCESSIBILITY_TARGET_PRESENCE_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.004f))
#define ACCESSIBILITY_TARGET_PRESENCE_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.012f))
#define ACCESSIBILITY_THREAT_ALERT_START_FREQUENCY_HZ 1000.0f
#define ACCESSIBILITY_THREAT_ALERT_END_FREQUENCY_HZ 2000.0f
#define ACCESSIBILITY_THREAT_ALERT_DURATION_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.12f))
#define ACCESSIBILITY_THREAT_ALERT_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.004f))
#define ACCESSIBILITY_THREAT_ALERT_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.018f))
#define ACCESSIBILITY_TRACKER_VOLUME 0.065f
#define ACCESSIBILITY_TRACKER_LEVEL_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.045f))
#define ACCESSIBILITY_TRACKER_DOUBLE_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_TRACKER_DOUBLE_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_TRACKER_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_TRACKER_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.008f))
#define ACCESSIBILITY_TRACKER_REAR_MODULATION_HZ 30.0f
#define ACCESSIBILITY_FRIENDLY_BASE_FREQUENCY_HZ 440.0f
#define ACCESSIBILITY_FRIENDLY_THIRD_FREQUENCY_HZ 550.0f
#define ACCESSIBILITY_FRIENDLY_VOLUME 0.10f
#define ACCESSIBILITY_FRIENDLY_BASE_GAIN 0.72f
#define ACCESSIBILITY_FRIENDLY_THIRD_GAIN 0.28f
#define ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES ACCESSIBILITY_TONE_SAMPLE_RATE
#define ACCESSIBILITY_FRIENDLY_THIRD_PULSE_OFF_SAMPLES ACCESSIBILITY_TONE_SAMPLE_RATE
#define ACCESSIBILITY_FRIENDLY_THIRD_PULSE_EDGE_SAMPLES \
		((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f))
#define ACCESSIBILITY_DOOR_FREQUENCY_HZ 440.0f
#define ACCESSIBILITY_DOOR_VOLUME 0.24f
#define ACCESSIBILITY_DOOR_PERIOD_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.375f))
#define ACCESSIBILITY_DOOR_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.10f))
#define ACCESSIBILITY_DOOR_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_DOOR_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.02f))
#define ACCESSIBILITY_RADAR_LEVEL_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.045f))
#define ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_RADAR_DOUBLE_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_RADAR_LAUNCH_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.020f))
#define ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_RADAR_EMPTY_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.020f))
#define ACCESSIBILITY_RADAR_UNAVAILABLE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.080f))
#define ACCESSIBILITY_RADAR_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_RADAR_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.008f))
#define ACCESSIBILITY_RADAR_REAR_MODULATION_HZ 30.0f
#define ACCESSIBILITY_CANE_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.003f))
#define ACCESSIBILITY_CANE_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.008f))
#define ACCESSIBILITY_CANE_CROUCH_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.025f))
#define ACCESSIBILITY_CANE_PATH_WALL_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_CANE_PATH_MAX_STEPS 10
#define ACCESSIBILITY_MARKER_BASE_VOLUME 0.08f
#define ACCESSIBILITY_MARKER_CHIRP_VOLUME 0.12f
#define ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ 300.0f
#define ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ 600.0f
#define ACCESSIBILITY_MARKER_IDENTITY_FREQUENCY_HZ 800.0f
#define ACCESSIBILITY_TONE_MARKER_VOICE_COUNT \
	(ACCESSIBILITY_TONE_MARKER_SLOT_COUNT \
			+ ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT)
#define ACCESSIBILITY_MARKER_REMOVAL_FREQUENCY_HZ 400.0f
#define ACCESSIBILITY_MARKER_SWEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 2.0f))
#define ACCESSIBILITY_MARKER_PERIOD_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 2.0f))
#define ACCESSIBILITY_MARKER_BEEP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.035f))
#define ACCESSIBILITY_MARKER_GAP_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.075f))
#define ACCESSIBILITY_MARKER_ATTACK_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_MARKER_RELEASE_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.005f))
#define ACCESSIBILITY_MARKER_MIN_START_SPACING_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.50f))
#define ACCESSIBILITY_HILL_CHIRP_PERIOD_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 1.0f))
#define ACCESSIBILITY_HILL_SCORING_CHIRP_PERIOD_SAMPLES ((s32)(ACCESSIBILITY_TONE_SAMPLE_RATE * 0.5f))
#define ACCESSIBILITY_HILL_REAR_MODULATION_HZ 30.0f
#define ACCESSIBILITY_TONE_MIX_BUFFER_SAMPLES 2048
#define TWO_PI 6.28318530717958647692f

static SDL_atomic_t g_AccessibilityToneEnabled;
static SDL_atomic_t g_AccessibilityToneFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityTonePatternFlags;
static SDL_atomic_t g_AccessibilityTonePatternSequence;
static SDL_atomic_t g_AccessibilityChirpSequence;
static SDL_atomic_t g_AccessibilityChirpEnabled;
static SDL_atomic_t g_AccessibilityChirpFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityChirpVolumeMillionths;
static SDL_atomic_t g_AccessibilityChirpGainMillionths;
static SDL_atomic_t g_AccessibilityChirpPanMillionths;
static SDL_atomic_t g_AccessibilityChirpPulses;
static SDL_atomic_t g_AccessibilityTargetPresenceSequence;
static SDL_atomic_t g_AccessibilityTargetPresenceEnabled;
static SDL_atomic_t g_AccessibilityTargetPresenceFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityTargetPresenceVolumeMillionths;
static SDL_atomic_t g_AccessibilityTargetPresencePanMillionths;
static SDL_atomic_t g_AccessibilityThreatAlertSequence;
static SDL_atomic_t g_AccessibilityThreatAlertEnabled;
static SDL_atomic_t g_AccessibilityThreatAlertVolumeMillionths;
static SDL_atomic_t g_AccessibilityThreatAlertPanMillionths;
static SDL_atomic_t g_AccessibilityToggleSequence;
static SDL_atomic_t g_AccessibilityTogglePattern;
static SDL_atomic_t g_AccessibilityTogglePulses;
static SDL_atomic_t g_AccessibilityCompassSequence;
static SDL_atomic_t g_AccessibilityCompassEnabled;
static SDL_atomic_t g_AccessibilityCompassPulses;
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
static SDL_atomic_t g_AccessibilityCombatEndFrequencyMilliHz[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatVolumeMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatPanMillionths[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatPeriodMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatDurationMs[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatFrequencyContour[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCombatContinuous[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerEnabled[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerSequence[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerFrequencyMilliHz[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerVolumeMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerPanMillionths[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerPeriodMs[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerHeight[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityTrackerRear[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityFriendlyEnabled[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityFriendlySequence[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityFriendlyVolumeMillionths[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityFriendlyPanMillionths[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityFriendlyPulseThird[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityDoorEnabled[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityDoorSequence[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityDoorVolumeMillionths[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityDoorPanMillionths[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityRadarSequence;
static SDL_atomic_t g_AccessibilityRadarEnabled;
static SDL_atomic_t g_AccessibilityRadarFrequencyMilliHz;
static SDL_atomic_t g_AccessibilityRadarVolumeMillionths;
static SDL_atomic_t g_AccessibilityRadarPanMillionths;
static SDL_atomic_t g_AccessibilityRadarHeight;
static SDL_atomic_t g_AccessibilityRadarRear;
static SDL_atomic_t g_AccessibilityRadarKind;
static SDL_atomic_t g_AccessibilityHillSequence;
static SDL_atomic_t g_AccessibilityHillEnabled;
static SDL_atomic_t g_AccessibilityHillVolumeMillionths;
static SDL_atomic_t g_AccessibilityHillPanMillionths;
static SDL_atomic_t g_AccessibilityHillRear;
static SDL_atomic_t g_AccessibilityHillIdentityChirp;
static SDL_atomic_t g_AccessibilityHillScoring;
static SDL_atomic_t g_AccessibilityCaneEnabled[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneSequence[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneEndFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneVolumeMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePanMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCaneDurationMs[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePattern[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePathFloorCount[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePathCount[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityCanePathFrequencyMilliHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static SDL_atomic_t g_AccessibilityCanePathVolumeMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static SDL_atomic_t g_AccessibilityCanePathPanMillionths[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static SDL_atomic_t g_AccessibilityMarkerEnabled[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static SDL_atomic_t g_AccessibilityMarkerSequence[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static SDL_atomic_t g_AccessibilityMarkerVolumeMillionths[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static SDL_atomic_t g_AccessibilityMarkerPanMillionths[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static SDL_atomic_t g_AccessibilityMarkerRemovalSequence[ACCESSIBILITY_TONE_MARKER_SLOT_COUNT];
static SDL_atomic_t g_AccessibilityMarkerResetSequence;
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
static s32 g_AccessibilityToneObservedPatternSequence;
static s32 g_AccessibilityTonePatternSample;
static f32 g_AccessibilityTonePenetrablePhase;
static s32 g_AccessibilityChirpObservedSequence;
static s32 g_AccessibilityChirpSamplesRemaining;
static s32 g_AccessibilityChirpSample;
static s32 g_AccessibilityChirpPulseCount;
static f32 g_AccessibilityChirpPhase;
static f32 g_AccessibilityChirpFrequencyHz;
static f32 g_AccessibilityChirpVolume;
static f32 g_AccessibilityChirpGain;
static f32 g_AccessibilityChirpPan;
static s32 g_AccessibilityTargetPresenceObservedSequence;
static s32 g_AccessibilityTargetPresenceSamplesRemaining;
static s32 g_AccessibilityTargetPresenceSample;
static f32 g_AccessibilityTargetPresencePhase;
static f32 g_AccessibilityTargetPresenceFrequencyHz;
static f32 g_AccessibilityTargetPresenceVolume;
static f32 g_AccessibilityTargetPresencePan;
static s32 g_AccessibilityThreatAlertObservedSequence;
static s32 g_AccessibilityThreatAlertSamplesRemaining;
static s32 g_AccessibilityThreatAlertSample;
static f32 g_AccessibilityThreatAlertPhase;
static f32 g_AccessibilityThreatAlertVolume;
static f32 g_AccessibilityThreatAlertPan;
static s32 g_AccessibilityToggleObservedSequence;
static s32 g_AccessibilityToggleSamplesRemaining;
static s32 g_AccessibilityToggleSample;
static s32 g_AccessibilityToggleCurrentPattern;
static f32 g_AccessibilityTogglePhase;
static s32 g_AccessibilityCompassObservedSequence;
static s32 g_AccessibilityCompassSamplesRemaining;
static s32 g_AccessibilityCompassSample;
static f32 g_AccessibilityCompassPhase;
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
static s32 g_AccessibilityFriendlyObservedSequence[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static f32 g_AccessibilityFriendlyPhaseBase[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static f32 g_AccessibilityFriendlyPhaseThird[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static f32 g_AccessibilityFriendlyPan[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static f32 g_AccessibilityFriendlyGain[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static s32 g_AccessibilityFriendlyCycleSample[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
static s32 g_AccessibilityDoorObservedSequence[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static s32 g_AccessibilityDoorCycleSample;
static s32 g_AccessibilityDoorWaitForWindow[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static f32 g_AccessibilityDoorPan[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
static s32 g_AccessibilityRadarObservedSequence;
static s32 g_AccessibilityRadarSamplesRemaining;
static s32 g_AccessibilityRadarSample;
static s32 g_AccessibilityRadarTotalSamples;
static s32 g_AccessibilityRadarKindState;
static s32 g_AccessibilityRadarHeightState;
static s32 g_AccessibilityRadarRearState;
static f32 g_AccessibilityRadarFrequencyHz;
static f32 g_AccessibilityRadarVolume;
static f32 g_AccessibilityRadarPan;
static f32 g_AccessibilityRadarPhase;
static f32 g_AccessibilityRadarModulationPhase;
static s32 g_AccessibilityHillObservedSequence;
static s32 g_AccessibilityHillSweepSample;
static s32 g_AccessibilityHillChirpSample;
static f32 g_AccessibilityHillPhaseA;
static f32 g_AccessibilityHillPhaseB;
static f32 g_AccessibilityHillChirpPhase;
static f32 g_AccessibilityHillModulationPhase;
static f32 g_AccessibilityHillGain;
static f32 g_AccessibilityHillPan;
static s32 g_AccessibilityCaneObservedSequence[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCaneSamplesRemaining[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCaneSample[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCaneDurationSamples[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCanePatternState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCanePathFloorCountState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityCanePathCountState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCanePathFrequencyHzState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static f32 g_AccessibilityCanePathVolumeState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static f32 g_AccessibilityCanePathPanState[ACCESSIBILITY_TONE_CANE_SLOT_COUNT][ACCESSIBILITY_CANE_PATH_MAX_STEPS];
static f32 g_AccessibilityCanePhase[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCaneFrequencyHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCaneEndFrequencyHz[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCaneVolume[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static f32 g_AccessibilityCanePan[ACCESSIBILITY_TONE_CANE_SLOT_COUNT];
static s32 g_AccessibilityMarkerObservedSequence[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static s32 g_AccessibilityMarkerEnabledState[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static s32 g_AccessibilityMarkerDueSample[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static f32 g_AccessibilityMarkerPhaseA[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static f32 g_AccessibilityMarkerPhaseB[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static f32 g_AccessibilityMarkerPan[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static f32 g_AccessibilityMarkerGain[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
static s32 g_AccessibilityMarkerSweepSample;
static s32 g_AccessibilityMarkerIdentitySlot = -1;
static s32 g_AccessibilityMarkerIdentitySample;
static s32 g_AccessibilityMarkerIdentityRemoval;
static s32 g_AccessibilityMarkerIdentityCooldown;
static s32 g_AccessibilityMarkerIdentityCursor;
static f32 g_AccessibilityMarkerIdentityPhase;
static s32 g_AccessibilityMarkerObservedRemovalSequence[ACCESSIBILITY_TONE_MARKER_SLOT_COUNT];
static s32 g_AccessibilityMarkerObservedResetSequence;
static s32 g_AccessibilityMarkerPendingRemovalMask;

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

static f32 accessibilityToneCombatWave(f32 phase)
{
	return sinf(phase) * ACCESSIBILITY_COMBAT_FUNDAMENTAL_GAIN
			+ sinf(phase * 2.0f) * ACCESSIBILITY_COMBAT_OCTAVE_GAIN;
}

void accessibilityToneSet(s32 enabled, f32 frequencyhz)
{
	accessibilityToneSetAlignment(enabled, frequencyhz, 0);
}

void accessibilityToneSetAlignment(s32 enabled, f32 frequencyhz,
		s32 patternflags)
{
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
	accessibilityTonePlayChirpPattern(frequencyhz, volume, pan, 1, 1.0f);
}

void accessibilityTonePlayChirpPattern(f32 frequencyhz, f32 volume, f32 pan,
		s32 pulses, f32 gain)
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
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, volume > 0.0f);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityToneStopChirp(void)
{
	SDL_AtomicSet(&g_AccessibilityChirpEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityChirpSequence, 1);
}

void accessibilityTonePlayTargetPresence(f32 frequencyhz,
		f32 volume, f32 pan)
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
	SDL_AtomicSet(&g_AccessibilityTargetPresenceEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityTargetPresenceSequence, 1);
}

void accessibilityTonePlayThreatAlert(f32 volume, f32 pan)
{
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
	SDL_AtomicSet(&g_AccessibilityThreatAlertEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityThreatAlertSequence, 1);
}

void accessibilityTonePlayToggleConfirmation(s32 enabled)
{
	SDL_AtomicSet(&g_AccessibilityTogglePattern,
			enabled ? ACCESSIBILITY_TOGGLE_PATTERN_ON
					: ACCESSIBILITY_TOGGLE_PATTERN_OFF);
	SDL_AtomicSet(&g_AccessibilityTogglePulses, 2);
	SDL_AtomicAdd(&g_AccessibilityToggleSequence, 1);
}

void accessibilityTonePlayCaneModeConfirmation(s32 mode)
{
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
	SDL_AtomicSet(&g_AccessibilityCompassEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityCompassSequence, 1);
}

void accessibilityTonePlayStanceConfirmation(s32 crouchpos)
{
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

void accessibilityToneSetCombatSlot(s32 slot, s32 enabled,
		f32 startfrequencyhz, f32 endfrequencyhz, f32 volume, f32 pan,
		s32 periodms, s32 durationms, s32 frequencycontour, s32 continuous,
		s32 restart, s32 triggernow)
{
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

void accessibilityToneSetFriendlySlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 pulsethird, s32 restart)
{
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
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityFriendlyEnabled[slot], 0);
	}
}

void accessibilityToneSetDoorSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart)
{
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
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_DOOR_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityDoorEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityDoorSequence[slot], 1);
	}
}

void accessibilityTonePlayRadarPing(f32 frequencyhz, f32 volume, f32 pan,
		s32 height, s32 rear, s32 kind)
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
	SDL_AtomicSet(&g_AccessibilityRadarEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityRadarSequence, 1);
}

void accessibilityToneSetHillBeacon(s32 enabled, f32 volume, f32 pan,
		s32 rear, s32 identitychirp, s32 scoring, s32 restart)
{
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
	SDL_AtomicSet(&g_AccessibilityHillEnabled, 0);
	SDL_AtomicAdd(&g_AccessibilityHillSequence, 1);
}

void accessibilityTonePlayCaneSlot(s32 slot, f32 startfrequencyhz,
		f32 endfrequencyhz, f32 volume, f32 pan, s32 durationms,
		s32 pattern, s32 pathfloorcount, s32 pathcount,
		const f32 *pathfrequencieshz, const f32 *pathvolumes,
		const f32 *pathpans)
{
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
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityCaneEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityCaneSequence[slot], 1);
	}
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
	SDL_AtomicAdd(&g_AccessibilityCaneStops, 1);
#endif
}

void accessibilityToneSetMarkerSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart)
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
	SDL_AtomicSet(&g_AccessibilityMarkerEnabled[slot],
			enabled != 0 && volume > 0.0f);

	if (restart) {
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[slot], 1);
	}
}

void accessibilityTonePlayMarkerRemoval(s32 slot)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_MARKER_SLOT_COUNT) {
		return;
	}

	SDL_AtomicAdd(&g_AccessibilityMarkerRemovalSequence[slot], 1);
}

void accessibilityToneStopMarkers(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT; slot++) {
		SDL_AtomicSet(&g_AccessibilityMarkerEnabled[slot], 0);
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[slot], 1);
	}
	SDL_AtomicAdd(&g_AccessibilityMarkerResetSequence, 1);
}

void accessibilityToneSetLandmarkSlot(s32 slot, s32 enabled,
		f32 volume, f32 pan, s32 restart)
{
	if (slot < 0 || slot >= ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT) {
		return;
	}

	accessibilityToneSetMarkerSlot(
			ACCESSIBILITY_TONE_MARKER_SLOT_COUNT + slot,
			enabled, volume, pan, restart);
}

void accessibilityToneStopLandmarks(void)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_LANDMARK_SLOT_COUNT; slot++) {
		s32 voice = ACCESSIBILITY_TONE_MARKER_SLOT_COUNT + slot;

		SDL_AtomicSet(&g_AccessibilityMarkerEnabled[voice], 0);
		SDL_AtomicAdd(&g_AccessibilityMarkerSequence[voice], 1);
	}
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

const s16 *accessibilityToneMix(const s16 *input, u32 len)
{
	s32 enabled = SDL_AtomicGet(&g_AccessibilityToneEnabled);
	s32 tonepatternflags = SDL_AtomicGet(&g_AccessibilityTonePatternFlags);
	s32 tonepatternsequence = SDL_AtomicGet(
			&g_AccessibilityTonePatternSequence);
	s32 chirpsequence = SDL_AtomicGet(&g_AccessibilityChirpSequence);
	s32 targetpresencesequence = SDL_AtomicGet(
			&g_AccessibilityTargetPresenceSequence);
	s32 threatalertsequence = SDL_AtomicGet(
			&g_AccessibilityThreatAlertSequence);
	s32 togglesequence = SDL_AtomicGet(&g_AccessibilityToggleSequence);
	s32 compassequence = SDL_AtomicGet(&g_AccessibilityCompassSequence);
	s32 weaponfunctionsequence = SDL_AtomicGet(
			&g_AccessibilityWeaponFunctionSequence);
	s32 radarsequence = SDL_AtomicGet(&g_AccessibilityRadarSequence);
	s32 hillsequence = SDL_AtomicGet(&g_AccessibilityHillSequence);
	s32 hillenabled = SDL_AtomicGet(&g_AccessibilityHillEnabled);
	s32 hillidentitychirp = SDL_AtomicGet(
			&g_AccessibilityHillIdentityChirp);
	s32 hillscoring = SDL_AtomicGet(&g_AccessibilityHillScoring);
	s32 hillrear = SDL_AtomicGet(&g_AccessibilityHillRear);
	s32 hazardenabled = SDL_AtomicGet(&g_AccessibilityHazardEnabled);
	s32 combatenabled[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatfrequency[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatendfrequency[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatvolume[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	f32 combatpan[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatperiodsamples[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatdurationsamples[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatfrequencycontour[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 combatcontinuous[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
	s32 trackerenabled[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackerfrequency[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackervolume[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	f32 trackerpan[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerperiodsamples[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerheight[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 trackerrear[ACCESSIBILITY_TONE_TRACKER_SLOT_COUNT];
	s32 friendlyenabled[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
	s32 friendlypulsethird[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
	f32 friendlyvolume[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
	f32 friendlypan[ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT];
	s32 doorenabled[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
	f32 doorvolume[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
	f32 doorpan[ACCESSIBILITY_TONE_DOOR_SLOT_COUNT];
	s32 markerenabled[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
	f32 markervolume[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
	f32 markerpan[ACCESSIBILITY_TONE_MARKER_VOICE_COUNT];
	s32 anycombatenabled = 0;
	s32 anytrackerenabled = 0;
	s32 anyfriendlyenabled = 0;
	s32 anydoorenabled = 0;
	s32 anycaneactive = 0;
	s32 anymarkerenabled = 0;
	s32 markerresetsequence = SDL_AtomicGet(
			&g_AccessibilityMarkerResetSequence);
	f32 targetfrequency = (f32)SDL_AtomicGet(
			&g_AccessibilityToneFrequencyMilliHz) / 1000.0f;
	f32 targetgain = 0.0f;
	f32 hazardtargetfrequency = (f32)SDL_AtomicGet(
			&g_AccessibilityHazardFrequencyMilliHz) / 1000.0f;
	f32 hazardtargetgain = hazardenabled
			? (f32)SDL_AtomicGet(&g_AccessibilityHazardVolumeMillionths)
					/ 1000000.0f * ACCESSIBILITY_HAZARD_VOLUME
			: 0.0f;
	f32 hazardtargetpan = (f32)SDL_AtomicGet(
			&g_AccessibilityHazardPanMillionths) / 1000000.0f;
	f32 hilltargetgain = hillenabled
			? (f32)SDL_AtomicGet(&g_AccessibilityHillVolumeMillionths)
					/ 1000000.0f * ACCESSIBILITY_MARKER_BASE_VOLUME
			: 0.0f;
	f32 hilltargetpan = (f32)SDL_AtomicGet(
			&g_AccessibilityHillPanMillionths) / 1000000.0f;
	f32 combatmastervolume;
	f32 combatgainstep;
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

	if (enabled) {
		targetgain = tonepatternflags
				& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED
				? accessibilityGetInterruptedTargetingVolume()
				: accessibilityGetTargetingVolume();
	}

	if (tonepatternsequence
			!= g_AccessibilityToneObservedPatternSequence) {
		g_AccessibilityToneObservedPatternSequence = tonepatternsequence;
		g_AccessibilityTonePatternSample = 0;
		g_AccessibilityTonePenetrablePhase = 0.0f;
	}

	accessibilityGetEnemyTuning(NULL, NULL, NULL, &combatmastervolume);
	combatgainstep = (combatmastervolume > 0.01f
			? combatmastervolume : 0.01f)
			/ (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f);

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityCombatSequence[slot]);
		s32 triggersequence = SDL_AtomicGet(
				&g_AccessibilityCombatTriggerSequence[slot]);
		s32 offset;

		combatenabled[slot] = SDL_AtomicGet(&g_AccessibilityCombatEnabled[slot]);
		combatfrequency[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityCombatFrequencyMilliHz[slot]) / 1000.0f;
		combatendfrequency[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityCombatEndFrequencyMilliHz[slot]) / 1000.0f;
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
		combatfrequencycontour[slot] = SDL_AtomicGet(
				&g_AccessibilityCombatFrequencyContour[slot]);
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

	for (slot = 0; slot < ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(
				&g_AccessibilityFriendlySequence[slot]);

		friendlyenabled[slot] = SDL_AtomicGet(
				&g_AccessibilityFriendlyEnabled[slot]);
		friendlypulsethird[slot] = SDL_AtomicGet(
				&g_AccessibilityFriendlyPulseThird[slot]);
		friendlyvolume[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityFriendlyVolumeMillionths[slot])
				/ 1000000.0f;
		friendlypan[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityFriendlyPanMillionths[slot])
				/ 1000000.0f;
		anyfriendlyenabled |= friendlyenabled[slot]
				|| g_AccessibilityFriendlyGain[slot] > 0.0f;

		if (sequence != g_AccessibilityFriendlyObservedSequence[slot]) {
			g_AccessibilityFriendlyObservedSequence[slot] = sequence;
			g_AccessibilityFriendlyPhaseBase[slot] = 0.0f;
			g_AccessibilityFriendlyPhaseThird[slot] = 0.0f;
			g_AccessibilityFriendlyCycleSample[slot] = 0;
			g_AccessibilityFriendlyPan[slot] = friendlypan[slot];
			g_AccessibilityFriendlyGain[slot] = 0.0f;
		}
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_DOOR_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityDoorSequence[slot]);

		doorenabled[slot] = SDL_AtomicGet(&g_AccessibilityDoorEnabled[slot]);
		doorvolume[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityDoorVolumeMillionths[slot]) / 1000000.0f;
		doorpan[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityDoorPanMillionths[slot]) / 1000000.0f;
		anydoorenabled |= doorenabled[slot];

		if (sequence != g_AccessibilityDoorObservedSequence[slot]) {
			s32 sample = g_AccessibilityDoorCycleSample
					- slot * ACCESSIBILITY_DOOR_PERIOD_SAMPLES
						/ ACCESSIBILITY_TONE_DOOR_SLOT_COUNT;

			if (sample < 0) {
				sample += ACCESSIBILITY_DOOR_PERIOD_SAMPLES;
			}

			g_AccessibilityDoorObservedSequence[slot] = sequence;
			g_AccessibilityDoorWaitForWindow[slot]
					= doorenabled[slot]
						&& sample < ACCESSIBILITY_DOOR_BEEP_SAMPLES;
			g_AccessibilityDoorPan[slot] = doorpan[slot];
		}
	}

	if (radarsequence != g_AccessibilityRadarObservedSequence) {
		g_AccessibilityRadarObservedSequence = radarsequence;
		g_AccessibilityRadarSample = 0;
		g_AccessibilityRadarPhase = 0.0f;
		g_AccessibilityRadarModulationPhase = 0.0f;

		if (SDL_AtomicGet(&g_AccessibilityRadarEnabled)) {
			g_AccessibilityRadarFrequencyHz = (f32)SDL_AtomicGet(
					&g_AccessibilityRadarFrequencyMilliHz) / 1000.0f;
			g_AccessibilityRadarVolume = (f32)SDL_AtomicGet(
					&g_AccessibilityRadarVolumeMillionths) / 1000000.0f;
			g_AccessibilityRadarPan = (f32)SDL_AtomicGet(
					&g_AccessibilityRadarPanMillionths) / 1000000.0f;
			g_AccessibilityRadarHeightState = SDL_AtomicGet(
					&g_AccessibilityRadarHeight);
			g_AccessibilityRadarRearState = SDL_AtomicGet(
					&g_AccessibilityRadarRear);
			g_AccessibilityRadarKindState = SDL_AtomicGet(
					&g_AccessibilityRadarKind);

			if (g_AccessibilityRadarKindState
					== ACCESSIBILITY_TONE_RADAR_LAUNCH) {
				g_AccessibilityRadarTotalSamples
						= ACCESSIBILITY_RADAR_LAUNCH_SAMPLES;
			} else if (g_AccessibilityRadarKindState
					== ACCESSIBILITY_TONE_RADAR_EMPTY) {
				g_AccessibilityRadarTotalSamples
						= ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES * 2
							+ ACCESSIBILITY_RADAR_EMPTY_GAP_SAMPLES;
			} else if (g_AccessibilityRadarKindState
					== ACCESSIBILITY_TONE_RADAR_UNAVAILABLE) {
				g_AccessibilityRadarTotalSamples
						= ACCESSIBILITY_RADAR_UNAVAILABLE_SAMPLES;
			} else if (g_AccessibilityRadarHeightState == 0) {
				g_AccessibilityRadarTotalSamples
						= ACCESSIBILITY_RADAR_LEVEL_BEEP_SAMPLES;
			} else {
				g_AccessibilityRadarTotalSamples
						= ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES * 2
							+ ACCESSIBILITY_RADAR_DOUBLE_GAP_SAMPLES;
			}
			g_AccessibilityRadarSamplesRemaining
					= g_AccessibilityRadarTotalSamples;
		} else {
			g_AccessibilityRadarSamplesRemaining = 0;
			g_AccessibilityRadarTotalSamples = 0;
		}
	}

	if (hillsequence != g_AccessibilityHillObservedSequence) {
		g_AccessibilityHillObservedSequence = hillsequence;
		g_AccessibilityHillSweepSample = 0;
		g_AccessibilityHillChirpSample = 0;
		g_AccessibilityHillPhaseA = 0.0f;
		g_AccessibilityHillPhaseB = 0.0f;
		g_AccessibilityHillChirpPhase = 0.0f;
		g_AccessibilityHillModulationPhase = 0.0f;
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityCaneSequence[slot]);

		if (sequence != g_AccessibilityCaneObservedSequence[slot]) {
			g_AccessibilityCaneObservedSequence[slot] = sequence;

			if (SDL_AtomicGet(&g_AccessibilityCaneEnabled[slot])) {
				g_AccessibilityCaneFrequencyHz[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCaneFrequencyMilliHz[slot]) / 1000.0f;
				g_AccessibilityCaneEndFrequencyHz[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCaneEndFrequencyMilliHz[slot])
						/ 1000.0f;
				g_AccessibilityCaneVolume[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCaneVolumeMillionths[slot]) / 1000000.0f;
				g_AccessibilityCanePan[slot] = (f32)SDL_AtomicGet(
						&g_AccessibilityCanePanMillionths[slot]) / 1000000.0f;
				g_AccessibilityCaneDurationSamples[slot]
						= (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE
								* (f32)SDL_AtomicGet(
										&g_AccessibilityCaneDurationMs[slot])
								/ 1000.0f);
				if (g_AccessibilityCaneDurationSamples[slot] < 1) {
					g_AccessibilityCaneDurationSamples[slot] = 1;
				}
				g_AccessibilityCanePatternState[slot] = SDL_AtomicGet(
						&g_AccessibilityCanePattern[slot]);
				g_AccessibilityCanePathFloorCountState[slot] = SDL_AtomicGet(
						&g_AccessibilityCanePathFloorCount[slot]);
				g_AccessibilityCanePathCountState[slot] = SDL_AtomicGet(
						&g_AccessibilityCanePathCount[slot]);
				for (i = 0; i < g_AccessibilityCanePathCountState[slot]; i++) {
					g_AccessibilityCanePathFrequencyHzState[slot][i]
							= (f32)SDL_AtomicGet(
									&g_AccessibilityCanePathFrequencyMilliHz[slot][i])
								/ 1000.0f;
					g_AccessibilityCanePathVolumeState[slot][i]
							= (f32)SDL_AtomicGet(
									&g_AccessibilityCanePathVolumeMillionths[slot][i])
								/ 1000000.0f;
					g_AccessibilityCanePathPanState[slot][i]
							= (f32)SDL_AtomicGet(
									&g_AccessibilityCanePathPanMillionths[slot][i])
								/ 1000000.0f;
				}
				g_AccessibilityCaneSamplesRemaining[slot]
						= g_AccessibilityCaneDurationSamples[slot];
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

	for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_VOICE_COUNT; slot++) {
		s32 sequence = SDL_AtomicGet(&g_AccessibilityMarkerSequence[slot]);

		markerenabled[slot] = SDL_AtomicGet(
				&g_AccessibilityMarkerEnabled[slot]);
		markervolume[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityMarkerVolumeMillionths[slot]) / 1000000.0f;
		markerpan[slot] = (f32)SDL_AtomicGet(
				&g_AccessibilityMarkerPanMillionths[slot]) / 1000000.0f;
		anymarkerenabled |= markerenabled[slot]
				|| g_AccessibilityMarkerGain[slot] > 0.0f;

		if (sequence != g_AccessibilityMarkerObservedSequence[slot]) {
			g_AccessibilityMarkerObservedSequence[slot] = sequence;
			g_AccessibilityMarkerDueSample[slot]
					= slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT
							&& markerenabled[slot] ? 0 : -1;
			g_AccessibilityMarkerEnabledState[slot] = markerenabled[slot];
			if (markerenabled[slot]) {
				g_AccessibilityMarkerPan[slot] = markerpan[slot];
			}
		} else if (!markerenabled[slot]) {
			g_AccessibilityMarkerDueSample[slot] = -1;
			g_AccessibilityMarkerEnabledState[slot] = 0;
		} else if (!g_AccessibilityMarkerEnabledState[slot]) {
			g_AccessibilityMarkerEnabledState[slot] = 1;
			if (slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT) {
				g_AccessibilityMarkerDueSample[slot]
						= ACCESSIBILITY_MARKER_PERIOD_SAMPLES
							* slot / ACCESSIBILITY_TONE_MARKER_SLOT_COUNT;
			}
			g_AccessibilityMarkerPan[slot] = markerpan[slot];
		}
	}

	for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT; slot++) {
		s32 removalsequence = SDL_AtomicGet(
				&g_AccessibilityMarkerRemovalSequence[slot]);

		if (removalsequence
				!= g_AccessibilityMarkerObservedRemovalSequence[slot]) {
			g_AccessibilityMarkerObservedRemovalSequence[slot]
					= removalsequence;
			g_AccessibilityMarkerPendingRemovalMask |= 1 << slot;
		}
	}

	if (markerresetsequence != g_AccessibilityMarkerObservedResetSequence) {
		g_AccessibilityMarkerObservedResetSequence = markerresetsequence;
		g_AccessibilityMarkerPendingRemovalMask = 0;
		g_AccessibilityMarkerIdentitySlot = -1;
		g_AccessibilityMarkerIdentityRemoval = 0;
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
			g_AccessibilityChirpGain = (f32)SDL_AtomicGet(
					&g_AccessibilityChirpGainMillionths) / 1000000.0f;
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

	if (targetpresencesequence
			!= g_AccessibilityTargetPresenceObservedSequence) {
		g_AccessibilityTargetPresenceObservedSequence
				= targetpresencesequence;

		if (SDL_AtomicGet(&g_AccessibilityTargetPresenceEnabled)) {
			g_AccessibilityTargetPresenceFrequencyHz
					= (f32)SDL_AtomicGet(
							&g_AccessibilityTargetPresenceFrequencyMilliHz)
						/ 1000.0f;
			g_AccessibilityTargetPresenceVolume
					= (f32)SDL_AtomicGet(
							&g_AccessibilityTargetPresenceVolumeMillionths)
						/ 1000000.0f;
			g_AccessibilityTargetPresencePan
					= (f32)SDL_AtomicGet(
							&g_AccessibilityTargetPresencePanMillionths)
						/ 1000000.0f;
			g_AccessibilityTargetPresenceSamplesRemaining
					= ACCESSIBILITY_TARGET_PRESENCE_DURATION_SAMPLES;
			g_AccessibilityTargetPresenceSample = 0;
			g_AccessibilityTargetPresencePhase = 0.0f;
		} else {
			g_AccessibilityTargetPresenceSamplesRemaining = 0;
		}
	}

	if (threatalertsequence != g_AccessibilityThreatAlertObservedSequence) {
		g_AccessibilityThreatAlertObservedSequence = threatalertsequence;

		if (SDL_AtomicGet(&g_AccessibilityThreatAlertEnabled)) {
			g_AccessibilityThreatAlertVolume
					= (f32)SDL_AtomicGet(
							&g_AccessibilityThreatAlertVolumeMillionths)
						/ 1000000.0f;
			g_AccessibilityThreatAlertPan
					= (f32)SDL_AtomicGet(
							&g_AccessibilityThreatAlertPanMillionths)
						/ 1000000.0f;
			g_AccessibilityThreatAlertSamplesRemaining
					= ACCESSIBILITY_THREAT_ALERT_DURATION_SAMPLES;
			g_AccessibilityThreatAlertSample = 0;
			g_AccessibilityThreatAlertPhase = 0.0f;
		} else {
			g_AccessibilityThreatAlertSamplesRemaining = 0;
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

	if (togglesequence != g_AccessibilityToggleObservedSequence) {
		s32 pulses = SDL_AtomicGet(&g_AccessibilityTogglePulses);

		g_AccessibilityToggleObservedSequence = togglesequence;
		g_AccessibilityToggleCurrentPattern = SDL_AtomicGet(
				&g_AccessibilityTogglePattern);
		if (pulses < 1 || pulses > 3) {
			pulses = 2;
		}
		g_AccessibilityToggleSample = 0;
		g_AccessibilityTogglePhase = 0.0f;
		g_AccessibilityToggleSamplesRemaining
				= ACCESSIBILITY_TOGGLE_BEEP_SAMPLES * pulses
					+ ACCESSIBILITY_TOGGLE_GAP_SAMPLES * (pulses - 1);
	}

	if (compassequence != g_AccessibilityCompassObservedSequence) {
		s32 pulses = SDL_AtomicGet(&g_AccessibilityCompassPulses);

		g_AccessibilityCompassObservedSequence = compassequence;
		g_AccessibilityCompassSample = 0;
		g_AccessibilityCompassPhase = 0.0f;
		g_AccessibilityCompassSamplesRemaining
				= SDL_AtomicGet(&g_AccessibilityCompassEnabled) && pulses > 0
				? ACCESSIBILITY_COMPASS_CLICK_SAMPLES * pulses
						+ ACCESSIBILITY_COMPASS_GAP_SAMPLES * (pulses - 1)
				: 0;
	}

	if (!enabled && g_AccessibilityToneGain <= 0.0f
			&& !hazardenabled && g_AccessibilityHazardGain <= 0.0f
			&& g_AccessibilityChirpSamplesRemaining <= 0
			&& g_AccessibilityTargetPresenceSamplesRemaining <= 0
			&& g_AccessibilityThreatAlertSamplesRemaining <= 0
			&& g_AccessibilityToggleSamplesRemaining <= 0
			&& g_AccessibilityCompassSamplesRemaining <= 0
			&& g_AccessibilityWeaponFunctionSamplesRemaining <= 0
			&& g_AccessibilityRadarSamplesRemaining <= 0
			&& !hillenabled && g_AccessibilityHillGain <= 0.0f
			&& !anycombatenabled && !anytrackerenabled
			&& !anyfriendlyenabled && !anydoorenabled && !anycaneactive
			&& !anymarkerenabled
			&& g_AccessibilityMarkerIdentitySlot < 0
			&& g_AccessibilityMarkerPendingRemovalMask == 0) {
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
		f32 alignmentenvelope = 1.0f;
		s32 chirpleft = 0;
		s32 chirpright = 0;
		s32 targetpresenceleft = 0;
		s32 targetpresenceright = 0;
		s32 threatalertleft = 0;
		s32 threatalertright = 0;
		s32 hazardleft = 0;
		s32 hazardright = 0;
		s32 toggletone = 0;
		s32 compasstone = 0;
		s32 weaponfunctiontone = 0;
		s32 combatleft = 0;
		s32 combatright = 0;
		s32 trackerleft = 0;
		s32 trackerright = 0;
		s32 friendlyleft = 0;
		s32 friendlyright = 0;
		s32 doorleft = 0;
		s32 doorright = 0;
		s32 radarleft = 0;
		s32 radarright = 0;
		s32 hillleft = 0;
		s32 hillright = 0;
		s32 caneleft = 0;
		s32 caneright = 0;
		s32 markerleft = 0;
		s32 markerright = 0;
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

		if (enabled && tonepatternflags) {
			if ((tonepatternflags
					& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED)
					&& g_AccessibilityTonePatternSample
					>= ACCESSIBILITY_ALIGNMENT_ON_SAMPLES) {
				alignmentenvelope = 0.0f;
			} else if ((tonepatternflags
					& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED)
					&& g_AccessibilityTonePatternSample
					< ACCESSIBILITY_ALIGNMENT_EDGE_SAMPLES) {
				alignmentenvelope
						= (f32)g_AccessibilityTonePatternSample
							/ (f32)ACCESSIBILITY_ALIGNMENT_EDGE_SAMPLES;
			} else if ((tonepatternflags
					& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED)
					&& ACCESSIBILITY_ALIGNMENT_ON_SAMPLES
					- g_AccessibilityTonePatternSample
						<= ACCESSIBILITY_ALIGNMENT_EDGE_SAMPLES) {
				alignmentenvelope
						= (f32)(ACCESSIBILITY_ALIGNMENT_ON_SAMPLES
							- g_AccessibilityTonePatternSample)
							/ (f32)ACCESSIBILITY_ALIGNMENT_EDGE_SAMPLES;
			}

			if (tonepatternflags
					& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_PENETRABLE) {
				f32 modulation = 0.5f
						+ 0.5f * sinf(g_AccessibilityTonePenetrablePhase);
				alignmentenvelope *= ACCESSIBILITY_ALIGNMENT_PENETRABLE_MIN_GAIN
						+ (1.0f - ACCESSIBILITY_ALIGNMENT_PENETRABLE_MIN_GAIN)
							* modulation;
				g_AccessibilityTonePenetrablePhase += TWO_PI
						* ACCESSIBILITY_ALIGNMENT_PENETRABLE_MODULATION_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityTonePenetrablePhase >= TWO_PI) {
					g_AccessibilityTonePenetrablePhase -= TWO_PI;
				}
			}

			g_AccessibilityTonePatternSample++;
			if ((tonepatternflags
					& ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED)
					&& g_AccessibilityTonePatternSample
					>= ACCESSIBILITY_ALIGNMENT_CYCLE_SAMPLES) {
				g_AccessibilityTonePatternSample = 0;
			} else if (g_AccessibilityTonePatternSample
					>= (s32)ACCESSIBILITY_TONE_SAMPLE_RATE) {
				g_AccessibilityTonePatternSample = 0;
			}
		}

		tone = (s32)(sinf(g_AccessibilityTonePhase)
				* g_AccessibilityToneGain * alignmentenvelope * 32767.0f);

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
					* g_AccessibilityChirpVolume * g_AccessibilityChirpGain
					* ACCESSIBILITY_CHIRP_VOLUME
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

		if (g_AccessibilityTargetPresenceSamplesRemaining > 0) {
			f32 envelope = 1.0f;
			f32 leftpan = g_AccessibilityTargetPresencePan > 0.0f
					? 1.0f - g_AccessibilityTargetPresencePan : 1.0f;
			f32 rightpan = g_AccessibilityTargetPresencePan < 0.0f
					? 1.0f + g_AccessibilityTargetPresencePan : 1.0f;
			f32 presence;

			if (g_AccessibilityTargetPresenceSample
					< ACCESSIBILITY_TARGET_PRESENCE_ATTACK_SAMPLES) {
				envelope = (f32)g_AccessibilityTargetPresenceSample
						/ (f32)ACCESSIBILITY_TARGET_PRESENCE_ATTACK_SAMPLES;
			} else if (g_AccessibilityTargetPresenceSamplesRemaining
					< ACCESSIBILITY_TARGET_PRESENCE_RELEASE_SAMPLES) {
				envelope = (f32)g_AccessibilityTargetPresenceSamplesRemaining
						/ (f32)ACCESSIBILITY_TARGET_PRESENCE_RELEASE_SAMPLES;
			}

			presence = accessibilityToneCombatWave(
					g_AccessibilityTargetPresencePhase) * envelope
					* g_AccessibilityTargetPresenceVolume
					* combatmastervolume * 32767.0f;
			targetpresenceleft = (s32)(presence * leftpan);
			targetpresenceright = (s32)(presence * rightpan);
			g_AccessibilityTargetPresencePhase += TWO_PI
					* g_AccessibilityTargetPresenceFrequencyHz
					/ ACCESSIBILITY_TONE_SAMPLE_RATE;
			if (g_AccessibilityTargetPresencePhase >= TWO_PI) {
				g_AccessibilityTargetPresencePhase -= TWO_PI;
			}
			g_AccessibilityTargetPresenceSample++;
			g_AccessibilityTargetPresenceSamplesRemaining--;
		}

		if (g_AccessibilityThreatAlertSamplesRemaining > 0) {
			f32 envelope = 1.0f;
			f32 progress = (f32)g_AccessibilityThreatAlertSample
					/ (f32)ACCESSIBILITY_THREAT_ALERT_DURATION_SAMPLES;
			f32 frequencyhz = ACCESSIBILITY_THREAT_ALERT_START_FREQUENCY_HZ
					+ (ACCESSIBILITY_THREAT_ALERT_END_FREQUENCY_HZ
						- ACCESSIBILITY_THREAT_ALERT_START_FREQUENCY_HZ)
						* progress;
			f32 leftpan = g_AccessibilityThreatAlertPan > 0.0f
					? 1.0f - g_AccessibilityThreatAlertPan : 1.0f;
			f32 rightpan = g_AccessibilityThreatAlertPan < 0.0f
					? 1.0f + g_AccessibilityThreatAlertPan : 1.0f;
			f32 alert;

			if (g_AccessibilityThreatAlertSample
					< ACCESSIBILITY_THREAT_ALERT_ATTACK_SAMPLES) {
				envelope = (f32)g_AccessibilityThreatAlertSample
						/ (f32)ACCESSIBILITY_THREAT_ALERT_ATTACK_SAMPLES;
			} else if (g_AccessibilityThreatAlertSamplesRemaining
					< ACCESSIBILITY_THREAT_ALERT_RELEASE_SAMPLES) {
				envelope = (f32)g_AccessibilityThreatAlertSamplesRemaining
						/ (f32)ACCESSIBILITY_THREAT_ALERT_RELEASE_SAMPLES;
			}

			alert = accessibilityToneCombatWave(
					g_AccessibilityThreatAlertPhase) * envelope
					* g_AccessibilityThreatAlertVolume
					* combatmastervolume * 32767.0f;
			threatalertleft = (s32)(alert * leftpan);
			threatalertright = (s32)(alert * rightpan);
			g_AccessibilityThreatAlertPhase += TWO_PI * frequencyhz
					/ ACCESSIBILITY_TONE_SAMPLE_RATE;
			if (g_AccessibilityThreatAlertPhase >= TWO_PI) {
				g_AccessibilityThreatAlertPhase -= TWO_PI;
			}
			g_AccessibilityThreatAlertSample++;
			g_AccessibilityThreatAlertSamplesRemaining--;
		}

		if (g_AccessibilityToggleSamplesRemaining > 0) {
			s32 cyclelength = ACCESSIBILITY_TOGGLE_BEEP_SAMPLES
					+ ACCESSIBILITY_TOGGLE_GAP_SAMPLES;
			s32 beepindex = g_AccessibilityToggleSample / cyclelength;
			s32 beepsample = g_AccessibilityToggleSample % cyclelength;

			if (beepsample < ACCESSIBILITY_TOGGLE_BEEP_SAMPLES) {
				f32 envelope = 1.0f;
				f32 frequencyhz = beepindex == 0
							? ACCESSIBILITY_TOGGLE_BASE_FREQUENCY_HZ
							: g_AccessibilityToggleCurrentPattern
									== ACCESSIBILITY_TOGGLE_PATTERN_OFF
										? ACCESSIBILITY_TOGGLE_OFF_FREQUENCY_HZ
										: ACCESSIBILITY_TOGGLE_ON_FREQUENCY_HZ;

				if (beepsample < ACCESSIBILITY_TOGGLE_ATTACK_SAMPLES) {
					envelope = (f32)beepsample
							/ (f32)ACCESSIBILITY_TOGGLE_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_TOGGLE_BEEP_SAMPLES - beepsample
						< ACCESSIBILITY_TOGGLE_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_TOGGLE_BEEP_SAMPLES
							- beepsample)
							/ (f32)ACCESSIBILITY_TOGGLE_RELEASE_SAMPLES;
				}

				toggletone = (s32)(sinf(g_AccessibilityTogglePhase)
						* envelope * ACCESSIBILITY_TOGGLE_VOLUME * 32767.0f);
				g_AccessibilityTogglePhase += TWO_PI * frequencyhz
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityTogglePhase >= TWO_PI) {
					g_AccessibilityTogglePhase -= TWO_PI;
				}
			} else {
				g_AccessibilityTogglePhase = 0.0f;
			}

			g_AccessibilityToggleSample++;
			g_AccessibilityToggleSamplesRemaining--;
		}

		if (g_AccessibilityCompassSamplesRemaining > 0) {
			s32 cyclelength = ACCESSIBILITY_COMPASS_CLICK_SAMPLES
					+ ACCESSIBILITY_COMPASS_GAP_SAMPLES;
			s32 clicksample = g_AccessibilityCompassSample % cyclelength;

			if (clicksample < ACCESSIBILITY_COMPASS_CLICK_SAMPLES) {
				f32 envelope = 1.0f;
				f32 progress = (f32)clicksample
						/ (f32)ACCESSIBILITY_COMPASS_CLICK_SAMPLES;
				f32 wave;

				if (clicksample < ACCESSIBILITY_COMPASS_ATTACK_SAMPLES) {
					envelope = (f32)clicksample
							/ (f32)ACCESSIBILITY_COMPASS_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_COMPASS_CLICK_SAMPLES - clicksample
						< ACCESSIBILITY_COMPASS_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_COMPASS_CLICK_SAMPLES
							- clicksample)
							/ (f32)ACCESSIBILITY_COMPASS_RELEASE_SAMPLES;
				}

				wave = sinf(g_AccessibilityCompassPhase) * 0.78f
						+ sinf(g_AccessibilityCompassPhase * 2.0f) * 0.22f;
				compasstone = (s32)(wave * envelope
						* (1.0f - progress * 0.35f)
						* ACCESSIBILITY_COMPASS_VOLUME * 32767.0f);
				g_AccessibilityCompassPhase += TWO_PI
						* ACCESSIBILITY_COMPASS_FREQUENCY_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityCompassPhase >= TWO_PI) {
					g_AccessibilityCompassPhase -= TWO_PI;
				}
			} else {
				g_AccessibilityCompassPhase = 0.0f;
			}

			g_AccessibilityCompassSample++;
			g_AccessibilityCompassSamplesRemaining--;
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
					? combatvolume[slot] * combatmastervolume
					: 0.0f;

			g_AccessibilityCombatPan[slot] += (combatpan[slot]
					- g_AccessibilityCombatPan[slot]) / (f32)(frames - i);

			if (g_AccessibilityCombatGain[slot] < targetcombatgain) {
				g_AccessibilityCombatGain[slot] += combatgainstep;
				if (g_AccessibilityCombatGain[slot] > targetcombatgain) {
					g_AccessibilityCombatGain[slot] = targetcombatgain;
				}
			} else if (g_AccessibilityCombatGain[slot] > targetcombatgain) {
				g_AccessibilityCombatGain[slot] -= combatgainstep;
				if (g_AccessibilityCombatGain[slot] < targetcombatgain) {
					g_AccessibilityCombatGain[slot] = targetcombatgain;
				}
			}

			if (g_AccessibilityCombatGain[slot] > 0.0f) {
				f32 leftpan = g_AccessibilityCombatPan[slot] > 0.0f
						? 1.0f - g_AccessibilityCombatPan[slot] : 1.0f;
				f32 rightpan = g_AccessibilityCombatPan[slot] < 0.0f
						? 1.0f + g_AccessibilityCombatPan[slot] : 1.0f;
				f32 combat = accessibilityToneCombatWave(
						g_AccessibilityCombatPhase[slot])
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
					f32 frequency = combatfrequency[slot];
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

					if (combatfrequencycontour[slot]
							== ACCESSIBILITY_TONE_COMBAT_CONTOUR_BASE_THEN_END) {
						if (sample >= combatdurationsamples[slot] / 2) {
							frequency = combatendfrequency[slot];
						}
					} else if (combatdurationsamples[slot] > 1) {
						f32 progress = (f32)sample
								/ (f32)(combatdurationsamples[slot] - 1);

						frequency += (combatendfrequency[slot]
								- combatfrequency[slot]) * progress;
					}

					combat = accessibilityToneCombatWave(
							g_AccessibilityCombatPhase[slot]) * envelope
							* combatvolume[slot] * combatmastervolume
							* 32767.0f;
					combatleft += (s32)(combat * leftpan);
					combatright += (s32)(combat * rightpan);
					g_AccessibilityCombatPhase[slot] += TWO_PI
							* frequency / ACCESSIBILITY_TONE_SAMPLE_RATE;

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

		for (slot = 0; slot < ACCESSIBILITY_TONE_FRIENDLY_SLOT_COUNT; slot++) {
			f32 targetfriendlygain = friendlyenabled[slot]
					? friendlyvolume[slot] * ACCESSIBILITY_FRIENDLY_VOLUME
					: 0.0f;
			f32 gainstep = ACCESSIBILITY_FRIENDLY_VOLUME * 2.0f
					/ (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f);
			f32 leftpan;
			f32 rightpan;
			f32 thirdgain = ACCESSIBILITY_FRIENDLY_THIRD_GAIN;
			f32 wave;
			f32 friendly;

			g_AccessibilityFriendlyPan[slot] += (friendlypan[slot]
					- g_AccessibilityFriendlyPan[slot])
					/ (f32)(frames - i);

			if (g_AccessibilityFriendlyGain[slot] < targetfriendlygain) {
				g_AccessibilityFriendlyGain[slot] += gainstep;
				if (g_AccessibilityFriendlyGain[slot] > targetfriendlygain) {
					g_AccessibilityFriendlyGain[slot] = targetfriendlygain;
				}
			} else if (g_AccessibilityFriendlyGain[slot]
					> targetfriendlygain) {
				g_AccessibilityFriendlyGain[slot] -= gainstep;
				if (g_AccessibilityFriendlyGain[slot] < targetfriendlygain) {
					g_AccessibilityFriendlyGain[slot] = targetfriendlygain;
				}
			}

			if (g_AccessibilityFriendlyGain[slot] <= 0.0f) {
				continue;
			}

			leftpan = g_AccessibilityFriendlyPan[slot] > 0.0f
					? 1.0f - g_AccessibilityFriendlyPan[slot] : 1.0f;
			rightpan = g_AccessibilityFriendlyPan[slot] < 0.0f
					? 1.0f + g_AccessibilityFriendlyPan[slot] : 1.0f;

			if (friendlypulsethird[slot]) {
				s32 sample = g_AccessibilityFriendlyCycleSample[slot];

				if (sample < ACCESSIBILITY_FRIENDLY_THIRD_PULSE_EDGE_SAMPLES) {
					thirdgain *= (f32)sample
							/ ACCESSIBILITY_FRIENDLY_THIRD_PULSE_EDGE_SAMPLES;
				} else if (sample >= ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES
						- ACCESSIBILITY_FRIENDLY_THIRD_PULSE_EDGE_SAMPLES
						&& sample < ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES) {
					thirdgain *= (f32)(ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES
							- sample)
							/ ACCESSIBILITY_FRIENDLY_THIRD_PULSE_EDGE_SAMPLES;
				} else if (sample >= ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES) {
					thirdgain = 0.0f;
				}
			}

			wave = sinf(g_AccessibilityFriendlyPhaseBase[slot])
						* ACCESSIBILITY_FRIENDLY_BASE_GAIN
					+ sinf(g_AccessibilityFriendlyPhaseThird[slot])
						* thirdgain;
			friendly = wave * g_AccessibilityFriendlyGain[slot] * 32767.0f;
			friendlyleft += (s32)(friendly * leftpan);
			friendlyright += (s32)(friendly * rightpan);

			g_AccessibilityFriendlyPhaseBase[slot] += TWO_PI
					* ACCESSIBILITY_FRIENDLY_BASE_FREQUENCY_HZ
					/ ACCESSIBILITY_TONE_SAMPLE_RATE;
			g_AccessibilityFriendlyPhaseThird[slot] += TWO_PI
					* ACCESSIBILITY_FRIENDLY_THIRD_FREQUENCY_HZ
					/ ACCESSIBILITY_TONE_SAMPLE_RATE;
			if (g_AccessibilityFriendlyPhaseBase[slot] >= TWO_PI) {
				g_AccessibilityFriendlyPhaseBase[slot] -= TWO_PI;
			}
			if (g_AccessibilityFriendlyPhaseThird[slot] >= TWO_PI) {
				g_AccessibilityFriendlyPhaseThird[slot] -= TWO_PI;
			}

			if (friendlyenabled[slot] && friendlypulsethird[slot]) {
				g_AccessibilityFriendlyCycleSample[slot]++;
				if (g_AccessibilityFriendlyCycleSample[slot]
						>= ACCESSIBILITY_FRIENDLY_THIRD_PULSE_ON_SAMPLES
							+ ACCESSIBILITY_FRIENDLY_THIRD_PULSE_OFF_SAMPLES) {
					g_AccessibilityFriendlyCycleSample[slot] = 0;
				}
			} else if (!friendlypulsethird[slot]) {
				g_AccessibilityFriendlyCycleSample[slot] = 0;
			}
		}

		for (slot = 0; slot < ACCESSIBILITY_TONE_DOOR_SLOT_COUNT; slot++) {
			s32 sample = g_AccessibilityDoorCycleSample
					- slot * ACCESSIBILITY_DOOR_PERIOD_SAMPLES
						/ ACCESSIBILITY_TONE_DOOR_SLOT_COUNT;
			f32 envelope = 1.0f;
			f32 phase;
			f32 leftpan;
			f32 rightpan;
			f32 door;

			if (sample < 0) {
				sample += ACCESSIBILITY_DOOR_PERIOD_SAMPLES;
			}

			if (!doorenabled[slot]) {
				continue;
			}

			g_AccessibilityDoorPan[slot] += (doorpan[slot]
					- g_AccessibilityDoorPan[slot]) / (f32)(frames - i);

			if (g_AccessibilityDoorWaitForWindow[slot]) {
				if (sample < ACCESSIBILITY_DOOR_BEEP_SAMPLES) {
					continue;
				}

				g_AccessibilityDoorWaitForWindow[slot] = 0;
			}

			if (sample < ACCESSIBILITY_DOOR_BEEP_SAMPLES) {
				if (sample < ACCESSIBILITY_DOOR_ATTACK_SAMPLES) {
					envelope = (f32)sample
							/ (f32)ACCESSIBILITY_DOOR_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_DOOR_BEEP_SAMPLES - sample
						< ACCESSIBILITY_DOOR_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_DOOR_BEEP_SAMPLES - sample)
							/ (f32)ACCESSIBILITY_DOOR_RELEASE_SAMPLES;
				}

				leftpan = g_AccessibilityDoorPan[slot] > 0.0f
						? 1.0f - g_AccessibilityDoorPan[slot] : 1.0f;
				rightpan = g_AccessibilityDoorPan[slot] < 0.0f
						? 1.0f + g_AccessibilityDoorPan[slot] : 1.0f;
				phase = TWO_PI * ACCESSIBILITY_DOOR_FREQUENCY_HZ
						* sample / ACCESSIBILITY_TONE_SAMPLE_RATE;
				door = sinf(phase) * envelope
						* doorvolume[slot] * ACCESSIBILITY_DOOR_VOLUME
						* 32767.0f;
				doorleft += (s32)(door * leftpan);
				doorright += (s32)(door * rightpan);
			}
		}

		g_AccessibilityDoorCycleSample++;
		if (g_AccessibilityDoorCycleSample
				>= ACCESSIBILITY_DOOR_PERIOD_SAMPLES) {
			g_AccessibilityDoorCycleSample = 0;
		}

		if (g_AccessibilityRadarSamplesRemaining > 0) {
			s32 sample = g_AccessibilityRadarSample;
			s32 beepsample = -1;
			s32 beeplength = g_AccessibilityRadarTotalSamples;
			f32 frequencymultiplier = 1.0f;
			f32 envelope = 1.0f;
			f32 leftpan = g_AccessibilityRadarPan > 0.0f
					? 1.0f - g_AccessibilityRadarPan : 1.0f;
			f32 rightpan = g_AccessibilityRadarPan < 0.0f
					? 1.0f + g_AccessibilityRadarPan : 1.0f;
			f32 modulation = g_AccessibilityRadarRearState
					? 0.75f + 0.25f * sinf(
							g_AccessibilityRadarModulationPhase)
					: 1.0f;
			f32 wave;
			f32 radar;

			if (g_AccessibilityRadarKindState
					== ACCESSIBILITY_TONE_RADAR_EMPTY) {
				beeplength = ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES;
				if (sample < ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES) {
					beepsample = sample;
				} else if (sample >= ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES
						+ ACCESSIBILITY_RADAR_EMPTY_GAP_SAMPLES) {
					beepsample = sample
							- ACCESSIBILITY_RADAR_EMPTY_BEEP_SAMPLES
							- ACCESSIBILITY_RADAR_EMPTY_GAP_SAMPLES;
				}
			} else if (g_AccessibilityRadarKindState
					== ACCESSIBILITY_TONE_RADAR_LAUNCH
					|| g_AccessibilityRadarKindState
						== ACCESSIBILITY_TONE_RADAR_UNAVAILABLE
					|| g_AccessibilityRadarHeightState == 0) {
				beepsample = sample;
				if (g_AccessibilityRadarKindState
						== ACCESSIBILITY_TONE_RADAR_UNAVAILABLE
						&& g_AccessibilityRadarTotalSamples > 1) {
					frequencymultiplier = 1.0f - 0.35f
							* (f32)sample
								/ (f32)(g_AccessibilityRadarTotalSamples - 1);
				}
			} else {
				beeplength = ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES;
				if (sample < ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES) {
					beepsample = sample;
					frequencymultiplier
							= g_AccessibilityRadarHeightState == 1
								? 0.9f : 1.1f;
				} else if (sample
						>= ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES
							+ ACCESSIBILITY_RADAR_DOUBLE_GAP_SAMPLES) {
					beepsample = sample
							- ACCESSIBILITY_RADAR_DOUBLE_BEEP_SAMPLES
							- ACCESSIBILITY_RADAR_DOUBLE_GAP_SAMPLES;
					frequencymultiplier
							= g_AccessibilityRadarHeightState == 1
								? 1.1f : 0.9f;
				}
			}

			if (beepsample >= 0) {
				if (beepsample == 0) {
					g_AccessibilityRadarPhase = 0.0f;
				}
				if (beepsample < ACCESSIBILITY_RADAR_ATTACK_SAMPLES) {
					envelope = (f32)beepsample
							/ (f32)ACCESSIBILITY_RADAR_ATTACK_SAMPLES;
				} else if (beeplength - beepsample
						< ACCESSIBILITY_RADAR_RELEASE_SAMPLES) {
					envelope = (f32)(beeplength - beepsample)
							/ (f32)ACCESSIBILITY_RADAR_RELEASE_SAMPLES;
				}

				wave = sinf(g_AccessibilityRadarPhase);
				if (g_AccessibilityRadarKindState
						== ACCESSIBILITY_TONE_RADAR_ENEMY) {
					wave = accessibilityToneCombatWave(
							g_AccessibilityRadarPhase);
				} else if (g_AccessibilityRadarKindState
						== ACCESSIBILITY_TONE_RADAR_OBJECTIVE
						&& beepsample
							>= ACCESSIBILITY_RADAR_ATTACK_SAMPLES * 3) {
					wave = wave * 0.78f
							+ sinf(g_AccessibilityRadarPhase * 2.0f) * 0.22f;
				}

				radar = wave * envelope * modulation
						* g_AccessibilityRadarVolume * 32767.0f;
				radarleft = (s32)(radar * leftpan);
				radarright = (s32)(radar * rightpan);
				g_AccessibilityRadarPhase += TWO_PI
						* g_AccessibilityRadarFrequencyHz
						* frequencymultiplier
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				g_AccessibilityRadarModulationPhase += TWO_PI
						* ACCESSIBILITY_RADAR_REAR_MODULATION_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityRadarPhase >= TWO_PI) {
					g_AccessibilityRadarPhase -= TWO_PI;
				}
				if (g_AccessibilityRadarModulationPhase >= TWO_PI) {
					g_AccessibilityRadarModulationPhase -= TWO_PI;
				}
			}

			g_AccessibilityRadarSample++;
			g_AccessibilityRadarSamplesRemaining--;
		}

		g_AccessibilityHillPan += (hilltargetpan
				- g_AccessibilityHillPan) / (f32)(frames - i);
		if (g_AccessibilityHillGain < hilltargetgain) {
			g_AccessibilityHillGain += ACCESSIBILITY_MARKER_BASE_VOLUME
					/ (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f);
			if (g_AccessibilityHillGain > hilltargetgain) {
				g_AccessibilityHillGain = hilltargetgain;
			}
		} else if (g_AccessibilityHillGain > hilltargetgain) {
			g_AccessibilityHillGain -= ACCESSIBILITY_MARKER_BASE_VOLUME
					/ (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f);
			if (g_AccessibilityHillGain < hilltargetgain) {
				g_AccessibilityHillGain = hilltargetgain;
			}
		}

		if (g_AccessibilityHillGain > 0.0f) {
			f32 sweep = (f32)g_AccessibilityHillSweepSample
					/ (f32)ACCESSIBILITY_MARKER_SWEEP_SAMPLES;
			f32 triangle = sweep < 0.5f ? sweep * 2.0f
					: (1.0f - sweep) * 2.0f;
			f32 frequencya = ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ
					+ (ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
							- ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ)
						* triangle;
			f32 frequencyb = ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
					- (ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
							- ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ)
						* triangle;
			f32 leftpan = g_AccessibilityHillPan > 0.0f
					? 1.0f - g_AccessibilityHillPan : 1.0f;
			f32 rightpan = g_AccessibilityHillPan < 0.0f
					? 1.0f + g_AccessibilityHillPan : 1.0f;
			f32 modulation = hillrear
					? 0.75f + 0.25f
						* sinf(g_AccessibilityHillModulationPhase)
					: 1.0f;
			f32 hill = (sinf(g_AccessibilityHillPhaseA)
					+ sinf(g_AccessibilityHillPhaseB)) * 0.5f
					* g_AccessibilityHillGain * modulation * 32767.0f;

			if (hillidentitychirp
					&& g_AccessibilityHillChirpSample
						< ACCESSIBILITY_MARKER_BEEP_SAMPLES) {
				s32 chirpsample = g_AccessibilityHillChirpSample;
				f32 envelope = 1.0f;
				f32 chirpgain = g_AccessibilityHillGain
						/ ACCESSIBILITY_MARKER_BASE_VOLUME
						* ACCESSIBILITY_MARKER_CHIRP_VOLUME;

				if (chirpsample < ACCESSIBILITY_MARKER_ATTACK_SAMPLES) {
					envelope = (f32)chirpsample
							/ (f32)ACCESSIBILITY_MARKER_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_MARKER_BEEP_SAMPLES - chirpsample
						< ACCESSIBILITY_MARKER_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_MARKER_BEEP_SAMPLES
							- chirpsample)
							/ (f32)ACCESSIBILITY_MARKER_RELEASE_SAMPLES;
				}
				hill += sinf(g_AccessibilityHillChirpPhase)
						* envelope * chirpgain * modulation * 32767.0f;
				g_AccessibilityHillChirpPhase += TWO_PI
						* ACCESSIBILITY_MARKER_IDENTITY_FREQUENCY_HZ
						/ ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityHillChirpPhase >= TWO_PI) {
					g_AccessibilityHillChirpPhase -= TWO_PI;
				}
			}

			hillleft = (s32)(hill * leftpan);
			hillright = (s32)(hill * rightpan);
			g_AccessibilityHillPhaseA += TWO_PI
					* frequencya / ACCESSIBILITY_TONE_SAMPLE_RATE;
			g_AccessibilityHillPhaseB += TWO_PI
					* frequencyb / ACCESSIBILITY_TONE_SAMPLE_RATE;
			g_AccessibilityHillModulationPhase += TWO_PI
					* ACCESSIBILITY_HILL_REAR_MODULATION_HZ
					/ ACCESSIBILITY_TONE_SAMPLE_RATE;
			if (g_AccessibilityHillPhaseA >= TWO_PI) {
				g_AccessibilityHillPhaseA -= TWO_PI;
			}
			if (g_AccessibilityHillPhaseB >= TWO_PI) {
				g_AccessibilityHillPhaseB -= TWO_PI;
			}
			if (g_AccessibilityHillModulationPhase >= TWO_PI) {
				g_AccessibilityHillModulationPhase -= TWO_PI;
			}
		}
		g_AccessibilityHillSweepSample++;
		if (g_AccessibilityHillSweepSample
				>= ACCESSIBILITY_MARKER_SWEEP_SAMPLES) {
			g_AccessibilityHillSweepSample = 0;
		}
		g_AccessibilityHillChirpSample++;
		if (g_AccessibilityHillChirpSample
				>= (hillscoring
						? ACCESSIBILITY_HILL_SCORING_CHIRP_PERIOD_SAMPLES
						: ACCESSIBILITY_HILL_CHIRP_PERIOD_SAMPLES)) {
			g_AccessibilityHillChirpSample = 0;
			g_AccessibilityHillChirpPhase = 0.0f;
		}

		if (anycaneactive) {
		s32 runwayactive = g_AccessibilityCaneSamplesRemaining[
				ACCESSIBILITY_TONE_CANE_SLOT_COUNT - 1] > 0
				&& g_AccessibilityCanePatternState[
						ACCESSIBILITY_TONE_CANE_SLOT_COUNT - 1]
						== ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY;

			for (slot = 0; slot < ACCESSIBILITY_TONE_CANE_SLOT_COUNT; slot++) {
				if (g_AccessibilityCaneSamplesRemaining[slot] > 0) {
					f32 envelope = 1.0f;
					f32 canepan = g_AccessibilityCanePan[slot];
					f32 leftpan;
					f32 rightpan;
					f32 frequency;
					f32 cane;
					f32 canevolume = g_AccessibilityCaneVolume[slot];
					s32 localsample = g_AccessibilityCaneSample[slot];
					s32 localremaining = g_AccessibilityCaneSamplesRemaining[slot];
					s32 sounding = 1;
					s32 wallstep = 0;

					if (g_AccessibilityCanePatternState[slot]
							== ACCESSIBILITY_TONE_CANE_PATTERN_CROUCH_DOUBLE) {
						s32 gap = ACCESSIBILITY_CANE_CROUCH_GAP_SAMPLES;
						s32 beep;

						if (gap > g_AccessibilityCaneDurationSamples[slot] / 3) {
							gap = g_AccessibilityCaneDurationSamples[slot] / 3;
						}
						beep = (g_AccessibilityCaneDurationSamples[slot] - gap) / 2;

						if (g_AccessibilityCaneSample[slot] < beep) {
							frequency = g_AccessibilityCaneFrequencyHz[slot];
							localremaining = beep - g_AccessibilityCaneSample[slot];
						} else if (g_AccessibilityCaneSample[slot] < beep + gap) {
							frequency = g_AccessibilityCaneFrequencyHz[slot];
							sounding = 0;
						} else {
							localsample = g_AccessibilityCaneSample[slot] - beep - gap;
							localremaining = g_AccessibilityCaneDurationSamples[slot]
									- g_AccessibilityCaneSample[slot];
							frequency = g_AccessibilityCaneEndFrequencyHz[slot];
							if (localsample == 0) {
								g_AccessibilityCanePhase[slot] = 0.0f;
							}
						}
					} else if (g_AccessibilityCanePatternState[slot]
							== ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY
							&& g_AccessibilityCanePathCountState[slot] > 0) {
						s32 count = g_AccessibilityCanePathCountState[slot];
						s32 stepduration
								= g_AccessibilityCaneDurationSamples[slot] / count;
						s32 step;
						s32 within;
						s32 beepsamples = (s32)(ACCESSIBILITY_TONE_SAMPLE_RATE
								* 0.024f);

						if (stepduration < 1) stepduration = 1;
						step = g_AccessibilityCaneSample[slot] / stepduration;
						if (step >= count) step = count - 1;
						within = g_AccessibilityCaneSample[slot]
								- step * stepduration;
						frequency = g_AccessibilityCanePathFrequencyHzState[slot][step];
						canevolume = g_AccessibilityCanePathVolumeState[slot][step];
						canepan = g_AccessibilityCanePathPanState[slot][step];
						sounding = within < beepsamples;
						localsample = within;
						localremaining = beepsamples - within;
						if (within == 0) g_AccessibilityCanePhase[slot] = 0.0f;
					} else if (g_AccessibilityCanePatternState[slot]
							== ACCESSIBILITY_TONE_CANE_PATTERN_PATH
							&& g_AccessibilityCanePathCountState[slot] > 0) {
						s32 count = g_AccessibilityCanePathCountState[slot];
						s32 stepduration = g_AccessibilityCaneDurationSamples[slot] / count;
						s32 floorcount
								= g_AccessibilityCanePathFloorCountState[slot];
						s32 floorduration;

						if (stepduration < 1) stepduration = 1;
						floorduration = stepduration * floorcount;

						if (floorcount > 0
								&& g_AccessibilityCaneSample[slot] < floorduration) {
							f32 position = floorcount > 1 && floorduration > 1
									? (f32)g_AccessibilityCaneSample[slot]
											* (f32)(floorcount - 1)
											/ (f32)(floorduration - 1) : 0.0f;
							s32 segment = (s32)position;
							f32 fraction;

							if (segment >= floorcount - 1) {
								segment = floorcount - 1;
								fraction = 0.0f;
							} else {
								fraction = position - (f32)segment;
							}
							frequency = g_AccessibilityCanePathFrequencyHzState[slot][segment];
							canevolume = g_AccessibilityCanePathVolumeState[slot][segment];
							canepan = g_AccessibilityCanePathPanState[slot][segment];
							if (segment + 1 < floorcount) {
								f32 nextfrequency
										= g_AccessibilityCanePathFrequencyHzState[slot][segment + 1];
								f32 nextvolume
										= g_AccessibilityCanePathVolumeState[slot][segment + 1];
								f32 nextpan
										= g_AccessibilityCanePathPanState[slot][segment + 1];
								frequency *= powf(nextfrequency / frequency, fraction);
								canevolume += (nextvolume - canevolume) * fraction;
								canepan += (nextpan - canepan) * fraction;
							}
							localsample = g_AccessibilityCaneSample[slot];
							localremaining = floorduration
									- g_AccessibilityCaneSample[slot];
						} else {
							s32 wallsample = g_AccessibilityCaneSample[slot]
									- floorduration;
							frequency = g_AccessibilityCanePathFrequencyHzState[slot][count - 1];
							canevolume = g_AccessibilityCanePathVolumeState[slot][count - 1];
							canepan = g_AccessibilityCanePathPanState[slot][count - 1];
							wallstep = 1;
							sounding = wallsample
									>= ACCESSIBILITY_CANE_PATH_WALL_GAP_SAMPLES;
							localsample = sounding
									? wallsample
											- ACCESSIBILITY_CANE_PATH_WALL_GAP_SAMPLES
									: 0;
							localremaining = g_AccessibilityCaneDurationSamples[slot]
									- g_AccessibilityCaneSample[slot];
							if (wallsample == ACCESSIBILITY_CANE_PATH_WALL_GAP_SAMPLES) {
								g_AccessibilityCanePhase[slot] = 0.0f;
							}
						}
					} else {
						f32 progress = (f32)g_AccessibilityCaneSample[slot]
								/ (f32)g_AccessibilityCaneDurationSamples[slot];
						frequency = g_AccessibilityCaneFrequencyHz[slot]
								* powf(g_AccessibilityCaneEndFrequencyHz[slot]
										/ g_AccessibilityCaneFrequencyHz[slot],
										progress);
					}

					leftpan = canepan > 0.0f ? 1.0f - canepan : 1.0f;
					rightpan = canepan < 0.0f ? 1.0f + canepan : 1.0f;
					if (runwayactive
							&& slot != ACCESSIBILITY_TONE_CANE_SLOT_COUNT - 1) {
						canevolume *= 0.2f;
					}

					if (localsample
							< ACCESSIBILITY_CANE_ATTACK_SAMPLES) {
						envelope = (f32)localsample
								/ (f32)ACCESSIBILITY_CANE_ATTACK_SAMPLES;
					} else if (localremaining
							< ACCESSIBILITY_CANE_RELEASE_SAMPLES) {
						envelope = (f32)localremaining
								/ (f32)ACCESSIBILITY_CANE_RELEASE_SAMPLES;
					}

					cane = sounding
							? (wallstep
									? sinf(g_AccessibilityCanePhase[slot]) * 0.8f
											+ sinf(g_AccessibilityCanePhase[slot] * 2.0f) * 0.2f
									: sinf(g_AccessibilityCanePhase[slot])) * envelope
							* canevolume * 32767.0f
							: 0.0f;
					caneleft += (s32)(cane * leftpan);
					caneright += (s32)(cane * rightpan);
					g_AccessibilityCanePhase[slot] += TWO_PI
							* frequency
							/ ACCESSIBILITY_TONE_SAMPLE_RATE;

					if (g_AccessibilityCanePhase[slot] >= TWO_PI) {
						g_AccessibilityCanePhase[slot] -= TWO_PI;
					}

					g_AccessibilityCaneSample[slot]++;
					g_AccessibilityCaneSamplesRemaining[slot]--;
				}
			}
		}

		for (slot = 0; slot < ACCESSIBILITY_TONE_MARKER_VOICE_COUNT; slot++) {
			f32 targetmarkergain = markerenabled[slot]
					? markervolume[slot] * ACCESSIBILITY_MARKER_BASE_VOLUME
					: 0.0f;
			f32 markergainstep = ACCESSIBILITY_MARKER_BASE_VOLUME
					/ (ACCESSIBILITY_TONE_SAMPLE_RATE * 0.01f);

			g_AccessibilityMarkerPan[slot] += (markerpan[slot]
					- g_AccessibilityMarkerPan[slot]) / (f32)(frames - i);

			if (g_AccessibilityMarkerGain[slot] < targetmarkergain) {
				g_AccessibilityMarkerGain[slot] += markergainstep;
				if (g_AccessibilityMarkerGain[slot] > targetmarkergain) {
					g_AccessibilityMarkerGain[slot] = targetmarkergain;
				}
			} else if (g_AccessibilityMarkerGain[slot] > targetmarkergain) {
				g_AccessibilityMarkerGain[slot] -= markergainstep;
				if (g_AccessibilityMarkerGain[slot] < targetmarkergain) {
					g_AccessibilityMarkerGain[slot] = targetmarkergain;
				}
			}

			if (g_AccessibilityMarkerGain[slot] > 0.0f) {
				f32 sweep = (f32)g_AccessibilityMarkerSweepSample
						/ (f32)ACCESSIBILITY_MARKER_SWEEP_SAMPLES;
				f32 triangle = sweep < 0.5f ? sweep * 2.0f
						: (1.0f - sweep) * 2.0f;
				f32 frequencya = ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ
						+ (ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
								- ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ)
							* triangle;
				f32 frequencyb = ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
						- (ACCESSIBILITY_MARKER_HIGH_FREQUENCY_HZ
								- ACCESSIBILITY_MARKER_LOW_FREQUENCY_HZ)
							* triangle;
				f32 leftpan = g_AccessibilityMarkerPan[slot] > 0.0f
						? 1.0f - g_AccessibilityMarkerPan[slot] : 1.0f;
				f32 rightpan = g_AccessibilityMarkerPan[slot] < 0.0f
						? 1.0f + g_AccessibilityMarkerPan[slot] : 1.0f;
				f32 marker = (sinf(g_AccessibilityMarkerPhaseA[slot])
						+ sinf(g_AccessibilityMarkerPhaseB[slot])) * 0.5f
						* g_AccessibilityMarkerGain[slot] * 32767.0f;

				markerleft += (s32)(marker * leftpan);
				markerright += (s32)(marker * rightpan);
				g_AccessibilityMarkerPhaseA[slot] += TWO_PI
						* frequencya / ACCESSIBILITY_TONE_SAMPLE_RATE;
				g_AccessibilityMarkerPhaseB[slot] += TWO_PI
						* frequencyb / ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityMarkerPhaseA[slot] >= TWO_PI) {
					g_AccessibilityMarkerPhaseA[slot] -= TWO_PI;
				}
				if (g_AccessibilityMarkerPhaseB[slot] >= TWO_PI) {
					g_AccessibilityMarkerPhaseB[slot] -= TWO_PI;
				}
			}

			if (markerenabled[slot]
					&& g_AccessibilityMarkerDueSample[slot] > 0) {
				g_AccessibilityMarkerDueSample[slot]--;
			}
		}

		if (g_AccessibilityMarkerIdentityCooldown > 0) {
			g_AccessibilityMarkerIdentityCooldown--;
		}

		if (g_AccessibilityMarkerIdentitySlot < 0
				&& g_AccessibilityMarkerIdentityCooldown <= 0) {
			if (g_AccessibilityMarkerPendingRemovalMask != 0) {
				for (slot = 0;
						slot < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT;
						slot++) {
					if (g_AccessibilityMarkerPendingRemovalMask
							& (1 << slot)) {
						g_AccessibilityMarkerIdentitySlot = slot;
						g_AccessibilityMarkerPendingRemovalMask
								&= ~(1 << slot);
						break;
					}
				}
				g_AccessibilityMarkerIdentityRemoval = 1;
			} else {
				s32 attempt;

				for (attempt = 0;
						attempt < ACCESSIBILITY_TONE_MARKER_SLOT_COUNT;
						attempt++) {
					s32 candidate = (g_AccessibilityMarkerIdentityCursor
							+ attempt) % ACCESSIBILITY_TONE_MARKER_SLOT_COUNT;

					if (markerenabled[candidate]
							&& g_AccessibilityMarkerDueSample[candidate] == 0) {
						g_AccessibilityMarkerIdentitySlot = candidate;
						g_AccessibilityMarkerIdentityRemoval = 0;
						g_AccessibilityMarkerDueSample[candidate]
								= ACCESSIBILITY_MARKER_PERIOD_SAMPLES;
						g_AccessibilityMarkerIdentityCursor
								= (candidate + 1)
									% ACCESSIBILITY_TONE_MARKER_SLOT_COUNT;
						break;
					}
				}
			}

			if (g_AccessibilityMarkerIdentitySlot >= 0) {
				g_AccessibilityMarkerIdentitySample = 0;
				g_AccessibilityMarkerIdentityPhase = 0.0f;
				g_AccessibilityMarkerIdentityCooldown
						= ACCESSIBILITY_MARKER_MIN_START_SPACING_SAMPLES;
			}
		}

		if (g_AccessibilityMarkerIdentitySlot >= 0) {
			s32 identityslot = g_AccessibilityMarkerIdentitySlot;
			s32 pulses = identityslot + 1;
			s32 totalpulses = pulses
					+ (g_AccessibilityMarkerIdentityRemoval ? 1 : 0);
			s32 cycle = ACCESSIBILITY_MARKER_BEEP_SAMPLES
					+ ACCESSIBILITY_MARKER_GAP_SAMPLES;
			s32 total = ACCESSIBILITY_MARKER_BEEP_SAMPLES * totalpulses
					+ ACCESSIBILITY_MARKER_GAP_SAMPLES * (totalpulses - 1);
			s32 beepindex = g_AccessibilityMarkerIdentitySample / cycle;
			s32 beepsample = g_AccessibilityMarkerIdentitySample % cycle;

			if (beepsample < ACCESSIBILITY_MARKER_BEEP_SAMPLES) {
				f32 envelope = 1.0f;
				f32 pan = g_AccessibilityMarkerIdentityRemoval ? 0.0f
						: g_AccessibilityMarkerPan[identityslot];
				f32 gain = g_AccessibilityMarkerIdentityRemoval
						? ACCESSIBILITY_MARKER_CHIRP_VOLUME
						: g_AccessibilityMarkerGain[identityslot]
								/ ACCESSIBILITY_MARKER_BASE_VOLUME
							* ACCESSIBILITY_MARKER_CHIRP_VOLUME;
				f32 frequency = beepindex < pulses
						? ACCESSIBILITY_MARKER_IDENTITY_FREQUENCY_HZ
						: ACCESSIBILITY_MARKER_REMOVAL_FREQUENCY_HZ;
				f32 leftpan = pan > 0.0f ? 1.0f - pan : 1.0f;
				f32 rightpan = pan < 0.0f ? 1.0f + pan : 1.0f;
				f32 chirp;

				if (beepsample < ACCESSIBILITY_MARKER_ATTACK_SAMPLES) {
					envelope = (f32)beepsample
							/ (f32)ACCESSIBILITY_MARKER_ATTACK_SAMPLES;
				} else if (ACCESSIBILITY_MARKER_BEEP_SAMPLES - beepsample
						< ACCESSIBILITY_MARKER_RELEASE_SAMPLES) {
					envelope = (f32)(ACCESSIBILITY_MARKER_BEEP_SAMPLES
							- beepsample)
							/ (f32)ACCESSIBILITY_MARKER_RELEASE_SAMPLES;
				}

				chirp = sinf(g_AccessibilityMarkerIdentityPhase)
						* envelope * gain * 32767.0f;
				markerleft += (s32)(chirp * leftpan);
				markerright += (s32)(chirp * rightpan);
				g_AccessibilityMarkerIdentityPhase += TWO_PI
						* frequency / ACCESSIBILITY_TONE_SAMPLE_RATE;
				if (g_AccessibilityMarkerIdentityPhase >= TWO_PI) {
					g_AccessibilityMarkerIdentityPhase -= TWO_PI;
				}
			}

			g_AccessibilityMarkerIdentitySample++;
			if (g_AccessibilityMarkerIdentitySample >= total) {
				g_AccessibilityMarkerIdentitySlot = -1;
				g_AccessibilityMarkerIdentitySample = 0;
				g_AccessibilityMarkerIdentityRemoval = 0;
			}
		}

		g_AccessibilityMarkerSweepSample++;
		if (g_AccessibilityMarkerSweepSample
				>= ACCESSIBILITY_MARKER_SWEEP_SAMPLES) {
			g_AccessibilityMarkerSweepSample = 0;
		}

		g_AccessibilityToneMixBuffer[index] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index] + tone + chirpleft
						+ targetpresenceleft + threatalertleft
						+ hazardleft + combatleft
						+ trackerleft + friendlyleft + doorleft + radarleft
						+ hillleft + caneleft
						+ markerleft + toggletone + compasstone
						+ weaponfunctiontone);
		g_AccessibilityToneMixBuffer[index + 1] = accessibilityToneClamp(
				(s32)g_AccessibilityToneMixBuffer[index + 1] + tone + chirpright
						+ targetpresenceright + threatalertright
						+ hazardright + combatright
						+ trackerright + friendlyright + doorright + radarright
						+ hillright + caneright
						+ markerright + toggletone + compasstone
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

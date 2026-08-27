#include <ultra64.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "system.h"
#include "game/propsnd.h"
#include "lib/lib_317f0.h"
#include "lib/snd.h"
#include "lib/vars.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_targeting.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_TARGETING_VISIBLE_FRAMES 2
#define ACCESSIBILITY_TARGETING_MISSING_FRAMES 2
#define ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS TICKS(36)
#define ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS TICKS(6)
#define ACCESSIBILITY_TARGETING_FULL_DISTANCE 200.0f
#define ACCESSIBILITY_TARGETING_FADE_DISTANCE 1200.0f
#define ACCESSIBILITY_TARGETING_SILENT_DISTANCE 1400.0f
#define ACCESSIBILITY_TARGETING_NAME_LENGTH 96
#define ACCESSIBILITY_TARGETING_OBSERVATION_LOG_TICKS TICKS(60)
#define ACCESSIBILITY_TARGETING_TELEMETRY_TICKS TICKS(60 * 30)
#define ACCESSIBILITY_TARGETING_ALIGNMENT_LOG_TICKS TICKS(6)
#define ACCESSIBILITY_TARGETING_COMBAT_AIM_LOSS_GRACE_FRAMES 2
#define ACCESSIBILITY_TARGETING_TONE_BASE_FREQUENCY_HZ 440.0f
#define ACCESSIBILITY_TARGETING_TONE_MIN_PITCH 1.5f
#define ACCESSIBILITY_TARGETING_TONE_MAX_PITCH 3.0f
#define ACCESSIBILITY_TARGETING_HEAD_LOCK_MULTIPLIER 1.25f
#define ACCESSIBILITY_TARGETING_ARM_LOCK_MULTIPLIER 0.8f
#define ACCESSIBILITY_TARGETING_COMBAT_FAR_PERIOD_MS 500
#define ACCESSIBILITY_TARGETING_COMBAT_CLOSE_PERIOD_MS 200
#define ACCESSIBILITY_TARGETING_COMBAT_FAR_DURATION_MS 180
#define ACCESSIBILITY_TARGETING_COMBAT_CLOSE_DURATION_MS 50
#define ACCESSIBILITY_TARGETING_COMBAT_TRIGGER_PERIOD_DELTA_MS 40
#define ACCESSIBILITY_TARGETING_COMBAT_CLOSE_EXIT_SCALE 1.1f
#define ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES 5.0f
#define ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES 45.0f
#define ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_BELOW_MULTIPLIER (2.0f / 3.0f)
#define ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_ABOVE_MULTIPLIER (5.0f / 3.0f)
#define ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_SMOOTHING 0.35f
#define ACCESSIBILITY_TARGETING_PRECISION_PERIOD_MS 160
#define ACCESSIBILITY_TARGETING_PRECISION_DURATION_MS 100
#define ACCESSIBILITY_TARGETING_PRECISION_PAN_SCALE 1.75f
#define ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_DEADZONE 0.025f
#define ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_LIMIT 0.5f
#define ACCESSIBILITY_TARGETING_PRECISION_FREQUENCY_SMOOTHING 0.65f
#define ACCESSIBILITY_TARGETING_PRECISION_SWITCH_MARGIN 0.08f
#define ACCESSIBILITY_TARGETING_CAMERA_START_FREQUENCY_HZ 1600.0f
#define ACCESSIBILITY_TARGETING_CAMERA_END_FREQUENCY_HZ 1000.0f
#define ACCESSIBILITY_TARGETING_CAMERA_PERIOD_MS 500
#define ACCESSIBILITY_TARGETING_CAMERA_DURATION_MS 140
#define ACCESSIBILITY_TARGETING_THREAT_KNOWN_CAPACITY 8
#define ACCESSIBILITY_TARGETING_THREAT_ALERT_CAPACITY 8
#define ACCESSIBILITY_TARGETING_THREAT_MISSING_GRACE_FRAMES 2
#define ACCESSIBILITY_TARGETING_THREAT_ALERT_GAP_TICKS TICKS(12)
#define ACCESSIBILITY_TARGETING_THREAT_AIM_REPEAT_TICKS TICKS(15)
#define ACCESSIBILITY_TARGETING_THREAT_ALERT_DURATION_TICKS TICKS(8)
#define ACCESSIBILITY_TARGETING_THREAT_SOUND_NONE 0
#define ACCESSIBILITY_TARGETING_THREAT_SOUND_NEW 1
#define ACCESSIBILITY_TARGETING_THREAT_SOUND_AIM 2

struct accessibilitytargetingrecord {
	struct accessibilitytargetingcandidate candidate;
	char localizedname[ACCESSIBILITY_TARGETING_NAME_LENGTH];
	s32 seenframes;
	s32 missingframes;
};

struct accessibilitytargetingpolicy {
	s32 basecycleticks;
	s32 minslotticks;
	s32 visibleframes;
	s32 missingframes;
	f32 fulldistance;
	f32 fadedistance;
	f32 silentdistance;
};

struct accessibilitytargetingcombatslot {
	struct accessibilitytargetingidentity identity;
	s32 assigned;
	s32 audible;
	s32 periodms;
	s32 durationms;
	s32 triggerperiodms;
	s32 distancezone;
	s32 elevationzone;
	s32 precisionguidance;
	f32 frequencyhz;
	s32 nextcadencelog60;
};

struct accessibilitytargetingthreatknown {
	struct accessibilitytargetingidentity identity;
	s32 missingframes;
};

struct accessibilitytargetingthreatalert {
	struct accessibilitytargetingcandidate candidate;
};

struct accessibilitytargetingstate {
	struct accessibilitytargetingrecord records[ACCESSIBILITY_TARGETING_MAX_CANDIDATES];
	s32 recordcount;
	struct accessibilitytargetingidentity lastpulseidentity;
	s32 haslastpulseidentity;
	struct accessibilitytargetingidentity aimedidentity;
	s32 hasaimedidentity;
	s32 proceduralpresenceactive;
	s32 alignmentactive;
	s32 alignmentinterrupted;
	s32 alignmentobstruction;
	s32 aimlossframes;
	f32 alignmentfrequencyhz;
	f32 alignmentquality;
	f32 alignmentdistance;
	s32 nextpresence60;
	s32 nextalignmentlog60;
	u64 observationcount;
	u64 presencepulsecount;
	u64 alignmentupdatecount;
	s32 haslastobservation;
	s32 lastcandidatecount;
	s32 lastaimed;
	s32 lastaimedcandidate;
	s32 lastaimedshootability;
	struct accessibilitytargetingidentity lastaimedidentity;
	s32 nextobservationlog60;
	s32 nexttelemetry60;
	s32 memorybaselinevalid;
	u64 workingsetbaseline;
	u64 privatebaseline;
	s32 profile;
	s32 threatdetectoractive;
	struct accessibilitytargetingthreatknown
			knownthreats[ACCESSIBILITY_TARGETING_THREAT_KNOWN_CAPACITY];
	s32 knownthreatcount;
	struct accessibilitytargetingthreatalert
			threatalerts[ACCESSIBILITY_TARGETING_THREAT_ALERT_CAPACITY];
	s32 threatalertcount;
	s32 nextthreatalert60;
	s32 nextthreataimalert60;
	s32 lastthreatsound60;
	s32 lastthreatsoundkind;
	struct accessibilitytargetingidentity lastthreatsoundidentity;
	s32 hasaimedthreat;
	struct accessibilitytargetingidentity aimedthreatidentity;
	u64 threatalertscount;
	u64 threataimpulsecount;
	s32 precisionguidanceactive;
	struct accessibilitytargetingidentity precisionguidanceidentity;
	struct accessibilitytargetingcombatslot
			combatslots[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
};

static const struct accessibilitytargetingpolicy g_AccessibilityTargetingRangePolicy = {
	ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS,
	ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS,
	ACCESSIBILITY_TARGETING_VISIBLE_FRAMES,
	ACCESSIBILITY_TARGETING_MISSING_FRAMES,
	ACCESSIBILITY_TARGETING_FULL_DISTANCE,
	ACCESSIBILITY_TARGETING_FADE_DISTANCE,
	ACCESSIBILITY_TARGETING_SILENT_DISTANCE,
};

static struct accessibilitytargetingpolicy g_AccessibilityTargetingCombatPolicy = {
	ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS,
	ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS,
	ACCESSIBILITY_TARGETING_VISIBLE_FRAMES,
	ACCESSIBILITY_TARGETING_MISSING_FRAMES,
	600.0f,
	3500.0f,
	4000.0f,
};

static const struct accessibilitytargetingpolicy g_AccessibilityTargetingDevicePolicy = {
	ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS,
	ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS,
	ACCESSIBILITY_TARGETING_VISIBLE_FRAMES,
	ACCESSIBILITY_TARGETING_MISSING_FRAMES,
	ACCESSIBILITY_TARGETING_FULL_DISTANCE,
	ACCESSIBILITY_TARGETING_FADE_DISTANCE,
	ACCESSIBILITY_TARGETING_SILENT_DISTANCE,
};

static struct accessibilitytargetingstate g_AccessibilityTargetingStates[MAX_PLAYERS];
static struct accessibilitytargetingstate *g_AccessibilityTargetingCurrentState;
static const struct accessibilitytargetingpolicy *g_AccessibilityTargetingCurrentPolicy;

#define g_AccessibilityTargetingRecords (g_AccessibilityTargetingCurrentState->records)
#define g_AccessibilityTargetingRecordCount (g_AccessibilityTargetingCurrentState->recordcount)
#define g_AccessibilityTargetingLastPulseIdentity (g_AccessibilityTargetingCurrentState->lastpulseidentity)
#define g_AccessibilityTargetingHasLastPulseIdentity (g_AccessibilityTargetingCurrentState->haslastpulseidentity)
#define g_AccessibilityTargetingAimedIdentity (g_AccessibilityTargetingCurrentState->aimedidentity)
#define g_AccessibilityTargetingHasAimedIdentity (g_AccessibilityTargetingCurrentState->hasaimedidentity)
#define g_AccessibilityTargetingProceduralPresenceActive (g_AccessibilityTargetingCurrentState->proceduralpresenceactive)
#define g_AccessibilityTargetingAlignmentActive (g_AccessibilityTargetingCurrentState->alignmentactive)
#define g_AccessibilityTargetingAlignmentInterrupted (g_AccessibilityTargetingCurrentState->alignmentinterrupted)
#define g_AccessibilityTargetingAlignmentObstruction (g_AccessibilityTargetingCurrentState->alignmentobstruction)
#define g_AccessibilityTargetingAimLossFrames (g_AccessibilityTargetingCurrentState->aimlossframes)
#define g_AccessibilityTargetingAlignmentFrequencyHz (g_AccessibilityTargetingCurrentState->alignmentfrequencyhz)
#define g_AccessibilityTargetingAlignmentQuality (g_AccessibilityTargetingCurrentState->alignmentquality)
#define g_AccessibilityTargetingAlignmentDistance (g_AccessibilityTargetingCurrentState->alignmentdistance)
#define g_AccessibilityTargetingNextPresence60 (g_AccessibilityTargetingCurrentState->nextpresence60)
#define g_AccessibilityTargetingNextAlignmentLog60 (g_AccessibilityTargetingCurrentState->nextalignmentlog60)
#define g_AccessibilityTargetingObservationCount (g_AccessibilityTargetingCurrentState->observationcount)
#define g_AccessibilityTargetingPresencePulseCount (g_AccessibilityTargetingCurrentState->presencepulsecount)
#define g_AccessibilityTargetingAlignmentUpdateCount (g_AccessibilityTargetingCurrentState->alignmentupdatecount)
#define g_AccessibilityTargetingCombatSlots (g_AccessibilityTargetingCurrentState->combatslots)

static void accessibilityTargetingResetCurrent(const char *reason);

static s32 accessibilityTargetingCombatSlotCount(void)
{
	s32 count = 0;
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		count += g_AccessibilityTargetingCombatSlots[slot].assigned != 0;
	}

	return count;
}

static const char *accessibilityTargetingShootabilityName(s32 shootability)
{
	switch (shootability) {
	case ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE:
		return "shootable";
	case ACCESSIBILITY_TARGETING_SHOOTABILITY_FACING_AWAY:
		return "facing_away";
	default:
		return "unknown";
	}
}

static void accessibilityTargetingLogTelemetry(s32 frame60)
{
	u64 workingset = 0;
	u64 privatebytes = 0;
	s32 memoryavailable = sysGetProcessMemoryUsage(&workingset, &privatebytes);
	s32 channels = IS4MB() ? 30 : 40;
	s32 inuse = 0;
	s32 stopped = 0;
	s32 i;

	for (i = 0; g_PsChannels && i < channels; i++) {
		if ((g_PsChannels[i].flags & PSFLAG_FREE) == 0) {
			inuse++;
			if (g_PsChannels[i].flags2 & PSFLAG2_STOPPED) {
				stopped++;
			}
		}
	}

	if (memoryavailable && !g_AccessibilityTargetingCurrentState->memorybaselinevalid) {
		g_AccessibilityTargetingCurrentState->memorybaselinevalid = true;
		g_AccessibilityTargetingCurrentState->workingsetbaseline = workingset;
		g_AccessibilityTargetingCurrentState->privatebaseline = privatebytes;
	}

	accessibilityLogEvent("targeting", "telemetry",
			"frame=%d observations=%llu presence_pulses=%llu alignment_updates=%llu records=%d aimed=%d memory_available=%d working_set_bytes=%llu working_set_delta=%lld private_bytes=%llu private_delta=%lld snd_states=%d prop_channels_in_use=%d prop_channels_total=%d prop_channels_stopped=%d procedural_presence_active=%d combat_oscillator_slots=%d combat_oscillator_capacity=%d alignment_active=%d alignment_frequency_hz=%.2f alignment_quality=%.4f alignment_distance=%.3f",
			frame60,
			(unsigned long long)g_AccessibilityTargetingObservationCount,
			(unsigned long long)g_AccessibilityTargetingPresencePulseCount,
			(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount,
			g_AccessibilityTargetingRecordCount,
			g_AccessibilityTargetingHasAimedIdentity, memoryavailable,
			(unsigned long long)workingset,
			(long long)workingset
					- (long long)g_AccessibilityTargetingCurrentState->workingsetbaseline,
			(unsigned long long)privatebytes,
			(long long)privatebytes
					- (long long)g_AccessibilityTargetingCurrentState->privatebaseline,
			g_SndNumPlaying, inuse, channels, stopped,
			g_AccessibilityTargetingProceduralPresenceActive,
			accessibilityTargetingCombatSlotCount(),
			ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT,
			g_AccessibilityTargetingAlignmentActive,
			g_AccessibilityTargetingAlignmentFrequencyHz,
			g_AccessibilityTargetingAlignmentQuality,
			g_AccessibilityTargetingAlignmentDistance);

	g_AccessibilityTargetingCurrentState->nexttelemetry60
			= frame60 + ACCESSIBILITY_TARGETING_TELEMETRY_TICKS;
}

static const struct accessibilitytargetingpolicy *accessibilityTargetingGetPolicy(
		s32 profile, const struct accessibilitytargetingobservation *observation)
{
	if (profile == ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE) {
		return &g_AccessibilityTargetingRangePolicy;
	}

	if (profile == ACCESSIBILITY_TARGETING_PROFILE_COMBAT) {
		f32 basefull;
		f32 basefade;
		f32 basemaximum;
		f32 scopedfull;
		f32 scopedfade;
		f32 scopedmaximum;
		f32 zoomblend = observation ? observation->zoomblend : 0.0f;

		if (zoomblend < 0.0f) {
			zoomblend = 0.0f;
		} else if (zoomblend > 1.0f) {
			zoomblend = 1.0f;
		}
		accessibilityGetEnemyTuning(
				&basefull, &basefade, &basemaximum, NULL);
		accessibilityGetEnemyScopedTuning(
				&scopedfull, &scopedfade, &scopedmaximum);
		g_AccessibilityTargetingCombatPolicy.fulldistance
				= basefull + (scopedfull - basefull) * zoomblend;
		g_AccessibilityTargetingCombatPolicy.fadedistance
				= basefade + (scopedfade - basefade) * zoomblend;
		g_AccessibilityTargetingCombatPolicy.silentdistance
				= basemaximum + (scopedmaximum - basemaximum) * zoomblend;
		return &g_AccessibilityTargetingCombatPolicy;
	}

	if (profile == ACCESSIBILITY_TARGETING_PROFILE_DEVICE) {
		return &g_AccessibilityTargetingDevicePolicy;
	}

	return NULL;
}

static s32 accessibilityTargetingIdentityEqual(
		const struct accessibilitytargetingidentity *a,
		const struct accessibilitytargetingidentity *b)
{
	return a->playernum == b->playernum
			&& a->source == b->source
			&& a->sourceslot == b->sourceslot
			&& a->propnum == b->propnum
			&& a->proptype == b->proptype
			&& a->objectidentity == b->objectidentity;
}

static s32 accessibilityTargetingFindCurrentThreat(
		const struct accessibilitytargetingobservation *observation,
		const struct accessibilitytargetingidentity *identity)
{
	s32 i;

	for (i = 0; i < observation->threatcount; i++) {
		if (accessibilityTargetingIdentityEqual(
				&observation->threats[i].identity, identity)) {
			return i;
		}
	}

	return -1;
}

static s32 accessibilityTargetingThreatAlertQueued(
		const struct accessibilitytargetingstate *state,
		const struct accessibilitytargetingidentity *identity)
{
	s32 i;

	for (i = 0; i < state->threatalertcount; i++) {
		if (accessibilityTargetingIdentityEqual(
				&state->threatalerts[i].candidate.identity, identity)) {
			return true;
		}
	}

	return false;
}

static void accessibilityTargetingPopThreatAlert(
		struct accessibilitytargetingstate *state)
{
	if (state->threatalertcount > 1) {
		memmove(&state->threatalerts[0], &state->threatalerts[1],
				sizeof(state->threatalerts[0])
					* (state->threatalertcount - 1));
	}

	if (state->threatalertcount > 0) {
		state->threatalertcount--;
		memset(&state->threatalerts[state->threatalertcount], 0,
				sizeof(state->threatalerts[state->threatalertcount]));
	}
}

static s32 accessibilityTargetingThreatSoundBusy(
		const struct accessibilitytargetingstate *state, s32 frame60)
{
	return state->lastthreatsoundkind
				!= ACCESSIBILITY_TARGETING_THREAT_SOUND_NONE
			&& frame60 - state->lastthreatsound60
				< ACCESSIBILITY_TARGETING_THREAT_ALERT_DURATION_TICKS;
}

static void accessibilityTargetingUpdateThreatAlerts(
		const struct accessibilitytargetingobservation *observation)
{
	struct accessibilitytargetingstate *state
			= g_AccessibilityTargetingCurrentState;
	s32 i;

	if (!observation->threatdetectoractive) {
		if (state->threatdetectoractive || state->threatalertcount > 0) {
			accessibilityToneStopThreatAlert();
			accessibilityLogEvent("targeting", "threat_detector_alert_reset",
					"frame=%d reason=detector_inactive known=%d queued=%d alerts=%llu",
					observation->frame60, state->knownthreatcount,
					state->threatalertcount,
					(unsigned long long)state->threatalertscount);
		}

		state->threatdetectoractive = false;
		state->knownthreatcount = 0;
		state->threatalertcount = 0;
		state->nextthreatalert60 = 0;
		state->nextthreataimalert60 = 0;
		state->lastthreatsound60 = 0;
		state->lastthreatsoundkind = ACCESSIBILITY_TARGETING_THREAT_SOUND_NONE;
		memset(&state->lastthreatsoundidentity, 0,
				sizeof(state->lastthreatsoundidentity));
		memset(state->knownthreats, 0, sizeof(state->knownthreats));
		memset(state->threatalerts, 0, sizeof(state->threatalerts));
		return;
	}

	state->threatdetectoractive = true;

	for (i = 0; i < state->knownthreatcount; i++) {
		state->knownthreats[i].missingframes++;
	}

	for (i = 0; i < observation->threatcount; i++) {
		const struct accessibilitytargetingcandidate *threat
				= &observation->threats[i];
		s32 knownindex = -1;
		s32 newlyknown = false;
		s32 j;

		for (j = 0; j < state->knownthreatcount; j++) {
			if (accessibilityTargetingIdentityEqual(
					&state->knownthreats[j].identity,
					&threat->identity)) {
				knownindex = j;
				break;
			}
		}

		if (knownindex >= 0) {
			state->knownthreats[knownindex].missingframes = 0;
			continue;
		}

		if (state->knownthreatcount
				< ACCESSIBILITY_TARGETING_THREAT_KNOWN_CAPACITY) {
			knownindex = state->knownthreatcount++;
			state->knownthreats[knownindex].identity = threat->identity;
			state->knownthreats[knownindex].missingframes = 0;
			newlyknown = true;
		}

		if (newlyknown && !accessibilityTargetingThreatAlertQueued(
					state, &threat->identity)
				&& state->threatalertcount
					< ACCESSIBILITY_TARGETING_THREAT_ALERT_CAPACITY) {
			state->threatalerts[state->threatalertcount++].candidate = *threat;
			accessibilityLogEvent("targeting", "threat_detector_new",
					"frame=%d prop=%p propnum=%d category=%d native_slot=%d distance=%.3f screen=%.3f,%.3f,%.3f,%.3f queued=%d",
					observation->frame60, (void *)threat->prop,
					threat->identity.propnum, threat->category, i,
					threat->distance, threat->screenx1, threat->screeny1,
					threat->screenx2, threat->screeny2,
					state->threatalertcount);
		}
	}

	for (i = state->knownthreatcount - 1; i >= 0; i--) {
		if (state->knownthreats[i].missingframes
				> ACCESSIBILITY_TARGETING_THREAT_MISSING_GRACE_FRAMES) {
			if (i + 1 < state->knownthreatcount) {
				memmove(&state->knownthreats[i],
						&state->knownthreats[i + 1],
						sizeof(state->knownthreats[0])
							* (state->knownthreatcount - i - 1));
			}
			state->knownthreatcount--;
			memset(&state->knownthreats[state->knownthreatcount], 0,
					sizeof(state->knownthreats[state->knownthreatcount]));
		}
	}

	if (observation->frame60 >= state->nextthreatalert60
			&& !accessibilityTargetingThreatSoundBusy(
				state, observation->frame60)) {
		while (state->threatalertcount > 0) {
			s32 threatindex = accessibilityTargetingFindCurrentThreat(
					observation,
					&state->threatalerts[0].candidate.identity);

			if (threatindex >= 0) {
				const struct accessibilitytargetingcandidate *threat
						= &observation->threats[threatindex];
				f32 fulldistance;
				f32 fadedistance;
				f32 silentdistance;
				struct coord position = threat->position;
				s32 volume;
				s32 pan;
				f32 normalizedvolume;
				f32 normalizedpan;

				accessibilityGetEnemyTuning(&fulldistance, &fadedistance,
						&silentdistance, NULL);
				volume = psCalculateVolumeFromDistance(threat->distance,
						fulldistance, fadedistance, silentdistance,
						AL_VOL_FULL);
				pan = psCalculatePan(&position,
						fulldistance, fadedistance, silentdistance,
						threat->distance, false, NULL);
				normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
				normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
						/ (f32)AL_PAN_CENTER;
				accessibilityTonePlayThreatAlert(
						normalizedvolume, normalizedpan);
				state->threatalertscount++;
				state->lastthreatsound60 = observation->frame60;
				state->lastthreatsoundkind
						= ACCESSIBILITY_TARGETING_THREAT_SOUND_NEW;
				state->lastthreatsoundidentity = threat->identity;
				state->nextthreatalert60 = observation->frame60
						+ ACCESSIBILITY_TARGETING_THREAT_ALERT_GAP_TICKS;
				accessibilityLogEvent("targeting",
						"threat_detector_alert",
						"alert=%llu frame=%d next_frame=%d prop=%p propnum=%d category=%d distance=%.3f volume=%.4f pan=%.4f queued_after=%d",
						(unsigned long long)state->threatalertscount,
						observation->frame60, state->nextthreatalert60,
						(void *)threat->prop, threat->identity.propnum,
						threat->category, threat->distance,
						normalizedvolume, normalizedpan,
						state->threatalertcount - 1);
				accessibilityTargetingPopThreatAlert(state);
				break;
			}

			accessibilityLogEvent("targeting",
					"threat_detector_alert_drop",
					"frame=%d propnum=%d reason=no_longer_visible queued_after=%d",
					observation->frame60,
					state->threatalerts[0].candidate.identity.propnum,
					state->threatalertcount - 1);
			accessibilityTargetingPopThreatAlert(state);
		}
	}
}

static void accessibilityTargetingUpdateThreatAim(
		const struct accessibilitytargetingobservation *observation,
		s32 aimedvalid,
		const struct accessibilitytargetingcandidate *aimedcandidate)
{
	struct accessibilitytargetingstate *state
			= g_AccessibilityTargetingCurrentState;
	const struct accessibilitytargetingcandidate *threat = NULL;
	s32 changed;
	s32 i;

	if (observation->threatdetectoractive && aimedvalid && aimedcandidate) {
		for (i = 0; i < observation->threatcount; i++) {
			if (observation->threats[i].prop == aimedcandidate->prop) {
				threat = &observation->threats[i];
				break;
			}
		}
	}

	if (!threat) {
		if (state->hasaimedthreat) {
			accessibilityLogEvent("targeting", "threat_detector_aim_stop",
					"frame=%d propnum=%d pulses=%llu reason=%s",
					observation->frame60,
					state->aimedthreatidentity.propnum,
					(unsigned long long)state->threataimpulsecount,
					observation->threatdetectoractive
						? "aim_left_threat" : "detector_inactive");
			if (state->lastthreatsoundkind
					== ACCESSIBILITY_TARGETING_THREAT_SOUND_AIM) {
				accessibilityToneStopThreatAlert();
				state->lastthreatsoundkind
						= ACCESSIBILITY_TARGETING_THREAT_SOUND_NONE;
				memset(&state->lastthreatsoundidentity, 0,
						sizeof(state->lastthreatsoundidentity));
			}
		}

		state->hasaimedthreat = false;
		state->nextthreataimalert60 = 0;
		memset(&state->aimedthreatidentity, 0,
				sizeof(state->aimedthreatidentity));
		return;
	}

	changed = !state->hasaimedthreat
			|| !accessibilityTargetingIdentityEqual(
				&state->aimedthreatidentity, &threat->identity);

	if (changed) {
		state->hasaimedthreat = true;
		state->aimedthreatidentity = threat->identity;
		state->nextthreataimalert60 = observation->frame60;
		accessibilityLogEvent("targeting", "threat_detector_aim_start",
				"frame=%d prop=%p propnum=%d category=%d distance=%.3f",
				observation->frame60, (void *)threat->prop,
				threat->identity.propnum, threat->category,
				threat->distance);
	}

	if (observation->frame60 >= state->nextthreataimalert60) {
		s32 soundbusy = accessibilityTargetingThreatSoundBusy(
				state, observation->frame60);
		s32 recentnewalert = soundbusy
				&& state->lastthreatsoundkind
					== ACCESSIBILITY_TARGETING_THREAT_SOUND_NEW
				&& accessibilityTargetingIdentityEqual(
					&state->lastthreatsoundidentity, &threat->identity);
		f32 fulldistance;
		f32 fadedistance;
		f32 silentdistance;
		struct coord position = threat->position;
		s32 volume;
		s32 pan;
		f32 normalizedvolume;
		f32 normalizedpan;

		accessibilityGetEnemyTuning(&fulldistance, &fadedistance,
				&silentdistance, NULL);
		volume = psCalculateVolumeFromDistance(threat->distance,
				fulldistance, fadedistance, silentdistance, AL_VOL_FULL);
		pan = psCalculatePan(&position,
				fulldistance, fadedistance, silentdistance,
				threat->distance, false, NULL);
		normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
		normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;

		if (soundbusy && !recentnewalert) {
			state->nextthreataimalert60 = state->lastthreatsound60
					+ ACCESSIBILITY_TARGETING_THREAT_ALERT_DURATION_TICKS;
			return;
		}

		if (!recentnewalert) {
			accessibilityTonePlayThreatAlert(
					normalizedvolume, normalizedpan);
			state->lastthreatsound60 = observation->frame60;
			state->lastthreatsoundkind
					= ACCESSIBILITY_TARGETING_THREAT_SOUND_AIM;
			state->lastthreatsoundidentity = threat->identity;
		}

		state->threataimpulsecount++;
		state->nextthreataimalert60 = (recentnewalert
					? state->lastthreatsound60 : observation->frame60)
				+ ACCESSIBILITY_TARGETING_THREAT_AIM_REPEAT_TICKS;
		accessibilityLogEvent("targeting", "threat_detector_aim_pulse",
				"pulse=%llu frame=%d next_frame=%d prop=%p propnum=%d distance=%.3f volume=%.4f pan=%.4f reused_new_alert=%d",
				(unsigned long long)state->threataimpulsecount,
				observation->frame60, state->nextthreataimalert60,
				(void *)threat->prop, threat->identity.propnum,
				threat->distance, normalizedvolume, normalizedpan,
				recentnewalert);
	}
}

static s32 accessibilityTargetingPropNum(const struct prop *prop)
{
	uintptr_t address;
	uintptr_t first;
	uintptr_t end;

	if (!prop || !g_Vars.props || g_Vars.maxprops <= 0) {
		return -1;
	}

	address = (uintptr_t)prop;
	first = (uintptr_t)g_Vars.props;
	end = first + sizeof(struct prop) * (uintptr_t)g_Vars.maxprops;

	if (address < first || address >= end
			|| (address - first) % sizeof(struct prop) != 0) {
		return -1;
	}

	return (s32)((address - first) / sizeof(struct prop));
}

static s32 accessibilityTargetingCandidateValid(
		const struct accessibilitytargetingcandidate *candidate)
{
	if (!candidate || !candidate->prop) {
		return false;
	}

	if (accessibilityTargetingPropNum(candidate->prop) != candidate->identity.propnum) {
		return false;
	}

	return candidate->prop->type == candidate->identity.proptype
			&& (uintptr_t)candidate->prop->obj == candidate->identity.objectidentity;
}

static void accessibilityTargetingStopPresence(const char *reason)
{
	s32 procedural = g_AccessibilityTargetingProceduralPresenceActive;

	if (procedural) {
		accessibilityToneStopTargetPresence();
	}

	if (procedural) {
		accessibilityLogEvent("targeting", "presence_stop",
				"reason=%s lane=procedural_combat_timbre",
				reason);
	}

	g_AccessibilityTargetingProceduralPresenceActive = false;
}

static void accessibilityTargetingStopCombatPresence(const char *reason)
{
	s32 count = accessibilityTargetingCombatSlotCount();

	accessibilityToneStopCombat();

	if (count > 0) {
		accessibilityLogEvent("targeting", "combat_presence_stop",
				"reason=%s assigned_slots=%d capacity=%d",
				reason, count, ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT);
	}

	memset(g_AccessibilityTargetingCombatSlots, 0,
			sizeof(g_AccessibilityTargetingCurrentState->combatslots));
}

static void accessibilityTargetingStopAlignment(const char *reason)
{
	if (g_AccessibilityTargetingAlignmentActive) {
		accessibilityToneSetAlignment(0, 0.0f, false);
		accessibilityLogEvent("targeting", "alignment_stop",
				"reason=%s frequency_hz=%.2f quality=%.4f distance=%.3f interrupted=%d obstruction=%d updates=%llu",
				reason, g_AccessibilityTargetingAlignmentFrequencyHz,
				g_AccessibilityTargetingAlignmentQuality,
				g_AccessibilityTargetingAlignmentDistance,
				g_AccessibilityTargetingAlignmentInterrupted,
				g_AccessibilityTargetingAlignmentObstruction,
				(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount);
	}

	g_AccessibilityTargetingAlignmentActive = false;
	g_AccessibilityTargetingAlignmentInterrupted = false;
	g_AccessibilityTargetingAlignmentObstruction
			= ACCESSIBILITY_TARGETING_OBSTRUCTION_NONE;
	g_AccessibilityTargetingAlignmentFrequencyHz = 0.0f;
	g_AccessibilityTargetingAlignmentQuality = 0.0f;
	g_AccessibilityTargetingAlignmentDistance = 0.0f;
	g_AccessibilityTargetingNextAlignmentLog60 = 0;
}

static s32 accessibilityTargetingFindRecord(
		const struct accessibilitytargetingidentity *identity)
{
	s32 i;

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		if (accessibilityTargetingIdentityEqual(
				identity, &g_AccessibilityTargetingRecords[i].candidate.identity)) {
			return i;
		}
	}

	return -1;
}

static void accessibilityTargetingCopyCandidate(
		struct accessibilitytargetingrecord *record,
		const struct accessibilitytargetingcandidate *candidate)
{
	record->candidate = *candidate;
	record->localizedname[0] = '\0';

	if (candidate->localizedname) {
		strncpy(record->localizedname, candidate->localizedname,
				sizeof(record->localizedname) - 1);
		record->localizedname[sizeof(record->localizedname) - 1] = '\0';
		record->candidate.localizedname = record->localizedname;
	} else {
		record->candidate.localizedname = NULL;
	}
}

static s32 accessibilityTargetingRecordCompare(const void *avalue, const void *bvalue)
{
	const struct accessibilitytargetingrecord *a = avalue;
	const struct accessibilitytargetingrecord *b = bvalue;
	s32 aaimed = g_AccessibilityTargetingHasAimedIdentity
			&& accessibilityTargetingIdentityEqual(&a->candidate.identity,
				&g_AccessibilityTargetingAimedIdentity);
	s32 baimed = g_AccessibilityTargetingHasAimedIdentity
			&& accessibilityTargetingIdentityEqual(&b->candidate.identity,
				&g_AccessibilityTargetingAimedIdentity);

	if (aaimed != baimed) {
		return baimed - aaimed;
	}

	if (a->candidate.horizontalscreenoffset < b->candidate.horizontalscreenoffset) {
		return -1;
	}

	if (a->candidate.horizontalscreenoffset > b->candidate.horizontalscreenoffset) {
		return 1;
	}

	if (a->candidate.distance < b->candidate.distance) {
		return -1;
	}

	if (a->candidate.distance > b->candidate.distance) {
		return 1;
	}

	if (a->candidate.identity.sourceslot != b->candidate.identity.sourceslot) {
		return a->candidate.identity.sourceslot - b->candidate.identity.sourceslot;
	}

	return a->candidate.identity.propnum - b->candidate.identity.propnum;
}

static void accessibilityTargetingMerge(
		const struct accessibilitytargetingobservation *observation)
{
	s32 matched[ACCESSIBILITY_TARGETING_MAX_CANDIDATES] = { 0 };
	s32 writeindex;
	s32 i;

	for (i = 0; i < observation->candidatecount; i++) {
		const struct accessibilitytargetingcandidate *candidate = &observation->candidates[i];
		s32 index = accessibilityTargetingFindRecord(&candidate->identity);

		if (index < 0 && g_AccessibilityTargetingRecordCount
				< ACCESSIBILITY_TARGETING_MAX_CANDIDATES) {
			index = g_AccessibilityTargetingRecordCount++;
			memset(&g_AccessibilityTargetingRecords[index], 0,
					sizeof(g_AccessibilityTargetingRecords[index]));
		}

		if (index >= 0) {
			struct accessibilitytargetingrecord *record
					= &g_AccessibilityTargetingRecords[index];
			s32 aimed = observation->hasaimedtarget
					&& accessibilityTargetingIdentityEqual(&candidate->identity,
						&observation->aimedidentity);

			accessibilityTargetingCopyCandidate(record, candidate);
			record->seenframes++;
			if (record->seenframes > g_AccessibilityTargetingCurrentPolicy->visibleframes) {
				record->seenframes = g_AccessibilityTargetingCurrentPolicy->visibleframes;
			}
			if (aimed) {
				record->seenframes = g_AccessibilityTargetingCurrentPolicy->visibleframes;
			}
			record->missingframes = 0;
			matched[index] = true;
		}
	}

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		if (!matched[i]) {
			g_AccessibilityTargetingRecords[i].missingframes++;
		}
	}

	writeindex = 0;
	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		if (g_AccessibilityTargetingRecords[i].missingframes
				< g_AccessibilityTargetingCurrentPolicy->missingframes) {
			if (writeindex != i) {
				g_AccessibilityTargetingRecords[writeindex]
						= g_AccessibilityTargetingRecords[i];
			}
			writeindex++;
		}
	}
	g_AccessibilityTargetingRecordCount = writeindex;

	qsort(g_AccessibilityTargetingRecords, g_AccessibilityTargetingRecordCount,
			sizeof(g_AccessibilityTargetingRecords[0]),
			accessibilityTargetingRecordCompare);

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		g_AccessibilityTargetingRecords[i].candidate.localizedname
				= g_AccessibilityTargetingRecords[i].localizedname[0]
					? g_AccessibilityTargetingRecords[i].localizedname : NULL;
	}
}

static s32 accessibilityTargetingEligibleCount(void)
{
	s32 count = 0;
	s32 i;

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		struct accessibilitytargetingrecord *record = &g_AccessibilityTargetingRecords[i];

		if (record->seenframes >= g_AccessibilityTargetingCurrentPolicy->visibleframes
				&& record->missingframes == 0
				&& accessibilityTargetingCandidateValid(&record->candidate)) {
			count++;
		}
	}

	return count;
}

static struct accessibilitytargetingrecord *accessibilityTargetingNextPresence(void)
{
	s32 start = 0;
	s32 i;

	if (g_AccessibilityTargetingHasLastPulseIdentity) {
		for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
			if (accessibilityTargetingIdentityEqual(
					&g_AccessibilityTargetingRecords[i].candidate.identity,
					&g_AccessibilityTargetingLastPulseIdentity)) {
				start = i + 1;
				break;
			}
		}
	}

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		s32 index = (start + i) % g_AccessibilityTargetingRecordCount;
		struct accessibilitytargetingrecord *record = &g_AccessibilityTargetingRecords[index];

		if (record->seenframes >= g_AccessibilityTargetingCurrentPolicy->visibleframes
				&& record->missingframes == 0
				&& accessibilityTargetingCandidateValid(&record->candidate)) {
			return record;
		}
	}

	return NULL;
}

static s32 accessibilityTargetingRecordEligible(
		const struct accessibilitytargetingrecord *record)
{
	return record
			&& record->seenframes >= g_AccessibilityTargetingCurrentPolicy->visibleframes
			&& record->missingframes == 0
			&& accessibilityTargetingCandidateValid(&record->candidate);
}

static s32 accessibilityTargetingRecordPresenceEligible(
		const struct accessibilitytargetingrecord *record)
{
	return accessibilityTargetingRecordEligible(record)
			&& !record->candidate.aimonly;
}

static s32 accessibilityTargetingPrecisionCandidateEligible(
		const struct accessibilitytargetingrecord *record)
{
	return accessibilityTargetingRecordPresenceEligible(record)
			&& record->candidate.precisionaimavailable
			&& record->candidate.hasscreenaimerror
			&& record->candidate.relationship
					== ACCESSIBILITY_TARGETING_RELATIONSHIP_HOSTILE;
}

static f32 accessibilityTargetingPrecisionScore(
		const struct accessibilitytargetingcandidate *candidate)
{
	f32 x = candidate->horizontalaimerrornormalized;
	f32 y = candidate->verticalaimerrornormalized;

	return sqrtf(x * x + y * y);
}

static void accessibilityTargetingUpdatePrecisionGuidance(
		const struct accessibilitytargetingobservation *observation)
{
	struct accessibilitytargetingrecord *best = NULL;
	struct accessibilitytargetingrecord *current = NULL;
	f32 bestscore = 0.0f;
	f32 currentscore = 0.0f;
	s32 i;

	if (!observation->precisionguidanceactive) {
		if (g_AccessibilityTargetingCurrentState->precisionguidanceactive) {
			accessibilityLogEvent("targeting", "precision_guidance_stop",
					"frame=%d reason=manual_aim_inactive propnum=%d",
					observation->frame60,
					g_AccessibilityTargetingCurrentState
							->precisionguidanceidentity.propnum);
		}
		g_AccessibilityTargetingCurrentState->precisionguidanceactive = false;
		memset(&g_AccessibilityTargetingCurrentState->precisionguidanceidentity,
				0, sizeof(g_AccessibilityTargetingCurrentState
						->precisionguidanceidentity));
		return;
	}

	for (i = 0; i < g_AccessibilityTargetingRecordCount; i++) {
		struct accessibilitytargetingrecord *record
				= &g_AccessibilityTargetingRecords[i];
		f32 score;

		if (!accessibilityTargetingPrecisionCandidateEligible(record)) {
			continue;
		}

		score = accessibilityTargetingPrecisionScore(&record->candidate);
		if (!best || score < bestscore) {
			best = record;
			bestscore = score;
		}
		if (g_AccessibilityTargetingCurrentState->precisionguidanceactive
				&& accessibilityTargetingIdentityEqual(&record->candidate.identity,
					&g_AccessibilityTargetingCurrentState
							->precisionguidanceidentity)) {
			current = record;
			currentscore = score;
		}
	}

	if (current && best != current
			&& bestscore + ACCESSIBILITY_TARGETING_PRECISION_SWITCH_MARGIN
					>= currentscore) {
		best = current;
		bestscore = currentscore;
	}

	if (!best) {
		if (g_AccessibilityTargetingCurrentState->precisionguidanceactive) {
			accessibilityLogEvent("targeting", "precision_guidance_stop",
					"frame=%d reason=no_visible_hostile propnum=%d",
					observation->frame60,
					g_AccessibilityTargetingCurrentState
							->precisionguidanceidentity.propnum);
		}
		g_AccessibilityTargetingCurrentState->precisionguidanceactive = false;
		memset(&g_AccessibilityTargetingCurrentState->precisionguidanceidentity,
				0, sizeof(g_AccessibilityTargetingCurrentState
						->precisionguidanceidentity));
		return;
	}

	if (!g_AccessibilityTargetingCurrentState->precisionguidanceactive
			|| !accessibilityTargetingIdentityEqual(&best->candidate.identity,
				&g_AccessibilityTargetingCurrentState
						->precisionguidanceidentity)) {
		accessibilityLogEvent("targeting", "precision_guidance_select",
				"frame=%d propnum=%d horizontal_error=%.4f vertical_error=%.4f score=%.4f retained=%d anchor_source=%d anchor_hitpart=%d anchor_node=%p anchors_examined=%d target_screen=%.2f,%.2f",
				observation->frame60, best->candidate.identity.propnum,
				best->candidate.horizontalaimerrornormalized,
				best->candidate.verticalaimerrornormalized, bestscore,
				current == best, best->candidate.precisionaimsource,
				best->candidate.precisionaimhitpart,
				(void *)best->candidate.precisionaimnode,
				best->candidate.precisionaimnodesexamined,
				best->candidate.precisionaimscreenx,
				best->candidate.precisionaimscreeny);
	}
	g_AccessibilityTargetingCurrentState->precisionguidanceactive = true;
	g_AccessibilityTargetingCurrentState->precisionguidanceidentity
			= best->candidate.identity;
}

static s32 accessibilityTargetingFindCombatSlot(
		const struct accessibilitytargetingidentity *identity)
{
	s32 slot;

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		if (g_AccessibilityTargetingCombatSlots[slot].assigned
				&& accessibilityTargetingIdentityEqual(identity,
					&g_AccessibilityTargetingCombatSlots[slot].identity)) {
			return slot;
		}
	}

	return -1;
}

static void accessibilityTargetingCombatCadence(f32 distance, f32 reference,
		s32 previouszone, s32 *periodms, s32 *durationms, s32 *zone,
		f32 *proximity)
{
	f32 value;
	f32 closethreshold;

	if (reference < 1.0f) {
		reference = 60.0f;
	}

	closethreshold = previouszone == 2
			? reference * ACCESSIBILITY_TARGETING_COMBAT_CLOSE_EXIT_SCALE
			: reference;

	if (distance <= closethreshold) {
		value = 1.0f;
		*zone = 2;
	} else if (distance >= reference * 5.0f) {
		value = 0.0f;
		*zone = 0;
	} else {
		value = (reference * 5.0f - distance) / (reference * 4.0f);
		*zone = 1;
	}

	*periodms = (s32)(ACCESSIBILITY_TARGETING_COMBAT_FAR_PERIOD_MS
			+ (ACCESSIBILITY_TARGETING_COMBAT_CLOSE_PERIOD_MS
					- ACCESSIBILITY_TARGETING_COMBAT_FAR_PERIOD_MS) * value
			+ 0.5f);
	*durationms = (s32)(ACCESSIBILITY_TARGETING_COMBAT_FAR_DURATION_MS
			+ (ACCESSIBILITY_TARGETING_COMBAT_CLOSE_DURATION_MS
					- ACCESSIBILITY_TARGETING_COMBAT_FAR_DURATION_MS) * value
			+ 0.5f);
	*proximity = value;
}

static s32 accessibilityTargetingCalculateVolumeFromDistance(f32 distance,
		f32 fulldistance, f32 fadedistance, f32 silentdistance,
		s32 fullvolume)
{
	f32 result;

	if (distance >= silentdistance || silentdistance <= 0.0f) {
		return 0;
	}
	if (distance <= fulldistance || fadedistance <= fulldistance) {
		return fullvolume;
	}
	if (distance < fadedistance) {
		f32 fraction = (distance - fulldistance)
				/ (fadedistance - fulldistance);
		result = fullvolume - sqrtf(fraction) * (fullvolume - 1000.0f);
	} else if (silentdistance > fadedistance) {
		result = (silentdistance - distance) * 1000.0f
				/ (silentdistance - fadedistance);
	} else {
		result = 0.0f;
	}

	if (result > fullvolume) {
		result = fullvolume;
	}
	if (result < 40.0f) {
		return 0;
	}
	return (s32)result;
}

static f32 accessibilityTargetingCombatElevationFrequency(
		const struct accessibilitytargetingcandidate *candidate,
		f32 basefrequency, f32 *rawelevationdegrees,
		f32 *elevationdegrees, s32 *elevationzone)
{
	f32 angle = candidate->hasverticalaimerror
			? candidate->verticalaimerrordegrees : 0.0f;
	f32 normalized;
	f32 multiplier;

	/*
	 * The adapter captures this angle with the projected target and crosspos
	 * before rendering. Do not recompute it here after camera/render state may
	 * have advanced.
	 */
	*rawelevationdegrees = angle;

	if (angle > ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES) {
		angle = ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES;
	} else if (angle < -ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES) {
		angle = -ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES;
	}

	if (angle > ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES) {
		*elevationzone = 1;
		normalized = (angle
				- ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES)
				/ (ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES
					- ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES);
		multiplier = powf(
				ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_ABOVE_MULTIPLIER,
				normalized);
	} else if (angle
			< -ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES) {
		*elevationzone = -1;
		normalized = (-angle
				- ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES)
				/ (ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_LIMIT_DEGREES
					- ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_DEADZONE_DEGREES);
		multiplier = powf(
				ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_BELOW_MULTIPLIER,
				normalized);
	} else {
		*elevationzone = 0;
		multiplier = 1.0f;
	}

	*elevationdegrees = angle;
	return basefrequency * multiplier;
}

static f32 accessibilityTargetingPrecisionFrequency(
		const struct accessibilitytargetingcandidate *candidate,
		f32 basefrequency, s32 *elevationzone)
{
	f32 offset = candidate->verticalaimerrornormalized;
	f32 magnitude = fabsf(offset);
	f32 normalized;
	f32 multiplier;

	if (magnitude <= ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_DEADZONE) {
		*elevationzone = 0;
		return basefrequency;
	}

	if (magnitude > ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_LIMIT) {
		magnitude = ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_LIMIT;
	}
	normalized = (magnitude
			- ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_DEADZONE)
			/ (ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_LIMIT
				- ACCESSIBILITY_TARGETING_PRECISION_VERTICAL_DEADZONE);

	if (offset > 0.0f) {
		*elevationzone = 1;
		multiplier = powf(
				ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_ABOVE_MULTIPLIER,
				normalized);
	} else {
		*elevationzone = -1;
		multiplier = powf(
				ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_BELOW_MULTIPLIER,
				normalized);
	}

	return basefrequency * multiplier;
}

static void accessibilityTargetingUpdateCombatPresence(s32 frame60,
		f32 distancecuereference)
{
	s32 slot;
	s32 index;
	f32 combatfrequency = accessibilityGetEnemyFrequency();

	for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
		struct accessibilitytargetingcombatslot *voice
				= &g_AccessibilityTargetingCombatSlots[slot];
		s32 recordindex;

		if (!voice->assigned) {
			continue;
		}

		recordindex = accessibilityTargetingFindRecord(&voice->identity);
		if (recordindex < 0 || !accessibilityTargetingRecordPresenceEligible(
				&g_AccessibilityTargetingRecords[recordindex])) {
			accessibilityToneSetCombatSlot(slot, false,
					combatfrequency, combatfrequency,
					0.0f, 0.0f,
					ACCESSIBILITY_TARGETING_COMBAT_FAR_PERIOD_MS,
					ACCESSIBILITY_TARGETING_COMBAT_FAR_DURATION_MS,
					ACCESSIBILITY_TONE_COMBAT_CONTOUR_LINEAR,
					false, true, false);
			accessibilityLogEvent("targeting", "combat_slot_release",
					"frame=%d oscillator_slot=%d source=%d slot=%d propnum=%d reason=target_unavailable",
					frame60, slot, voice->identity.source,
					voice->identity.sourceslot, voice->identity.propnum);
			memset(voice, 0, sizeof(*voice));
		}
	}

	for (index = 0; index < g_AccessibilityTargetingRecordCount; index++) {
		struct accessibilitytargetingrecord *record
				= &g_AccessibilityTargetingRecords[index];
		struct accessibilitytargetingcombatslot *voice;
		s32 restart = false;
		s32 triggernow = false;
		s32 volume;
		s32 pan;
		s32 periodms;
		s32 durationms;
		s32 distancezone;
		f32 normalizedvolume;
		f32 normalizedpan;
		f32 proximity;
		f32 cuedistance;
		f32 startfrequency;
		f32 endfrequency;
		f32 rawelevationdegrees = 0.0f;
		f32 elevationdegrees = 0.0f;
		f32 elevationfrequency;
		s32 elevationzone = 0;
		s32 camera;
		s32 precisionguidance;
		s32 frequencycontour = ACCESSIBILITY_TONE_COMBAT_CONTOUR_LINEAR;

		if (!accessibilityTargetingRecordPresenceEligible(record)) {
			continue;
		}

		camera = record->candidate.category
				== ACCESSIBILITY_TARGETING_CATEGORY_SECURITY_CAMERA;
		precisionguidance
				= g_AccessibilityTargetingCurrentState->precisionguidanceactive
				&& accessibilityTargetingIdentityEqual(
					&record->candidate.identity,
					&g_AccessibilityTargetingCurrentState
							->precisionguidanceidentity);
		elevationfrequency = camera ? combatfrequency
				: accessibilityTargetingCombatElevationFrequency(
					&record->candidate, combatfrequency,
					&rawelevationdegrees,
					&elevationdegrees, &elevationzone);
		if (precisionguidance) {
			elevationfrequency = accessibilityTargetingPrecisionFrequency(
					&record->candidate, combatfrequency, &elevationzone);
		}
		startfrequency = camera && !precisionguidance
				? ACCESSIBILITY_TARGETING_CAMERA_START_FREQUENCY_HZ
				: elevationfrequency;
		endfrequency = camera && !precisionguidance
				? ACCESSIBILITY_TARGETING_CAMERA_END_FREQUENCY_HZ
				: elevationfrequency;
		slot = accessibilityTargetingFindCombatSlot(&record->candidate.identity);
		if (slot < 0) {
			for (slot = 0; slot < ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT; slot++) {
				if (!g_AccessibilityTargetingCombatSlots[slot].assigned) {
					break;
				}
			}

			if (slot >= ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT) {
				continue;
			}

			voice = &g_AccessibilityTargetingCombatSlots[slot];
			voice->assigned = true;
			voice->identity = record->candidate.identity;
			voice->frequencyhz = elevationfrequency;
			voice->elevationzone = elevationzone;
			restart = true;
			accessibilityLogEvent("targeting", "combat_slot_assign",
					"frame=%d oscillator_slot=%d source=%d slot=%d propnum=%d prop=%p category=%d cue=%s raw_elevation_degrees=%.2f elevation_degrees=%.2f elevation_zone=%s target_screen=%.2f,%.2f aim_screen=%.2f,%.2f precision_anchor_source=%d precision_anchor_hitpart=%d precision_anchor_node=%p precision_anchor_nodes_examined=%d base_frequency_hz=%.1f target_frequency_hz=%.1f punch_range=%.3f",
					frame60, slot, voice->identity.source,
					voice->identity.sourceslot, voice->identity.propnum,
					(void *)record->candidate.prop,
					record->candidate.category,
					precisionguidance ? "manual_aim_precision"
						: camera ? "security_camera_sweep"
						: "enemy_proximity",
					rawelevationdegrees,
					elevationdegrees,
					elevationzone > 0 ? "above"
						: elevationzone < 0 ? "below" : "level",
					precisionguidance
						? record->candidate.precisionaimscreenx
						: (record->candidate.screenx1
							+ record->candidate.screenx2) * 0.5f,
					precisionguidance
						? record->candidate.precisionaimscreeny
						: (record->candidate.screeny1
							+ record->candidate.screeny2) * 0.5f,
					record->candidate.aimscreenx,
					record->candidate.aimscreeny,
					record->candidate.precisionaimsource,
					record->candidate.precisionaimhitpart,
					(void *)record->candidate.precisionaimnode,
					record->candidate.precisionaimnodesexamined,
					combatfrequency, elevationfrequency,
					distancecuereference);
		}

		voice = &g_AccessibilityTargetingCombatSlots[slot];
		if (!camera || precisionguidance) {
			voice->frequencyhz += (elevationfrequency - voice->frequencyhz)
					* (precisionguidance
						? ACCESSIBILITY_TARGETING_PRECISION_FREQUENCY_SMOOTHING
						: ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_SMOOTHING);
		}

		volume = accessibilityTargetingCalculateVolumeFromDistance(
				record->candidate.distance,
				g_AccessibilityTargetingCurrentPolicy->fulldistance,
				g_AccessibilityTargetingCurrentPolicy->fadedistance,
				g_AccessibilityTargetingCurrentPolicy->silentdistance,
				AL_VOL_FULL);
		pan = psCalculatePan(&record->candidate.position,
				g_AccessibilityTargetingCurrentPolicy->fulldistance,
				g_AccessibilityTargetingCurrentPolicy->fadedistance,
				g_AccessibilityTargetingCurrentPolicy->silentdistance,
				record->candidate.distance, false, NULL);
		normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
		normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
				/ (f32)AL_PAN_CENTER;
		if (precisionguidance) {
			normalizedpan = record->candidate.horizontalaimerrornormalized
					* ACCESSIBILITY_TARGETING_PRECISION_PAN_SCALE;
			if (normalizedpan < -1.0f) {
				normalizedpan = -1.0f;
			} else if (normalizedpan > 1.0f) {
				normalizedpan = 1.0f;
			}
		}
		restart |= normalizedvolume > 0.0f && !voice->audible;
		cuedistance = record->candidate.hasdistancecue
				? record->candidate.distancecue : record->candidate.distance;
		if (precisionguidance) {
			periodms = ACCESSIBILITY_TARGETING_PRECISION_PERIOD_MS;
			durationms = ACCESSIBILITY_TARGETING_PRECISION_DURATION_MS;
			distancezone = 0;
			proximity = 0.0f;
		} else if (camera) {
			periodms = ACCESSIBILITY_TARGETING_CAMERA_PERIOD_MS;
			durationms = ACCESSIBILITY_TARGETING_CAMERA_DURATION_MS;
			distancezone = 0;
			proximity = 0.0f;
		} else {
			accessibilityTargetingCombatCadence(cuedistance,
					distancecuereference, voice->distancezone,
					&periodms, &durationms,
					&distancezone, &proximity);
		}
		if (camera && !precisionguidance) {
			startfrequency = ACCESSIBILITY_TARGETING_CAMERA_START_FREQUENCY_HZ;
			endfrequency = ACCESSIBILITY_TARGETING_CAMERA_END_FREQUENCY_HZ;
		} else if (distancezone == 2 && !precisionguidance) {
			startfrequency = voice->frequencyhz;
			endfrequency = voice->frequencyhz;
		} else {
			startfrequency = combatfrequency;
			endfrequency = voice->frequencyhz;
			frequencycontour
					= ACCESSIBILITY_TONE_COMBAT_CONTOUR_BASE_THEN_END;
		}
		if (precisionguidance && !voice->precisionguidance) {
			triggernow = true;
		}
		if (voice->triggerperiodms <= 0) {
			voice->triggerperiodms = periodms;
		} else if (periodms > voice->triggerperiodms) {
			voice->triggerperiodms = periodms;
		} else if (periodms <= voice->triggerperiodms
				- ACCESSIBILITY_TARGETING_COMBAT_TRIGGER_PERIOD_DELTA_MS) {
			triggernow = true;
			voice->triggerperiodms = periodms;
		}

		if (voice->distancezone == 2 && distancezone != 2) {
			triggernow = true;
		}
		if (restart || triggernow || distancezone != voice->distancezone
				|| elevationzone != voice->elevationzone
#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
				|| frame60 >= voice->nextcadencelog60
#endif
		) {
			accessibilityLogEvent("targeting", "combat_slot_cadence",
					"frame=%d oscillator_slot=%d propnum=%d category=%d cue=%s center_distance=%.3f cue_distance=%.3f cue_distance_available=%d punch_range=%.3f punch_range_exit=%.3f far_threshold=%.3f zone=%s proximity=%.4f period_ms=%d duration_ms=%d contour=%s continuous=%d trigger_now=%d raw_elevation_degrees=%.2f elevation_degrees=%.2f elevation_zone=%s target_screen=%.2f,%.2f aim_screen=%.2f,%.2f precision_anchor_source=%d precision_anchor_hitpart=%d precision_anchor_node=%p precision_anchor_nodes_examined=%d base_frequency_hz=%.1f target_frequency_hz=%.1f start_frequency_hz=%.1f end_frequency_hz=%.1f volume=%.4f pan=%.4f",
					frame60, slot, voice->identity.propnum,
					record->candidate.category,
					precisionguidance ? "manual_aim_precision"
						: camera ? "security_camera_sweep"
						: "enemy_proximity",
					record->candidate.distance, cuedistance,
					record->candidate.hasdistancecue, distancecuereference,
					distancecuereference
							* ACCESSIBILITY_TARGETING_COMBAT_CLOSE_EXIT_SCALE,
					distancecuereference * 5.0f,
					distancezone == 2 ? "punch_range"
						: distancezone == 1 ? "ramping" : "far",
					proximity, periodms, durationms,
					camera && !precisionguidance ? "camera_sweep"
						: frequencycontour
								== ACCESSIBILITY_TONE_COMBAT_CONTOUR_BASE_THEN_END
							? "base_then_elevation"
						: "continuous_elevation",
					!camera && !precisionguidance && distancezone == 2,
					triggernow,
					rawelevationdegrees,
					elevationdegrees,
					elevationzone > 0 ? "above"
						: elevationzone < 0 ? "below" : "level",
					precisionguidance
						? record->candidate.precisionaimscreenx
						: (record->candidate.screenx1
							+ record->candidate.screenx2) * 0.5f,
					precisionguidance
						? record->candidate.precisionaimscreeny
						: (record->candidate.screeny1
							+ record->candidate.screeny2) * 0.5f,
					record->candidate.aimscreenx,
					record->candidate.aimscreeny,
					record->candidate.precisionaimsource,
					record->candidate.precisionaimhitpart,
					(void *)record->candidate.precisionaimnode,
					record->candidate.precisionaimnodesexamined,
					combatfrequency, elevationfrequency,
					startfrequency, endfrequency,
					normalizedvolume, normalizedpan);
			voice->nextcadencelog60 = frame60 + TICKS(60);
		}
		accessibilityToneSetCombatSlot(slot, true,
				startfrequency, endfrequency,
				normalizedvolume, normalizedpan, periodms, durationms,
				frequencycontour,
				!camera && !precisionguidance && distancezone == 2,
				restart, triggernow);
		voice->audible = normalizedvolume > 0.0f;
		voice->periodms = periodms;
		voice->durationms = durationms;
		voice->distancezone = distancezone;
		voice->elevationzone = elevationzone;
		voice->precisionguidance = precisionguidance;
	}
}

static void accessibilityTargetingPulsePresence(s32 frame60)
{
	struct accessibilitytargetingrecord *record = accessibilityTargetingNextPresence();
	s32 count = accessibilityTargetingEligibleCount();
	s32 interval;
	s32 volume;
	s32 pan;
	f32 normalizedvolume;
	f32 normalizedpan;
	f32 frequencyhz;

	if (!record || count <= 0) {
		accessibilityTargetingStopPresence("no_eligible_target");
		return;
	}

	interval = g_AccessibilityTargetingCurrentPolicy->basecycleticks / count;
	if (interval < g_AccessibilityTargetingCurrentPolicy->minslotticks) {
		interval = g_AccessibilityTargetingCurrentPolicy->minslotticks;
	}

	volume = psCalculateVolumeFromDistance(record->candidate.distance,
			g_AccessibilityTargetingCurrentPolicy->fulldistance,
			g_AccessibilityTargetingCurrentPolicy->fadedistance,
			g_AccessibilityTargetingCurrentPolicy->silentdistance,
			AL_VOL_FULL);
	pan = psCalculatePan(&record->candidate.position,
			g_AccessibilityTargetingCurrentPolicy->fulldistance,
			g_AccessibilityTargetingCurrentPolicy->fadedistance,
			g_AccessibilityTargetingCurrentPolicy->silentdistance,
			record->candidate.distance, false, NULL);
	normalizedvolume = (f32)volume / (f32)AL_VOL_FULL;
	normalizedpan = ((f32)pan - (f32)AL_PAN_CENTER)
			/ (f32)AL_PAN_CENTER;
	frequencyhz = accessibilityGetEnemyFrequency();
	accessibilityTonePlayTargetPresence(frequencyhz,
			normalizedvolume, normalizedpan);
	g_AccessibilityTargetingProceduralPresenceActive = true;
	g_AccessibilityTargetingLastPulseIdentity = record->candidate.identity;
	g_AccessibilityTargetingHasLastPulseIdentity = true;
	g_AccessibilityTargetingNextPresence60 = frame60 + interval;
	g_AccessibilityTargetingPresencePulseCount++;

	accessibilityLogEvent("targeting", "presence_pulse",
			"pulse=%llu frame=%d next_frame=%d interval=%d eligible=%d lane=procedural_combat_timbre frequency_hz=%.3f volume=%.4f pan=%.4f source=%d slot=%d propnum=%d prop=%p distance=%.3f screen=%.3f,%.3f,%.3f,%.3f position=%.3f,%.3f,%.3f",
			(unsigned long long)g_AccessibilityTargetingPresencePulseCount,
			frame60, g_AccessibilityTargetingNextPresence60, interval, count,
			frequencyhz, normalizedvolume, normalizedpan,
			record->candidate.identity.source,
			record->candidate.identity.sourceslot,
			record->candidate.identity.propnum, (void *)record->candidate.prop,
			record->candidate.distance, record->candidate.screenx1,
			record->candidate.screeny1, record->candidate.screenx2,
			record->candidate.screeny2, record->candidate.position.x,
			record->candidate.position.y, record->candidate.position.z);
}

static void accessibilityTargetingUpdateAlignment(s32 frame60,
		const struct accessibilitytargetingcandidate *candidate,
		const char *reason)
{
	f32 quality = candidate->hasaimquality ? candidate->aimquality : 0.0f;
	f32 frequencyhz;
	s32 patternflags = ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_CONTINUOUS;
	s32 starting = !g_AccessibilityTargetingAlignmentActive;
	s32 interrupted = candidate->relationship
				== ACCESSIBILITY_TARGETING_RELATIONSHIP_PROTECTED
			|| candidate->category
				== ACCESSIBILITY_TARGETING_CATEGORY_BREAKABLE_PATH_BLOCKER
			|| candidate->category
				== ACCESSIBILITY_TARGETING_CATEGORY_LOOT_CONTAINER;
	interrupted = interrupted || candidate->category
			== ACCESSIBILITY_TARGETING_CATEGORY_REACTIVE_OBJECT;
	interrupted = interrupted || candidate->category
			== ACCESSIBILITY_TARGETING_CATEGORY_DESTROYABLE_OBJECT;
	if (interrupted) {
		patternflags |= ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED;
	}
	if (candidate->obstruction
			== ACCESSIBILITY_TARGETING_OBSTRUCTION_PENETRABLE_GLASS) {
		patternflags |= ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_PENETRABLE;
	}
	if (quality < 0.0f) {
		quality = 0.0f;
	} else if (quality > 1.0f) {
		quality = 1.0f;
	}

	frequencyhz = ACCESSIBILITY_TARGETING_TONE_BASE_FREQUENCY_HZ
			* ACCESSIBILITY_TARGETING_TONE_MIN_PITCH
			* powf(ACCESSIBILITY_TARGETING_TONE_MAX_PITCH
					/ ACCESSIBILITY_TARGETING_TONE_MIN_PITCH, quality);
	if (!candidate->hasaimquality) {
		if (candidate->aimregion == ACCESSIBILITY_TARGETING_AIM_REGION_HEAD) {
			frequencyhz *= ACCESSIBILITY_TARGETING_HEAD_LOCK_MULTIPLIER;
		} else if (candidate->aimregion
				== ACCESSIBILITY_TARGETING_AIM_REGION_ARM) {
			frequencyhz *= ACCESSIBILITY_TARGETING_ARM_LOCK_MULTIPLIER;
		}
	}
	accessibilityToneSetAlignment(1, frequencyhz, patternflags);
	g_AccessibilityTargetingAlignmentActive = true;
	g_AccessibilityTargetingAlignmentInterrupted = interrupted;
	g_AccessibilityTargetingAlignmentObstruction = candidate->obstruction;
	g_AccessibilityTargetingAlignmentFrequencyHz = frequencyhz;
	g_AccessibilityTargetingAlignmentQuality = quality;
	g_AccessibilityTargetingAlignmentDistance = candidate->aimdistance;
	g_AccessibilityTargetingAlignmentUpdateCount++;

	if (starting || frame60 >= g_AccessibilityTargetingNextAlignmentLog60) {
		accessibilityLogEvent("targeting",
				starting ? "alignment_start" : "alignment_update",
				"update=%llu frame=%d reason=%s source=%d slot=%d propnum=%d category=%d relationship=%d interrupted=%d obstruction=%d pattern_flags=0x%x pattern=%s aim_region=%d quality_available=%d quality=%.4f distance=%.3f frequency_hz=%.2f volume=%.4f",
				(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount,
				frame60, reason,
				g_AccessibilityTargetingAimedIdentity.source,
				g_AccessibilityTargetingAimedIdentity.sourceslot,
				g_AccessibilityTargetingAimedIdentity.propnum,
				candidate->category, candidate->relationship,
				interrupted,
				candidate->obstruction, patternflags,
				candidate->obstruction
						== ACCESSIBILITY_TARGETING_OBSTRUCTION_PENETRABLE_GLASS
					? interrupted ? "interrupted_penetrable_tremolo"
						: "penetrable_tremolo"
					: interrupted ? "90ms_on_10ms_off" : "continuous",
				candidate->aimregion,
				candidate->hasaimquality, quality, candidate->aimdistance,
				frequencyhz, accessibilityGetTargetingVolume());
		g_AccessibilityTargetingNextAlignmentLog60
				= frame60 + ACCESSIBILITY_TARGETING_ALIGNMENT_LOG_TICKS;
	}
}

void accessibilityTargetingObserve(
		const struct accessibilitytargetingobservation *observation)
{
	s32 acquisition;
	s32 aimedcandidate;
	s32 aimedshootability;
	s32 aimedvalid;
	s32 alignmentpermitted;
	s32 retainingaim;
	s32 i;
	struct accessibilitytargetingidentity effectiveaimidentity;
	const struct accessibilitytargetingcandidate *aimedrecord = NULL;

	if (!observation || !observation->inscope
			|| !accessibilityIsTargetingFeedbackEnabled()) {
		accessibilityTargetingReset(observation ? "scope_lost" : "null_observation");
		return;
	}

	if (observation->playernum < 0 || observation->playernum >= MAX_PLAYERS) {
		accessibilityTargetingReset("invalid_player");
		return;
	}

	g_AccessibilityTargetingCurrentPolicy
			= accessibilityTargetingGetPolicy(observation->profile, observation);
	if (!g_AccessibilityTargetingCurrentPolicy) {
		accessibilityTargetingReset("invalid_profile");
		return;
	}
	g_AccessibilityTargetingCurrentState
			= &g_AccessibilityTargetingStates[observation->playernum];

	if (g_AccessibilityTargetingCurrentState->profile != 0
			&& g_AccessibilityTargetingCurrentState->profile
					!= observation->profile) {
		accessibilityTargetingResetCurrent("profile_changed");
	}
	g_AccessibilityTargetingCurrentState->profile = observation->profile;
	accessibilityTargetingUpdateThreatAlerts(observation);

	g_AccessibilityTargetingObservationCount++;
	aimedcandidate = false;
	aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
	aimedvalid = false;
	alignmentpermitted = observation->targetindicatorvisible;
	retainingaim = false;
	memset(&effectiveaimidentity, 0, sizeof(effectiveaimidentity));

	if (observation->hasaimedtarget) {
		for (i = 0; i < observation->candidatecount; i++) {
			if (accessibilityTargetingIdentityEqual(&observation->aimedidentity,
					&observation->candidates[i].identity)) {
				aimedcandidate = true;
				aimedrecord = &observation->candidates[i];
				aimedshootability = observation->candidates[i].shootability;
				aimedvalid = aimedshootability
						== ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE;
				effectiveaimidentity = observation->aimedidentity;
				break;
			}
		}
	}

	/*
	 * Alignment mirrors the native visible aiming point. Keep semantic target
	 * observations available for presence and distance feedback, but do not
	 * expose the hidden camera-center attack ray while the game suppresses its
	 * target indicator (notably while unarmed).
	 */
	if (aimedvalid && !alignmentpermitted) {
		aimedvalid = false;
	}

	if (!aimedvalid
			&& alignmentpermitted
			&& observation->profile == ACCESSIBILITY_TARGETING_PROFILE_COMBAT
			&& g_AccessibilityTargetingHasAimedIdentity
			&& g_AccessibilityTargetingAimLossFrames
					< ACCESSIBILITY_TARGETING_COMBAT_AIM_LOSS_GRACE_FRAMES) {
		for (i = 0; i < observation->candidatecount; i++) {
			if (accessibilityTargetingIdentityEqual(
					&g_AccessibilityTargetingAimedIdentity,
					&observation->candidates[i].identity)
					&& (observation->candidates[i].category
							== ACCESSIBILITY_TARGETING_CATEGORY_CHARACTER
						|| observation->candidates[i].category
							== ACCESSIBILITY_TARGETING_CATEGORY_PLAYER)
					&& observation->candidates[i].shootability
						== ACCESSIBILITY_TARGETING_SHOOTABILITY_SHOOTABLE) {
				aimedcandidate = true;
				aimedrecord = &observation->candidates[i];
				aimedshootability = aimedrecord->shootability;
				aimedvalid = true;
				retainingaim = true;
				effectiveaimidentity = g_AccessibilityTargetingAimedIdentity;
				g_AccessibilityTargetingAimLossFrames++;

				if (g_AccessibilityTargetingAimLossFrames == 1) {
					accessibilityLogEvent("targeting", "aim_loss_grace_start",
							"frame=%d source=%d slot=%d propnum=%d grace_frames=%d",
							observation->frame60,
							effectiveaimidentity.source,
							effectiveaimidentity.sourceslot,
							effectiveaimidentity.propnum,
							ACCESSIBILITY_TARGETING_COMBAT_AIM_LOSS_GRACE_FRAMES);
				}
				break;
			}
		}
	}

	if (aimedvalid && !retainingaim) {
		if (g_AccessibilityTargetingAimLossFrames > 0) {
			accessibilityLogEvent("targeting", "aim_loss_grace_cancel",
					"frame=%d recovered_after_frames=%d source=%d slot=%d propnum=%d",
					observation->frame60,
					g_AccessibilityTargetingAimLossFrames,
					effectiveaimidentity.source,
					effectiveaimidentity.sourceslot,
					effectiveaimidentity.propnum);
		}
		g_AccessibilityTargetingAimLossFrames = 0;
	}

	acquisition = aimedvalid && (!g_AccessibilityTargetingHasAimedIdentity
			|| !accessibilityTargetingIdentityEqual(&effectiveaimidentity,
				&g_AccessibilityTargetingAimedIdentity));
	accessibilityTargetingUpdateThreatAim(
			observation, aimedvalid, aimedrecord);

	if (!aimedvalid) {
		if (g_AccessibilityTargetingHasAimedIdentity) {
			accessibilityLogEvent("targeting", "aim_loss",
					"frame=%d source=%d slot=%d propnum=%d aimed_candidate=%d shootability=%d reason=%s",
					observation->frame60,
					g_AccessibilityTargetingAimedIdentity.source,
					g_AccessibilityTargetingAimedIdentity.sourceslot,
					g_AccessibilityTargetingAimedIdentity.propnum,
					aimedcandidate, aimedshootability,
					!alignmentpermitted
						? "target_indicator_hidden"
						: aimedcandidate
						? accessibilityTargetingShootabilityName(aimedshootability)
						: "aim_lost");
		}

		g_AccessibilityTargetingHasAimedIdentity = false;
		g_AccessibilityTargetingAimLossFrames = 0;
		memset(&g_AccessibilityTargetingAimedIdentity, 0,
				sizeof(g_AccessibilityTargetingAimedIdentity));
		accessibilityTargetingStopAlignment(!alignmentpermitted
				? "target_indicator_hidden"
				: aimedcandidate
					? accessibilityTargetingShootabilityName(aimedshootability)
					: "aim_lost");
	} else {
		if (acquisition && g_AccessibilityTargetingHasAimedIdentity) {
			accessibilityTargetingStopAlignment("aim_changed");
		}

		g_AccessibilityTargetingAimedIdentity = effectiveaimidentity;
		g_AccessibilityTargetingHasAimedIdentity = true;

		if (acquisition) {
			accessibilityLogEvent("targeting", "aim_acquisition",
					"frame=%d source=%d slot=%d propnum=%d shootability=%d reason=%s native_expected=%d quality_available=%d quality=%.4f distance=%.3f",
					observation->frame60, observation->aimedidentity.source,
					observation->aimedidentity.sourceslot,
					observation->aimedidentity.propnum,
					aimedshootability,
					accessibilityTargetingShootabilityName(aimedshootability),
					observation->nativealignmentexpected,
					aimedrecord->hasaimquality, aimedrecord->aimquality,
					aimedrecord->aimdistance);
		}

		accessibilityTargetingUpdateAlignment(observation->frame60,
				aimedrecord, acquisition ? "acquisition"
					: retainingaim ? "loss_grace" : "held");
	}

	accessibilityTargetingMerge(observation);
	accessibilityTargetingUpdatePrecisionGuidance(observation);

	if (observation->profile == ACCESSIBILITY_TARGETING_PROFILE_COMBAT) {
		accessibilityTargetingStopPresence("combat_profile");
		g_AccessibilityTargetingHasLastPulseIdentity = false;
		g_AccessibilityTargetingNextPresence60 = 0;
		accessibilityTargetingUpdateCombatPresence(observation->frame60,
				observation->distancecuereference);
	} else if (observation->profile == ACCESSIBILITY_TARGETING_PROFILE_DEVICE) {
		accessibilityTargetingStopPresence("device_profile");
		g_AccessibilityTargetingHasLastPulseIdentity = false;
		g_AccessibilityTargetingNextPresence60 = 0;
	} else if (g_AccessibilityTargetingHasLastPulseIdentity) {
		s32 lastindex = accessibilityTargetingFindRecord(
				&g_AccessibilityTargetingLastPulseIdentity);

		if (lastindex < 0
				|| g_AccessibilityTargetingRecords[lastindex].missingframes != 0
				|| !accessibilityTargetingCandidateValid(
					&g_AccessibilityTargetingRecords[lastindex].candidate)) {
			accessibilityTargetingStopPresence("last_target_unavailable");
			g_AccessibilityTargetingNextPresence60 = observation->frame60;
		}
	}

	if (observation->profile != ACCESSIBILITY_TARGETING_PROFILE_COMBAT
			&& observation->profile != ACCESSIBILITY_TARGETING_PROFILE_DEVICE
			&& observation->frame60 >= g_AccessibilityTargetingNextPresence60) {
		accessibilityTargetingPulsePresence(observation->frame60);
	}

	if (!g_AccessibilityTargetingCurrentState->haslastobservation
			|| observation->candidatecount
					!= g_AccessibilityTargetingCurrentState->lastcandidatecount
			|| aimedvalid != g_AccessibilityTargetingCurrentState->lastaimed
			|| aimedcandidate
					!= g_AccessibilityTargetingCurrentState->lastaimedcandidate
			|| aimedshootability
					!= g_AccessibilityTargetingCurrentState->lastaimedshootability
			|| (aimedcandidate && !accessibilityTargetingIdentityEqual(
				&effectiveaimidentity,
				&g_AccessibilityTargetingCurrentState->lastaimedidentity))
			|| observation->frame60
					>= g_AccessibilityTargetingCurrentState->nextobservationlog60) {
		accessibilityLogEvent("targeting", "observation",
				"count=%llu frame=%d stage=%d player=%d source=%d profile=%d sight_on=%d indicator_visible=%d alignment_permitted=%d view_fovy=%.3f default_fovy=%.3f zoom_blend=%.4f precision_guidance=%d precision_propnum=%d range_profile=%s full_distance=%.3f fade_distance=%.3f maximum_distance=%.3f candidates=%d tracked=%d aimed_candidate=%d aimed_shootable=%d shootability=%d shootability_reason=%s acquisition=%d next_presence=%d alignment_active=%d quality_available=%d quality=%.4f distance=%.3f frequency_hz=%.2f",
				(unsigned long long)g_AccessibilityTargetingObservationCount,
				observation->frame60, observation->stagenum, observation->playernum,
				observation->source, observation->profile, observation->sighton,
				observation->targetindicatorvisible,
				alignmentpermitted,
				observation->viewfovy, observation->defaultfovy,
				observation->zoomblend,
				g_AccessibilityTargetingCurrentState->precisionguidanceactive,
				g_AccessibilityTargetingCurrentState->precisionguidanceactive
						? g_AccessibilityTargetingCurrentState
								->precisionguidanceidentity.propnum : -1,
				observation->zoomblend > 0.0f ? "zoom_blend" : "standard",
				g_AccessibilityTargetingCurrentPolicy->fulldistance,
				g_AccessibilityTargetingCurrentPolicy->fadedistance,
				g_AccessibilityTargetingCurrentPolicy->silentdistance,
				observation->candidatecount,
				g_AccessibilityTargetingRecordCount, aimedcandidate, aimedvalid,
				aimedshootability,
				!alignmentpermitted && aimedcandidate
					? "target_indicator_hidden"
					: accessibilityTargetingShootabilityName(aimedshootability),
				acquisition,
				g_AccessibilityTargetingNextPresence60,
				g_AccessibilityTargetingAlignmentActive,
				aimedrecord ? aimedrecord->hasaimquality : 0,
				aimedrecord ? aimedrecord->aimquality : 0.0f,
				aimedrecord ? aimedrecord->aimdistance : 0.0f,
				g_AccessibilityTargetingAlignmentFrequencyHz);
		g_AccessibilityTargetingCurrentState->nextobservationlog60
				= observation->frame60 + ACCESSIBILITY_TARGETING_OBSERVATION_LOG_TICKS;
	}

	g_AccessibilityTargetingCurrentState->haslastobservation = true;
	g_AccessibilityTargetingCurrentState->lastcandidatecount
			= observation->candidatecount;
	g_AccessibilityTargetingCurrentState->lastaimed = aimedvalid;
	g_AccessibilityTargetingCurrentState->lastaimedcandidate = aimedcandidate;
	g_AccessibilityTargetingCurrentState->lastaimedshootability = aimedshootability;
	if (aimedcandidate) {
		g_AccessibilityTargetingCurrentState->lastaimedidentity
				= effectiveaimidentity;
	}

	if (g_AccessibilityTargetingCurrentState->nexttelemetry60 == 0
			|| observation->frame60
					>= g_AccessibilityTargetingCurrentState->nexttelemetry60) {
		accessibilityTargetingLogTelemetry(observation->frame60);
	}
}

static void accessibilityTargetingResetCurrent(const char *reason)
{
	s32 hadstate = g_AccessibilityTargetingRecordCount
			|| g_AccessibilityTargetingHasAimedIdentity
			|| g_AccessibilityTargetingProceduralPresenceActive
			|| accessibilityTargetingCombatSlotCount() > 0
			|| g_AccessibilityTargetingAlignmentActive
			|| g_AccessibilityTargetingCurrentState->threatdetectoractive
			|| g_AccessibilityTargetingCurrentState->threatalertcount > 0
			|| g_AccessibilityTargetingCurrentState->hasaimedthreat;

	accessibilityTargetingStopPresence(reason ? reason : "reset");
	accessibilityTargetingStopCombatPresence(reason ? reason : "reset");
	accessibilityTargetingStopAlignment(reason ? reason : "reset");
	accessibilityToneStopThreatAlert();

	if (hadstate) {
		accessibilityLogEvent("targeting", "reset",
				"reason=%s observations=%llu presence_pulses=%llu alignment_updates=%llu records=%d aimed=%d",
				reason ? reason : "reset",
				(unsigned long long)g_AccessibilityTargetingObservationCount,
				(unsigned long long)g_AccessibilityTargetingPresencePulseCount,
				(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount,
				g_AccessibilityTargetingRecordCount,
				g_AccessibilityTargetingHasAimedIdentity);
	}

	memset(g_AccessibilityTargetingRecords, 0,
			sizeof(g_AccessibilityTargetingRecords));
	memset(&g_AccessibilityTargetingLastPulseIdentity, 0,
			sizeof(g_AccessibilityTargetingLastPulseIdentity));
	memset(&g_AccessibilityTargetingAimedIdentity, 0,
			sizeof(g_AccessibilityTargetingAimedIdentity));
	g_AccessibilityTargetingRecordCount = 0;
	g_AccessibilityTargetingHasLastPulseIdentity = false;
	g_AccessibilityTargetingHasAimedIdentity = false;
	g_AccessibilityTargetingAimLossFrames = 0;
	g_AccessibilityTargetingNextPresence60 = 0;
	g_AccessibilityTargetingNextAlignmentLog60 = 0;
	g_AccessibilityTargetingObservationCount = 0;
	g_AccessibilityTargetingPresencePulseCount = 0;
	g_AccessibilityTargetingAlignmentUpdateCount = 0;
	g_AccessibilityTargetingCurrentState->haslastobservation = false;
	g_AccessibilityTargetingCurrentState->lastcandidatecount = 0;
	g_AccessibilityTargetingCurrentState->lastaimed = false;
	memset(&g_AccessibilityTargetingCurrentState->lastaimedidentity, 0,
			sizeof(g_AccessibilityTargetingCurrentState->lastaimedidentity));
	g_AccessibilityTargetingCurrentState->nextobservationlog60 = 0;
	g_AccessibilityTargetingCurrentState->nexttelemetry60 = 0;
	g_AccessibilityTargetingCurrentState->memorybaselinevalid = false;
	g_AccessibilityTargetingCurrentState->workingsetbaseline = 0;
	g_AccessibilityTargetingCurrentState->privatebaseline = 0;
	g_AccessibilityTargetingCurrentState->threatdetectoractive = false;
	g_AccessibilityTargetingCurrentState->knownthreatcount = 0;
	g_AccessibilityTargetingCurrentState->threatalertcount = 0;
	g_AccessibilityTargetingCurrentState->nextthreatalert60 = 0;
	g_AccessibilityTargetingCurrentState->nextthreataimalert60 = 0;
	g_AccessibilityTargetingCurrentState->lastthreatsound60 = 0;
	g_AccessibilityTargetingCurrentState->lastthreatsoundkind
			= ACCESSIBILITY_TARGETING_THREAT_SOUND_NONE;
	memset(&g_AccessibilityTargetingCurrentState->lastthreatsoundidentity, 0,
			sizeof(g_AccessibilityTargetingCurrentState
				->lastthreatsoundidentity));
	g_AccessibilityTargetingCurrentState->hasaimedthreat = false;
	g_AccessibilityTargetingCurrentState->threatalertscount = 0;
	g_AccessibilityTargetingCurrentState->threataimpulsecount = 0;
	memset(&g_AccessibilityTargetingCurrentState->aimedthreatidentity, 0,
			sizeof(g_AccessibilityTargetingCurrentState->aimedthreatidentity));
	memset(g_AccessibilityTargetingCurrentState->knownthreats, 0,
			sizeof(g_AccessibilityTargetingCurrentState->knownthreats));
	memset(g_AccessibilityTargetingCurrentState->threatalerts, 0,
			sizeof(g_AccessibilityTargetingCurrentState->threatalerts));
}

s32 accessibilityTargetingGetPrecisionGuidancePropnum(void)
{
	struct accessibilitytargetingstate *state;

	if (g_Vars.currentplayernum < 0
			|| g_Vars.currentplayernum >= MAX_PLAYERS) {
		return -1;
	}

	state = &g_AccessibilityTargetingStates[g_Vars.currentplayernum];

	return state->precisionguidanceactive
			? state->precisionguidanceidentity.propnum : -1;
}

void accessibilityTargetingReset(const char *reason)
{
	s32 playernum;

	for (playernum = 0; playernum < MAX_PLAYERS; playernum++) {
		g_AccessibilityTargetingCurrentState
				= &g_AccessibilityTargetingStates[playernum];
		accessibilityTargetingResetCurrent(reason);
		g_AccessibilityTargetingCurrentState->profile = 0;
	}

	g_AccessibilityTargetingCurrentState = NULL;
	g_AccessibilityTargetingCurrentPolicy = NULL;
}

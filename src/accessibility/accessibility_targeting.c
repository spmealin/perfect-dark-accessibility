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
#define ACCESSIBILITY_TARGETING_CAMERA_START_FREQUENCY_HZ 1600.0f
#define ACCESSIBILITY_TARGETING_CAMERA_END_FREQUENCY_HZ 1000.0f
#define ACCESSIBILITY_TARGETING_CAMERA_PERIOD_MS 500
#define ACCESSIBILITY_TARGETING_CAMERA_DURATION_MS 140

struct accessibilitytargetingrecord {
	struct accessibilitytargetingcandidate candidate;
	char localizedname[ACCESSIBILITY_TARGETING_NAME_LENGTH];
	s32 seenframes;
	s32 missingframes;
};

struct accessibilitytargetingpolicy {
	s32 profile;
	s16 presencesound;
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
	f32 frequencyhz;
	s32 nextcadencelog60;
};

struct accessibilitytargetingstate {
	struct accessibilitytargetingrecord records[ACCESSIBILITY_TARGETING_MAX_CANDIDATES];
	s32 recordcount;
	struct accessibilitytargetingidentity lastpulseidentity;
	s32 haslastpulseidentity;
	struct accessibilitytargetingidentity aimedidentity;
	s32 hasaimedidentity;
	s32 presencechannel;
	s32 proceduralpresenceactive;
	s32 alignmentactive;
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
	struct accessibilitytargetingcombatslot
			combatslots[ACCESSIBILITY_TONE_COMBAT_SLOT_COUNT];
};

static const struct accessibilitytargetingpolicy g_AccessibilityTargetingRangePolicy = {
	ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE,
	SFX_MENU_SELECT,
	ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS,
	ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS,
	ACCESSIBILITY_TARGETING_VISIBLE_FRAMES,
	ACCESSIBILITY_TARGETING_MISSING_FRAMES,
	ACCESSIBILITY_TARGETING_FULL_DISTANCE,
	ACCESSIBILITY_TARGETING_FADE_DISTANCE,
	ACCESSIBILITY_TARGETING_SILENT_DISTANCE,
};

static struct accessibilitytargetingpolicy g_AccessibilityTargetingCombatPolicy = {
	ACCESSIBILITY_TARGETING_PROFILE_COMBAT,
	SFX_MENU_SELECT,
	ACCESSIBILITY_TARGETING_BASE_CYCLE_TICKS,
	ACCESSIBILITY_TARGETING_MIN_SLOT_TICKS,
	ACCESSIBILITY_TARGETING_VISIBLE_FRAMES,
	ACCESSIBILITY_TARGETING_MISSING_FRAMES,
	600.0f,
	3500.0f,
	4000.0f,
};

static const struct accessibilitytargetingpolicy g_AccessibilityTargetingDevicePolicy = {
	ACCESSIBILITY_TARGETING_PROFILE_DEVICE,
	SFX_MENU_SELECT,
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
static s32 g_AccessibilityTargetingStatesInitialized;

#define g_AccessibilityTargetingRecords (g_AccessibilityTargetingCurrentState->records)
#define g_AccessibilityTargetingRecordCount (g_AccessibilityTargetingCurrentState->recordcount)
#define g_AccessibilityTargetingLastPulseIdentity (g_AccessibilityTargetingCurrentState->lastpulseidentity)
#define g_AccessibilityTargetingHasLastPulseIdentity (g_AccessibilityTargetingCurrentState->haslastpulseidentity)
#define g_AccessibilityTargetingAimedIdentity (g_AccessibilityTargetingCurrentState->aimedidentity)
#define g_AccessibilityTargetingHasAimedIdentity (g_AccessibilityTargetingCurrentState->hasaimedidentity)
#define g_AccessibilityTargetingPresenceChannel (g_AccessibilityTargetingCurrentState->presencechannel)
#define g_AccessibilityTargetingProceduralPresenceActive (g_AccessibilityTargetingCurrentState->proceduralpresenceactive)
#define g_AccessibilityTargetingAlignmentActive (g_AccessibilityTargetingCurrentState->alignmentactive)
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

static s32 accessibilityTargetingPresenceOwned(void);
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
	s32 owned = 0;
	s32 i;

	for (i = 0; g_PsChannels && i < channels; i++) {
		if ((g_PsChannels[i].flags & PSFLAG_FREE) == 0) {
			inuse++;
			if (g_PsChannels[i].flags2 & PSFLAG2_STOPPED) {
				stopped++;
			}
			if (g_PsChannels[i].type == PSTYPE_ACCESSIBILITY_TARGETING) {
				owned++;
			}
		}
	}

	if (memoryavailable && !g_AccessibilityTargetingCurrentState->memorybaselinevalid) {
		g_AccessibilityTargetingCurrentState->memorybaselinevalid = true;
		g_AccessibilityTargetingCurrentState->workingsetbaseline = workingset;
		g_AccessibilityTargetingCurrentState->privatebaseline = privatebytes;
	}

	accessibilityLogEvent("targeting", "telemetry",
			"frame=%d observations=%llu presence_pulses=%llu alignment_updates=%llu records=%d aimed=%d memory_available=%d working_set_bytes=%llu working_set_delta=%lld private_bytes=%llu private_delta=%lld snd_states=%d prop_channels_in_use=%d prop_channels_total=%d prop_channels_stopped=%d targeting_channels=%d presence_channel=%d presence_owned=%d procedural_presence_active=%d combat_oscillator_slots=%d combat_oscillator_capacity=%d alignment_active=%d alignment_frequency_hz=%.2f alignment_quality=%.4f alignment_distance=%.3f",
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
			g_SndNumPlaying, inuse, channels, stopped, owned,
			g_AccessibilityTargetingPresenceChannel,
			accessibilityTargetingPresenceOwned(),
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

static const struct accessibilitytargetingpolicy *accessibilityTargetingGetPolicy(s32 profile)
{
	if (profile == ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE) {
		return &g_AccessibilityTargetingRangePolicy;
	}

	if (profile == ACCESSIBILITY_TARGETING_PROFILE_COMBAT) {
		accessibilityGetEnemyTuning(
				&g_AccessibilityTargetingCombatPolicy.fulldistance,
				&g_AccessibilityTargetingCombatPolicy.fadedistance,
				&g_AccessibilityTargetingCombatPolicy.silentdistance,
				NULL);
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

static s32 accessibilityTargetingChannelCount(void)
{
	return IS4MB() ? 30 : 40;
}

static s32 accessibilityTargetingPresenceOwned(void)
{
	return g_PsChannels
			&& g_AccessibilityTargetingPresenceChannel >= 0
			&& g_AccessibilityTargetingPresenceChannel < accessibilityTargetingChannelCount()
			&& (g_PsChannels[g_AccessibilityTargetingPresenceChannel].flags & PSFLAG_FREE) == 0
			&& g_PsChannels[g_AccessibilityTargetingPresenceChannel].type
					== PSTYPE_ACCESSIBILITY_TARGETING;
}

static s32 accessibilityTargetingPresenceChannelReusable(s32 channel)
{
	return g_PsChannels && channel >= 0
			&& channel < accessibilityTargetingChannelCount()
			&& ((g_PsChannels[channel].flags & PSFLAG_FREE)
				|| g_PsChannels[channel].type == PSTYPE_ACCESSIBILITY_TARGETING);
}

static void accessibilityTargetingStopPresence(const char *reason)
{
	s32 channel = g_AccessibilityTargetingPresenceChannel;
	s32 owned = accessibilityTargetingPresenceOwned();
	s32 procedural = g_AccessibilityTargetingProceduralPresenceActive;

	if (owned) {
		psStopChannel(channel);
	}
	if (procedural) {
		accessibilityToneStopTargetPresence();
	}

	if (channel >= 0 || procedural) {
		accessibilityLogEvent("targeting", "presence_stop",
				"reason=%s channel=%d owned=%d procedural=%d",
				reason, channel, owned, procedural);
	}

	g_AccessibilityTargetingPresenceChannel = -1;
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
		accessibilityToneSet(0, 0.0f);
		accessibilityLogEvent("targeting", "alignment_stop",
				"reason=%s frequency_hz=%.2f quality=%.4f distance=%.3f updates=%llu",
				reason, g_AccessibilityTargetingAlignmentFrequencyHz,
				g_AccessibilityTargetingAlignmentQuality,
				g_AccessibilityTargetingAlignmentDistance,
				(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount);
	}

	g_AccessibilityTargetingAlignmentActive = false;
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

static f32 accessibilityTargetingCombatElevationFrequency(
		const struct accessibilitytargetingcandidate *candidate,
		f32 basefrequency, f32 *elevationdegrees, s32 *elevationzone)
{
	f32 dx = candidate->position.x - g_Vars.currentplayer->cam_pos.x;
	f32 dy = candidate->position.y - g_Vars.currentplayer->cam_pos.y;
	f32 dz = candidate->position.z - g_Vars.currentplayer->cam_pos.z;
	f32 horizontal = sqrtf(dx * dx + dz * dz);
	f32 lookhorizontal = sqrtf(
			g_Vars.currentplayer->cam_look.x
					* g_Vars.currentplayer->cam_look.x
			+ g_Vars.currentplayer->cam_look.z
					* g_Vars.currentplayer->cam_look.z);
	f32 targetangle = atan2f(dy, horizontal);
	f32 lookangle = atan2f(g_Vars.currentplayer->cam_look.y, lookhorizontal);
	/*
	 * The renderer's camera-look Y axis is negative when looking upward,
	 * opposite the world-space target delta used above.
	 */
	f32 angle = (targetangle + lookangle) * 180.0f / M_PI;
	f32 normalized;
	f32 multiplier;

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
		f32 elevationdegrees = 0.0f;
		f32 elevationfrequency;
		s32 elevationzone = 0;
		s32 camera;

		if (!accessibilityTargetingRecordPresenceEligible(record)) {
			continue;
		}

		camera = record->candidate.category
				== ACCESSIBILITY_TARGETING_CATEGORY_SECURITY_CAMERA;
		elevationfrequency = camera ? combatfrequency
				: accessibilityTargetingCombatElevationFrequency(
					&record->candidate, combatfrequency,
					&elevationdegrees, &elevationzone);
		startfrequency = camera
				? ACCESSIBILITY_TARGETING_CAMERA_START_FREQUENCY_HZ
				: elevationfrequency;
		endfrequency = camera
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
			voice->frequencyhz = startfrequency;
			voice->elevationzone = elevationzone;
			restart = true;
			accessibilityLogEvent("targeting", "combat_slot_assign",
					"frame=%d oscillator_slot=%d source=%d slot=%d propnum=%d prop=%p category=%d cue=%s elevation_degrees=%.2f elevation_zone=%s base_frequency_hz=%.1f target_frequency_hz=%.1f start_frequency_hz=%.1f end_frequency_hz=%.1f punch_range=%.3f",
					frame60, slot, voice->identity.source,
					voice->identity.sourceslot, voice->identity.propnum,
					(void *)record->candidate.prop,
					record->candidate.category,
					camera ? "security_camera_sweep" : "enemy_proximity",
					elevationdegrees,
					elevationzone > 0 ? "above"
						: elevationzone < 0 ? "below" : "level",
					combatfrequency, elevationfrequency,
					startfrequency, endfrequency,
					distancecuereference);
		}

		voice = &g_AccessibilityTargetingCombatSlots[slot];
		if (!camera) {
			voice->frequencyhz += (elevationfrequency - voice->frequencyhz)
					* ACCESSIBILITY_TARGETING_COMBAT_ELEVATION_SMOOTHING;
			startfrequency = voice->frequencyhz;
			endfrequency = voice->frequencyhz;
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
		restart |= normalizedvolume > 0.0f && !voice->audible;
		cuedistance = record->candidate.hasdistancecue
				? record->candidate.distancecue : record->candidate.distance;
		if (camera) {
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
					"frame=%d oscillator_slot=%d propnum=%d category=%d cue=%s center_distance=%.3f cue_distance=%.3f cue_distance_available=%d punch_range=%.3f punch_range_exit=%.3f far_threshold=%.3f zone=%s proximity=%.4f period_ms=%d duration_ms=%d continuous=%d trigger_now=%d elevation_degrees=%.2f elevation_zone=%s base_frequency_hz=%.1f target_frequency_hz=%.1f start_frequency_hz=%.1f end_frequency_hz=%.1f volume=%.4f pan=%.4f",
					frame60, slot, voice->identity.propnum,
					record->candidate.category,
					camera ? "security_camera_sweep" : "enemy_proximity",
					record->candidate.distance, cuedistance,
					record->candidate.hasdistancecue, distancecuereference,
					distancecuereference
							* ACCESSIBILITY_TARGETING_COMBAT_CLOSE_EXIT_SCALE,
					distancecuereference * 5.0f,
					distancezone == 2 ? "punch_range"
						: distancezone == 1 ? "ramping" : "far",
					proximity, periodms, durationms,
					!camera && distancezone == 2, triggernow,
					elevationdegrees,
					elevationzone > 0 ? "above"
						: elevationzone < 0 ? "below" : "level",
					combatfrequency, elevationfrequency,
					startfrequency, endfrequency,
					normalizedvolume, normalizedpan);
			voice->nextcadencelog60 = frame60 + TICKS(60);
		}
		accessibilityToneSetCombatSlot(slot, true,
				startfrequency, endfrequency,
				normalizedvolume, normalizedpan, periodms, durationms,
				!camera && distancezone == 2, restart, triggernow);
		voice->audible = normalizedvolume > 0.0f;
		voice->periodms = periodms;
		voice->durationms = durationms;
		voice->distancezone = distancezone;
		voice->elevationzone = elevationzone;
	}
}

static void accessibilityTargetingPulsePresence(s32 frame60)
{
	struct accessibilitytargetingrecord *record = accessibilityTargetingNextPresence();
	s32 count = accessibilityTargetingEligibleCount();
	s32 previouschannel = g_AccessibilityTargetingPresenceChannel;
	s32 interval;
	s16 channel = -1;
	s32 reused = false;
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

	if (g_AccessibilityTargetingCurrentPolicy->profile
			== ACCESSIBILITY_TARGETING_PROFILE_FIRING_RANGE) {
		if (accessibilityTargetingPresenceOwned()) {
			psStopChannel(previouschannel);
		}
		g_AccessibilityTargetingPresenceChannel = -1;
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
		g_AccessibilityTargetingLastPulseIdentity
				= record->candidate.identity;
		g_AccessibilityTargetingHasLastPulseIdentity = true;
		g_AccessibilityTargetingNextPresence60 = frame60 + interval;
		g_AccessibilityTargetingPresencePulseCount++;
		accessibilityLogEvent("targeting", "presence_pulse",
				"pulse=%llu frame=%d next_frame=%d interval=%d eligible=%d lane=procedural_combat_timbre frequency_hz=%.3f volume=%.4f pan=%.4f source=%d slot=%d propnum=%d prop=%p distance=%.3f screen=%.3f,%.3f,%.3f,%.3f position=%.3f,%.3f,%.3f",
				(unsigned long long)g_AccessibilityTargetingPresencePulseCount,
				frame60, g_AccessibilityTargetingNextPresence60,
				interval, count, frequencyhz, normalizedvolume,
				normalizedpan, record->candidate.identity.source,
				record->candidate.identity.sourceslot,
				record->candidate.identity.propnum,
				(void *)record->candidate.prop,
				record->candidate.distance,
				record->candidate.screenx1, record->candidate.screeny1,
				record->candidate.screenx2, record->candidate.screeny2,
				record->candidate.position.x, record->candidate.position.y,
				record->candidate.position.z);
		return;
	}

	if (accessibilityTargetingPresenceChannelReusable(previouschannel)) {
		if ((g_PsChannels[previouschannel].flags & PSFLAG_FREE) == 0) {
			psStopChannel(previouschannel);
		}

		channel = psCreate(&g_PsChannels[previouschannel],
				record->candidate.prop, g_AccessibilityTargetingCurrentPolicy->presencesound, -1,
				AL_VOL_FULL, 0, PSFLAG2_MPPAUSABLE,
				PSTYPE_ACCESSIBILITY_TARGETING, NULL, -1.0f, NULL, -1,
				g_AccessibilityTargetingCurrentPolicy->fulldistance,
				g_AccessibilityTargetingCurrentPolicy->fadedistance,
				g_AccessibilityTargetingCurrentPolicy->silentdistance);
		reused = channel == previouschannel;
	}

	if (channel < 0) {
		accessibilityTargetingStopPresence("round_robin_advance");
		psStopSound(record->candidate.prop, PSTYPE_ACCESSIBILITY_TARGETING, 0);
		channel = psCreate(NULL, record->candidate.prop,
				g_AccessibilityTargetingCurrentPolicy->presencesound, -1,
				AL_VOL_FULL, 0, PSFLAG2_MPPAUSABLE,
				PSTYPE_ACCESSIBILITY_TARGETING, NULL, -1.0f, NULL, -1,
				g_AccessibilityTargetingCurrentPolicy->fulldistance,
				g_AccessibilityTargetingCurrentPolicy->fadedistance,
				g_AccessibilityTargetingCurrentPolicy->silentdistance);
	}

	g_AccessibilityTargetingPresenceChannel = channel;
	g_AccessibilityTargetingLastPulseIdentity = record->candidate.identity;
	g_AccessibilityTargetingHasLastPulseIdentity = true;
	g_AccessibilityTargetingNextPresence60 = frame60 + interval;
	g_AccessibilityTargetingPresencePulseCount++;

	accessibilityLogEvent("targeting", "presence_pulse",
			"pulse=%llu frame=%d next_frame=%d interval=%d eligible=%d channel=%d previous_channel=%d reused=%d sound=%d source=%d slot=%d propnum=%d prop=%p distance=%.3f screen=%.3f,%.3f,%.3f,%.3f position=%.3f,%.3f,%.3f",
			(unsigned long long)g_AccessibilityTargetingPresencePulseCount,
			frame60, g_AccessibilityTargetingNextPresence60, interval, count,
			channel, previouschannel, reused,
			g_AccessibilityTargetingCurrentPolicy->presencesound,
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
	s32 starting = !g_AccessibilityTargetingAlignmentActive;

	if (quality < 0.0f) {
		quality = 0.0f;
	} else if (quality > 1.0f) {
		quality = 1.0f;
	}

	frequencyhz = ACCESSIBILITY_TARGETING_TONE_BASE_FREQUENCY_HZ
			* ACCESSIBILITY_TARGETING_TONE_MIN_PITCH
			* powf(ACCESSIBILITY_TARGETING_TONE_MAX_PITCH
					/ ACCESSIBILITY_TARGETING_TONE_MIN_PITCH, quality);
	accessibilityToneSet(1, frequencyhz);
	g_AccessibilityTargetingAlignmentActive = true;
	g_AccessibilityTargetingAlignmentFrequencyHz = frequencyhz;
	g_AccessibilityTargetingAlignmentQuality = quality;
	g_AccessibilityTargetingAlignmentDistance = candidate->aimdistance;
	g_AccessibilityTargetingAlignmentUpdateCount++;

	if (starting || frame60 >= g_AccessibilityTargetingNextAlignmentLog60) {
		accessibilityLogEvent("targeting",
				starting ? "alignment_start" : "alignment_update",
				"update=%llu frame=%d reason=%s source=%d slot=%d propnum=%d quality_available=%d quality=%.4f distance=%.3f frequency_hz=%.2f",
				(unsigned long long)g_AccessibilityTargetingAlignmentUpdateCount,
				frame60, reason,
				g_AccessibilityTargetingAimedIdentity.source,
				g_AccessibilityTargetingAimedIdentity.sourceslot,
				g_AccessibilityTargetingAimedIdentity.propnum,
				candidate->hasaimquality, quality, candidate->aimdistance,
				frequencyhz);
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
			= accessibilityTargetingGetPolicy(observation->profile);
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

	g_AccessibilityTargetingObservationCount++;
	aimedcandidate = false;
	aimedshootability = ACCESSIBILITY_TARGETING_SHOOTABILITY_UNKNOWN;
	aimedvalid = false;
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

	if (!aimedvalid
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

	if (!aimedvalid) {
		if (g_AccessibilityTargetingHasAimedIdentity) {
			accessibilityLogEvent("targeting", "aim_loss",
					"frame=%d source=%d slot=%d propnum=%d aimed_candidate=%d shootability=%d reason=%s",
					observation->frame60,
					g_AccessibilityTargetingAimedIdentity.source,
					g_AccessibilityTargetingAimedIdentity.sourceslot,
					g_AccessibilityTargetingAimedIdentity.propnum,
					aimedcandidate, aimedshootability,
					aimedcandidate
						? accessibilityTargetingShootabilityName(aimedshootability)
						: "aim_lost");
		}

		g_AccessibilityTargetingHasAimedIdentity = false;
		g_AccessibilityTargetingAimLossFrames = 0;
		memset(&g_AccessibilityTargetingAimedIdentity, 0,
				sizeof(g_AccessibilityTargetingAimedIdentity));
		accessibilityTargetingStopAlignment(aimedcandidate
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
				"count=%llu frame=%d stage=%d player=%d source=%d profile=%d sight_on=%d indicator_visible=%d candidates=%d tracked=%d aimed_candidate=%d aimed_shootable=%d shootability=%d shootability_reason=%s acquisition=%d next_presence=%d alignment_active=%d quality_available=%d quality=%.4f distance=%.3f frequency_hz=%.2f",
				(unsigned long long)g_AccessibilityTargetingObservationCount,
				observation->frame60, observation->stagenum, observation->playernum,
				observation->source, observation->profile, observation->sighton,
				observation->targetindicatorvisible, observation->candidatecount,
				g_AccessibilityTargetingRecordCount, aimedcandidate, aimedvalid,
				aimedshootability,
				accessibilityTargetingShootabilityName(aimedshootability), acquisition,
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
			|| g_AccessibilityTargetingPresenceChannel >= 0
			|| g_AccessibilityTargetingProceduralPresenceActive
			|| accessibilityTargetingCombatSlotCount() > 0
			|| g_AccessibilityTargetingAlignmentActive;

	accessibilityTargetingStopPresence(reason ? reason : "reset");
	accessibilityTargetingStopCombatPresence(reason ? reason : "reset");
	accessibilityTargetingStopAlignment(reason ? reason : "reset");

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
}

void accessibilityTargetingReset(const char *reason)
{
	s32 playernum;

	if (!g_AccessibilityTargetingStatesInitialized) {
		for (playernum = 0; playernum < MAX_PLAYERS; playernum++) {
			g_AccessibilityTargetingStates[playernum].presencechannel = -1;
		}
		g_AccessibilityTargetingStatesInitialized = true;
	}

	for (playernum = 0; playernum < MAX_PLAYERS; playernum++) {
		g_AccessibilityTargetingCurrentState
				= &g_AccessibilityTargetingStates[playernum];
		accessibilityTargetingResetCurrent(reason);
		g_AccessibilityTargetingCurrentState->profile = 0;
	}

	g_AccessibilityTargetingCurrentState = NULL;
	g_AccessibilityTargetingCurrentPolicy = NULL;
}

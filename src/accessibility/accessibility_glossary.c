#include <math.h>
#include "constants.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_glossary.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"

enum glossarykind {
	DOOR, OBJECT, PICKUP, MOVABLE, PERSON, ENEMY, CAMERA, RANGE_PRESENCE,
	LOCK, RANGE_LOCK, THREAT, CANE, PATH, RUNWAY, EDGE, VEHICLE, LASER,
	ROCKET, TRACKER, RADAR, HILL, MARKER, REMOVAL, LANDMARK, TOGGLE,
	CANE_MODE, COMPASS, STANCE, FUNCTION,
};

struct glossaryentry {
	const char *name;
	s32 kind;
	s32 variant;
	s32 durationms;
};

/* English accessibility vocabulary and complete cue inventory. Identical sounds
 * can have separate entries when their gameplay meanings differ. Localize this
 * table through an accessibility-owned catalog, never generated ROM assets. */
static const struct glossaryentry g_Glossary[] = {
	{ "Door", DOOR, 0, 120 },
	{ "Interactable object", OBJECT, 0, 120 },
	{ "Pickup item", PICKUP, 0, 170 },
	{ "Movable puzzle rock", MOVABLE, 0, 280 },
	{ "Neutral or protected person", PERSON, 0, 2200 },
	{ "Friendly combat character", PERSON, 1, 4000 },
	{ "Enemy: distant", ENEMY, 0, 200 },
	{ "Enemy: nearby", ENEMY, 1, 70 },
	{ "Enemy: melee range", ENEMY, 2, 1000 },
	{ "Enemy: above your aim", ENEMY, 3, 200 },
	{ "Enemy: below your aim", ENEMY, 4, 200 },
	{ "Precise aim: target left", ENEMY, 5, 120 },
	{ "Precise aim: target right", ENEMY, 6, 120 },
	{ "Security camera", CAMERA, 0, 160 },
	{ "Firing-range target", RANGE_PRESENCE, 0, 120 },
	{ "Enemy targeted: body", LOCK, 0, 1000 },
	{ "Enemy targeted: head", LOCK, 1, 1000 },
	{ "Enemy targeted: arm", LOCK, 2, 1000 },
	{ "Protected person targeted: body", LOCK, 3, 1000 },
	{ "Protected person targeted: head", LOCK, 4, 1000 },
	{ "Protected person targeted: arm", LOCK, 5, 1000 },
	{ "Destructible object targeted", LOCK, 3, 1000 },
	{ "Enemy targeted through glass", LOCK, 6, 1000 },
	{ "Protected target through glass", LOCK, 7, 1000 },
	{ "Valid special-device target", LOCK, 0, 1000 },
	{ "Range aim: outer scoring area", RANGE_LOCK, 0, 1000 },
	{ "Range aim: bullseye", RANGE_LOCK, 1, 1000 },
	{ "Threat detector: new contact", THREAT, 0, 140 },
	{ "Threat detector: targeted", THREAT, 1, 890 },
	{ "Cane: nearby barrier", CANE, 0, 55 },
	{ "Cane: distant barrier", CANE, 1, 55 },
	{ "Cane: uncertain rise", CANE, 2, 160 },
	{ "Cane: uncertain descent", CANE, 3, 160 },
	{ "Cane: ladder", CANE, 4, 240 },
	{ "Cane: crouch passage", CANE, 5, 155 },
	{ "Cane: isolated drop", CANE, 6, 280 },
	{ "Cane: rising floor contour", PATH, 0, 500 },
	{ "Cane: descending floor contour", PATH, 1, 500 },
	{ "Cane: contour ending at wall", PATH, 2, 500 },
	{ "Stair boundaries: ascending", RUNWAY, 0, 260 },
	{ "Stair boundaries: descending", RUNWAY, 1, 260 },
	{ "Dangerous drop-edge warning", EDGE, 0, 1600 },
	{ "Hoverbike blocked", VEHICLE, 0, 110 },
	{ "Environmental laser hazard", LASER, 0, 1500 },
	{ "Incoming rocket: distant", ROCKET, 0, 180 },
	{ "Incoming rocket: urgent", ROCKET, 1, 110 },
	{ "R-Tracker: mission object", TRACKER, 0, 140 },
	{ "R-Tracker: character", TRACKER, 1, 140 },
	{ "R-Tracker: cheat item", TRACKER, 2, 140 },
	{ "R-Tracker: above", TRACKER, 3, 140 },
	{ "R-Tracker: below", TRACKER, 4, 140 },
	{ "R-Tracker: behind", TRACKER, 5, 140 },
	{ "IR Scanner highlighted object", TRACKER, 0, 140 },
	{ "Combat radar: enemy", RADAR, 0, 140 },
	{ "Combat radar: ally", RADAR, 1, 140 },
	{ "Combat radar: objective", RADAR, 2, 140 },
	{ "Combat radar: other contact", RADAR, 3, 140 },
	{ "Combat radar: above", RADAR, 4, 140 },
	{ "Combat radar: below", RADAR, 5, 140 },
	{ "Combat radar: behind", RADAR, 6, 140 },
	{ "Combat radar: pulse start", RADAR, 7, 40 },
	{ "Combat radar: no contacts", RADAR, 8, 100 },
	{ "Combat radar: unavailable", RADAR, 9, 100 },
	{ "Hill: center beacon", HILL, 0, 950 },
	{ "Hill: scoring", HILL, 1, 950 },
	{ "Hill: behind", HILL, 2, 950 },
	{ "Hill: radar direction guide", HILL, 3, 1950 },
	{ "Player marker 1", MARKER, 0, 1950 },
	{ "Player marker 2", MARKER, 1, 1950 },
	{ "Player marker 3", MARKER, 2, 1950 },
	{ "Player marker 4", MARKER, 3, 1950 },
	{ "Remove player marker 1", REMOVAL, 0, 650 },
	{ "Remove player marker 2", REMOVAL, 1, 650 },
	{ "Remove player marker 3", REMOVAL, 2, 650 },
	{ "Remove player marker 4", REMOVAL, 3, 650 },
	{ "Mission objective landmark", LANDMARK, 0, 1950 },
	{ "Vulnerable boss spike", LANDMARK, 1, 1950 },
	{ "Intact Battle Shrine pillar", LANDMARK, 2, 2200 },
	{ "Scanner enabled", TOGGLE, 1, 120 },
	{ "Scanner disabled", TOGGLE, 0, 120 },
	{ "Cane slow mode", CANE_MODE, 1, 120 },
	{ "Cane fast mode", CANE_MODE, 2, 180 },
	{ "Cane off", CANE_MODE, 0, 120 },
	{ "Compass: north", COMPASS, 1, 60 },
	{ "Compass: east", COMPASS, 2, 160 },
	{ "Compass: south", COMPASS, 3, 260 },
	{ "Compass: west", COMPASS, 4, 360 },
	{ "Standing", STANCE, CROUCHPOS_STAND, 60 },
	{ "Crouching", STANCE, CROUCHPOS_DUCK, 120 },
	{ "Double-crouching", STANCE, CROUCHPOS_SQUAT, 180 },
	{ "Weapon primary function", FUNCTION, 0, 55 },
	{ "Weapon secondary function", FUNCTION, 1, 120 },
};

static s32 g_GlossaryPlaying = -1;
static u64 g_GlossaryStarted;
static u64 g_GlossaryNextPulse;

s32 accessibilityGlossaryGetCount(void)
{
	return sizeof(g_Glossary) / sizeof(g_Glossary[0]);
}

const char *accessibilityGlossaryGetName(s32 index)
{
	return index >= 0 && index < accessibilityGlossaryGetCount()
			? g_Glossary[index].name : "";
}

void accessibilityGlossaryStop(void)
{
	if (g_GlossaryPlaying >= 0) {
		accessibilityLogEvent("glossary", "stop", "index=%d name=%s",
				g_GlossaryPlaying, g_Glossary[g_GlossaryPlaying].name);
	}
	g_GlossaryPlaying = -1;
	accessibilityTonePreviewStop();
}

void accessibilityGlossaryPlay(s32 index)
{
	const struct glossaryentry *entry;
	f32 frequency = accessibilityGetEnemyFrequency();
	f32 volume = accessibilityGetVirtualCaneVolume();
	f32 enemyvolume;
	f32 markervolume;
	f32 radarvolume;
	f32 projectilevolume;
	s32 v;

	accessibilityGlossaryStop();
	if (!accessibilityIsEnabled() || index < 0
			|| index >= accessibilityGlossaryGetCount()) return;
	entry = &g_Glossary[index];
	v = entry->variant;
	accessibilityGetEnemyTuning(NULL, NULL, NULL, &enemyvolume);
	accessibilityGetMarkerTuning(NULL, &markervolume);
	accessibilityGetCombatRadarTuning(NULL, NULL, &radarvolume);
	accessibilityGetProjectileHazardTuning(NULL, &projectilevolume);
	accessibilityTonePreviewBegin();
	switch (entry->kind) {
	case DOOR:
		accessibilityToneSetDoorSlot(0, 1, 1.0f, 0.0f, 1);
		break;
	case OBJECT:
		accessibilityTonePlayChirp(880.0f, 1.0f, 0.0f);
		break;
	case PICKUP:
		accessibilityTonePlayChirpPattern(880.0f, 1.0f, 0.0f, 3, 1.0f);
		break;
	case MOVABLE:
		accessibilityTonePlayMovableLandmark(1.0f, 0.0f);
		break;
	case PERSON:
		accessibilityToneSetFriendlySlot(0, 1, 2.0f, 0.0f, v, 1);
		break;
	case ENEMY:
		accessibilityToneSetCombatSlot(0, 1, frequency,
				v == 3 ? frequency * 1.5f : v == 4 ? frequency * 0.7f : frequency,
				1.0f, v == 5 ? -0.8f : v == 6 ? 0.8f : 0.0f,
				10000, v == 1 ? 50 : v >= 5 ? 100 : 180,
				ACCESSIBILITY_TONE_COMBAT_CONTOUR_BASE_THEN_END, v == 2, 1, 1);
		break;
	case CAMERA:
		accessibilityToneSetCombatSlot(0, 1, 1600, 1000, 1.0f,
				0, 10000, 140, ACCESSIBILITY_TONE_COMBAT_CONTOUR_LINEAR, 0, 1, 1);
		break;
	case RANGE_PRESENCE:
		accessibilityTonePlayTargetPresence(frequency, enemyvolume, 0);
		break;
	case LOCK: {
		static const f32 pitches[] = { 660, 825, 528, 396, 495, 316.8f, 660, 396 };
		s32 flags = (v >= 3 && v <= 5) || v == 7
				? ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_INTERRUPTED : 0;
		if (v >= 6) flags |= ACCESSIBILITY_TONE_ALIGNMENT_PATTERN_PENETRABLE;
		accessibilityToneSetAlignment(1, pitches[v], flags);
		break;
	}
	case RANGE_LOCK:
		accessibilityToneSet(1, v ? 1320 : 660);
		break;
	case THREAT:
		accessibilityTonePlayThreatAlert(enemyvolume, 0);
		if (v) accessibilityToneSet(1, 660);
		break;
	case CANE: {
		f32 nearhz, farhz;
		f32 start, end;
		s32 duration = 35;
		s32 pattern = ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR;
		accessibilityGetVirtualCanePitch(&nearhz, &farhz);
		start = end = v == 0 ? nearhz : v == 1 ? farhz : sqrtf(nearhz * farhz);
		if (v == 2) { start *= 0.7f; duration = 140; }
		if (v == 3) { end *= 0.7f; duration = 140; }
		if (v == 4) { start *= 0.7f; end *= 2; duration = 220; }
		if (v == 5) { end /= 1.5f; duration = 135; pattern = ACCESSIBILITY_TONE_CANE_PATTERN_CROUCH_DOUBLE; }
		if (v == 6) { start *= 1.2f; end *= 0.35f; duration = 260; }
		accessibilityTonePlayCaneSlot(0, start, end, volume, 0, duration,
				pattern, 0, 0, NULL, NULL, NULL);
		break;
	}
	case PATH:
	case RUNWAY: {
		f32 hz[] = { 424, 424, 500, 600, 600, 400 };
		f32 gains[] = { volume, volume * .9f, volume * .8f,
				volume * .7f, volume * .6f, volume * .5f };
		f32 pans[] = { -.6f, -.6f, -.6f, .6f, .6f, .6f };
		s32 i;
		if (entry->kind == RUNWAY) {
			for (i = 0; i < 6; i++) {
				hz[i] = (i < 3 ? 330 : 440) * powf(2,
						(v ? -1 : 1) * (i % 3) * .15f);
			}
		} else if (v == 1) {
			for (i = 0; i < 5; i++) hz[i] = 424.0f * 424.0f / hz[i];
		}
		accessibilityTonePlayCaneSlot(0, hz[0], hz[5], volume, 0,
				entry->kind == RUNWAY ? 240 : 480,
				entry->kind == RUNWAY ? ACCESSIBILITY_TONE_CANE_PATTERN_RUNWAY
						: ACCESSIBILITY_TONE_CANE_PATTERN_PATH,
				v == 2 ? 5 : 6, 6, hz, gains, entry->kind == RUNWAY ? pans : NULL);
		break;
	}
	case EDGE:
		accessibilityToneSetCaneEdge(1, volume, -.8f);
		break;
	case VEHICLE:
		accessibilityTonePlayCaneSlot(0, 220, 140, volume * .9f, 0, 90,
				ACCESSIBILITY_TONE_CANE_PATTERN_CONTOUR, 0, 0, NULL, NULL, NULL);
		break;
	case LASER:
		accessibilityToneSetHazard(1, 220, 1, -.8f);
		break;
	case ROCKET:
		accessibilityToneSetProjectileSlot(0, 1, v ? 520 : 260,
				projectilevolume, 0, 10000, v ? 90 : 160, 1);
		break;
	case TRACKER:
		accessibilityToneSetTrackerSlot(0, 1, v == 1 ? 1200 : v == 2 ? 1800 : 1450,
				1, 0, 10000, v == 3 ? 1 : v == 4 ? -1 : 0, v == 5, 1);
		break;
	case RADAR: {
		static const s32 kinds[] = {
			ACCESSIBILITY_TONE_RADAR_ENEMY, ACCESSIBILITY_TONE_RADAR_ALLY,
			ACCESSIBILITY_TONE_RADAR_OBJECTIVE, ACCESSIBILITY_TONE_RADAR_OTHER,
			ACCESSIBILITY_TONE_RADAR_ENEMY, ACCESSIBILITY_TONE_RADAR_ENEMY,
			ACCESSIBILITY_TONE_RADAR_ENEMY, ACCESSIBILITY_TONE_RADAR_LAUNCH,
			ACCESSIBILITY_TONE_RADAR_EMPTY, ACCESSIBILITY_TONE_RADAR_UNAVAILABLE,
		};
		accessibilityTonePlayRadarPing(v == 7 ? 1500 : v == 8 ? 760 : v == 9 ? 320 : 1000,
				radarvolume, 0, v == 4 ? 1 : v == 5 ? -1 : 0, v == 6, kinds[v]);
		break;
	}
	case HILL:
		accessibilityToneSetHillBeacon(1, v == 3 ? markervolume * .5f : markervolume,
				0, v == 2, 1, v == 1, 1);
		break;
	case MARKER:
		accessibilityToneSetMarkerSlot(v, 1, markervolume, 0, 1);
		break;
	case REMOVAL:
		accessibilityTonePlayMarkerRemoval(v);
		break;
	case LANDMARK:
		accessibilityToneSetLandmarkSlot(0, 1, markervolume, 0, v, 1);
		break;
	case TOGGLE:
		accessibilityTonePlayToggleConfirmation(v);
		break;
	case CANE_MODE:
		accessibilityTonePlayCaneModeConfirmation(v);
		break;
	case COMPASS:
		accessibilityTonePlayCompass(v);
		break;
	case STANCE:
		accessibilityTonePlayStanceConfirmation(v);
		break;
	case FUNCTION:
		accessibilityTonePlayWeaponFunction(v);
		break;
	}
	accessibilityTonePreviewCommit(entry->durationms);
	g_GlossaryPlaying = index;
	g_GlossaryStarted = sysGetMicroseconds();
	g_GlossaryNextPulse = g_GlossaryStarted + 250000;
	accessibilityLogEvent("glossary", "play",
			"index=%d name=%s kind=%d variant=%d duration_ms=%d",
			index, entry->name, entry->kind, v, entry->durationms);
}

void accessibilityGlossaryTick(void)
{
	u64 now;
	const struct glossaryentry *entry;
	if (g_GlossaryPlaying < 0) return;
	now = sysGetMicroseconds();
	entry = &g_Glossary[g_GlossaryPlaying];
	if (!accessibilityIsEnabled()
			|| now - g_GlossaryStarted >= (u64)entry->durationms * 1000) {
		accessibilityGlossaryStop();
		return;
	}
	accessibilityTonePreviewCommands(1);
	if (entry->kind == EDGE || entry->kind == LASER) {
		f32 t = (f32)(now - g_GlossaryStarted) / (entry->durationms * 1000.0f);
		f32 pan = t < .5f ? -.8f + 3.2f * t : 2.4f - 3.2f * t;
		if (entry->kind == EDGE) {
			accessibilityToneSetCaneEdge(1, accessibilityGetVirtualCaneVolume(), pan);
		} else {
			accessibilityToneSetHazard(1, 220, 1, pan);
		}
	} else if (entry->kind == THREAT && entry->variant
			&& now >= g_GlossaryNextPulse) {
		f32 volume;
		accessibilityGetEnemyTuning(NULL, NULL, NULL, &volume);
		accessibilityTonePlayThreatAlert(volume, 0);
		g_GlossaryNextPulse = now + 250000;
	}
	accessibilityTonePreviewCommands(0);
}

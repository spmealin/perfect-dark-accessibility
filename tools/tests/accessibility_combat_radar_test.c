#include <assert.h>
#include "../../src/accessibility/accessibility_combat_radar.c"

struct g_vars g_Vars;
static s32 beaconprop = -1;
static s32 played;
static s32 lastpulses;
static f32 lastgain;

s32 accessibilityGetCombatRadarContactAlerts(void) { return true; }
s32 accessibilityTargetingHasAudibleCombatBeacon(s32 playernum, s32 propnum)
{
	return propnum == beaconprop;
}
void accessibilityGetCombatRadarTuning(f32 *medium, f32 *close, f32 *volume)
{
	if (medium) *medium = 2000;
	if (close) *close = 750;
	if (volume) *volume = 0.2f;
}
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...) {}
void accessibilityTonePlayRadarBurst(f32 frequency, f32 gain, f32 pan,
		s32 height, s32 rear, s32 kind, s32 pulses)
{
	played++;
	lastpulses = pulses;
	lastgain = gain;
}
#if ACCESSIBILITY_BINAURAL_EXPERIMENT
void accessibilityToneSetSpatialPosition(s32 slot, const struct coord *position) {}
#endif

static void setMarker(s32 i, f32 distance)
{
	struct accessibilitycombatradarmarker *marker = &g_AccessibilityCombatRadarFrame.markers[i];
	marker->identity = i + 1;
	marker->propnum = i;
	marker->category = ACCESSIBILITY_COMBAT_RADAR_CATEGORY_ENEMY;
	marker->horizontaldistance = distance;
}

int main(void)
{
	struct accessibilitycombatradarevent *events = g_AccessibilityCombatRadarEvents;
	g_AccessibilityCombatRadarFrame.available = true;
	g_AccessibilityCombatRadarFrame.retainedcount = 3;
	setMarker(0, 3000);
	setMarker(1, 600);
	setMarker(2, 500);
	accessibilityCombatRadarScheduleManual();
	assert(g_AccessibilityCombatRadarEventCount == 4);
	assert(events[1].propnum == 2 && events[2].propnum == 1 && events[3].propnum == 0);
	g_AccessibilityCombatRadarEventCount = 0;
	accessibilityCombatRadarBaselineContacts();

	/* A new close ping wins over a queued far alert. */
	accessibilityCombatRadarQueueContactEvent(&g_AccessibilityCombatRadarFrame.markers[0],
			ACCESSIBILITY_COMBAT_RADAR_EVENT_NEW);
	accessibilityCombatRadarEmitNext();
	assert(lastpulses == 1 && g_AccessibilityCombatRadarContacts[2].nextclosetick > 0);
	assert(lastgain < 0.13f);
	g_Vars.lvframe60 = g_AccessibilityCombatRadarNextVoiceFrame;
	accessibilityCombatRadarEmitNext();
	assert(lastpulses == 1 && g_AccessibilityCombatRadarContacts[1].nextclosetick > 0);
	g_Vars.lvframe60 = g_AccessibilityCombatRadarNextVoiceFrame;
	accessibilityCombatRadarEmitNext();
	assert(lastpulses == 3);

	/* Medium burst, followed by close handoff and immediate return from cover. */
	setMarker(0, 1500);
	accessibilityCombatRadarUpdateContacts();
	g_Vars.lvframe60 = g_AccessibilityCombatRadarNextVoiceFrame;
	accessibilityCombatRadarEmitNext();
	assert(lastpulses == 2);
	beaconprop = 2;
	g_AccessibilityCombatRadarFrame.retainedcount = 1;
	g_AccessibilityCombatRadarFrame.markers[0] = g_AccessibilityCombatRadarFrame.markers[2];
	g_Vars.lvframe60 += TICKS(60);
	accessibilityCombatRadarEmitNext();
	assert(played == 4);
	beaconprop = -1;
	accessibilityCombatRadarEmitNext();
	assert(played == 5 && lastpulses == 1);
	return 0;
}

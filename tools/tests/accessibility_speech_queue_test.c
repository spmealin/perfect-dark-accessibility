/* Production queue + announcement adapters, injected monotonic clock/provider. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../src/accessibility/accessibility_speech_queue.c"
#include "../../src/accessibility/accessibility_announcement.c"
static u64 clockus = 1000000;
static s32 outputs, cancels, speaking = -1, succeed = 1, interrupts[512];
static char spoken[512][128];
u64 sysGetMicroseconds(void) { return clockus; }
void accessibilityGetSpeechQueueTuning(s32 *ms, s32 *grace) { *ms = 60; *grace = 200; }
s32 accessibilityGetPickupSpeechDurationMs(void) { return 1000; }
s32 accessibilitySpeechIsAvailable(void) { return 1; }
s32 accessibilitySpeechGetSpeaking(void) { return speaking; }
s32 accessibilitySpeechCancel(void) { cancels++; return 1; }
s32 accessibilitySpeechOutput(const char *text, s32 interrupt)
{
	assert(outputs < 512);
	snprintf(spoken[outputs], sizeof(spoken[outputs]), "%s", text);
	interrupts[outputs++] = interrupt;
	return succeed;
}
void accessibilityLogEvent(const char *category, const char *event, const char *format, ...) { }
static void fresh(void)
{
	accessibilitySpeechQueueReset();
	outputs = cancels = 0; speaking = -1; succeed = 1; clockus += 1000000;
	accessibilitySpeechQueueTick(1, 0, 1);
}
static void tick(u64 delta) { clockus += delta; accessibilitySpeechQueueTick(1, 0, 1); }
int main(void)
{
	s32 i;
	fresh();
	accessibilityAnnouncementWeaponChange("Callisto NTG", "quick_selection", 0, 1);
	tick(84000);
	accessibilityAnnouncementStatus("East", "compass", 0, 1);
	tick(0);
	assert(outputs == 1 && !strcmp(spoken[0], "Callisto NTG"));
	tick(1000000); assert(outputs == 1 && compass == -1);
	fresh();
	accessibilityAnnouncementQueueHud("Picked up ammo", 0, 0, 0, -1, 1);
	tick(0); assert(outputs == 1 && !interrupts[0]);
	accessibilityAnnouncementWeaponChange("Falcon 2", "quick_selection", 0, 1);
	assert(outputs == 2 && interrupts[1] && hudcount == 1);
	accessibilityAnnouncementWeaponChange("Dragon", "quick_selection", 0, 1);
	assert(outputs == 3 && hudcount == 1);
	tick(2000000);
	assert(outputs == 4 && !strcmp(spoken[3], "Picked up ammo") && !interrupts[3]);
	fresh();
	for (i = 0; i < 3; i++) accessibilityAnnouncementQueueHud("Ammo", 0, 0, 0, -1, i);
	for (i = 0; i < 3; i++) tick(1000000);
	assert(outputs == 3 && !hudcount);
	fresh();
	accessibilityAnnouncementWeaponChange("Falcon 2", "quick_selection", 0, 1);
	accessibilityAnnouncementWeaponChange("Dragon", "quick_selection", 0, 1);
	accessibilityAnnouncementWeaponChange("Falcon 2", "quick_selection", 0, 1);
	accessibilityAnnouncementWeaponChange("Double Falcon 2", "quick_selection", 0, 1);
	assert(outputs == 4 && !weaponcount); /* All in one tick, no debounce. */
	assert(!strcmp(spoken[2], "Falcon 2") && !strcmp(spoken[3], "Double Falcon 2"));
	accessibilityAnnouncementWeaponFunction("20 ammo", 0);
	assert(outputs == 4 && weaponcount == 1);
	tick(2000000); assert(outputs == 5 && !interrupts[4]);
	fresh();
	accessibilityAnnouncementWeaponChange("Falcon", "quick_selection", 0, 1);
	accessibilityAnnouncementWeaponFunction("20 ammo", 0);
	accessibilityAnnouncementWeaponChange("Dragon", "quick_selection", 0, 1);
	assert(!weaponcount); /* Old ammo never follows a newer selection. */
	fresh();
	accessibilityAnnouncementQueueHud("Objective complete", 0, 0, 0, -1, 1);
	tick(0);
	accessibilityAnnouncementReplaceMenu("Pause menu", ACCESSIBILITY_ANNOUNCEMENT_DIALOG);
	accessibilitySpeechQueueTick(1, 1, 1);
	assert(hudcount == 1 && outputs == 2);
	clockus += 5000000; accessibilitySpeechQueueTick(1, 1, 1);
	assert(outputs == 2);
	accessibilityAnnouncementCancel(); tick(0);
	assert(outputs == 3 && !strcmp(spoken[2], "Objective complete"));
	fresh();
	accessibilitySpeechQueueCompassHeading(0);
	accessibilityAnnouncementStatus("North", "compass", 0, 1);
	accessibilitySpeechQueueCompassHeading(30); tick(0);
	assert(!outputs && compass == -1);
	fresh();
	accessibilityAnnouncementStatus("North", "compass", 0, 1); tick(0);
	i = cancels;
	accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_COMPASS, -1);
	assert(cancels == i && active < 0); /* A compass invalidation never stops other provider speech. */
	fresh();
	speaking = 0;
	accessibilityAnnouncementWeaponChange("A long weapon name", "quick_selection", 0, 1);
	tick(250000); assert(active >= 0); /* Startup idle is not completion. */
	speaking = 1; tick(50000); assert(active >= 0);
	speaking = 0; tick(50000); assert(active == -1);
	fresh();
	speaking = 1;
	accessibilityAnnouncementWeaponChange("Falcon", "quick_selection", 0, 1);
	tick(10000000); assert(active == -1); /* Stuck-busy watchdog. */
	fresh();
	succeed = 0;
	accessibilityAnnouncementWeaponChange("Falcon", "quick_selection", 0, 1);
	accessibilityAnnouncementWeaponChange("Dragon", "quick_selection", 0, 1);
	assert(outputs == 1 && weaponcount == 2);
	tick(100000); assert(outputs == 1);
	succeed = 1; tick(200000);
	assert(outputs == 2 && !strcmp(spoken[1], "Falcon"));
	tick(1000000); assert(outputs == 3 && !strcmp(spoken[2], "Dragon"));
	fresh();
	succeed = 0;
	accessibilityAnnouncementReplaceMenu("Old focus", ACCESSIBILITY_ANNOUNCEMENT_FOCUS);
	accessibilitySpeechQueueTick(1, 1, 1);
	accessibilityAnnouncementReplaceMenu("New focus", ACCESSIBILITY_ANNOUNCEMENT_FOCUS);
	assert(hudcount == 1);
	succeed = 1; clockus += 300000; accessibilitySpeechQueueTick(1, 1, 1);
	assert(outputs == 3 && !strcmp(spoken[2], "New focus"));
	accessibilityAnnouncementCancel(); tick(0); assert(!hudcount);
	fresh();
	for (i = 0; i < 70; i++) accessibilityAnnouncementQueueHud("Ammo", 0, 0, 0, -1, i);
	assert(hudcount == QUEUE_CAPACITY && overflows >= 6);
	accessibilitySpeechQueueTick(2, 0, 1); assert(!hudcount && active < 0);
	fresh();
	accessibilityAnnouncementRespawnCountdown("Wait 10", 0, 10, 1);
	accessibilityAnnouncementRespawnCountdown("9", 0, 9, 0);
	tick(0);
	assert(!strcmp(spoken[0], "Wait 10"));
	accessibilityAnnouncementRespawnCountdown("8", 0, 8, 0);
	assert(hudcount == 1 && !strcmp(messages[hud[0]].text, "8"));
	accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_COUNTDOWN, 0);
	assert(!hudcount && active < 0);
	accessibilityAnnouncementQueueHud("Ammo", 0, 0, 0, -1, 1);
	accessibilitySpeechQueueTick(1, 0, 0); assert(!hudcount);
	fresh();
	/* Semantic pickup flags, not English text matching. No added quiet gap;
	 * a weapon interrupts ammo, drops old ammo backlog, but preserves later ammo. */
	accessibilityAnnouncementQueueHud("Ammo one", 0, HUDMSGFLAG_PICKUP_AMMO, 0, -1, 1); tick(0);
	accessibilityAnnouncementQueueHud("Ammo two", 0, HUDMSGFLAG_PICKUP_AMMO, 0, -1, 2);
	tick(100000);
	accessibilityAnnouncementQueueHud("Picked up a weapon", 0, HUDMSGFLAG_PICKUP_WEAPON, 0, -1, 3);
	assert(outputs == 2 && interrupts[1] && !hudcount && !strcmp(spoken[1], "Picked up a weapon"));
	accessibilityAnnouncementQueueHud("Ammo three", 0, HUDMSGFLAG_PICKUP_AMMO, 0, -1, 4);
	speaking = 1; /* Even an overlong busy signal cannot add seconds of padding. */
	tick(950000); assert(outputs == 2);
	tick(50000); assert(outputs == 3 && !interrupts[2] && !strcmp(spoken[2], "Ammo three"));
	tick(1000000); assert(active < 0);
	fresh();
	accessibilityAnnouncementWeaponChange("Selection", "quick_selection", 0, 1);
	accessibilityAnnouncementQueueHud("Weapon pickup", 0, HUDMSGFLAG_PICKUP_WEAPON, 0, -1, 1);
	assert(outputs == 1); /* Selection priority is not weakened. */
	tick(1000000); assert(outputs == 2 && !strcmp(spoken[1], "Weapon pickup"));
	fresh();
	accessibilityAnnouncementQueueHud("Shield", 0, HUDMSGFLAG_PICKUP_ITEM, 0, -1, 1); tick(0);
	accessibilityAnnouncementQueueHud("Key", 0, HUDMSGFLAG_PICKUP_ITEM, 0, -1, 2);
	tick(1000000); assert(outputs == 2 && !interrupts[1]);
	fresh();
	for (i = 0; i < 100000; i++) tick(16667);
	assert(!outputs && active < 0 && !hudcount && !weaponcount); /* Idle service is inert. */
	puts("Speech priorities, immediate selections, HUD replay/FIFO, context, completion, retries and overflow passed.");
	return 0;
}

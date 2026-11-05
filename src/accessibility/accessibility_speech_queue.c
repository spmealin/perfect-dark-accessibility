/* Main-thread fixed storage. A provider accepting text is NOT a delivery ack. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_announcement.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_speech.h"
#include "accessibility/accessibility_speech_queue.h"

#define QUEUE_CAPACITY 64
#define MESSAGE_CAPACITY (QUEUE_CAPACITY * 2 + 4)
struct speechmessage {
	s32 used, kind, player, attempts;
	u64 id, created, retry;
	f32 heading;
	char text[ACCESSIBILITY_ANNOUNCEMENT_TEXT_MAX];
};
static struct speechmessage messages[MESSAGE_CAPACITY];
static s32 hud[QUEUE_CAPACITY], weapons[QUEUE_CAPACITY], hudcount, weaponcount;
static s32 active = -1, compass = -1, stageid = -1, inmenu;
static u64 nextid, started, deadline, nextpoll, overflows, replays;
static u64 pollcalls, polltotalus, pollmaxus, outputcalls, outputtotalus, outputmaxus;
static s32 sawbusy, millisecondspercharacter = 60, startupgracems = 200;
static f32 currentheading;

static s32 isPickup(s32 kind)
{
	return kind == ACCESSIBILITY_SPEECH_PICKUP_AMMO
			|| kind == ACCESSIBILITY_SPEECH_PICKUP_WEAPON
			|| kind == ACCESSIBILITY_SPEECH_PICKUP_ITEM;
}

static void record(const char *event, s32 index, const char *reason)
{
	struct speechmessage *m = index >= 0 ? &messages[index] : NULL;
	accessibilityLogEvent("speech_queue", event,
			"id=%llu kind=%d player=%d age_us=%llu hud_depth=%d weapon_depth=%d reason=%s text=%s",
			(unsigned long long)(m ? m->id : 0), m ? m->kind : -1,
			m ? m->player : -1,
			(unsigned long long)(m ? sysGetMicroseconds() - m->created : 0),
			hudcount, weaponcount, reason, m ? m->text : "");
}

static void discard(s32 index, const char *reason)
{
	if (index < 0) return;
	record("invalidated", index, reason);
	messages[index].used = 0;
}

static s32 take(s32 *queue, s32 *count, s32 position)
{
	s32 index = queue[position];
	memmove(queue + position, queue + position + 1, (--*count - position) * sizeof(*queue));
	return index;
}
static s32 pop(s32 *queue, s32 *count) { return take(queue, count, 0); }

static void enqueue(s32 index, s32 front)
{
	s32 isweapon = messages[index].kind == ACCESSIBILITY_SPEECH_WEAPON
			|| messages[index].kind == ACCESSIBILITY_SPEECH_FUNCTION;
	s32 *queue = isweapon ? weapons : hud;
	s32 *count = isweapon ? &weaponcount : &hudcount;
	if (*count == QUEUE_CAPACITY) {
		s32 lost = pop(queue, count);
		overflows++;
		discard(lost, "queue_overflow_oldest");
	}
	if (front) memmove(queue + 1, queue, *count * sizeof(*queue));
	queue[front ? 0 : *count] = index;
	++*count;
	record("queued", index, front ? "replay_head" : "fifo");
}

static void preempt(void)
{
	if (active < 0) return;
	record("preempted", active, "higher_priority_or_new_selection");
	if (messages[active].kind == ACCESSIBILITY_SPEECH_HUD
			|| messages[active].kind == ACCESSIBILITY_SPEECH_COUNTDOWN_INITIAL
			|| isPickup(messages[active].kind)) {
		enqueue(active, 1);
		replays++;
	} else {
		discard(active, "superseded_after_dispatch");
	}
	active = -1;
}

static s32 dispatch(s32 index, s32 interrupt)
{
	struct speechmessage *m = &messages[index];
	u64 now = sysGetMicroseconds();
	u64 characters = 0;
	const unsigned char *cursor;
	s32 result = accessibilitySpeechOutput(m->text, interrupt);
	u64 elapsed = sysGetMicroseconds() - now;
	outputcalls++; outputtotalus += elapsed;
	if (elapsed > outputmaxus) outputmaxus = elapsed;
	record(result ? "backend_accepted" : "backend_rejected", index,
			interrupt ? "interrupt" : "append");
	if (!result) {
		m->attempts++;
		m->retry = now + 250000;
		if (m->attempts >= 3) discard(index, "backend_retry_exhausted");
		else enqueue(index, 1);
		return 0;
	}
	active = index;
	started = now;
	nextpoll = now + (u64)startupgracems * 1000;
	sawbusy = 0;
	for (cursor = (const unsigned char *)m->text; *cursor; cursor++) {
		if ((*cursor & 0xc0) != 0x80) characters++;
	}
	deadline = now + (u64)startupgracems * 1000
			+ characters * millisecondspercharacter * 1000;
	/* Owner-paced pickup budget: total duration, not duration plus grace or
	 * character estimate. Lower-priority output still appends, never cuts it. */
	if (isPickup(m->kind)) {
		deadline = now + (u64)accessibilityGetPickupSpeechDurationMs() * 1000;
	}
	return 1;
}

void accessibilitySpeechQueueInvalidate(enum accessibilityspeechkind kind, s32 player)
{
	s32 i, j;
	if (kind == ACCESSIBILITY_SPEECH_COUNTDOWN) {
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_COUNTDOWN_INITIAL, player);
	}
	for (j = 0; j < 2; j++) {
		s32 *queue = j ? weapons : hud;
		s32 *count = j ? &weaponcount : &hudcount;
		for (i = 0; i < *count;) {
			s32 index = queue[i];
			if (messages[index].kind == kind && (player < 0 || messages[index].player == player)) {
				discard(index, "source_invalidated");
				memmove(queue + i, queue + i + 1, (--*count - i) * sizeof(*queue));
			} else i++;
		}
	}
	if (compass >= 0 && kind == ACCESSIBILITY_SPEECH_COMPASS) {
		discard(compass, "compass_invalidated"); compass = -1;
	}
	if (active >= 0 && messages[active].kind == kind
			&& (player < 0 || messages[active].player == player)) {
		/* Compass must never issue a global stop: on an estimated provider
		 * deadline its appended word may still be behind higher-priority text. */
		if (kind != ACCESSIBILITY_SPEECH_COMPASS && kind != ACCESSIBILITY_SPEECH_MENU) accessibilitySpeechCancel();
		discard(active, "active_source_invalidated"); active = -1;
	}
}

s32 accessibilitySpeechQueueSubmit(enum accessibilityspeechkind kind,
		const char *text, s32 player)
{
	s32 index, urgent = kind == ACCESSIBILITY_SPEECH_WEAPON
			|| kind == ACCESSIBILITY_SPEECH_REQUEST || kind == ACCESSIBILITY_SPEECH_MENU;
	if (!text || !*text || strlen(text) >= ACCESSIBILITY_ANNOUNCEMENT_TEXT_MAX
			|| !accessibilitySpeechIsAvailable()) return 0;
	if (inmenu && (kind == ACCESSIBILITY_SPEECH_WEAPON || kind == ACCESSIBILITY_SPEECH_FUNCTION
			|| kind == ACCESSIBILITY_SPEECH_COMPASS)) return 0;
	if (kind == ACCESSIBILITY_SPEECH_WEAPON) {
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_FUNCTION, -1);
	}
	if (kind == ACCESSIBILITY_SPEECH_MENU) {
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_MENU, -1);
	}
	if (kind == ACCESSIBILITY_SPEECH_PICKUP_WEAPON) {
		/* Weapon discovery replaces low-value ammo chatter rather than
		 * replaying the interrupted ammo report or draining stale ammo first. */
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_PICKUP_AMMO, player);
		if (!inmenu && !weaponcount && (active < 0
				|| messages[active].kind == ACCESSIBILITY_SPEECH_COMPASS)) urgent = 1;
	}
	if (kind == ACCESSIBILITY_SPEECH_COUNTDOWN) {
		/* Preserve a current initial wait message; replace only pending seconds. */
		for (index = 0; index < hudcount; index++) {
			s32 slot = hud[index];
			if (messages[slot].kind == kind && messages[slot].player == player) {
				record("replaced", slot, "latest_countdown");
				snprintf(messages[slot].text, sizeof(messages[slot].text), "%s", text);
				messages[slot].created = sysGetMicroseconds();
				return 1;
			}
		}
	}
	for (index = 0; index < MESSAGE_CAPACITY && messages[index].used; index++);
	if (index == MESSAGE_CAPACITY) { overflows++; record("overflow", -1, "pool_full"); return 0; }
	memset(&messages[index], 0, sizeof(messages[index]));
	messages[index].used = 1; messages[index].kind = kind; messages[index].player = player;
	messages[index].id = ++nextid; messages[index].created = sysGetMicroseconds();
	messages[index].heading = currentheading;
	strcpy(messages[index].text, text);
	record("submitted", index, "event");
	if (kind == ACCESSIBILITY_SPEECH_COMPASS) {
		discard(compass, "newer_crossing"); compass = index; return 1;
	}
	if (urgent) {
		/* Never discard an undispatched weapon in favor of a newer one. */
		if (kind == ACCESSIBILITY_SPEECH_WEAPON && weaponcount) { enqueue(index, 0); return 1; }
		preempt();
		dispatch(index, 1);
	} else enqueue(index, 0);
	return 1;
}

s32 accessibilitySpeechQueueCancel(void)
{
	s32 result = accessibilitySpeechCancel();
	preempt();
	accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_MENU, -1);
	return result;
}

void accessibilitySpeechQueueReset(void)
{
	s32 i;
	if (active >= 0) accessibilitySpeechCancel();
	for (i = 0; i < MESSAGE_CAPACITY; i++) if (messages[i].used) discard(i, "context_reset");
	hudcount = weaponcount = 0; active = compass = -1;
	inmenu = 0; stageid = -1;
}

void accessibilitySpeechQueueCompassHeading(f32 heading) { currentheading = heading; }

void accessibilitySpeechQueueTick(s32 stage, s32 menu, s32 enabled)
{
	u64 now = sysGetMicroseconds();
	if (!enabled) { if (active >= 0 || hudcount || weaponcount || compass >= 0) accessibilitySpeechQueueReset(); return; }
	accessibilityGetSpeechQueueTuning(&millisecondspercharacter, &startupgracems);
	if (stageid >= 0 && stage != stageid) accessibilitySpeechQueueReset();
	stageid = stage;
	if (menu && !inmenu) {
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_WEAPON, -1);
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_FUNCTION, -1);
		accessibilitySpeechQueueInvalidate(ACCESSIBILITY_SPEECH_COMPASS, -1);
		if (active >= 0 && messages[active].kind != ACCESSIBILITY_SPEECH_MENU
				&& messages[active].kind != ACCESSIBILITY_SPEECH_REQUEST) accessibilitySpeechQueueCancel();
	}
	inmenu = menu;
	if (active >= 0 && now >= nextpoll) {
		s32 speaking = accessibilitySpeechGetSpeaking();
		u64 elapsed = sysGetMicroseconds() - now;
		pollcalls++; polltotalus += elapsed;
		if (elapsed > pollmaxus) pollmaxus = elapsed;
		nextpoll = now + 50000;
		if (speaking > 0) sawbusy = 1;
		if ((speaking == 0 && sawbusy) || (now >= deadline
				&& (speaking <= 0 || isPickup(messages[active].kind)))
				|| now - started > (deadline - started) * 3 + 5000000) {
			record(speaking == 0 && sawbusy ? "provider_idle" : "estimated_finished", active,
					"not_delivery_acknowledgement");
			messages[active].used = 0; active = -1;
		}
	}
	if (compass >= 0) {
		f32 difference = fabsf(currentheading - messages[compass].heading);
		if (difference > 180) difference = 360 - difference;
		if (now - messages[compass].created > 500000 || difference > 15) {
			discard(compass, "stale_crossing"); compass = -1;
		}
	}
	if (inmenu) {
		s32 i;
		if (active >= 0) return;
		for (i = 0; i < hudcount; i++) {
			s32 index = hud[i];
			if ((messages[index].kind == ACCESSIBILITY_SPEECH_MENU
					|| messages[index].kind == ACCESSIBILITY_SPEECH_REQUEST)
					&& messages[index].retry <= now) {
				dispatch(take(hud, &hudcount, i), 1);
				break;
			}
		}
		return;
	}
	if (weaponcount && (active < 0 || messages[active].kind == ACCESSIBILITY_SPEECH_HUD
			|| messages[active].kind == ACCESSIBILITY_SPEECH_COMPASS
			|| messages[active].kind == ACCESSIBILITY_SPEECH_COUNTDOWN
			|| messages[active].kind == ACCESSIBILITY_SPEECH_COUNTDOWN_INITIAL
			|| isPickup(messages[active].kind))) {
		if (messages[weapons[0]].retry <= now) {
			s32 index = pop(weapons, &weaponcount);
			s32 interrupt = active >= 0 || messages[index].kind == ACCESSIBILITY_SPEECH_WEAPON;
			preempt(); dispatch(index, interrupt);
		}
		return;
	}
	if (active >= 0) {
		if (hudcount && messages[active].kind == ACCESSIBILITY_SPEECH_COMPASS) {
			preempt(); dispatch(pop(hud, &hudcount), 1);
		}
		return;
	}
	if (hudcount) {
		if (messages[hud[0]].retry <= now) dispatch(pop(hud, &hudcount), 0);
	} else if (compass >= 0 && !weaponcount) {
		s32 index = compass; compass = -1; dispatch(index, 0);
	}
}

void accessibilitySpeechQueueDump(u64 capture)
{
	s32 i;
	accessibilityLogEvent("incident", "speech_queue",
			"capture=%llu active_id=%llu hud_depth=%d weapon_depth=%d compass_id=%llu overflows=%llu replays=%llu ms_per_character=%d startup_grace_ms=%d",
			(unsigned long long)capture, (unsigned long long)(active >= 0 ? messages[active].id : 0),
			hudcount, weaponcount, (unsigned long long)(compass >= 0 ? messages[compass].id : 0),
			(unsigned long long)overflows, (unsigned long long)replays,
			millisecondspercharacter, startupgracems);
	for (i = 0; i < MESSAGE_CAPACITY; i++) if (messages[i].used) record("capture_pending", i, "diagnostic");
	accessibilityLogEvent("incident", "speech_queue_cost",
			"capture=%llu output_calls=%llu output_total_us=%llu output_max_us=%llu poll_calls=%llu poll_total_us=%llu poll_max_us=%llu",
			(unsigned long long)capture, (unsigned long long)outputcalls,
			(unsigned long long)outputtotalus, (unsigned long long)outputmaxus,
			(unsigned long long)pollcalls, (unsigned long long)polltotalus,
			(unsigned long long)pollmaxus);
	accessibilityLogEvent("incident", "speech_pickup_timing", "capture=%llu pickup_duration_ms=%d",
			(unsigned long long)capture, accessibilityGetPickupSpeechDurationMs());
}

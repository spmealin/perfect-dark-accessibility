#ifndef ACCESSIBILITY_SPEECH_QUEUE_H
#define ACCESSIBILITY_SPEECH_QUEUE_H
#include <PR/ultratypes.h>
enum accessibilityspeechkind {
	ACCESSIBILITY_SPEECH_HUD, ACCESSIBILITY_SPEECH_COMPASS,
	ACCESSIBILITY_SPEECH_WEAPON, ACCESSIBILITY_SPEECH_FUNCTION,
	ACCESSIBILITY_SPEECH_COUNTDOWN, ACCESSIBILITY_SPEECH_REQUEST,
	ACCESSIBILITY_SPEECH_MENU, ACCESSIBILITY_SPEECH_COUNTDOWN_INITIAL,
	ACCESSIBILITY_SPEECH_PICKUP_AMMO, ACCESSIBILITY_SPEECH_PICKUP_WEAPON,
	ACCESSIBILITY_SPEECH_PICKUP_ITEM
};
s32 accessibilitySpeechQueueSubmit(enum accessibilityspeechkind kind,
		const char *text, s32 player);
void accessibilitySpeechQueueTick(s32 stage, s32 menu, s32 enabled);
void accessibilitySpeechQueueReset(void);
s32 accessibilitySpeechQueueCancel(void);
void accessibilitySpeechQueueInvalidate(enum accessibilityspeechkind kind, s32 player);
void accessibilitySpeechQueueCompassHeading(f32 heading);
void accessibilitySpeechQueueDump(u64 capture);
#endif

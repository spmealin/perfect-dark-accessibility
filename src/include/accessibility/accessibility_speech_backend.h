#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_SPEECH_BACKEND_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_SPEECH_BACKEND_H

#include <PR/ultratypes.h>

s32 accessibilitySpeechBackendInit(void);
void accessibilitySpeechBackendShutdown(void);
s32 accessibilitySpeechBackendIsAvailable(void);
const char *accessibilitySpeechBackendGetName(void);
s32 accessibilitySpeechBackendOutput(const char *text, s32 interrupt);
s32 accessibilitySpeechBackendCancel(void);

#endif

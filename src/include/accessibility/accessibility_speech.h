#ifndef _IN_ACCESSIBILITY_ACCESSIBILITY_SPEECH_H
#define _IN_ACCESSIBILITY_ACCESSIBILITY_SPEECH_H

#include <PR/ultratypes.h>

// All text is UTF-8. This API is main-thread-only until synchronization is added.
s32 accessibilitySpeechInit(void);
void accessibilitySpeechShutdown(void);
s32 accessibilitySpeechIsAvailable(void);
const char *accessibilitySpeechGetBackendName(void);
s32 accessibilitySpeechOutput(const char *text, s32 interrupt);
s32 accessibilitySpeechCancel(void);

#endif

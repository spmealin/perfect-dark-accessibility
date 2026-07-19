#include <PR/ultratypes.h>
#include "accessibility/accessibility_speech_backend.h"

s32 accessibilitySpeechBackendInit(void)
{
	return 0;
}

void accessibilitySpeechBackendShutdown(void)
{
}

s32 accessibilitySpeechBackendIsAvailable(void)
{
	return 0;
}

const char *accessibilitySpeechBackendGetName(void)
{
	return "";
}

s32 accessibilitySpeechBackendOutput(const char *text, s32 interrupt)
{
	(void)text;
	(void)interrupt;
	return 0;
}

s32 accessibilitySpeechBackendCancel(void)
{
	return 0;
}

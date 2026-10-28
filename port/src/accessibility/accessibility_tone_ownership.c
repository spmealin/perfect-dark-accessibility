#include "accessibility_tone_ownership.h"

enum accessibilitytoneowner {
	ACCESSIBILITY_TONE_OWNER_WORLD,
	ACCESSIBILITY_TONE_OWNER_PREVIEW_COMMANDS,
	ACCESSIBILITY_TONE_OWNER_PREVIEW_HELD,
};

static enum accessibilitytoneowner owner;

int accessibilityToneOwnershipCanPublish(void)
{
	return owner != ACCESSIBILITY_TONE_OWNER_PREVIEW_HELD;
}

int accessibilityToneOwnershipIsPreview(void)
{
	return owner != ACCESSIBILITY_TONE_OWNER_WORLD;
}

int accessibilityToneOwnershipCanCommit(void)
{
	return owner == ACCESSIBILITY_TONE_OWNER_PREVIEW_COMMANDS;
}

void accessibilityToneOwnershipBegin(void)
{
	owner = ACCESSIBILITY_TONE_OWNER_PREVIEW_COMMANDS;
}

void accessibilityToneOwnershipCommands(int enabled)
{
	if (accessibilityToneOwnershipIsPreview()) {
		owner = enabled ? ACCESSIBILITY_TONE_OWNER_PREVIEW_COMMANDS
				: ACCESSIBILITY_TONE_OWNER_PREVIEW_HELD;
	}
}

void accessibilityToneOwnershipRelease(void)
{
	owner = ACCESSIBILITY_TONE_OWNER_WORLD;
}

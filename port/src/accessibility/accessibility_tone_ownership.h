#ifndef PD_ACCESSIBILITY_TONE_OWNERSHIP_H
#define PD_ACCESSIBILITY_TONE_OWNERSHIP_H

/* Main-thread policy only. Never read this state from the audio callback. */
int accessibilityToneOwnershipCanPublish(void);
int accessibilityToneOwnershipIsPreview(void);
int accessibilityToneOwnershipCanCommit(void);
void accessibilityToneOwnershipBegin(void);
void accessibilityToneOwnershipCommands(int enabled);
void accessibilityToneOwnershipRelease(void);

#endif

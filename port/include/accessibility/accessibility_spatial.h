#ifndef PD_ACCESSIBILITY_SPATIAL_H
#define PD_ACCESSIBILITY_SPATIAL_H
#include "accessibility/accessibility_tone.h"
int accessibilitySpatialAvailable(void);
int accessibilitySpatialGetEnabled(void);
void accessibilitySpatialSetEnabled(int value);
void accessibilitySpatialInit(void);
void accessibilitySpatialShutdown(void);
int accessibilitySpatialEnabled(void);
void accessibilitySpatialDirection(int slot, float x, float y, float z);
int accessibilitySpatialGetDirection(int slot, float *direction);
void accessibilitySpatialReset(int slot);
void accessibilitySpatialSample(int slot, float mono, float *left, float *right);
void accessibilitySpatialReport(int queuedFrames);
#endif

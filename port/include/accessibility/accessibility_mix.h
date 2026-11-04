#ifndef PD_ACCESSIBILITY_MIX_H
#define PD_ACCESSIBILITY_MIX_H
#include <stdint.h>

/* Normalized floating-point buses; fixed state owned exclusively by the mixer.
 * No allocation, I/O, locks, device calls, lookahead, or buffered audio. */
struct accessibilitymixsettings {
	int enabled;
	float worldgain, feedbackgain, headroomdb;
};
struct accessibilitymixstate {
	int initialized, hold, holdframes;
	float rate, headroomdb, gamefloor, gamedownstep, gameupstep, release;
	float gamegain;
	double limitergain; /* Avoid a float-envelope plateau below unity on release. */
	uint32_t frames, limitedframes, nonfiniteinputs;
	float peak, outputpeak, minimumgain;
	struct accessibilitymixsettings settings;
};
void accessibilityMixConfigure(struct accessibilitymixstate *state,
		const struct accessibilitymixsettings *settings, float rate);
void accessibilityMixReset(struct accessibilitymixstate *state);
int accessibilityMixNeedsProcessing(const struct accessibilitymixstate *state);
void accessibilityMixFrame(struct accessibilitymixstate *state,
		float gameleft, float gameright, float worldleft, float worldright,
		float feedbackleft, float feedbackright, int16_t *left, int16_t *right);
#endif

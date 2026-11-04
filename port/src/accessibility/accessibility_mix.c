#include <math.h>
#include <string.h>
#include "accessibility/accessibility_mix.h"

static float mixBound(float value, float fallback, float minimum, float maximum)
{
	if (!isfinite(value)) return fallback;
	return fmaxf(minimum, fminf(maximum, value));
}

void accessibilityMixReset(struct accessibilitymixstate *state)
{
	state->gamegain = state->limitergain = 1.0f;
	state->hold = 0;
}

void accessibilityMixConfigure(struct accessibilitymixstate *state,
		const struct accessibilitymixsettings *settings, float rate)
{
	float headroom = mixBound(settings->headroomdb, 3.0f, 0, 12);
	rate = mixBound(rate, 22020, 1000, 192000);
	if (!state->initialized) {
		memset(state, 0, sizeof(*state));
		accessibilityMixReset(state);
		state->minimumgain = 1;
	}
	if (!state->initialized || state->rate != rate || state->headroomdb != headroom) {
		state->rate = rate;
		state->headroomdb = headroom;
		state->gamefloor = powf(10.0f, -headroom / 20.0f);
		state->gamedownstep = (1 - state->gamefloor) / (rate * .010f);
		state->gameupstep = (1 - state->gamefloor) / (rate * .100f);
		state->holdframes = (int)(rate * .250f);
		state->release = 1 - expf(-1 / (rate * .100f));
	}
	state->initialized = 1;
	state->settings.enabled = settings->enabled != 0;
	state->settings.worldgain = mixBound(settings->worldgain, 1, 0, 2);
	state->settings.feedbackgain = mixBound(settings->feedbackgain, 1, 0, 2);
	state->settings.headroomdb = headroom;
	if (!state->settings.enabled) accessibilityMixReset(state);
}

int accessibilityMixNeedsProcessing(const struct accessibilitymixstate *state)
{
	return state->settings.enabled && (state->hold > 0
			|| state->gamegain != 1 || state->limitergain != 1);
}

static float mixFinite(struct accessibilitymixstate *state, float sample)
{
	if (isfinite(sample)) return sample;
	state->nonfiniteinputs++;
	return 0;
}

static int16_t mixPcm(float value)
{
	/* Final integer guard is not the normal overload-management mechanism. */
	if (!isfinite(value)) return 0;
	if (value >= 32767.0f / 32768.0f) return 32767;
	if (value <= -1) return -32768;
	return (int16_t)lrintf(value * 32768.0f);
}

void accessibilityMixFrame(struct accessibilitymixstate *state,
		float gameleft, float gameright, float worldleft, float worldright,
		float feedbackleft, float feedbackright, int16_t *left, int16_t *right)
{
	float cueleft, cueright, peak, target, ceiling;
	gameleft = mixFinite(state, gameleft);
	gameright = mixFinite(state, gameright);
	cueleft = mixFinite(state, worldleft) * state->settings.worldgain
			+ mixFinite(state, feedbackleft) * state->settings.feedbackgain;
	cueright = mixFinite(state, worldright) * state->settings.worldgain
			+ mixFinite(state, feedbackright) * state->settings.feedbackgain;
	cueleft = mixFinite(state, cueleft);
	cueright = mixFinite(state, cueright);
	if (fmaxf(fabsf(cueleft), fabsf(cueright)) > .000001f)
		state->hold = state->holdframes;
	else if (state->hold > 0) state->hold--;
	target = state->hold > 0 ? state->gamefloor : 1;
	if (state->gamegain > target)
		state->gamegain = fmaxf(target, state->gamegain - state->gamedownstep);
	else if (state->gamegain < target)
		state->gamegain = fminf(target, state->gamegain + state->gameupstep);
	/* A zero-dB setting must also release headroom left by a prior setting. */
	if (state->gamefloor == 1) state->gamegain = 1;
	gameleft = gameleft * state->gamegain + cueleft;
	gameright = gameright * state->gamegain + cueright;
	gameleft = mixFinite(state, gameleft);
	gameright = mixFinite(state, gameright);
	peak = fmaxf(fabsf(gameleft), fabsf(gameright));
	state->peak = fmaxf(state->peak, peak);
	/* Sample-peak, stereo-linked limiter: instantaneous attack, 100 ms
	 * release. No lookahead or latency. It is not an inter-sample/true-peak
	 * limiter. Recover to unity/full-scale passthrough when cues disappear. */
	ceiling = state->hold > 0 ? .98f : 1;
	target = peak > ceiling ? ceiling / peak : 1;
	if (target < state->limitergain) state->limitergain = target;
	else state->limitergain += (target - state->limitergain) * state->release;
	if (target == 1 && state->limitergain > .99999f) state->limitergain = 1;
	if (state->limitergain < .99999f) state->limitedframes++;
	state->minimumgain = fminf(state->minimumgain, state->limitergain);
	gameleft *= state->limitergain;
	gameright *= state->limitergain;
	state->outputpeak = fmaxf(state->outputpeak,
			fmaxf(fabsf(gameleft), fabsf(gameright)));
	state->frames++;
	*left = mixPcm(gameleft);
	*right = mixPcm(gameright);
}

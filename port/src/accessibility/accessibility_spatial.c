#include <math.h>
#include <string.h>
#include <SDL.h>
#include "platform.h"
#include "config.h"
#include "system.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_spatial.h"

#if ACCESSIBILITY_BINAURAL_EXPERIMENT
#include <phonon.h>
#include <malloc.h>
#define BLOCK 128
#define RATE 48000
#define NATIVE_RATE 22020
static void *library;
#define API(name) static __typeof__(name) *p##name
API(iplContextCreate); API(iplContextRelease);
API(iplHRTFCreate); API(iplHRTFRelease);
API(iplBinauralEffectCreate); API(iplBinauralEffectRelease);
API(iplBinauralEffectApply); API(iplBinauralEffectGetTail); API(iplBinauralEffectReset);
static IPLContext context;
static IPLHRTF hrtf;
static struct {
	IPLBinauralEffect effect;
	SDL_atomic_t direction[3];
	float input[BLOCK], output[2][BLOCK];
	double resamplePhase;
	float previousMono, previousLeft, previousRight, lastLeft, lastRight;
	int cursor, nonzero, tail;
} voices[ACCESSIBILITY_SPATIAL_VOICES];
static int ready, enabled = 1;
static unsigned long long allocations, frees, blocks, processUs, maxUs;
static unsigned long long warmAllocations;
static u64 nextReport;

static void *IPLCALL allocate(IPLsize size, IPLsize alignment)
{
	void *result = _aligned_malloc(size, alignment < sizeof(void *) ? sizeof(void *) : alignment);
	if (result) allocations++;
	return result;
}
static void IPLCALL release(void *memory) { if (memory) { frees++; _aligned_free(memory); } }
static void IPLCALL sdkLog(IPLLogLevel level, const char *message)
{
	/* Initialization runs on the main thread; never log from active DSP. */
	if (!ready) accessibilityLogEvent("spatial_audio", "sdk_init", "level=%d message=%s", level, message);
}

void accessibilitySpatialReset(int slot)
{
	if (!ready || slot < 0 || slot >= ACCESSIBILITY_SPATIAL_VOICES) return;
	piplBinauralEffectReset(voices[slot].effect);
	memset(voices[slot].input, 0, sizeof(voices[slot].input));
	memset(voices[slot].output, 0, sizeof(voices[slot].output));
	voices[slot].cursor = voices[slot].nonzero = voices[slot].tail = 0;
	voices[slot].resamplePhase = 0;
	voices[slot].previousMono = voices[slot].previousLeft = voices[slot].previousRight = 0;
	voices[slot].lastLeft = voices[slot].lastRight = 0;
}

void accessibilitySpatialShutdown(void)
{
	int i;
	ready = 0;
	for (i = 0; i < ACCESSIBILITY_SPATIAL_VOICES; i++) {
		if (voices[i].effect) piplBinauralEffectRelease(&voices[i].effect);
	}
	if (hrtf) piplHRTFRelease(&hrtf);
	if (context) piplContextRelease(&context);
	if (library) SDL_UnloadObject(library);
	library = NULL;
	memset(voices, 0, sizeof(voices));
}

void accessibilitySpatialInit(void)
{
	IPLAudioSettings audio = { RATE, BLOCK };
	IPLContextSettings settings = {0};
	IPLHRTFSettings hs = {0};
	IPLBinauralEffectSettings bs = {0};
	char path[2048];
	char *base;
	int i;
	const char *step = "load_library";
	IPLerror status = IPL_STATUS_SUCCESS;
	if (ready) return;
	base = SDL_GetBasePath();
	if (!base) goto failed;
	snprintf(path, sizeof(path), "%sphonon.dll", base);
	SDL_free(base);
	library = SDL_LoadObject(path);
	if (!library) goto failed;
#define LOAD(name) if (!(p##name = (__typeof__(&name))SDL_LoadFunction(library, #name))) goto failed
	LOAD(iplContextCreate); LOAD(iplContextRelease);
	LOAD(iplHRTFCreate); LOAD(iplHRTFRelease);
	LOAD(iplBinauralEffectCreate); LOAD(iplBinauralEffectRelease);
	LOAD(iplBinauralEffectApply); LOAD(iplBinauralEffectGetTail); LOAD(iplBinauralEffectReset);
#undef LOAD
	settings.version = STEAMAUDIO_VERSION;
	settings.simdLevel = IPL_SIMDLEVEL_SSE2;
	settings.allocateCallback = allocate;
	settings.freeCallback = release;
	settings.logCallback = sdkLog;
	step = "context";
	if ((status = piplContextCreate(&settings, &context)) != IPL_STATUS_SUCCESS) goto failed;
	hs.type = IPL_HRTFTYPE_DEFAULT;
	hs.volume = 1.0f;
	hs.normType = IPL_HRTFNORMTYPE_RMS;
	step = "hrtf";
	if ((status = piplHRTFCreate(context, &audio, &hs, &hrtf)) != IPL_STATUS_SUCCESS) goto failed;
	bs.hrtf = hrtf;
	for (i = 0; i < ACCESSIBILITY_SPATIAL_VOICES; i++) {
		step = "effect";
		if ((status = piplBinauralEffectCreate(context, &audio, &bs, &voices[i].effect))
				!= IPL_STATUS_SUCCESS) goto failed;
		SDL_AtomicSet(&voices[i].direction[2], -1000000);
	}
	ready = 1;
	/* Warm every convolution path before gameplay, then clear its history. */
	for (i = 0; i < ACCESSIBILITY_SPATIAL_VOICES; i++) {
		int n;
		float l, r;
		for (n = 0; n < BLOCK * 4; n++) accessibilitySpatialSample(i, n == 0 ? .1f : 0, &l, &r);
		accessibilitySpatialReset(i);
	}
	warmAllocations = allocations;
	blocks = processUs = maxUs = 0;
	accessibilityLogEvent("spatial_audio", "init", "backend=steam_audio_4.8.1 ready=1 enabled=%d voices=%d sample_rate=%d native_rate=%d block_frames=%d block_buffer_latency_ms=%.3f allocations=%llu",
			enabled, ACCESSIBILITY_SPATIAL_VOICES, RATE, NATIVE_RATE, BLOCK, (BLOCK + 1) * 1000.0 / RATE + 1000.0 / NATIVE_RATE, allocations);
	return;
failed:
	accessibilityLogEvent("spatial_audio", "init", "ready=0 fallback=stereo step=%s sdk_status=%d error=%s", step, status, SDL_GetError());
	accessibilitySpatialShutdown();
}

int accessibilitySpatialEnabled(void) { return ready && enabled; }
int accessibilitySpatialGetDirection(int slot, float *direction)
{
	int axis;
	if (!direction || !ready || slot < 0 || slot >= ACCESSIBILITY_SPATIAL_VOICES) return 0;
	for (axis = 0; axis < 3; axis++)
		direction[axis] = SDL_AtomicGet(&voices[slot].direction[axis]) / 1000000.0f;
	return 1;
}
int accessibilitySpatialAvailable(void) { return ready; }
int accessibilitySpatialGetEnabled(void) { return enabled; }
void accessibilitySpatialSetEnabled(int value)
{
	enabled = value != 0;
	accessibilityLogEvent("spatial_audio", "setting", "enabled=%d available=%d", enabled, ready);
}
void accessibilitySpatialDirection(int slot, float x, float y, float z)
{
	float length = sqrtf(x*x + y*y + z*z);
	if (!ready || slot < 0 || slot >= ACCESSIBILITY_SPATIAL_VOICES || !isfinite(length) || length < .0001f) return;
	SDL_AtomicSet(&voices[slot].direction[0], (int)(x / length * 1000000));
	SDL_AtomicSet(&voices[slot].direction[1], (int)(y / length * 1000000));
	SDL_AtomicSet(&voices[slot].direction[2], (int)(z / length * 1000000));
}

static void spatialSdkSample(int slot, float mono, float *left, float *right)
{
	int cursor;
	float *in[1], *out[2];
	IPLAudioBuffer input = {1, BLOCK, in}, output = {2, BLOCK, out};
	IPLBinauralEffectParams params = {0};
	u64 started, elapsed;
	*left = *right = 0;
	if (!ready || slot < 0 || slot >= ACCESSIBILITY_SPATIAL_VOICES) return;
	cursor = voices[slot].cursor;
	*left = voices[slot].output[0][cursor];
	*right = voices[slot].output[1][cursor];
	voices[slot].input[cursor] = mono;
	voices[slot].nonzero |= mono != 0;
	if (++voices[slot].cursor < BLOCK) return;
	voices[slot].cursor = 0;
	if (!voices[slot].nonzero && !voices[slot].tail) {
		memset(voices[slot].output, 0, sizeof(voices[slot].output));
		return;
	}
	in[0] = voices[slot].input;
	out[0] = voices[slot].output[0]; out[1] = voices[slot].output[1];
	started = sysGetMicroseconds();
	if (voices[slot].nonzero) {
		params.direction = (IPLVector3){ SDL_AtomicGet(&voices[slot].direction[0]) / 1000000.0f,
			SDL_AtomicGet(&voices[slot].direction[1]) / 1000000.0f,
			SDL_AtomicGet(&voices[slot].direction[2]) / 1000000.0f };
		params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
		params.spatialBlend = 1;
		params.hrtf = hrtf;
		voices[slot].tail = piplBinauralEffectApply(voices[slot].effect, &params, &input, &output)
				== IPL_AUDIOEFFECTSTATE_TAILREMAINING;
	} else {
		voices[slot].tail = piplBinauralEffectGetTail(voices[slot].effect, &output)
				== IPL_AUDIOEFFECTSTATE_TAILREMAINING;
	}
	elapsed = sysGetMicroseconds() - started;
	blocks++; processUs += elapsed; if (elapsed > maxUs) maxUs = elapsed;
	voices[slot].nonzero = 0;
}

/* The game remains at 22020 Hz. Only these mono cue voices are linearly
 * resampled to the SDK's supported 48 kHz rate and back, with bounded state.
 * Output reconstruction has one SDK-sample delay; no queued device is added. */
void accessibilitySpatialSample(int slot, float mono, float *left, float *right)
{
	const double step = (double)NATIVE_RATE / RATE;
	double fraction;
	*left = *right = 0;
	if (!ready || slot < 0 || slot >= ACCESSIBILITY_SPATIAL_VOICES) return;
	while (voices[slot].resamplePhase < 1.0) {
		float interpolated = voices[slot].previousMono
				+ (mono - voices[slot].previousMono) * voices[slot].resamplePhase;
		voices[slot].previousLeft = voices[slot].lastLeft;
		voices[slot].previousRight = voices[slot].lastRight;
		spatialSdkSample(slot, interpolated, &voices[slot].lastLeft, &voices[slot].lastRight);
		voices[slot].resamplePhase += step;
	}
	voices[slot].resamplePhase -= 1.0;
	voices[slot].previousMono = mono;
	fraction = 1.0 - voices[slot].resamplePhase / step;
	*left = voices[slot].previousLeft + (voices[slot].lastLeft - voices[slot].previousLeft) * fraction;
	*right = voices[slot].previousRight + (voices[slot].lastRight - voices[slot].previousRight) * fraction;
}

void accessibilitySpatialReport(int queuedFrames)
{
	u64 now = sysGetMicroseconds();
	if (!ready || now < nextReport) return;
	accessibilityLogEvent("spatial_audio", "window", "blocks=%llu dsp_us=%llu max_block_us=%llu allocations_after_warmup=%llu queued_frames=%d queued_ms=%.3f",
			blocks, processUs, maxUs, allocations - warmAllocations,
			queuedFrames, queuedFrames * 1000.0 / NATIVE_RATE);
	blocks = processUs = maxUs = 0;
	nextReport = now + 1000000;
}
PD_CONSTRUCTOR static void spatialConfig(void) { configRegisterInt("Accessibility.BinauralAudio", &enabled, 0, 1); }
#else
void accessibilitySpatialInit(void) { }
void accessibilitySpatialShutdown(void) { }
int accessibilitySpatialEnabled(void) { return 0; }
int accessibilitySpatialAvailable(void) { return 0; }
int accessibilitySpatialGetEnabled(void) { return 0; }
void accessibilitySpatialSetEnabled(int value) { }
void accessibilitySpatialDirection(int slot, float x, float y, float z) { }
int accessibilitySpatialGetDirection(int slot, float *direction) { return 0; }
void accessibilitySpatialReset(int slot) { }
void accessibilitySpatialSample(int slot, float mono, float *left, float *right) { *left = *right = 0; }
void accessibilitySpatialReport(int queuedFrames) { }
#endif

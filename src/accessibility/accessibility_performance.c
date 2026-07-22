#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS

#include <inttypes.h>
#include <string.h>
#include <ultra64.h>
#include "bss.h"
#include "data.h"
#include "types.h"
#include "lib/main.h"
#include "system.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_PERFORMANCE_LOG_INTERVAL_US 1000000ULL

static u64 g_AccessibilityPerformanceWindowStartUs;
static u64 g_AccessibilityPerformancePreviousFrameUs;
static u64 g_AccessibilityPerformanceMaxFrameGapUs;
static s32 g_AccessibilityPerformanceFrames;
static s32 g_AccessibilityPerformanceStartLvFrame60;
static s32 g_AccessibilityPerformanceMemoryBaselineValid;
static u64 g_AccessibilityPerformanceWorkingSetBaseline;
static u64 g_AccessibilityPerformancePrivateBaseline;
static struct accessibilitytonediagnostics g_AccessibilityPerformancePreviousTone;

void accessibilityPerformanceTick(void)
{
	struct accessibilitytonediagnostics tone;
	u64 now;
	u64 elapsed;
	u64 gap;
	u64 workingset = 0;
	u64 privatebytes = 0;
	s32 memoryavailable;
	s32 gameticks;
	f64 renderfps;
	f64 gametickrate;

	if (!accessibilityIsEnabled() || !accessibilityLogIsOpen()) {
		return;
	}

	now = sysGetMicroseconds();

	if (g_AccessibilityPerformanceWindowStartUs == 0) {
		g_AccessibilityPerformanceWindowStartUs = now;
		g_AccessibilityPerformancePreviousFrameUs = now;
		g_AccessibilityPerformanceStartLvFrame60 = g_Vars.lvframe60;
		accessibilityToneGetDiagnostics(&g_AccessibilityPerformancePreviousTone);
		return;
	}

	gap = now - g_AccessibilityPerformancePreviousFrameUs;
	g_AccessibilityPerformancePreviousFrameUs = now;
	g_AccessibilityPerformanceFrames++;

	if (gap > g_AccessibilityPerformanceMaxFrameGapUs) {
		g_AccessibilityPerformanceMaxFrameGapUs = gap;
	}

	elapsed = now - g_AccessibilityPerformanceWindowStartUs;

	if (elapsed < ACCESSIBILITY_PERFORMANCE_LOG_INTERVAL_US) {
		return;
	}

	gameticks = g_Vars.lvframe60 - g_AccessibilityPerformanceStartLvFrame60;
	renderfps = (f64)g_AccessibilityPerformanceFrames * 1000000.0 / (f64)elapsed;
	gametickrate = (f64)gameticks * 1000000.0 / (f64)elapsed;
	memoryavailable = sysGetProcessMemoryUsage(&workingset, &privatebytes);

	if (memoryavailable && !g_AccessibilityPerformanceMemoryBaselineValid) {
		g_AccessibilityPerformanceMemoryBaselineValid = true;
		g_AccessibilityPerformanceWorkingSetBaseline = workingset;
		g_AccessibilityPerformancePrivateBaseline = privatebytes;
	}

	memset(&tone, 0, sizeof(tone));
	accessibilityToneGetDiagnostics(&tone);
	accessibilityLogEvent("performance", "frame_window",
			"window_us=%" PRIu64 " render_frames=%d render_fps=%.3f max_frame_gap_us=%" PRIu64 " game_ticks=%d game_tick_rate=%.3f stage=%d lvframe60=%d diffframe60=%d lvupdate60=%d tickmode=%d menu_count=%d memory_available=%d working_set_bytes=%" PRIu64 " working_set_delta=%lld private_bytes=%" PRIu64 " private_delta=%lld tone_enabled=%d chirp_enabled=%d chirp_sequence=%d hazard_enabled=%d combat_enabled_slots=%d hazard_frequency_millihz=%d hazard_volume_millionths=%d hazard_pan_millionths=%d mixer_calls_delta=%d mixer_passthrough_delta=%d mixer_active_delta=%d mixer_frames_delta=%d mixer_calls_total=%d mixer_active_total=%d",
			(uint64_t)elapsed, g_AccessibilityPerformanceFrames, renderfps,
			(uint64_t)g_AccessibilityPerformanceMaxFrameGapUs,
			gameticks, gametickrate, mainGetStageNum(), g_Vars.lvframe60,
			g_Vars.diffframe60, g_Vars.lvupdate60, g_Vars.tickmode,
			g_MenuData.count, memoryavailable, (uint64_t)workingset,
			(long long)workingset
					- (long long)g_AccessibilityPerformanceWorkingSetBaseline,
			(uint64_t)privatebytes, (long long)privatebytes
					- (long long)g_AccessibilityPerformancePrivateBaseline,
			tone.toneenabled, tone.chirpenabled, tone.chirpsequence,
			tone.hazardenabled, tone.combatenabledslots,
			tone.hazardfrequencymillihz,
			tone.hazardvolumemillionths, tone.hazardpanmillionths,
			tone.mixcalls - g_AccessibilityPerformancePreviousTone.mixcalls,
			tone.passthroughcalls
					- g_AccessibilityPerformancePreviousTone.passthroughcalls,
			tone.activecalls - g_AccessibilityPerformancePreviousTone.activecalls,
			tone.mixedframes - g_AccessibilityPerformancePreviousTone.mixedframes,
			tone.mixcalls, tone.activecalls);

	g_AccessibilityPerformancePreviousTone = tone;
	g_AccessibilityPerformanceWindowStartUs = now;
	g_AccessibilityPerformanceMaxFrameGapUs = 0;
	g_AccessibilityPerformanceFrames = 0;
	g_AccessibilityPerformanceStartLvFrame60 = g_Vars.lvframe60;
}

#else

void accessibilityPerformanceTick(void)
{
}

#endif

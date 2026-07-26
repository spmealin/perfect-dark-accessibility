#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS

#include <inttypes.h>
#include <string.h>
#include <ultra64.h>
#include "bss.h"
#include "data.h"
#include "types.h"
#include "lib/main.h"
#include "system.h"
#include "video.h"
#include "accessibility/accessibility.h"
#include "accessibility/accessibility_cane.h"
#include "accessibility/accessibility_log.h"
#include "accessibility/accessibility_tone.h"

#define ACCESSIBILITY_PERFORMANCE_LOG_INTERVAL_US 1000000ULL
#define ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY 180
#define ACCESSIBILITY_GRAPHICS_EPISODE_CAPACITY 480
#define ACCESSIBILITY_GRAPHICS_SLOW_FRAME_US 50000ULL
#define ACCESSIBILITY_GRAPHICS_SEVERE_FRAME_US 250000ULL
#define ACCESSIBILITY_GRAPHICS_LOW_FPS 30.0
#define ACCESSIBILITY_GRAPHICS_RECOVERY_FPS 50.0
#define ACCESSIBILITY_GRAPHICS_RECOVERY_WINDOWS 2
#define ACCESSIBILITY_GRAPHICS_WORST_RECORDS 10
#define ACCESSIBILITY_GRAPHICS_TIMELINE_RECORDS 12

struct accessibilitygraphicsrecord {
	struct videoframediagnostics frame;
	s32 stage;
	s32 menu_count;
	s32 tickmode;
	s32 lvupdate60;
};

struct accessibilitygraphicswindow {
	u64 frames;
#define ACCESSIBILITY_GRAPHICS_PHASE(name) u64 name##_total; u64 name##_max
	ACCESSIBILITY_GRAPHICS_PHASE(interval);
	ACCESSIBILITY_GRAPHICS_PHASE(video_start);
	ACCESSIBILITY_GRAPHICS_PHASE(event);
	ACCESSIBILITY_GRAPHICS_PHASE(dimensions);
	ACCESSIBILITY_GRAPHICS_PHASE(framebuffer_maintenance);
	ACCESSIBILITY_GRAPHICS_PHASE(video_submit);
	ACCESSIBILITY_GRAPHICS_PHASE(backend_start);
	ACCESSIBILITY_GRAPHICS_PHASE(framebuffer_setup);
	ACCESSIBILITY_GRAPHICS_PHASE(display_list);
	ACCESSIBILITY_GRAPHICS_PHASE(composite);
	ACCESSIBILITY_GRAPHICS_PHASE(renderer_end);
	ACCESSIBILITY_GRAPHICS_PHASE(frame_limit);
	ACCESSIBILITY_GRAPHICS_PHASE(swap);
	ACCESSIBILITY_GRAPHICS_PHASE(swap_total);
	ACCESSIBILITY_GRAPHICS_PHASE(video_end);
	ACCESSIBILITY_GRAPHICS_PHASE(finish);
#undef ACCESSIBILITY_GRAPHICS_PHASE
};

static u64 g_AccessibilityPerformanceWindowStartUs;
static u64 g_AccessibilityPerformancePreviousFrameUs;
static u64 g_AccessibilityPerformanceMaxFrameGapUs;
static s32 g_AccessibilityPerformanceFrames;
static s32 g_AccessibilityPerformanceStartLvFrame60;
static s32 g_AccessibilityPerformanceMemoryBaselineValid;
static u64 g_AccessibilityPerformanceWorkingSetBaseline;
static u64 g_AccessibilityPerformancePrivateBaseline;
static struct accessibilitytonediagnostics g_AccessibilityPerformancePreviousTone;
static struct accessibilitycanediagnostics g_AccessibilityPerformancePreviousCane;
static struct accessibilitygraphicsrecord
		g_AccessibilityGraphicsHistory[ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY];
static struct accessibilitygraphicsrecord
		g_AccessibilityGraphicsEpisode[ACCESSIBILITY_GRAPHICS_EPISODE_CAPACITY];
static struct accessibilitygraphicswindow g_AccessibilityGraphicsWindow;
static u64 g_AccessibilityGraphicsLastSequence;
static s32 g_AccessibilityGraphicsHistoryWrite;
static s32 g_AccessibilityGraphicsHistoryCount;
static s32 g_AccessibilityGraphicsEpisodeCount;
static s32 g_AccessibilityGraphicsEpisodeTrigger;
static s32 g_AccessibilityGraphicsEpisodeActive;
static s32 g_AccessibilityGraphicsEpisodeTruncated;
static s32 g_AccessibilityGraphicsEpisodeId;
static s32 g_AccessibilityGraphicsConsecutiveSlow;
static s32 g_AccessibilityGraphicsRecoveryWindows;
static s32 g_AccessibilityGraphicsMetadataLogged;

static void accessibilityGraphicsWindowAddPhase(u64 value, u64 *total,
		u64 *maximum)
{
	*total += value;

	if (value > *maximum) {
		*maximum = value;
	}
}

static void accessibilityGraphicsWindowAdd(
		const struct videoframediagnostics *frame)
{
#define ACCESSIBILITY_GRAPHICS_ADD(name) \
	accessibilityGraphicsWindowAddPhase(frame->name##_us, \
			&g_AccessibilityGraphicsWindow.name##_total, \
			&g_AccessibilityGraphicsWindow.name##_max)
	g_AccessibilityGraphicsWindow.frames++;
	ACCESSIBILITY_GRAPHICS_ADD(interval);
	ACCESSIBILITY_GRAPHICS_ADD(video_start);
	ACCESSIBILITY_GRAPHICS_ADD(event);
	ACCESSIBILITY_GRAPHICS_ADD(dimensions);
	ACCESSIBILITY_GRAPHICS_ADD(framebuffer_maintenance);
	ACCESSIBILITY_GRAPHICS_ADD(video_submit);
	ACCESSIBILITY_GRAPHICS_ADD(backend_start);
	ACCESSIBILITY_GRAPHICS_ADD(framebuffer_setup);
	ACCESSIBILITY_GRAPHICS_ADD(display_list);
	ACCESSIBILITY_GRAPHICS_ADD(composite);
	ACCESSIBILITY_GRAPHICS_ADD(renderer_end);
	ACCESSIBILITY_GRAPHICS_ADD(frame_limit);
	ACCESSIBILITY_GRAPHICS_ADD(swap);
	ACCESSIBILITY_GRAPHICS_ADD(swap_total);
	ACCESSIBILITY_GRAPHICS_ADD(video_end);
	ACCESSIBILITY_GRAPHICS_ADD(finish);
#undef ACCESSIBILITY_GRAPHICS_ADD
}

static void accessibilityGraphicsCopyHistory(void)
{
	s32 count = g_AccessibilityGraphicsHistoryCount;
	s32 first = g_AccessibilityGraphicsHistoryWrite - count;
	s32 i;

	if (first < 0) {
		first += ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY;
	}

	g_AccessibilityGraphicsEpisodeCount = 0;

	for (i = 0; i < count
			&& i < ACCESSIBILITY_GRAPHICS_EPISODE_CAPACITY; i++) {
		g_AccessibilityGraphicsEpisode[i] =
				g_AccessibilityGraphicsHistory[
				(first + i) % ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY];
		g_AccessibilityGraphicsEpisodeCount++;
	}

	g_AccessibilityGraphicsEpisodeTrigger =
			g_AccessibilityGraphicsEpisodeCount > 0
			? g_AccessibilityGraphicsEpisodeCount - 1 : 0;
}

static void accessibilityGraphicsStartEpisode(const char *reason,
		f64 windowfps)
{
	if (g_AccessibilityGraphicsEpisodeActive) {
		return;
	}

	accessibilityGraphicsCopyHistory();
	g_AccessibilityGraphicsEpisodeActive = true;
	g_AccessibilityGraphicsEpisodeTruncated = false;
	g_AccessibilityGraphicsRecoveryWindows = 0;
	g_AccessibilityGraphicsEpisodeId++;
	accessibilityLogEvent("graphics", "stall_detected",
			"episode=%d reason=%s gate_fps=%.3f pretrigger_frames=%d trigger_index=%d slow_threshold_us=%d severe_threshold_us=%d",
			g_AccessibilityGraphicsEpisodeId, reason ? reason : "unknown",
			windowfps, g_AccessibilityGraphicsEpisodeTrigger,
			g_AccessibilityGraphicsEpisodeTrigger,
			(s32)ACCESSIBILITY_GRAPHICS_SLOW_FRAME_US,
			(s32)ACCESSIBILITY_GRAPHICS_SEVERE_FRAME_US);
}

static void accessibilityGraphicsAppendEpisode(
		const struct accessibilitygraphicsrecord *record)
{
	if (g_AccessibilityGraphicsEpisodeCount
			< ACCESSIBILITY_GRAPHICS_EPISODE_CAPACITY) {
		g_AccessibilityGraphicsEpisode[
				g_AccessibilityGraphicsEpisodeCount++] = *record;
	} else {
		g_AccessibilityGraphicsEpisodeTruncated = true;
	}
}

static u64 accessibilityGraphicsDominantValue(
		const struct videoframediagnostics *frame, const char **name)
{
	u64 accountedsubmit = frame->backend_start_us
			+ frame->framebuffer_setup_us + frame->display_list_us
			+ frame->composite_us + frame->renderer_end_us
			+ frame->swap_total_us;
	u64 accountedstart = frame->event_us + frame->dimensions_us
			+ frame->framebuffer_maintenance_us;
	u64 startunaccounted = frame->video_start_us > accountedstart
			? frame->video_start_us - accountedstart : 0;
	u64 submitunaccounted = frame->video_submit_us > accountedsubmit
			? frame->video_submit_us - accountedsubmit : 0;
	u64 endunaccounted = frame->video_end_us > frame->finish_us
			? frame->video_end_us - frame->finish_us : 0;
	u64 value = frame->event_us;

	*name = "event";
#define ACCESSIBILITY_GRAPHICS_DOMINANT(field) \
	if (frame->field##_us > value) { \
		value = frame->field##_us; \
		*name = #field; \
	}
	ACCESSIBILITY_GRAPHICS_DOMINANT(dimensions);
	ACCESSIBILITY_GRAPHICS_DOMINANT(framebuffer_maintenance);
	ACCESSIBILITY_GRAPHICS_DOMINANT(backend_start);
	ACCESSIBILITY_GRAPHICS_DOMINANT(framebuffer_setup);
	ACCESSIBILITY_GRAPHICS_DOMINANT(display_list);
	ACCESSIBILITY_GRAPHICS_DOMINANT(composite);
	ACCESSIBILITY_GRAPHICS_DOMINANT(renderer_end);
	ACCESSIBILITY_GRAPHICS_DOMINANT(frame_limit);
	ACCESSIBILITY_GRAPHICS_DOMINANT(swap);
	ACCESSIBILITY_GRAPHICS_DOMINANT(finish);
#undef ACCESSIBILITY_GRAPHICS_DOMINANT

	if (startunaccounted > value) {
		value = startunaccounted;
		*name = "start_unaccounted";
	}

	if (submitunaccounted > value) {
		value = submitunaccounted;
		*name = "submit_unaccounted";
	}

	if (endunaccounted > value) {
		value = endunaccounted;
		*name = "end_unaccounted";
	}

	return value;
}

static void accessibilityGraphicsLogRecord(const char *event, s32 index,
		const struct accessibilitygraphicsrecord *record)
{
	const char *dominant;
	u64 dominantus = accessibilityGraphicsDominantValue(
			&record->frame, &dominant);

	accessibilityLogEvent("graphics", event,
			"episode=%d index=%d relative_to_trigger=%d sequence=%" PRIu64 " completed_us=%" PRIu64 " interval_us=%" PRIu64 " stage=%d menu_count=%d tickmode=%d lvupdate60=%d dominant=%s dominant_us=%" PRIu64 " video_start_us=%" PRIu64 " event_us=%" PRIu64 " dimensions_us=%" PRIu64 " framebuffer_maintenance_us=%" PRIu64 " video_submit_us=%" PRIu64 " backend_start_us=%" PRIu64 " framebuffer_setup_us=%" PRIu64 " display_list_us=%" PRIu64 " composite_us=%" PRIu64 " renderer_end_us=%" PRIu64 " frame_limit_us=%" PRIu64 " swap_us=%" PRIu64 " swap_total_us=%" PRIu64 " video_end_us=%" PRIu64 " finish_us=%" PRIu64,
			g_AccessibilityGraphicsEpisodeId, index,
			index - g_AccessibilityGraphicsEpisodeTrigger,
			(uint64_t)record->frame.sequence,
			(uint64_t)record->frame.completed_us,
			(uint64_t)record->frame.interval_us,
			record->stage, record->menu_count, record->tickmode,
			record->lvupdate60, dominant, (uint64_t)dominantus,
			(uint64_t)record->frame.video_start_us,
			(uint64_t)record->frame.event_us,
			(uint64_t)record->frame.dimensions_us,
			(uint64_t)record->frame.framebuffer_maintenance_us,
			(uint64_t)record->frame.video_submit_us,
			(uint64_t)record->frame.backend_start_us,
			(uint64_t)record->frame.framebuffer_setup_us,
			(uint64_t)record->frame.display_list_us,
			(uint64_t)record->frame.composite_us,
			(uint64_t)record->frame.renderer_end_us,
			(uint64_t)record->frame.frame_limit_us,
			(uint64_t)record->frame.swap_us,
			(uint64_t)record->frame.swap_total_us,
			(uint64_t)record->frame.video_end_us,
			(uint64_t)record->frame.finish_us);
}

static void accessibilityGraphicsFinishEpisode(const char *reason)
{
	s32 selected[ACCESSIBILITY_GRAPHICS_WORST_RECORDS];
	s32 timelineprevious = -1;
	s32 i;
	s32 j;
	s32 best;
	u64 bestinterval;
	u64 maximuminterval = 0;
	u64 firstus = 0;
	u64 lastus = 0;

	if (!g_AccessibilityGraphicsEpisodeActive) {
		return;
	}

	for (i = 0; i < ACCESSIBILITY_GRAPHICS_WORST_RECORDS; i++) {
		selected[i] = -1;
	}

	if (g_AccessibilityGraphicsEpisodeCount > 0) {
		firstus = g_AccessibilityGraphicsEpisode[0].frame.completed_us;
		lastus = g_AccessibilityGraphicsEpisode[
				g_AccessibilityGraphicsEpisodeCount - 1].frame.completed_us;
	}

	for (i = 0; i < g_AccessibilityGraphicsEpisodeCount; i++) {
		if (g_AccessibilityGraphicsEpisode[i].frame.interval_us
				> maximuminterval) {
			maximuminterval =
					g_AccessibilityGraphicsEpisode[i].frame.interval_us;
		}
	}

	accessibilityLogEvent("graphics", "stall_episode",
			"episode=%d reason=%s records=%d pretrigger_frames=%d trigger_index=%d truncated=%d first_completed_us=%" PRIu64 " last_completed_us=%" PRIu64 " duration_us=%" PRIu64 " maximum_interval_us=%" PRIu64,
			g_AccessibilityGraphicsEpisodeId,
			reason ? reason : "unknown",
			g_AccessibilityGraphicsEpisodeCount,
			g_AccessibilityGraphicsEpisodeTrigger,
			g_AccessibilityGraphicsEpisodeTrigger,
			g_AccessibilityGraphicsEpisodeTruncated,
			(uint64_t)firstus, (uint64_t)lastus,
			(uint64_t)(lastus >= firstus ? lastus - firstus : 0),
			(uint64_t)maximuminterval);

	for (i = 0; i < ACCESSIBILITY_GRAPHICS_WORST_RECORDS; i++) {
		best = -1;
		bestinterval = 0;

		for (j = 0; j < g_AccessibilityGraphicsEpisodeCount; j++) {
			s32 k;
			s32 alreadyselected = false;

			for (k = 0; k < i; k++) {
				if (selected[k] == j) {
					alreadyselected = true;
					break;
				}
			}

			if (!alreadyselected
					&& g_AccessibilityGraphicsEpisode[j].frame.interval_us
					> bestinterval) {
				best = j;
				bestinterval =
						g_AccessibilityGraphicsEpisode[j].frame.interval_us;
			}
		}

		if (best < 0) {
			break;
		}

		selected[i] = best;
		accessibilityGraphicsLogRecord("stall_worst_frame", best,
				&g_AccessibilityGraphicsEpisode[best]);
	}

	for (i = 0; i < ACCESSIBILITY_GRAPHICS_TIMELINE_RECORDS
			&& g_AccessibilityGraphicsEpisodeCount > 0; i++) {
		s32 index = ACCESSIBILITY_GRAPHICS_TIMELINE_RECORDS == 1
				? 0 : i * (g_AccessibilityGraphicsEpisodeCount - 1)
				/ (ACCESSIBILITY_GRAPHICS_TIMELINE_RECORDS - 1);

		if (index != timelineprevious) {
			accessibilityGraphicsLogRecord("stall_timeline_frame", index,
					&g_AccessibilityGraphicsEpisode[index]);
			timelineprevious = index;
		}
	}

	g_AccessibilityGraphicsEpisodeActive = false;
	g_AccessibilityGraphicsEpisodeCount = 0;
	g_AccessibilityGraphicsEpisodeTrigger = 0;
	g_AccessibilityGraphicsEpisodeTruncated = false;
	g_AccessibilityGraphicsRecoveryWindows = 0;
}

static void accessibilityGraphicsObserveFrame(void)
{
	struct accessibilitygraphicsrecord record;
	s32 wasactive;

	memset(&record, 0, sizeof(record));

	if (!videoGetFrameDiagnostics(&record.frame)
			|| record.frame.sequence == g_AccessibilityGraphicsLastSequence) {
		return;
	}

	g_AccessibilityGraphicsLastSequence = record.frame.sequence;
	record.stage = mainGetStageNum();
	record.menu_count = g_MenuData.count;
	record.tickmode = g_Vars.tickmode;
	record.lvupdate60 = g_Vars.lvupdate60;
	accessibilityGraphicsWindowAdd(&record.frame);
	wasactive = g_AccessibilityGraphicsEpisodeActive;

	g_AccessibilityGraphicsHistory[
			g_AccessibilityGraphicsHistoryWrite] = record;
	g_AccessibilityGraphicsHistoryWrite =
			(g_AccessibilityGraphicsHistoryWrite + 1)
			% ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY;

	if (g_AccessibilityGraphicsHistoryCount
			< ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY) {
		g_AccessibilityGraphicsHistoryCount++;
	}

	if (wasactive) {
		accessibilityGraphicsAppendEpisode(&record);
	}

	if (record.frame.interval_us >= ACCESSIBILITY_GRAPHICS_SLOW_FRAME_US) {
		g_AccessibilityGraphicsConsecutiveSlow++;
	} else {
		g_AccessibilityGraphicsConsecutiveSlow = 0;
	}

	if (!wasactive && !g_AccessibilityGraphicsEpisodeActive) {
		if (record.frame.interval_us
				>= ACCESSIBILITY_GRAPHICS_SEVERE_FRAME_US) {
			accessibilityGraphicsStartEpisode("severe_frame", 0.0);
		} else if (g_AccessibilityGraphicsConsecutiveSlow >= 3) {
			accessibilityGraphicsStartEpisode("three_slow_frames", 0.0);
		}
	}
}

static void accessibilityGraphicsLogMetadata(void)
{
	struct videographicsmetadata metadata;
	char *fields[] = {
		metadata.api,
		metadata.video_driver,
		metadata.vendor,
		metadata.renderer,
		metadata.version,
		metadata.shading_language,
	};
	s32 i;
	s32 j;

	if (g_AccessibilityGraphicsMetadataLogged) {
		return;
	}

	memset(&metadata, 0, sizeof(metadata));
	videoGetGraphicsDiagnosticMetadata(&metadata);

	for (i = 0; i < (s32)(sizeof(fields) / sizeof(fields[0])); i++) {
		for (j = 0; fields[i][j] != '\0'; j++) {
			if (fields[i][j] == ' ' || fields[i][j] == '\t'
					|| fields[i][j] == '\r'
					|| fields[i][j] == '\n') {
				fields[i][j] = '_';
			}
		}
	}

	accessibilityLogEvent("graphics", "metadata",
			"api=%s video_driver=%s vendor=%s renderer=%s version=%s shading_language=%s refresh_rate=%u drawable_width=%u drawable_height=%u fullscreen=%d exclusive_fullscreen=%d swap_interval=%d framerate_limit=%d framebuffer_effects=%d msaa=%d history_capacity=%d episode_capacity=%d",
			metadata.api, metadata.video_driver, metadata.vendor,
			metadata.renderer, metadata.version, metadata.shading_language,
			metadata.refresh_rate, metadata.drawable_width,
			metadata.drawable_height, metadata.fullscreen,
			metadata.fullscreen_mode, metadata.vsync,
			metadata.framerate_limit, metadata.framebuffer_effects,
			metadata.msaa, ACCESSIBILITY_GRAPHICS_HISTORY_CAPACITY,
			ACCESSIBILITY_GRAPHICS_EPISODE_CAPACITY);
	g_AccessibilityGraphicsMetadataLogged = true;
}

void accessibilityPerformanceTick(void)
{
	struct accessibilitytonediagnostics tone;
	struct accessibilitycanediagnostics cane;
	u64 now;
	u64 elapsed;
	u64 gap;
	u64 workingset = 0;
	u64 privatebytes = 0;
	s32 memoryavailable;
	s32 gameticks;
	f64 renderfps;
	f64 gametickrate;

	if (!accessibilityLogIsOpen()) {
		return;
	}

	accessibilityGraphicsLogMetadata();
	accessibilityGraphicsObserveFrame();
	now = sysGetMicroseconds();

	if (g_AccessibilityPerformanceWindowStartUs == 0) {
		g_AccessibilityPerformanceWindowStartUs = now;
		g_AccessibilityPerformancePreviousFrameUs = now;
		g_AccessibilityPerformanceStartLvFrame60 = g_Vars.lvframe60;
		accessibilityToneGetDiagnostics(&g_AccessibilityPerformancePreviousTone);
		accessibilityCaneGetDiagnostics(&g_AccessibilityPerformancePreviousCane);
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
	memset(&cane, 0, sizeof(cane));
	accessibilityToneGetDiagnostics(&tone);
	accessibilityCaneGetDiagnostics(&cane);
	accessibilityLogEvent("performance", "frame_window",
			"window_us=%" PRIu64 " render_frames=%d render_fps=%.3f max_frame_gap_us=%" PRIu64 " game_ticks=%d game_tick_rate=%.3f stage=%d lvframe60=%d diffframe60=%d lvupdate60=%d tickmode=%d menu_count=%d memory_available=%d working_set_bytes=%" PRIu64 " working_set_delta=%lld private_bytes=%" PRIu64 " private_delta=%lld tone_enabled=%d chirp_enabled=%d chirp_sequence=%d weapon_function_sequence=%d weapon_function_pulses=%d hazard_enabled=%d combat_enabled_slots=%d tracker_enabled_slots=%d cane_mode=%d cane_requested_mask=0x%x cane_active_mask=0x%x cane_commands_delta=%d cane_tones_started_delta=%d cane_stops_delta=%d cane_queries_delta=%" PRIu64 " cane_hits_delta=%" PRIu64 " cane_misses_delta=%" PRIu64 " cane_skipped_delta=%" PRIu64 " cane_sweeps_delta=%" PRIu64 " cane_missed_cycles_delta=%" PRIu64 " cane_query_us_delta=%" PRIu64 " cane_query_max_us=%" PRIu64 " hazard_frequency_millihz=%d hazard_volume_millionths=%d hazard_pan_millionths=%d mixer_calls_delta=%d mixer_passthrough_delta=%d mixer_active_delta=%d mixer_frames_delta=%d mixer_calls_total=%d mixer_active_total=%d",
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
			tone.weaponfunctionsequence, tone.weaponfunctionpulses,
			tone.hazardenabled, tone.combatenabledslots,
			tone.trackerenabledslots,
			accessibilityGetVirtualCaneMode(), tone.canerequestedmask,
			tone.caneactivemask,
			tone.canecommands - g_AccessibilityPerformancePreviousTone.canecommands,
			tone.canetonesstarted
					- g_AccessibilityPerformancePreviousTone.canetonesstarted,
			tone.canestops - g_AccessibilityPerformancePreviousTone.canestops,
			(uint64_t)(cane.queries
					- g_AccessibilityPerformancePreviousCane.queries),
			(uint64_t)(cane.hits
					- g_AccessibilityPerformancePreviousCane.hits),
			(uint64_t)(cane.misses
					- g_AccessibilityPerformancePreviousCane.misses),
			(uint64_t)(cane.skipped
					- g_AccessibilityPerformancePreviousCane.skipped),
			(uint64_t)(cane.sweeps
					- g_AccessibilityPerformancePreviousCane.sweeps),
			(uint64_t)(cane.missedcycles
					- g_AccessibilityPerformancePreviousCane.missedcycles),
			(uint64_t)(cane.querytotalus
					- g_AccessibilityPerformancePreviousCane.querytotalus),
			(uint64_t)cane.querymaxus,
			tone.hazardfrequencymillihz,
			tone.hazardvolumemillionths, tone.hazardpanmillionths,
			tone.mixcalls - g_AccessibilityPerformancePreviousTone.mixcalls,
			tone.passthroughcalls
					- g_AccessibilityPerformancePreviousTone.passthroughcalls,
			tone.activecalls - g_AccessibilityPerformancePreviousTone.activecalls,
			tone.mixedframes - g_AccessibilityPerformancePreviousTone.mixedframes,
			tone.mixcalls, tone.activecalls);

	accessibilityLogEvent("graphics", "frame_window",
			"window_us=%" PRIu64 " render_frames=%d render_fps=%.3f phase_frames=%" PRIu64 " interval_total_us=%" PRIu64 " interval_max_us=%" PRIu64 " video_start_total_us=%" PRIu64 " video_start_max_us=%" PRIu64 " event_total_us=%" PRIu64 " event_max_us=%" PRIu64 " dimensions_total_us=%" PRIu64 " dimensions_max_us=%" PRIu64 " framebuffer_maintenance_total_us=%" PRIu64 " framebuffer_maintenance_max_us=%" PRIu64 " video_submit_total_us=%" PRIu64 " video_submit_max_us=%" PRIu64 " backend_start_total_us=%" PRIu64 " backend_start_max_us=%" PRIu64 " framebuffer_setup_total_us=%" PRIu64 " framebuffer_setup_max_us=%" PRIu64 " display_list_total_us=%" PRIu64 " display_list_max_us=%" PRIu64 " composite_total_us=%" PRIu64 " composite_max_us=%" PRIu64 " renderer_end_total_us=%" PRIu64 " renderer_end_max_us=%" PRIu64 " frame_limit_total_us=%" PRIu64 " frame_limit_max_us=%" PRIu64 " swap_total_us=%" PRIu64 " swap_max_us=%" PRIu64 " swap_wrapper_total_us=%" PRIu64 " swap_wrapper_max_us=%" PRIu64 " video_end_total_us=%" PRIu64 " video_end_max_us=%" PRIu64 " finish_total_us=%" PRIu64 " finish_max_us=%" PRIu64 " episode_active=%d episode=%d history_frames=%d",
			(uint64_t)elapsed, g_AccessibilityPerformanceFrames, renderfps,
			(uint64_t)g_AccessibilityGraphicsWindow.frames,
			(uint64_t)g_AccessibilityGraphicsWindow.interval_total,
			(uint64_t)g_AccessibilityGraphicsWindow.interval_max,
			(uint64_t)g_AccessibilityGraphicsWindow.video_start_total,
			(uint64_t)g_AccessibilityGraphicsWindow.video_start_max,
			(uint64_t)g_AccessibilityGraphicsWindow.event_total,
			(uint64_t)g_AccessibilityGraphicsWindow.event_max,
			(uint64_t)g_AccessibilityGraphicsWindow.dimensions_total,
			(uint64_t)g_AccessibilityGraphicsWindow.dimensions_max,
			(uint64_t)g_AccessibilityGraphicsWindow.framebuffer_maintenance_total,
			(uint64_t)g_AccessibilityGraphicsWindow.framebuffer_maintenance_max,
			(uint64_t)g_AccessibilityGraphicsWindow.video_submit_total,
			(uint64_t)g_AccessibilityGraphicsWindow.video_submit_max,
			(uint64_t)g_AccessibilityGraphicsWindow.backend_start_total,
			(uint64_t)g_AccessibilityGraphicsWindow.backend_start_max,
			(uint64_t)g_AccessibilityGraphicsWindow.framebuffer_setup_total,
			(uint64_t)g_AccessibilityGraphicsWindow.framebuffer_setup_max,
			(uint64_t)g_AccessibilityGraphicsWindow.display_list_total,
			(uint64_t)g_AccessibilityGraphicsWindow.display_list_max,
			(uint64_t)g_AccessibilityGraphicsWindow.composite_total,
			(uint64_t)g_AccessibilityGraphicsWindow.composite_max,
			(uint64_t)g_AccessibilityGraphicsWindow.renderer_end_total,
			(uint64_t)g_AccessibilityGraphicsWindow.renderer_end_max,
			(uint64_t)g_AccessibilityGraphicsWindow.frame_limit_total,
			(uint64_t)g_AccessibilityGraphicsWindow.frame_limit_max,
			(uint64_t)g_AccessibilityGraphicsWindow.swap_total,
			(uint64_t)g_AccessibilityGraphicsWindow.swap_max,
			(uint64_t)g_AccessibilityGraphicsWindow.swap_total_total,
			(uint64_t)g_AccessibilityGraphicsWindow.swap_total_max,
			(uint64_t)g_AccessibilityGraphicsWindow.video_end_total,
			(uint64_t)g_AccessibilityGraphicsWindow.video_end_max,
			(uint64_t)g_AccessibilityGraphicsWindow.finish_total,
			(uint64_t)g_AccessibilityGraphicsWindow.finish_max,
			g_AccessibilityGraphicsEpisodeActive,
			g_AccessibilityGraphicsEpisodeId,
			g_AccessibilityGraphicsHistoryCount);

	if (!g_AccessibilityGraphicsEpisodeActive
			&& renderfps < ACCESSIBILITY_GRAPHICS_LOW_FPS) {
		accessibilityGraphicsStartEpisode("window_below_30_fps",
				renderfps);
	}

	if (g_AccessibilityGraphicsEpisodeActive) {
		if (renderfps > ACCESSIBILITY_GRAPHICS_RECOVERY_FPS) {
			g_AccessibilityGraphicsRecoveryWindows++;
		} else {
			g_AccessibilityGraphicsRecoveryWindows = 0;
		}

		if (g_AccessibilityGraphicsRecoveryWindows
				>= ACCESSIBILITY_GRAPHICS_RECOVERY_WINDOWS) {
			accessibilityGraphicsFinishEpisode("recovered");
		}
	}

	g_AccessibilityPerformancePreviousTone = tone;
	g_AccessibilityPerformancePreviousCane = cane;
	memset(&g_AccessibilityGraphicsWindow, 0,
			sizeof(g_AccessibilityGraphicsWindow));
	g_AccessibilityPerformanceWindowStartUs = now;
	g_AccessibilityPerformanceMaxFrameGapUs = 0;
	g_AccessibilityPerformanceFrames = 0;
	g_AccessibilityPerformanceStartLvFrame60 = g_Vars.lvframe60;
}

void accessibilityPerformanceShutdown(void)
{
	if (accessibilityLogIsOpen()
			&& g_AccessibilityGraphicsEpisodeActive) {
		accessibilityGraphicsFinishEpisode("shutdown");
	}
}

#else

void accessibilityPerformanceTick(void)
{
}

void accessibilityPerformanceShutdown(void)
{
}

#endif

#ifndef _IN_VIDEO_H
#define _IN_VIDEO_H

#include <PR/ultratypes.h>
#include <PR/gbi.h>

// maximum framerate; if the game runs faster than this, things will break
#if PAL
#define VIDEO_MAX_FPS 200
#else
#define VIDEO_MAX_FPS 240
#endif

typedef struct {
	s32 width;
	s32 height;
} displaymode;

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
struct videoframediagnostics {
	u64 sequence;
	u64 completed_us;
	u64 interval_us;
	u64 video_start_us;
	u64 event_us;
	u64 dimensions_us;
	u64 framebuffer_maintenance_us;
	u64 video_submit_us;
	u64 backend_start_us;
	u64 framebuffer_setup_us;
	u64 display_list_us;
	u64 composite_us;
	u64 renderer_end_us;
	u64 frame_limit_us;
	u64 swap_us;
	u64 swap_total_us;
	u64 video_end_us;
	u64 finish_us;
};

struct videographicsmetadata {
	char api[32];
	char video_driver[32];
	char vendor[128];
	char renderer[128];
	char version[128];
	char shading_language[128];
	u32 refresh_rate;
	u32 drawable_width;
	u32 drawable_height;
	s32 fullscreen;
	s32 fullscreen_mode;
	s32 vsync;
	s32 framerate_limit;
	s32 framebuffer_effects;
	s32 msaa;
};
#endif

s32 videoInit(void);
void videoStartFrame(void);
void videoSubmitCommands(Gfx *cmds);
void videoClearScreen(void);
void videoEndFrame(void);

void *videoGetWindowHandle(void);

void videoUpdateNativeResolution(s32 w, s32 h);
s32 videoGetNativeWidth(void);
s32 videoGetNativeHeight(void);

s32 videoGetWidth(void);
s32 videoGetHeight(void);
f32 videoGetAspect(void);
s32 videoGetFullscreen(void);
s32 videoGetFullscreenMode(void);
s32 videoGetMaximizeWindow(void);
void videoSetMaximizeWindow(s32 fs);
s32 videoGetCenterWindow(void);
void videoSetCenterWindow(s32 center);
u32 videoGetTextureFilter(void);
s32 videoGetTextureFilter2D(void);
u32 videoGetAnisotropicFilter(void);
u32 videoGetMaxAnisotropyLevel(void);
s32 videoGetDetailTextures(void);
s32 videoGetDisplayModeIndex(void);
s32 videoGetDisplayMode(displaymode *out, const s32 index);
s32 videoGetNumDisplayModes(void);
s32 videoGetVsync(void);
s32 videoGetFramerateLimit(void);
s32 videoGetDisplayFPS(void);
s32 videoGetMSAA(void);
f32 videoGetGlareBrightness(void);
f32 videoGetOverexposureScale(void);

f32 videoGetAverageFPS(void);

void videoSetWindowOffset(s32 x, s32 y);
void videoSetFullscreen(s32 fs);
void videoSetFullscreenMode(s32 mode);
void videoSetTextureFilter(u32 filter);
void videoSetTextureFilter2D(s32 filter);
void videoSetAnisotropicFilter(u32 filter);
void videoSetDetailTextures(s32 detail);
void videoSetDisplayMode(const s32 index);
void videoSetVsync(const s32 vsync);
void videoSetFramerateLimit(const s32 limit);
void videoSetDisplayFPS(const s32 displayfps);
void videoSetMSAA(const s32 msaa);
void videoSetGlareBrightness(f32 bright);
void videoSetOverexposureScale(f32 scale);

s32 videoCreateFramebuffer(u32 w, u32 h, s32 upscale, s32 autoresize);
void videoSetFramebuffer(s32 target);
void videoResetFramebuffer(void);
void videoCopyFramebuffer(s32 dst, s32 src, s32 left, s32 top);
void videoResizeFramebuffer(s32 target, u32 w, u32 h, s32 upscale, s32 autoresize);
s32 videoFramebuffersSupported(void);

void videoResetTextureCache(void);
void videoFreeCachedTexture(const void *texptr);
void videoFreeCachedTextures(const void *start, const void *end);

void videoShutdown(void);

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
s32 videoGetFrameDiagnostics(struct videoframediagnostics *diagnostics);
void videoGetGraphicsDiagnosticMetadata(
		struct videographicsmetadata *metadata);
#endif

#endif

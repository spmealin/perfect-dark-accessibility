#ifndef GFX_SDL_H
#define GFX_SDL_H

#include <stddef.h>
#include <stdint.h>
#include "gfx_window_manager_api.h"

extern struct GfxWindowManagerAPI gfx_sdl;

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
#ifdef __cplusplus
extern "C" {
#endif
void gfx_sdl_get_frame_diagnostics(uint64_t *frame_limit_us,
        uint64_t *swap_us);
const char *gfx_sdl_get_video_driver(void);
void gfx_sdl_get_window_diagnostics(uint32_t *refresh_rate,
        uint32_t *drawable_width, uint32_t *drawable_height);
#ifdef __cplusplus
}
#endif
#endif

#endif

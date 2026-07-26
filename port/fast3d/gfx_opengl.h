#ifndef GFX_OPENGL_H
#define GFX_OPENGL_H

#include <stddef.h>
#include "gfx_rendering_api.h"

extern struct GfxRenderingAPI gfx_opengl_api;

#if ACCESSIBILITY_PERFORMANCE_DIAGNOSTICS
#ifdef __cplusplus
extern "C" {
#endif
void gfx_opengl_get_diagnostic_metadata(char *vendor, size_t vendor_size,
        char *renderer, size_t renderer_size, char *version,
        size_t version_size, char *shading_language,
        size_t shading_language_size);
#ifdef __cplusplus
}
#endif
#endif

#endif

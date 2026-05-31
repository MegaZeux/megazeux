/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __RENDER_SDL_H
#define __RENDER_SDL_H

#include "../../compat.h"

__M_BEGIN_DECLS

#include "../render.h"
#include "../../graphics.h"
#include "../../platform/sdl/SDLmzx.h"

struct sdl_render_data
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Renderer *renderer;
  SDL_Texture *texture[3];
  SDL_Palette *palette;
  SDL_GLContext context;
  SDL_PixelFormatDetails *pixel_format;
#else
  SDL_Overlay *overlay;
#endif
  SDL_Window *window; // always NULL in SDL 1.2
  SDL_Surface *screen;
  SDL_Surface *shadow;
  SDL_Color *palette_colors;
  const SDL_PixelFormatDetails *flat_format; // format used by sdl_update_colors.

  // SDL Renderer and overlay renderer texture format configuration.
  uint32_t (*rgb_to_yuv)(uint8_t r, uint8_t g, uint8_t b);
  set_colors_function subsample_set_colors;
  uint32_t texture_format;
  uint32_t texture_amask;
  uint32_t texture_bpp;
  boolean allow_subsampling;
  boolean use_texture_streaming;
};

// Mac OS X has a special OpenGL YCbCr native texture mode which is
// faster than RGB for older machines.
#define YUV_PRIORITY_APPLE 1000000
#define YUV_PRIORITY 422
#define YUV_DISABLE 0

static inline SDL_Window *sdl_get_current_window(void)
{
  const struct video_window *window = video_get_window(1);
  return SDL_GetWindowFromID(window ? window->platform_id : 0);
}

int sdl_flags(const struct video_window *window);
void sdl_destruct_window(struct graphics_data *graphics);
void sdl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count);

boolean sdl_create_window_soft(struct graphics_data *graphics,
 struct video_window *window);
boolean sdl_resize_window(struct graphics_data *graphics,
 struct video_window *window);
boolean sdl_set_window_caption(struct graphics_data *graphics,
 struct video_window *window, const char *caption);
boolean sdl_set_window_icon(struct graphics_data *graphics,
 struct video_window *window, const char *icon_path);

#if !SDL_VERSION_ATLEAST(2,0,0)
// Used internally only.
boolean sdl_check_video_mode(struct graphics_data *graphics,
 struct video_window *window, boolean renderer_supports_scaling, int flags);
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
boolean sdl_create_window_renderer(struct graphics_data *graphics,
 struct video_window *window, boolean requires_blend_ops);
void sdl_set_texture_scale_mode(struct graphics_data *graphics,
 struct video_window *window, int texture_id, boolean allow_non_integer);
#endif

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

#include "../opengl/render_gl.h"

#if SDL_VERSION_ATLEAST(2,0,0) && !SDL_VERSION_ATLEAST(3,0,0)
/* SDL_WINDOW_FULLSCREEN_DESKTOP removed in SDL3. */
#define GL_ALLOW_FLAGS \
 (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_RESIZABLE)
#else
#define GL_ALLOW_FLAGS (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE)
#endif

#define GL_STRIP_FLAGS(A) ((A & GL_ALLOW_FLAGS) | SDL_WINDOW_OPENGL)

#define gl_set_window_caption sdl_set_window_caption
#define gl_set_window_icon    sdl_set_window_icon

boolean gl_create_window(struct graphics_data *graphics,
 struct video_window *window, struct gl_version req_ver);
boolean gl_resize_window(struct graphics_data *graphics,
 struct video_window *window);
void gl_set_attributes(struct graphics_data *graphics);
boolean gl_swap_buffers(struct graphics_data *graphics);

static inline void gl_cleanup(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);
}

static inline boolean GL_LoadLibrary(enum gl_lib_type type)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  return SDL_GL_LoadLibrary(NULL);
#else
  if(SDL_GL_LoadLibrary(NULL) == 0)
    return true;

#if !SDL_VERSION_ATLEAST(2,0,0)
  // If the context already exists, don't reload the library
  // This is for SDL 1.2 which doesn't let us unload OpenGL
  if(strcmp(SDL_GetError(), "OpenGL context already created") == 0) return true;
#endif
  return false;
#endif
}

static inline dso_fn_ptr GL_GetProcAddress(const char *proc)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  return SDL_GL_GetProcAddress(proc);
#else
  /* SDL1/2 returns void * instead of a function pointer. */
  union dso_suppress_warning value;
  value.in = SDL_GL_GetProcAddress(proc);
  return value.out;
#endif
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

__M_END_DECLS

#endif // __RENDER_SDL_H

/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007,2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __RENDER_GL_H
#define __RENDER_GL_H

#include "../../compat.h"

__M_BEGIN_DECLS

#include "../../configure.h"
#include "../../graphics.h"
#include "../../util.h"
#include "../../platform/platform.h"

#ifdef CONFIG_SDL
# include "../../platform/sdl/SDLmzx.h"
# ifdef CONFIG_GLES
#   ifdef CONFIG_RENDER_GL_FIXED
#     if SDL_VERSION_ATLEAST(3,0,0)
#       include <SDL3/SDL_opengles.h>
#     else
#       include <SDL_opengles.h>
#     endif
#   endif
#   ifdef CONFIG_RENDER_GL_PROGRAM
#     if SDL_VERSION_ATLEAST(3,0,0)
#       include <SDL3/SDL_opengles2.h>
#     else
#       include <SDL_opengles2.h>
#     endif
#   endif
# else
#   if SDL_VERSION_ATLEAST(3,0,0)
#     include <SDL3/SDL_opengl.h>
#   else
#     include <SDL_opengl.h>
#   endif
# endif
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

#ifndef GL_APIENTRY
#define GL_APIENTRY GLAPIENTRY
#endif

// Next power of 2 over SCREEN_PIX_W
#define GL_POWER_2_WIDTH          1024
// Next power of 2 over SCREEN_PIX_H
#define GL_POWER_2_HEIGHT         512

extern const float vertex_array_single[2 * 4];

#ifdef DEBUG
void gl_error(const char *file, int line,
              GLenum (GL_APIENTRY *glGetError)(void));
#else
static inline void gl_error(const char *file, int line,
                            GLenum (GL_APIENTRY *glGetError)(void)) { }
#endif

boolean gl_load_syms(const struct dso_syms_map *map);
void gl_set_filter_method(enum gl_filter_type method,
 void (GL_APIENTRY *glTexParameterf_p)(GLenum target, GLenum pname,
  GLfloat param));

// Used to request an OpenGL API version with gl_set_video_mode.
// Currently this is only used to configure SDL on platforms that require
// OpenGL ES, as OpenGL ES 1 and OpenGL ES 2 are not compatible.
struct gl_version
{
  int major;
  int minor;
};

enum gl_lib_type
{
  GL_LIB_FIXED,
  GL_LIB_PROGRAMMABLE,
};

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
/**
 * The GL renderers use GL_UNSIGNED_BYTE for 32-bit texture buffers for GLES 2
 * compatibility. This leads to some problems when packing bytes on big endian
 * machines because each of these packings assumes GL_UNSIGNED_INT_8_8_8_8_REV
 * (which isn't in GLES 2).
 *
 * TODO: bswap32 here if compatibility defines for it are added.
 */
static inline uint32_t gl_pack_u32(uint32_t x)
{
  return
   ((x & 0xFF000000) >> 24) |
   ((x & 0x00FF0000) >> 8) |
   ((x & 0x0000FF00) << 8) |
   ((x & 0x000000FF) << 24);
}
#else
/**
 * For little endian, GL_UNSIGNED_BYTE and GL_UNSIGNED_INT_8_8_8_8_REV are
 * equivalent, so don't change anything.
 */
#define gl_pack_u32(x) (x)
#endif

__M_END_DECLS

#endif // __RENDER_GL_H

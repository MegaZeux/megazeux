/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __RENDER_EGL_H
#define __RENDER_EGL_H

#include "../../compat.h"

__M_BEGIN_DECLS

#include "render_gl.h"

#include <EGL/egl.h>

boolean gl_create_window(struct graphics_data *graphics,
 struct video_window *window, struct gl_version req_ver);
boolean gl_resize_window(struct graphics_data *graphics,
 struct video_window *window);
boolean gl_set_window_caption(struct graphics_data *graphics,
 struct video_window *window, const char *caption);
boolean gl_set_window_icon(struct graphics_data *graphics,
 struct video_window *window, const char *icon_path);
void gl_set_attributes(struct graphics_data *graphics);
boolean gl_swap_buffers(struct graphics_data *graphics);
void gl_cleanup(struct graphics_data *graphics);

boolean GL_LoadLibrary(enum gl_lib_type type);

#ifndef ANDROID

static inline dso_fn_ptr GL_GetProcAddress(const char *proc)
{
  return eglGetProcAddress(proc);
}

#else /* ANDROID */

/* Android's eglGetProcAddress is currently broken, so we
 * have to roll our own..
 */
dso_fn_ptr GL_GetProcAddress(const char *proc);

#endif /* !ANDROID */

struct egl_render_data
{
  EGLNativeDisplayType native_display;
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  EGLSurface surface;
};

__M_END_DECLS

#endif // __RENDER_EGL_H

/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007,2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#include "../render.h"

#include "../../util.h"
#include "../../platform/platform_dso.h"

#ifdef CONFIG_SDL
#include "../sdl/render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include <GLES/gl.h>
#include "render_egl.h"
#endif

#include "render_gl.h"

const float vertex_array_single[2 * 4] = {
  -1.0f,  1.0f,
  -1.0f, -1.0f,
   1.0f,  1.0f,
   1.0f, -1.0f,
};

#ifdef DEBUG

void gl_error(const char *file, int line,
              GLenum (GL_APIENTRY *glGetError)(void))
{
  const char *error;

  switch(glGetError())
  {
    case GL_NO_ERROR:
      return;
    case GL_INVALID_ENUM:
      error = "Invalid enum";
      break;
    case GL_INVALID_VALUE:
      error = "Invalid value";
      break;
    case GL_INVALID_OPERATION:
      error = "Invalid operation";
      break;
#if !defined(CONFIG_GLES) || defined(CONFIG_RENDER_GL_FIXED)
    // These aren't defined by OpenGL ES 2.
    case GL_STACK_OVERFLOW:
      error = "Stack overflow";
      break;
    case GL_STACK_UNDERFLOW:
      error = "Stack underflow";
      break;
#endif
    case GL_OUT_OF_MEMORY:
      error = "Out of memory";
      break;
    default:
      error = "Unknown error";
      break;
  }

  warn("%s:%d: GL Error: %s\n", file, line, error);
}

#endif // DEBUG

boolean gl_load_syms(const struct dso_syms_map *map)
{
  int i = 0;

  for(i = 0; map[i].name != NULL; i++)
  {
    dso_fn_ptr *sym_ptr = map[i].sym_ptr.value;

    *sym_ptr = GL_GetProcAddress(map[i].name);
    if(!*sym_ptr)
    {
      warn("Failed to load GL function '%s', aborting..\n", map[i].name);
      return false;
    }
  }

  return true;
}

void gl_set_filter_method(enum gl_filter_type method,
 void (GL_APIENTRY *glTexParameterf_p)(GLenum target, GLenum pname,
  GLfloat param))
{
  GLfloat gl_filter_method = GL_LINEAR;

  if(method == CONFIG_GL_FILTER_NEAREST)
    gl_filter_method = GL_NEAREST;

  glTexParameterf_p(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_method);
  glTexParameterf_p(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_method);
  glTexParameterf_p(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf_p(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

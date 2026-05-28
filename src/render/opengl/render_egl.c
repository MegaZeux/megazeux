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

#include <GLES/gl.h>
#include "render_egl.h"

#include "../../util.h"

#include <assert.h>
#include <dlfcn.h>

#ifdef CONFIG_X11
#include <X11/Xlib.h>
#endif

static void *glso;
static enum gl_lib_type gl_type;

#ifdef ANDROID

dso_fn_ptr GL_GetProcAddress(const char *proc)
{
  union dso_suppress_warning value;
  value.in = dlsym(glso, proc);
  return value.out;
}

#endif /* ANDROID */

boolean GL_LoadLibrary(enum gl_lib_type type)
{
  const char *filename;

  gl_type = type;

  if(type == GL_LIB_FIXED)
    filename = "libGLESv1_CM.so";
  else
    filename = "libGLESv2.so";

  glso = dlopen(filename, RTLD_NOW);
  return glso != NULL;
}

// EGL is not directly comparable to SDL as it does not handle "windows"
// in the traditional sense. It is expected that the platform provides a
// native "window" type that handles things like resolution and resizing.

static EGLint config16[] =
{
  EGL_RED_SIZE,        5,
  EGL_GREEN_SIZE,      6,
  EGL_BLUE_SIZE,       5,
  EGL_ALPHA_SIZE,      0,
  EGL_DEPTH_SIZE,      0,
  EGL_RENDERABLE_TYPE, 0, /* Placeholder */
  EGL_NONE,
};

static EGLint config32[] =
{
  EGL_RED_SIZE,        8,
  EGL_GREEN_SIZE,      8,
  EGL_BLUE_SIZE,       8,
  EGL_ALPHA_SIZE,      8,
  EGL_DEPTH_SIZE,      0,
  EGL_RENDERABLE_TYPE, 0, /* Placeholder */
  EGL_NONE,
};

static void update_config_attribs(EGLint *config)
{
  int i;

  for(i = 0; config[i] != EGL_NONE; i++)
  {
    if(config[i] != EGL_RENDERABLE_TYPE)
      continue;

    if(gl_type == GL_LIB_FIXED)
      config[i + 1] = EGL_OPENGL_ES_BIT;
    else
      config[i + 1] = EGL_OPENGL_ES2_BIT;

    break;
  }
}

static const EGLint *get_current_config(int depth)
{
  if(depth == 32)
  {
    debug("Selected RGBA 8888 EGLConfig\n");
    update_config_attribs(config32);
    return config32;
  }

  debug("Selected RGB 565 EGLConfig\n");
  update_config_attribs(config16);
  return config16;
}

static const EGLint gles_v1_attribs[] =
{
  EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE
};

static const EGLint gles_v2_attribs[] =
{
  EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
};

boolean gl_create_window(struct graphics_data *graphics,
 struct video_window *window, struct gl_version req_ver)
{
  struct egl_render_data *egl_render_data = graphics->render_data;
  const EGLint *attribs = gles_v1_attribs;
  EGLNativeWindowType egl_window = 0;
  EGLint num_configs;
  EGLint w, h;

  assert(egl_render_data != NULL);

#ifdef CONFIG_X11
  egl_render_data->native_display = XOpenDisplay(NULL);
  if(egl_render_data->native_display == NULL)
  {
    warn("XOpenDisplay failed\n");
    return false;
  }

  egl_window = XCreateSimpleWindow(egl_render_data->native_display,
                               RootWindow(egl_render_data->native_display, 0),
                               0, 0, 640, 350, 0,
                               BlackPixel(egl_render_data->native_display, 0),
                               BlackPixel(egl_render_data->native_display, 0));
  XMapWindow(egl_render_data->native_display, egl_window);
  XFlush(egl_render_data->native_display);
#endif

  egl_render_data->display = eglGetDisplay(egl_render_data->native_display);
  if(egl_render_data->display == EGL_NO_DISPLAY)
  {
    warn("eglGetDisplay failed\n");
    return false;
  }

  if(!eglInitialize(egl_render_data->display, NULL, NULL))
  {
    warn("eglInitialize failed\n");
    return false;
  }

  if(!eglChooseConfig(egl_render_data->display,
                      get_current_config(window->bits_per_pixel),
                      &egl_render_data->config, 1, &num_configs))
  {
    warn("eglChooseConfig failed\n");
    return false;
  }

  egl_render_data->surface = eglCreateWindowSurface(egl_render_data->display,
                                                    egl_render_data->config,
                                                    egl_window, NULL);
  if(egl_render_data->surface == EGL_NO_SURFACE)
  {
    warn("eglCreateWindowSurface failed\n");
    goto err_cleanup;
  }

  if(!eglBindAPI(EGL_OPENGL_ES_API))
  {
    warn("eglBindAPI failed\n");
    goto err_cleanup;
  }

  if(gl_type == GL_LIB_PROGRAMMABLE)
    attribs = gles_v2_attribs;

  egl_render_data->context = eglCreateContext(egl_render_data->display,
                                              egl_render_data->config,
                                              EGL_NO_CONTEXT, attribs);
  if(egl_render_data->context == EGL_NO_CONTEXT)
  {
    warn("eglCreateContext failed\n");
    goto err_cleanup;
  }

  if(!eglQuerySurface(egl_render_data->display, egl_render_data->surface,
   EGL_WIDTH, &w))
  {
    warn("eglQuerySurface (width) failed\n");
    goto err_cleanup;
  }

  if(!eglQuerySurface(egl_render_data->display, egl_render_data->surface,
   EGL_HEIGHT, &h))
  {
    warn("eglQuerySurface (height) failed\n");
    goto err_cleanup;
  }
  window->width_px = w;
  window->height_px = h;

  if(!eglMakeCurrent(egl_render_data->display, egl_render_data->surface,
                     egl_render_data->surface, egl_render_data->context))
  {
    warn("eglMakeCurrent failed\n");
    goto err_cleanup;
  }

  // Can now issue GL ES commands
  return true;

err_cleanup:
  gl_cleanup(graphics);
  return false;
}

boolean gl_resize_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct gl_version dummy = { 0, 0 }; // TODO: fix this
  gl_cleanup(graphics);
  return gl_create_window(graphics, window, dummy);
}

boolean gl_set_window_caption(struct graphics_data *graphics,
 struct video_window *window, const char *caption)
{
  // TODO: implement
  return false;
}

boolean gl_set_window_icon(struct graphics_data *graphics,
 struct video_window *window, const char *icon_path)
{
  // TODO: implement
  return false;
}

void gl_set_attributes(struct graphics_data *graphics)
{
  // Note that this function is called twice- both before and after
  // gl_set_video_mode
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  // EGL is not concerned with double buffering -- per-platform hacks should
  // go here if double buffering isn't the default or can be toggled.

  if(graphics->gl_vsync == 0)
    eglSwapInterval(egl_render_data->display, 0);
  else if(graphics->gl_vsync >= 1)
    eglSwapInterval(egl_render_data->display, 1);
}

boolean gl_swap_buffers(struct graphics_data *graphics)
{
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  return eglSwapBuffers(egl_render_data->display, egl_render_data->surface);
}

void gl_cleanup(struct graphics_data *graphics)
{
  struct egl_render_data *egl_render_data = graphics->render_data;

  assert(egl_render_data != NULL);

  if(egl_render_data->display != EGL_NO_DISPLAY)
  {
    eglMakeCurrent(egl_render_data->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
     EGL_NO_CONTEXT);

    if(egl_render_data->context != EGL_NO_CONTEXT)
    {
      eglDestroyContext(egl_render_data->display, egl_render_data->context);
      egl_render_data->context = EGL_NO_CONTEXT;
    }

    if(egl_render_data->surface != EGL_NO_SURFACE)
    {
      eglDestroySurface(egl_render_data->display, egl_render_data->surface);
      egl_render_data->surface = EGL_NO_SURFACE;
    }

    eglTerminate(egl_render_data->display);
    egl_render_data->display = EGL_NO_DISPLAY;
  }

#ifdef CONFIG_X11
  if(egl_render_data->native_display)
  {
    XCloseDisplay(egl_render_data->native_display);
    egl_render_data->native_display = NULL;
  }
#endif
}

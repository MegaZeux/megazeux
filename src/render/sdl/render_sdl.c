/* MegaZeux
 *
 * Copyright (C) 2007-2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2019-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "render_sdl.h"
#include "../render.h"
#include "../yuv.h"

#include "../../util.h"
#include "../../event/event.h"
#include "../../platform/log.h"

#include <limits.h>
#include <stdlib.h>

static void sdl_set_system_cursor(struct graphics_data *graphics)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  if(graphics->system_mouse == SYSTEM_MOUSE_OFF)
    SDL_HideCursor();
  else
    SDL_ShowCursor();
#else
  int value = (graphics->system_mouse == SYSTEM_MOUSE_OFF) ? SDL_DISABLE : SDL_ENABLE;
  SDL_ShowCursor(value);
#endif
}

static void sdl_set_window_grab(struct sdl_render_data *render_data,
 boolean enable)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  SDL_SetWindowMouseGrab(render_data->window, enable);
#elif SDL_VERSION_ATLEAST(2,0,0)
  SDL_SetWindowGrab(render_data->window, enable);
#else
  // Not a perfect equivalent; the SDL 1.2 version will grab even if unfocused
  SDL_GrabMode mode = enable ? SDL_GRAB_ON : SDL_GRAB_OFF;
  SDL_WM_GrabInput(mode);
#endif
}

#if SDL_VERSION_ATLEAST(2,0,0)
static void sdl_set_screensaver_enabled(boolean enable)
{
  debug("SDL %s screensaver.\n", enable ? "enabling" : "disabling");
  if(enable)
    SDL_EnableScreenSaver();
  else
    SDL_DisableScreenSaver();
}
#endif

int sdl_flags(const struct video_window *window)
{
  int flags = 0;

  if(window->is_fullscreen)
  {
#if SDL_VERSION_ATLEAST(2,0,0) && !SDL_VERSION_ATLEAST(3,0,0)
    /* SDL3 removed the ability to specify this at window creation time. */
    if(window->is_fullscreen_windowed)
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    else
#endif
      flags |= SDL_WINDOW_FULLSCREEN;

#if !SDL_VERSION_ATLEAST(2,0,0)
    if(window->bits_per_pixel == 8)
      flags |= SDL_HWPALETTE;
#endif
  }
  else
    if(window->allow_resize)
      flags |= SDL_WINDOW_RESIZABLE;

  return flags;
}

#if SDL_VERSION_ATLEAST(2,0,0)
// Get the current desktop resolution, if possible.
static boolean sdl_get_desktop_display_mode(SDL_DisplayMode *display_mode)
{
#if SDL_VERSION_ATLEAST(3,0,0)

  const SDL_DisplayMode **list;
  const SDL_DisplayMode *mode;
  int count;

  SDL_DisplayID id = SDL_GetPrimaryDisplay();
  if(id == 0)
    return false;

  mode = SDL_GetDesktopDisplayMode(id);
  if(mode)
  {
    *display_mode = *mode;
    return true;
  }

  warn("Failed to query desktop display mode: %s\n", SDL_GetError());
  list = (const SDL_DisplayMode **)SDL_GetFullscreenDisplayModes(0, &count);
  if(list)
  {
    if(count)
      mode = list[0];

    SDL_free(list);
    if(mode)
    {
      *display_mode = *mode;
      return true;
    }
  }

#else

  if(SDL_GetDesktopDisplayMode(0, display_mode) == 0)
  {
    debug("Queried desktop mode: %d x %d, %dHz, %s\n",
     display_mode->w, display_mode->h, display_mode->refresh_rate,
     SDL_GetPixelFormatName(display_mode->format));
    return true;
  }

  warn("Failed to query desktop display mode: %s\n", SDL_GetError());

  if(SDL_GetNumDisplayModes(0))
    if(SDL_GetDisplayMode(0, 0, display_mode) == 0)
      return true;

#endif

  return false;
}

// Get the smallest possible resolution bigger than both 640x350 and requested.
// Smaller means the software renderer will occupy more screen space.
static boolean sdl_get_closest_usable_display_mode(SDL_DisplayMode *display_mode,
 int width, int height)
{
#if SDL_VERSION_ATLEAST(3,0,0)

  int min_size = INT_MAX;
  int count;
  int i;

  const SDL_DisplayMode **list;
  SDL_DisplayID id = SDL_GetPrimaryDisplay();
  if(id == 0)
    return false;

  list = (const SDL_DisplayMode **)SDL_GetFullscreenDisplayModes(id, &count);
  if(!list)
    return false;

  debug("Display modes:\n");

  for(i = 0; i < count; i++)
  {
    if(!list[i])
      continue;

    debug("%d: %d x %d, %.2fHz, %s\n", i, list[i]->w, list[i]->h,
     list[i]->refresh_rate, SDL_GetPixelFormatName(list[i]->format));

    if((list[i]->w * list[i]->h < min_size) &&
     (list[i]->w >= width) && (list[i]->h >= height))
    {
      min_size = list[i]->w * list[i]->h;
      *display_mode = *list[i];
    }
  }
  SDL_free(list);

  if(min_size < INT_MAX)
    return true;

#else

  SDL_DisplayMode mode;
  int min_size = INT_MAX;
  int count;
  int i;

  count = SDL_GetNumDisplayModes(0);

  debug("Display modes:\n");

  for(i = 0; i < count; i++)
  {
    if(SDL_GetDisplayMode(0, i, &mode) != 0)
      continue;

    debug("%d: %d x %d, %dHz, %s\n", i, mode.w, mode.h, mode.refresh_rate,
     SDL_GetPixelFormatName(mode.format));

    if((mode.w * mode.h < min_size) && (mode.w >= width) && (mode.h >= height))
    {
      min_size = mode.w * mode.h;
      *display_mode = mode;
    }
  }
  if(min_size < INT_MAX)
    return true;

#endif /* !SDL_VERSION_ATLEAST(3,0,0) */

  return false;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

static boolean sdl_get_fullscreen_resolution(int *width, int *height,
 boolean match_current_desktop)
{
#if SDL_VERSION_ATLEAST(2,0,0)

  SDL_DisplayMode display_mode;
  boolean have_mode = false;

  if(match_current_desktop)
  {
    // Use the current desktop resolution.
    have_mode = sdl_get_desktop_display_mode(&display_mode);
  }
  else
  {
    have_mode = sdl_get_closest_usable_display_mode(&display_mode, 640, 350);
  }

  if(have_mode)
  {
    debug("\n");
    debug("selected: %d x %d, %.02fHz, %s\n", display_mode.w, display_mode.h,
     (float)display_mode.refresh_rate, SDL_GetPixelFormatName(display_mode.format));
    *width = display_mode.w;
    *height = display_mode.h;
    return true;
  }
  else
    warn("Failed to get display mode: %s\n", SDL_GetError());
#endif

  return false;
}

/**
 * Automatically detect and correct the fullscreen size if required.
 * This should be done even if the window isn't initially fullscreen.
 * If the window is fullscreen, the window size will also be updated.
 */
static void auto_fullscreen_size(struct graphics_data *graphics,
 struct video_window *window, boolean renderer_supports_scaling)
{
  int width = graphics->resolution_width;
  int height = graphics->resolution_height;

  // Fullscreen windowed should always autodetect the size regardless of config.
  if(width <= 0 || height <= 0 || window->is_fullscreen_windowed)
  {
    if(!sdl_get_fullscreen_resolution(&width, &height,
     renderer_supports_scaling || window->is_fullscreen_windowed))
    {
      width = 640;
      height = 480;
    }

    graphics->resolution_width = width;
    graphics->resolution_height = height;
    if(window->is_fullscreen)
    {
      window->width_px = graphics->resolution_width;
      window->height_px = graphics->resolution_height;
    }
  }
  video_window_update_viewport(window);
}

#if SDL_VERSION_ATLEAST(2,0,0)
/**
 * Determine if MZX supports a particular SDL pixel format. Returns a priority
 * value if the format is supported or 0 if the format is not supported. Values
 * returned for separate formats can be compared; higher values correspond to
 * higher priority.
 *
 * A bits_per_pixel value can be provided to filter out pixel formats that do
 * not correspond to the desired BPP value.
 */
static uint32_t sdl_pixel_format_priority(uint32_t pixel_format,
 uint32_t bits_per_pixel, uint32_t yuv_priority)
{
  switch(pixel_format)
  {
    case SDL_PIXELFORMAT_INDEX8:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 8)
        return 256;

      break;
    }

#if SDL_VERSION_ATLEAST(2,0,12)
    case SDL_PIXELFORMAT_XBGR4444:
#endif
    case SDL_PIXELFORMAT_XRGB4444:
    case SDL_PIXELFORMAT_ARGB4444:
    case SDL_PIXELFORMAT_RGBA4444:
    case SDL_PIXELFORMAT_ABGR4444:
    case SDL_PIXELFORMAT_BGRA4444:
    {
      // Favor 16bpp modes with more colors if possible.
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 444;

      break;
    }

    case SDL_PIXELFORMAT_XRGB1555:
    case SDL_PIXELFORMAT_XBGR1555:
    case SDL_PIXELFORMAT_ARGB1555:
    case SDL_PIXELFORMAT_RGBA5551:
    case SDL_PIXELFORMAT_ABGR1555:
    case SDL_PIXELFORMAT_BGRA5551:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 555;

      break;
    }

    case SDL_PIXELFORMAT_RGB565:
    case SDL_PIXELFORMAT_BGR565:
    {
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16)
        return 565;

      break;
    }

    case SDL_PIXELFORMAT_XRGB8888:
    case SDL_PIXELFORMAT_XBGR8888:
    case SDL_PIXELFORMAT_RGBX8888:
    case SDL_PIXELFORMAT_BGRX8888:
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_RGBA8888:
    case SDL_PIXELFORMAT_ABGR8888:
    case SDL_PIXELFORMAT_BGRA8888:
    case SDL_PIXELFORMAT_ARGB2101010:
#if SDL_VERSION_ATLEAST(3,0,0)
    case SDL_PIXELFORMAT_XRGB2101010:
    case SDL_PIXELFORMAT_XBGR2101010:
    case SDL_PIXELFORMAT_ABGR2101010:
#endif
    {
      // Any 32-bit RGB format is okay.
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 32)
        return 888;

      break;
    }

    case SDL_PIXELFORMAT_YUY2:
    case SDL_PIXELFORMAT_UYVY:
    case SDL_PIXELFORMAT_YVYU:
    {
      if(!yuv_priority)
        break;

      // These can work as either 32-bpp (full encode) or partial 16-bpp
      // (chroma subsampling for render_graph, full encode for layers).
      if(bits_per_pixel == BPP_AUTO || bits_per_pixel == 16 || bits_per_pixel == 32)
        return yuv_priority;

      break;
    }
  }
  return 0;
}
#endif /* SDL_VERSION_ATLEAST(2,0,0) */

#if !SDL_VERSION_ATLEAST(2,0,0)
/**
 * Precheck a provided video mode and select a BPP (if requested). This
 * works somewhat opposite to how SDL 2 works: instead of a pixel format
 * being picked by SDL (which needs to be checked for MZX support), MZX
 * instead needs to test its preferred pixel format by querying SDL.
 */
boolean sdl_check_video_mode(struct graphics_data *graphics,
 struct video_window *window, boolean renderer_supports_scaling, int flags)
{
  static int system_bpp = 0;
  static boolean has_system_bpp = false;
  int width;
  int height;
  int in_depth = window->bits_per_pixel;
  int out_depth;

  auto_fullscreen_size(graphics, window, renderer_supports_scaling);
  width = window->width_px;
  height = window->height_px;

  if(!has_system_bpp)
  {
    // Query the "best" video mode. This might actually be the current video
    // mode if another renderer has been initialized, so always do this first
    // (even if the provided depth isn't BPP_AUTO).
    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    if(info && info->vfmt)
    {
      system_bpp = info->vfmt->BytesPerPixel * 8;
      has_system_bpp = true;
      debug("SDL_GetVideoInfo BPP=%d\n", system_bpp);
    }
  }

  if(in_depth == BPP_AUTO)
    in_depth = system_bpp;

  out_depth = SDL_VideoModeOK(width, height, in_depth, flags);
  if(out_depth <= 0)
  {
    debug("SDL_VideoModeOK failed.\n");
    return false;
  }

  if(window->bits_per_pixel == BPP_AUTO)
  {
    if(out_depth == 8 || out_depth == 16 || out_depth == 32)
    {
      debug("SDL_VideoModeOK recommends BPP=%d\n", out_depth);
      graphics->bits_per_pixel = out_depth;
      window->bits_per_pixel = out_depth;
    }
    else

    if(out_depth > 0)
    {
      debug("SDL_VideoModeOK recommends unsupported BPP=%d; using 32bpp\n", out_depth);
      graphics->bits_per_pixel = 32;
      window->bits_per_pixel = 32;
    }
  }
  return true;
}
#endif /* !SDL_VERSION_ATLEAST(2,0,0) */

void sdl_destruct_window(struct graphics_data *graphics)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  size_t i;

  // Used by the software renderer as a fallback if the window surface doesn't
  // match the pixel format MZX wants.
  if(render_data->shadow)
  {
    SDL_DestroySurface(render_data->shadow);
    render_data->shadow = NULL;
  }

  // Used for 8bpp support for the software renderer.
  // This is attached to the surface in SDL3 and should not be destroyed.
  if(render_data->palette)
  {
#if !SDL_VERSION_ATLEAST(3,0,0)
    SDL_DestroyPalette(render_data->palette);
#endif
    render_data->palette = NULL;
  }

#if !SDL_VERSION_ATLEAST(3,0,0)
  // Used for generating mapped colors for the SDL_Renderer renderers.
  if(render_data->pixel_format)
  {
    SDL_FreeFormat(render_data->pixel_format);
    render_data->pixel_format = NULL;
  }
#endif

  // Used by the SDL renderer-based renderers for HW acceleration.
  for(i = 0; i < ARRAY_SIZE(render_data->texture); i++)
  {
    if(render_data->texture[i])
    {
      SDL_DestroyTexture(render_data->texture[i]);
      render_data->texture[i] = NULL;
    }
  }

  // Used by the softscale renderer for HW acceleration. Don't use for software.
  // Destroying this when exiting fullscreen can be slow for some reason.
  if(render_data->renderer)
  {
    SDL_DestroyRenderer(render_data->renderer);
    render_data->renderer = NULL;
  }

  // Used by the OpenGL renderers.
  if(render_data->context)
  {
    SDL_GL_DestroyContext(render_data->context);
    render_data->context = NULL;
  }

  // Used by all SDL renderers.
  if(render_data->window)
  {
    SDL_DestroyWindow(render_data->window);
    render_data->window = NULL;
  }

  // This is attached to the window for renderers that use it. Just clear it...
  render_data->screen = NULL;

#else /* !SDL_VERSION_ATLEAST(2,0,0) */

  // Used by the YUV renderers.
  if(render_data->overlay)
  {
    SDL_FreeYUVOverlay(render_data->overlay);
    render_data->overlay = NULL;
  }

  // Don't free render_data->screen. This pointer is managed by SDL.
  // The shadow surface isn't used by SDL 1.2 builds.
  render_data->screen = NULL;
  render_data->shadow = NULL;

#endif

  // Used to send the palette to SDL in indexed mode.
  if(render_data->palette_colors)
  {
    free(render_data->palette_colors);
    render_data->palette_colors = NULL;
  }
  render_data->flat_format = NULL;
}

boolean sdl_set_window_caption(struct graphics_data *graphics,
 struct video_window *window, const char *caption)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(3,0,0)
  if(!SDL_SetWindowTitle(render_data->window, caption))
  {
    debug("failed SDL_SetWindowTitle: %s\n", SDL_GetError());
    return false;
  }
#else
  SDL_SetWindowTitle(render_data->window, caption);
#endif
  return true;
}

#if defined(CONFIG_ICON) && !defined(_WIN32)
#include "../../io/image_png.h"
#include "../../io/vio.h"

static size_t icon_read_vfile_fn(void * RESTRICT buffer, size_t count,
 void * RESTRICT handle)
{
  return vfread(buffer, 1, count, (vfile *)handle);
}

static png_bool icon_alloc_fn(uint32_t width, uint32_t height,
 struct png_rgba **pixels, size_t *pixels_row_pitch, void *priv)
{
  SDL_Surface **dest = (SDL_Surface **)priv;
  SDL_Surface *s;

  /* Icons must be multiples of 16 and square. */
  if(width != height || width % 16)
    return IMAGE_FALSE;

#if SDL_VERSION_ATLEAST(2,0,0)
  s = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
#else
  Uint32 rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;
#else
  rmask = 0x000000ff;
  gmask = 0x0000ff00;
  bmask = 0x00ff0000;
  amask = 0xff000000;
#endif

  s = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, rmask, gmask, bmask, amask);
#endif
  if(!s)
    return IMAGE_FALSE;

  *dest = s;
  *pixels = (struct png_rgba *)s->pixels;
  *pixels_row_pitch = s->pitch;
  return IMAGE_TRUE;
}

static SDL_Surface *png_read_icon(const char *name)
{
  SDL_Surface *s = NULL;
  enum png_error ret;

  vfile *vf = vfopen_unsafe(name, "rb");
  if(!vf)
    return NULL;

  ret = png_read(vf, icon_read_vfile_fn, &s, icon_alloc_fn, false);
  vfclose(vf);

  if(ret != PNG_OK)
  {
    SDL_DestroySurface(s);
    return NULL;
  }
  return s;
}
#endif // CONFIG_ICON && !_WIN32

boolean sdl_set_window_icon(struct graphics_data *graphics,
 struct video_window *window, const char *icon_path)
{
#ifdef CONFIG_ICON
  struct sdl_render_data *render_data = graphics->render_data;
  /* This function may be a NOP for some configurations. */
  (void)render_data;

#ifdef _WIN32
  {
    /* Load the icon directly from the executable resource. */
    HICON icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    if(icon)
    {
      SendMessage((HWND)SDL_GetWindowProperty_HWND(render_data->window),
       WM_SETICON, ICON_BIG, (LPARAM)icon);
      return true;
    }
    else
      debug("failed to open embedded icon\n");
  }
#else /* !_WIN32 */
  {
    SDL_Surface *icon;
    if(!icon_path)
    {
      warn("NULL icon path, report this!\n");
      return false;
    }

    icon = png_read_icon(icon_path);
    if(icon)
    {
#if SDL_VERSION_ATLEAST(3,0,0)
      if(!SDL_SetWindowIcon(render_data->window, icon))
      {
        debug("failed SDL_SetWindowIcon: %s\n", SDL_GetError());
        SDL_DestroySurface(icon);
        return false;
      }
#else
      SDL_SetWindowIcon(render_data->window, icon);
#endif
      SDL_DestroySurface(icon);
      return true;
    }
    else
      warn("failed to open icon file '%s'\n", icon_path);
  }
#endif // !_WIN32
#endif // CONFIG_ICON
  return false;
}

void sdl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct sdl_render_data *render_data = (struct sdl_render_data *)graphics->render_data;
  unsigned int i;

  if(graphics->bits_per_pixel == 8)
  {
    if(!render_data->palette_colors)
      return;
    if(count > 256)
      count = 256;

    for(i = 0; i < count; i++)
    {
      render_data->palette_colors[i].r = palette[i].r;
      render_data->palette_colors[i].g = palette[i].g;
      render_data->palette_colors[i].b = palette[i].b;
    }

#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_SetPaletteColors(render_data->palette,
     render_data->palette_colors, 0, count);
#else
    SDL_SetColors(render_data->screen, render_data->palette_colors, 0, count);
#endif
  }
  else

  if(!render_data->rgb_to_yuv)
  {
    if(!render_data->flat_format)
      return;
    for(i = 0; i < count; i++)
    {
#if SDL_VERSION_ATLEAST(3,0,0)
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(render_data->flat_format, NULL,
        palette[i].r, palette[i].g, palette[i].b, SDL_ALPHA_OPAQUE);
#else
      graphics->flat_intensity_palette[i] =
       SDL_MapRGBA(render_data->flat_format,
        palette[i].r, palette[i].g, palette[i].b, SDL_ALPHA_OPAQUE);
#endif
    }
  }
  else
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
       render_data->rgb_to_yuv(palette[i].r, palette[i].g, palette[i].b);
    }
  }
}


/* SDL software renderer functions for: *************************************/
// software
// gp2x

boolean sdl_create_window_soft(struct graphics_data *graphics,
 struct video_window *window)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Surface *target;
  int depth = window->bits_per_pixel;
  boolean matched = false;
  Uint32 fmt;

  sdl_destruct_window(graphics);

  auto_fullscreen_size(graphics, window, false);

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Also, a hint needs to be set to make SDL_UpdateWindowSurface not crash.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

  render_data->window = SDL_CreateWindow("MegaZeux",
   window->width_px, window->height_px, sdl_flags(window));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  render_data->screen = SDL_GetWindowSurface(render_data->window);
  if(!render_data->screen)
  {
    warn("Failed to get window surface: %s\n", SDL_GetError());
    goto err_free;
  }

  /* SDL 2.0 allows the window system to offer up buffers of a specific
   * format, and expects the application to perform a blit if the format
   * doesn't match what the app wanted. To accomodate this, allocate a
   * shadow screen that we render to in the case the formats do not match,
   * and blit it at present time.
   */

  fmt = SDL_GetWindowPixelFormat(render_data->window);
  debug("Window pixel format: %s\n", SDL_GetPixelFormatName(fmt));

  if(!sdl_pixel_format_priority(fmt, depth, YUV_DISABLE))
  {
    switch(depth)
    {
      case 8:
        fmt = SDL_PIXELFORMAT_INDEX8;
        break;
      case 16:
        fmt = SDL_PIXELFORMAT_RGB565;
        break;
      case 32:
      default:
        fmt = SDL_PIXELFORMAT_RGBA8888;
        break;
    }
  }
  else
    matched = true;

  debug("Using pixel format: %s\n", SDL_GetPixelFormatName(fmt));

  if(depth == BPP_AUTO)
    graphics->bits_per_pixel = window->bits_per_pixel = SDL_BYTESPERPIXEL(fmt) * 8;

  if((unsigned)render_data->screen->w < window->width_px ||
   (unsigned)render_data->screen->h < window->height_px)
  {
    matched = false;
    warn("Window surface (%dx%d) is smaller than expected (%ux%u).\n",
     render_data->screen->w, render_data->screen->h,
     window->width_px, window->height_px);
  }

  if(!matched)
  {
    render_data->shadow = SDL_CreateSurface(window->width_px,
     window->height_px, fmt);
    debug("Blitting enabled. Rendering performance will be reduced.\n");
  }
  else
  {
    render_data->shadow = NULL;
  }

  target = render_data->shadow ? render_data->shadow : render_data->screen;
#if SDL_VERSION_ATLEAST(3,0,0)
  render_data->flat_format = SDL_GetPixelFormatDetails(target->format);
#else
  render_data->flat_format = target->format;
#endif

  if(fmt == SDL_PIXELFORMAT_INDEX8)
  {
#if SDL_VERSION_ATLEAST(3,0,0)
    render_data->palette = SDL_CreateSurfacePalette(target);
    if(!render_data->palette)
    {
      warn("Failed to create surface palette: %s\n", SDL_GetError());
      goto err_free;
    }
#else
    render_data->palette = SDL_AllocPalette(SMZX_PAL_SIZE);
    if(!render_data->palette)
    {
      warn("Failed to allocate palette: %s\n", SDL_GetError());
      goto err_free;
    }

    if(SDL_SetPixelFormatPalette(target->format, render_data->palette))
    {
      warn("Failed to set pixel format palette: %s\n", SDL_GetError());
      goto err_free;
    }
#endif

    render_data->palette_colors =
     (SDL_Color *)ccalloc(SMZX_PAL_SIZE, sizeof(SDL_Color));
    if(!render_data->palette_colors)
    {
      warn("Failed to allocate palette colors\n");
      goto err_free;
    }
  }
  else
  {
    render_data->palette = NULL;
    render_data->palette_colors = NULL;
  }

  window->platform_id = SDL_GetWindowID(render_data->window);
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!sdl_check_video_mode(graphics, window, false, sdl_flags(window)))
    return false;

  render_data->screen = SDL_SetVideoMode(window->width_px, window->height_px,
   window->bits_per_pixel, sdl_flags(window));

  if(!render_data->screen)
    return false;

  render_data->shadow = NULL;
  render_data->flat_format = render_data->screen->format;

  if(window->bits_per_pixel == 8)
  {
    render_data->palette_colors =
     (SDL_Color *)ccalloc(SMZX_PAL_SIZE, sizeof(SDL_Color));
    if(!render_data->palette_colors)
      return false;
  }

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  // Wipe the letterbox area, if any.
  SDL_FillSurfaceRect(render_data->screen, NULL, 0);

  sdl_set_system_cursor(graphics);
  sdl_set_window_grab(render_data, window->grab_mouse);
  sdl_init_window_text_events(window->platform_id);
  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free:
  sdl_destruct_window(graphics);
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

boolean sdl_resize_window(struct graphics_data *graphics,
 struct video_window *window)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  struct sdl_render_data *render_data = graphics->render_data;

  if(window->is_fullscreen)
  {
    // Unlike window creation, SDL can't be nicely provided with a target
    // resolution for real fullscreen, so MZX has to manually find one.
    SDL_DisplayMode mode;
    boolean real_fullscreen = false;

    if(!window->is_fullscreen_windowed &&
     sdl_get_closest_usable_display_mode(&mode, window->width_px, window->height_px))
    {
      debug("\n");
      debug("selected: %d x %d, %.02fHz, %s\n", mode.w, mode.h,
       (float)mode.refresh_rate, SDL_GetPixelFormatName(mode.format));

#if SDL_VERSION_ATLEAST(3,0,0)
      if(SDL_SetWindowFullscreenMode(render_data->window, &mode) &&
         SDL_SetWindowFullscreen(render_data->window, true))
#else
      if(SDL_SetWindowDisplayMode(render_data->window, &mode) == 0 &&
         SDL_SetWindowFullscreen(render_data->window, SDL_WINDOW_FULLSCREEN) == 0)
#endif
      {
        graphics->resolution_width = mode.w;
        graphics->resolution_height = mode.h;
        window->width_px = mode.w;
        window->height_px = mode.h;
        // graphics.c will do this anyway.
        //video_window_update_viewport(window);
        real_fullscreen = true;
      }
      else
      {
        debug("failed to set selected fullscreen mode: %s\n", SDL_GetError());
        debug("falling back to fullscreen windowed\n");
      }
    }

    if(!real_fullscreen)
    {
#if SDL_VERSION_ATLEAST(3,0,0)
      // Select borderless desktop fullscreen.
      SDL_SetWindowFullscreenMode(render_data->window, NULL);
      SDL_SetWindowFullscreen(render_data->window, true);
#else
      SDL_SetWindowFullscreen(render_data->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
    }
  }
  else
  {
    SDL_SetWindowFullscreen(render_data->window, 0);
    SDL_SetWindowBordered(render_data->window, 1);
    SDL_SetWindowResizable(render_data->window, window->allow_resize);
    // TODO: doesn't seem to reliably work??
    SDL_SetWindowSize(render_data->window, window->width_px, window->height_px);
    SDL_SetWindowPosition(render_data->window,
     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  }

  sdl_set_system_cursor(graphics);
  sdl_set_window_grab(render_data, window->grab_mouse);
  return true;

#else
  // SDL_SetVideoMode
  return sdl_create_window_soft(graphics, window);
#endif
}


/* SDL hardware renderer API functions for: *********************************/
// softscale
// sdlaccel

#if defined(CONFIG_RENDER_SOFTSCALE) || defined(CONFIG_RENDER_SDLACCEL)

#if SDL_VERSION_ATLEAST(3,0,0)
#define BEST_RENDERER NULL
#else
#define BEST_RENDERER -1
#endif

/* The official SDL line on SDL_LockTexture is that the buffer is write-only,
 * but in practice, this is variable. MegaZeux doesn't care about whether or
 * not the buffer is initialized, but it needs to be able to read after writing
 * for transparent layers. The "official" way this should be done is using a
 * buffer and SDL_UpdateTexture.
 *
 * + direct3d, software, PS2, PSP, Vita, Wii U:
 *   SDL_UpdateTexture calls SDL_LockTexture and SDL_memcpy internally.
 *   SDL_LockTexture in these instances returns a read+write buffer.
 *   This is safe for rendering and faster than using SDL_UpdateTexture.
 * + OpenGL(ES) uses glTexSubImage2D either way, so negligible.
 * + direct3d11/direct3d12 map texture memory as write-only and incur massive
 *   performance penalties for ANY read. Always use SDL_UpdateTexture.
 * + vulkan, gpu may use direct3d11/direct3d12 mapping internally.
 * + metal is only supported on new enough computers that it doesn't matter.
 * + ogc (Wii/GC) has a faster SDL_UpdateTexture.
 */
#if defined(CONFIG_PS2) || defined(CONFIG_PSP) || \
    defined(CONFIG_PSVITA) || defined(CONFIG_WIIU)
#define USE_TEXTURE_STREAMING(driver) (true)
#elif defined(_WIN32)
#define USE_TEXTURE_STREAMING(driver) \
 (is_software_renderer || !strcasecmp((driver), "direct3d"))
#else
#define USE_TEXTURE_STREAMING(driver) (is_software_renderer)
#endif

static uint32_t get_format_amask(uint32_t format)
{
  Uint32 rmask, gmask, bmask, amask;
  int bpp;

  SDL_GetMasksForPixelFormat(format, &bpp, &rmask, &gmask, &bmask, &amask);
  return amask;
}

static void find_texture_format(struct graphics_data *graphics,
 boolean requires_blend_ops)
{
  struct sdl_render_data *render_data = (struct sdl_render_data *)graphics->render_data;
  uint32_t texture_format = SDL_PIXELFORMAT_UNKNOWN;
  unsigned int texture_bpp = 0;
  uint32_t texture_amask = 0;
  boolean allow_subsampling = false;
  boolean need_alpha = false;
  uint32_t yuv_priority = YUV_PRIORITY;
  uint32_t priority = 0;
  boolean is_software_renderer = false;
  const char *renderer_name = NULL;
  int num_formats;

#if SDL_VERSION_ATLEAST(3,0,0)

  SDL_PropertiesID props = SDL_GetRendererProperties(render_data->renderer);
  const SDL_PixelFormat *formats = NULL;
  const SDL_PixelFormat *pos;

  renderer_name = SDL_GetRendererName(render_data->renderer);
  if(!strcmp(renderer_name, SDL_SOFTWARE_RENDERER))
    is_software_renderer = true;

  formats = (const SDL_PixelFormat *)SDL_GetPointerProperty(props,
   SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
  num_formats = 0;
  for(pos = formats; pos && *pos != SDL_PIXELFORMAT_UNKNOWN; pos++)
    num_formats++;

#else

  SDL_RendererInfo rinfo;
  const uint32_t *formats = NULL;

  if(!SDL_GetRendererInfo(render_data->renderer, &rinfo))
  {
    renderer_name = rinfo.name;
    num_formats = rinfo.num_texture_formats;
    formats = rinfo.texture_formats;

    if(rinfo.flags & SDL_RENDERER_SOFTWARE)
      is_software_renderer = true;
  }
  else
    warn("Failed to get renderer info!\n");

#endif /* !SDL_VERSION_ATLEAST(3,0,0) */

  if(formats)
  {
    unsigned int depth = graphics->bits_per_pixel;
    int i;

    info("SDL render driver: '%s'\n", renderer_name);
    if(is_software_renderer)
      warn("Accelerated renderer not available. Rendering will be SLOW!\n");

#ifdef __MACOSX__
    // Not clear if Metal supports the custom Apple YUV texture format.
    if(!strcasecmp(renderer_name, "opengl"))
      yuv_priority = YUV_PRIORITY_APPLE;
#endif

    // Anything using hardware blending must support alpha.
    if(requires_blend_ops)
      need_alpha = true;

    // Try to use a native texture format to improve performance.
    for(i = 0; i < num_formats; i++)
    {
      uint32_t format = formats[i];
      unsigned int format_priority;

      debug("%d: %s\n", i, SDL_GetPixelFormatName(format));

      if(SDL_ISPIXELFORMAT_INDEXED(format))
        continue;
      if(need_alpha && !SDL_ISPIXELFORMAT_ALPHA(format))
        continue;

      format_priority = sdl_pixel_format_priority(format, depth, yuv_priority);
      if(format_priority > priority)
      {
        texture_format = format;
        priority = format_priority;
      }
    }
  }

  if(texture_format == SDL_PIXELFORMAT_UNKNOWN)
  {
    // 16bpp RGB seems moderately faster than YUV with chroma subsampling
    // when neither are natively supported.
    if(graphics->bits_per_pixel == 16)
      texture_format = SDL_PIXELFORMAT_RGB565;
    else
      texture_format = SDL_PIXELFORMAT_ARGB8888;

    debug("No matching pixel format. Using %s. Rendering may be slower.\n",
     SDL_GetPixelFormatName(texture_format));
  }
  else
    debug("Using pixel format %s.\n", SDL_GetPixelFormatName(texture_format));

  if(priority == yuv_priority)
  {
    texture_bpp = 32;

    if(texture_format == SDL_PIXELFORMAT_YUY2)
    {
      render_data->subsample_set_colors = yuy2_subsample_set_colors_mzx;
      render_data->rgb_to_yuv = rgb_to_yuy2;
    }

    if(texture_format == SDL_PIXELFORMAT_UYVY)
    {
      render_data->subsample_set_colors = uyvy_subsample_set_colors_mzx;
#ifdef __MACOSX__
      render_data->rgb_to_yuv = rgb_to_apple_ycbcr_422;
#else
      render_data->rgb_to_yuv = rgb_to_uyvy;
#endif
    }

    if(texture_format == SDL_PIXELFORMAT_YVYU)
    {
      render_data->subsample_set_colors = yvyu_subsample_set_colors_mzx;
      render_data->rgb_to_yuv = rgb_to_yvyu;
    }

    // If this renderer was activated with force_bpp=16, enable subsampling
    // support for render_graph.
    if(graphics->bits_per_pixel == 16)
    {
      debug("Allowing YUV subsampling for render_graph.\n");
      allow_subsampling = true;
    }
  }
  else
  {
    texture_bpp = SDL_BYTESPERPIXEL(texture_format) * 8;
    texture_amask = get_format_amask(texture_format);
  }

  if(graphics->bits_per_pixel == BPP_AUTO)
    graphics->bits_per_pixel = texture_bpp;

  // Initialize the texture format data.
  render_data->texture_format = texture_format;
  render_data->texture_amask = texture_amask;
  render_data->texture_bpp = texture_bpp;
  render_data->allow_subsampling = allow_subsampling;
  render_data->use_texture_streaming = USE_TEXTURE_STREAMING(renderer_name);
}

/**
 * Initialize a window for SDL Renderer-based renderers. This function will
 * automatically select a native texture format for the renderer (if applicable)
 * for better performance. This texture format is stored to `sdl_render_data`.
 * In some cases (but usually not), this may include YUV texture formats. The
 * corresponding `sdlrender_update_colors` should be used with these renderers.
 *
 * This function will also pick nearest or linear scaling automatically based
 * on the scaling ratio and window size.
 */
boolean sdl_create_window_renderer(struct graphics_data *graphics,
 struct video_window *window, boolean requires_blend_ops)
{
  struct sdl_render_data *render_data = graphics->render_data;

  sdl_destruct_window(graphics);

  auto_fullscreen_size(graphics, window, true);

#if !SDL_VERSION_ATLEAST(2,0,12)
  // Use linear filtering unless the display is being integer scaled.
  if(window->is_integer_scaled)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
#endif

#if defined(__EMSCRIPTEN__) && SDL_VERSION_ATLEAST(2,0,10)
  // Not clear if this hint is required to make this renderer not crash, but
  // considering both software and GLSL need it...
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif

  if(graphics->sdl_render_driver[0])
  {
    info("Requesting SDL render driver: '%s'\n", graphics->sdl_render_driver);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, graphics->sdl_render_driver);
  }

  render_data->window = SDL_CreateWindow("MegaZeux",
   window->width_px, window->height_px, sdl_flags(window));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

#if SDL_VERSION_ATLEAST(3,0,0)
  render_data->renderer = SDL_CreateRenderer(render_data->window, BEST_RENDERER);
#else
  render_data->renderer = SDL_CreateRenderer(render_data->window, BEST_RENDERER,
   requires_blend_ops ? SDL_RENDERER_TARGETTEXTURE : 0);
#endif
  if(!render_data->renderer)
  {
    warn("Failed to create renderer: %s\n", SDL_GetError());
    goto err_free;
  }

  find_texture_format(graphics, requires_blend_ops);
  window->bits_per_pixel = graphics->bits_per_pixel;

  if(!render_data->rgb_to_yuv)
  {
#if SDL_VERSION_ATLEAST(3,0,0)
    render_data->flat_format =
     SDL_GetPixelFormatDetails(render_data->texture_format);
#else
    // This is required for SDL_MapRGBA to work, but YUV formats can ignore it.
    render_data->pixel_format = SDL_AllocFormat(render_data->texture_format);
    if(!render_data->pixel_format)
    {
      warn("Failed to allocate pixel format: %s\n", SDL_GetError());
      goto err_free;
    }
    render_data->flat_format = render_data->pixel_format;
#endif
  }

  SDL_SetRenderDrawColor(render_data->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(render_data->renderer);

  window->platform_id = SDL_GetWindowID(render_data->window);
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);
  sdl_set_system_cursor(graphics);
  sdl_set_window_grab(render_data, window->grab_mouse);
  sdl_init_window_text_events(window->platform_id);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

/* Set the texture scaling mode for a given texture.
 * If allow_non_integer is true, the best is selected given whether or not
 * the screen is an integer multiple of 640x350. If false, nearest is used.
 * This only works for SDL 2.0.12 and up; prior should rely on hints. */
void sdl_set_texture_scale_mode(struct graphics_data *graphics,
 struct video_window *window, int texture_id, boolean allow_non_integer)
{
#if SDL_VERSION_ATLEAST(2,0,12)
  struct sdl_render_data *render_data =
   (struct sdl_render_data *)graphics->render_data;

  if(render_data->texture[texture_id])
  {
    SDL_ScaleMode mode;
    if(!allow_non_integer || window->is_integer_scaled)
      mode = SDL_SCALEMODE_NEAREST;
    else
      mode = SDL_SCALEMODE_LINEAR;

    SDL_SetTextureScaleMode(render_data->texture[texture_id], mode);
  }
  else
    warn("Texture %d is null!\n", texture_id);
#endif
}

#endif /* CONFIG_RENDER_SOFTSCALE || CONFIG_RENDER_SDLACCEL */


/* SDL direct OpenGL functions for: *****************************************/
// opengl1
// opengl2
// glsl / glslscale

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM)

boolean gl_create_window(struct graphics_data *graphics,
 struct video_window *window, struct gl_version req_ver)
{
  struct sdl_render_data *render_data = graphics->render_data;

#if SDL_VERSION_ATLEAST(2,0,0)
  sdl_destruct_window(graphics);

  auto_fullscreen_size(graphics, window, true);

#ifdef CONFIG_GLES
  // Hints to make SDL use OpenGL ES drivers (e.g. ANGLE) on Windows/Linux.
  // These may be ignored by SDL unless using the Windows or X11 video drivers.
#if SDL_VERSION_ATLEAST(2,0,6)
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
#if SDL_VERSION_ATLEAST(2,0,2)
  SDL_SetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER, "none");
#endif

  // Declare the OpenGL version to source functions from. This is necessary
  // since OpenGL ES 1.x and OpenGL ES 2.x are not compatible. This must be
  // done after old windows are destroyed and before new windows are created.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, req_ver.major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, req_ver.minor);
#endif

// Emscripten's EGL requires explicitly declaring alpha
// for RGBA textures to work.
#ifdef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

#if SDL_VERSION_ATLEAST(2,0,10)
  // Also, a hint needs to be set to make gl_swap_buffers not crash. Brilliant.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
#endif

  render_data->window = SDL_CreateWindow("MegaZeux",
   window->width_px, window->height_px, GL_STRIP_FLAGS(sdl_flags(window)));

  if(!render_data->window)
  {
    warn("Failed to create window: %s\n", SDL_GetError());
    goto err_free;
  }

  /* This automatically makes the context current. */
  render_data->context = SDL_GL_CreateContext(render_data->window);
  if(!render_data->context)
  {
    warn("Failed to create context: %s\n", SDL_GetError());
    goto err_free;
  }

  window->platform_id = SDL_GetWindowID(render_data->window);
  sdl_set_screensaver_enabled(graphics->disable_screensaver == SCREENSAVER_ENABLE);

#else // !SDL_VERSION_ATLEAST(2,0,0)

  if(!sdl_check_video_mode(graphics, window, true, GL_STRIP_FLAGS(sdl_flags(window))))
    return false;

  if(!SDL_SetVideoMode(window->width_px, window->height_px, window->bits_per_pixel,
       GL_STRIP_FLAGS(sdl_flags(window))))
    return false;

#endif // !SDL_VERSION_ATLEAST(2,0,0)

  render_data->screen = NULL;
  render_data->shadow = NULL;

  sdl_set_system_cursor(graphics);
  sdl_set_window_grab(render_data, window->grab_mouse);
  sdl_init_window_text_events(window->platform_id);
  return true;

#if SDL_VERSION_ATLEAST(2,0,0)
err_free:
  sdl_destruct_window(graphics);
  return false;
#endif // SDL_VERSION_ATLEAST(2,0,0)
}

boolean gl_resize_window(struct graphics_data *graphics,
 struct video_window *window)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  return sdl_resize_window(graphics, window);
#else
  // SDL_SetVideoMode
  struct gl_version dummy = { 0, 0 };
  return gl_create_window(graphics, window, dummy);
#endif
}

void gl_set_attributes(struct graphics_data *graphics)
{
  // Note that this function is called twice- both before and after
  // gl_create_window
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  if(graphics->gl_vsync == 0)
    SDL_GL_SetSwapInterval(0);
  else if(graphics->gl_vsync >= 1)
    SDL_GL_SetSwapInterval(1);
}

boolean gl_swap_buffers(struct graphics_data *graphics)
{
#if SDL_VERSION_ATLEAST(2,0,0)
  struct sdl_render_data *render_data = graphics->render_data;
  SDL_GL_SwapWindow(render_data->window);
#else
  SDL_GL_SwapBuffers();
#endif
  return true;
}

#endif // CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM

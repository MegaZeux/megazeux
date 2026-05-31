/* MegaZeux
 *
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

#include <stdlib.h>

#include "../render.h"
#include "../render_layer.h"
#include "../renderers.h"
#include "../yuv.h"
#include "render_sdl.h"

#include "../../util.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#error Overlay renderer no longer supported for SDL 2+. Use softscale!
#endif

#define YUV1_OVERLAY_WIDTH   (SCREEN_PIX_W * 2)
#define YUV1_OVERLAY_HEIGHT  SCREEN_PIX_H

#define YUV2_OVERLAY_WIDTH   SCREEN_PIX_W
#define YUV2_OVERLAY_HEIGHT  SCREEN_PIX_H

struct yuv_render_data
{
  struct sdl_render_data sdl;
  Uint32 bpp;
};

static boolean yuv_set_video_mode_size(struct graphics_data *graphics,
 struct video_window *window, int yuv_width, int yuv_height)
{
  struct yuv_render_data *render_data = graphics->render_data;

  window->bits_per_pixel = 32;
  if(!sdl_check_video_mode(graphics, window, true, sdl_flags(window) | SDL_ANYFORMAT))
    return false;

  // the YUV renderer _requires_ 32bit colour
  render_data->sdl.screen =
   SDL_SetVideoMode(window->width_px, window->height_px, 32,
    sdl_flags(window) | SDL_ANYFORMAT);

  if(!render_data->sdl.screen)
    goto err_free;

  // try with a YUY2 pixel format first
  render_data->sdl.rgb_to_yuv = rgb_to_yuy2;
  render_data->sdl.subsample_set_colors = yuy2_subsample_set_colors_mzx;
  render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
   SDL_YUY2_OVERLAY, render_data->sdl.screen);

  // didn't work, try with a UYVY pixel format next
  if(!render_data->sdl.overlay)
  {
    render_data->sdl.rgb_to_yuv = rgb_to_uyvy;
    render_data->sdl.subsample_set_colors = uyvy_subsample_set_colors_mzx;
    render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
     SDL_UYVY_OVERLAY, render_data->sdl.screen);
  }

  // Since we support this format now too, might as well try.
  if(!render_data->sdl.overlay)
  {
    render_data->sdl.rgb_to_yuv = rgb_to_yvyu;
    render_data->sdl.subsample_set_colors = yvyu_subsample_set_colors_mzx;
    render_data->sdl.overlay = SDL_CreateYUVOverlay(yuv_width, yuv_height,
     SDL_YVYU_OVERLAY, render_data->sdl.screen);
  }

  // failed to create an overlay
  if(!render_data->sdl.overlay)
    goto err_free;

  // Wipe the letterbox area, if any.
  // This must be performed on the window surface, not the overlay.
  SDL_FillRect(render_data->sdl.screen, NULL, 0);
  SDL_Flip(render_data->sdl.screen);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static boolean yuv1_set_video_mode(struct graphics_data *graphics,
 struct video_window *window)
{
  struct yuv_render_data *render_data = graphics->render_data;
  render_data->bpp = 32;

  return yuv_set_video_mode_size(graphics, window,
   YUV1_OVERLAY_WIDTH, YUV1_OVERLAY_HEIGHT);
}

static boolean yuv2_set_video_mode(struct graphics_data *graphics,
 struct video_window *window)
{
  struct yuv_render_data *render_data = graphics->render_data;
  render_data->bpp = 16;

  return yuv_set_video_mode_size(graphics, window,
   YUV2_OVERLAY_WIDTH, YUV2_OVERLAY_HEIGHT);
}

static boolean yuv_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct yuv_render_data *render_data =
   (struct yuv_render_data *)cmalloc(sizeof(struct yuv_render_data));
  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct yuv_render_data));

  graphics->bits_per_pixel = 32;
  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  graphics->ratio = conf->video_ratio;
  return true;
}

static void yuv_free_video(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static void yuv_lock_overlay(struct yuv_render_data *render_data,
 Uint32 **pixels, Uint32 *pitch)
{
  SDL_LockYUVOverlay(render_data->sdl.overlay);

  *pixels = (Uint32 *)render_data->sdl.overlay->pixels[0];
  *pitch = render_data->sdl.overlay->pitches[0];
}

static void yuv_unlock_overlay(struct yuv_render_data *render_data)
{
  SDL_UnlockYUVOverlay(render_data->sdl.overlay);
}

static void yuv_render_graph(struct graphics_data *graphics)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 mode = graphics->screen_mode;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  if(bpp == 16)
  {
    // Subsampled mode
    if(!mode)
      render_graph16((Uint16 *)pixels, pitch, graphics,
       render_data->sdl.subsample_set_colors);
    else
      render_graph16((Uint16 *)pixels, pitch, graphics, set_colors32[mode]);
  }
  else
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);
  }

  yuv_unlock_overlay(render_data);
}

static void yuv1_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_layer(pixels, SCREEN_PIX_W, SCREEN_PIX_H, pitch, bpp, graphics, layer);

  yuv_unlock_overlay(render_data);
}

static void yuv_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_cursor(pixels, pitch, bpp, x, y,
   graphics->flat_intensity_palette[color], lines, offset);

  yuv_unlock_overlay(render_data);
}

static void yuv_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct yuv_render_data *render_data = graphics->render_data;
  Uint32 bpp = render_data->bpp;
  Uint32 *pixels;
  Uint32 pitch;

  yuv_lock_overlay(render_data, &pixels, &pitch);

  render_mouse(pixels, pitch, bpp, x, y, 0xFFFFFFFF, 0x0, w, h);

  yuv_unlock_overlay(render_data);
}

static void yuv_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct yuv_render_data *render_data = graphics->render_data;
  SDL_Rect rect;

  rect.x = window->viewport_x;
  rect.y = window->viewport_y;
  rect.w = window->viewport_width;
  rect.h = window->viewport_height;

  SDL_DisplayYUVOverlay(render_data->sdl.overlay, &rect);
}

const struct renderer renderer_yuv1 =
{
  "overlay1",
  NULL,
  yuv_init_video,
  yuv_free_video,
  yuv1_set_video_mode,
  yuv1_set_video_mode,
  NULL,
  set_window_viewport_scaled,
  sdl_set_window_caption,
  sdl_set_window_icon,
  NULL,
  sdl_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  yuv_render_graph,
  yuv1_render_layer,
  yuv_render_cursor,
  NULL,
  yuv_render_mouse,
  yuv_sync_screen,
  NULL,
};

const struct renderer renderer_yuv2 =
{
  "overlay2",
  NULL,
  yuv_init_video,
  yuv_free_video,
  yuv2_set_video_mode,
  yuv2_set_video_mode,
  NULL,
  set_window_viewport_scaled,
  sdl_set_window_caption,
  sdl_set_window_icon,
  NULL,
  sdl_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  yuv_render_graph,
  NULL,
  yuv_render_cursor,
  NULL,
  yuv_render_mouse,
  yuv_sync_screen,
  NULL,
};

/* MegaZeux
 *
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

/**
 * Software renderer with SDL hardware accelerated scaling. This renderer takes
 * advantage of texture streaming, as well as texture format optimization that
 * is now performed by `sdlrender_set_video_mode`.
 */

#include <stdlib.h>

#include "../render.h"
#include "../render_layer.h"
#include "../renderers.h"
#include "render_sdl.h"

#include "../../util.h"

struct softscale_render_data
{
  struct sdl_render_data sdl;
  SDL_Rect texture_rect;
  void *private_pixels;
  size_t private_pitch;
  uint32_t *texture_pixels;
  unsigned int texture_pitch;
  unsigned int texture_width;
  boolean enable_subsampling;
};

static void softscale_free_video(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;

  if(render_data)
  {
    sdl_destruct_window(graphics);

    graphics->render_data = NULL;
    free(render_data->private_pixels);
    free(render_data);
  }
}

static boolean softscale_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct softscale_render_data *render_data = (struct softscale_render_data *)
   cmalloc(sizeof(struct softscale_render_data));
  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct softscale_render_data));
  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

  // This renderer can technically do 8bpp indexed or RGB332. The former is
  // too slow to be worthwhile and the latter requires fixing assumptions in
  // the layer renderer and a new set_colors8 (also, it looks awful).
  if(conf->force_bpp == BPP_AUTO || conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  snprintf(graphics->sdl_render_driver, ARRAY_SIZE(graphics->sdl_render_driver),
   "%s", conf->sdl_render_driver);

  return true;
}

static boolean softscale_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct softscale_render_data *render_data =
   (struct softscale_render_data *)graphics->render_data;
  SDL_Texture *tex;
  void *tmp;

  if(!sdl_create_window_renderer(graphics, window, false))
    return false;

  // YUV texture modes are effectively 16-bit to SDL, but MegaZeux treats them
  // like 32-bit most of the time. This means each MZX pixel needs two YUV
  // pixels when subsampling is off.
  render_data->texture_width = SCREEN_PIX_W;
  if(render_data->sdl.rgb_to_yuv)
    render_data->texture_width <<= 1;

  render_data->texture_pixels = NULL;

  if(!render_data->sdl.use_texture_streaming)
  {
    render_data->private_pitch = render_data->texture_width *
     SDL_BYTESPERPIXEL(render_data->sdl.texture_format);

    tmp = crealloc(render_data->private_pixels, render_data->private_pitch * SCREEN_PIX_H);
    if(!tmp)
      goto err_free;

    render_data->private_pixels = tmp;
  }

  // Initialize the screen texture.
  tex = SDL_CreateTexture(render_data->sdl.renderer, render_data->sdl.texture_format,
   SDL_TEXTUREACCESS_STREAMING, render_data->texture_width, SCREEN_PIX_H);
  if(!tex)
  {
    warn("Failed to create texture: %s\n", SDL_GetError());
    goto err_free;
  }
  render_data->sdl.texture[0] = tex;

  sdl_set_texture_scale_mode(graphics, window, 0, true);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static boolean softscale_resize_callback(struct graphics_data *graphics,
 struct video_window *window)
{
  sdl_set_texture_scale_mode(graphics, window, 0, true);
  return true;
}

/**
 * Prepare the texture for streaming. This is a lot faster than handling a
 * separate pixels buffer. If the texture is already locked, only return the
 * stored pixels and pitch.
 */
static void softscale_lock_texture(struct softscale_render_data *render_data,
 boolean is_render_graph, uint32_t **pixels, unsigned int *pitch, unsigned int *bpp)
{
  if(!render_data->texture_pixels)
  {
    SDL_Rect *texture_rect = &(render_data->texture_rect);
    void *pixels;
    int pitch;

    texture_rect->x = 0;
    texture_rect->y = 0;
    texture_rect->w = render_data->texture_width;
    texture_rect->h = SCREEN_PIX_H;

    if(is_render_graph && render_data->sdl.allow_subsampling)
    {
      // Chroma subsampling can be used to draw half as much at the cost of
      // color accuracy. This trick only works with render_graph.
      texture_rect->w = render_data->texture_width / 2;
      render_data->enable_subsampling = true;
    }
    else
      render_data->enable_subsampling = false;

    if(render_data->sdl.use_texture_streaming)
    {
      SDL_LockTexture(render_data->sdl.texture[0], texture_rect, &pixels, &pitch);
      render_data->texture_pixels = pixels;
      render_data->texture_pitch = pitch;
    }
    else
    {
      render_data->texture_pixels = render_data->private_pixels;
      render_data->texture_pitch = render_data->private_pitch;
    }
  }

  *pixels = render_data->texture_pixels;
  *pitch = render_data->texture_pitch;
  *bpp = render_data->enable_subsampling ? 16 : render_data->sdl.texture_bpp;
}

/**
 * Commit the newly rendered screen to the texture and invalidate the pixel
 * array pointer.
 */
static void softscale_unlock_texture(struct softscale_render_data *render_data)
{
  if(render_data->texture_pixels)
  {
    if(render_data->sdl.use_texture_streaming)
    {
      SDL_UnlockTexture(render_data->sdl.texture[0]);
    }
    else
    {
      SDL_UpdateTexture(render_data->sdl.texture[0],
       &render_data->texture_rect, render_data->texture_pixels,
       render_data->texture_pitch);
    }
    render_data->texture_pixels = NULL;
  }
}

static void softscale_render_graph(struct graphics_data *graphics)
{
  struct softscale_render_data *render_data = graphics->render_data;
  int mode = graphics->screen_mode;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, true, &pixels, &pitch, &bpp);

  if(render_data->enable_subsampling)
  {
    if(!mode)
      render_graph16((uint16_t *)pixels, pitch, graphics,
       render_data->sdl.subsample_set_colors);
    else
      render_graph16((uint16_t *)pixels, pitch, graphics, set_colors32[mode]);
  }
  else

  if(bpp == 16)
    render_graph16((uint16_t *)pixels, pitch, graphics, set_colors16[mode]);
  else

  if(bpp == 32)
  {
    if(!mode)
      render_graph32(pixels, pitch, graphics);
    else
      render_graph32s(pixels, pitch, graphics);
  }
}

static void softscale_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);
  render_layer(pixels, SCREEN_PIX_W, SCREEN_PIX_H, pitch, bpp, graphics, layer);
}

static void softscale_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t flatcolor;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  flatcolor = graphics->flat_intensity_palette[color];

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);

  if(bpp == 16 && !render_data->enable_subsampling)
    flatcolor |= flatcolor << 16;

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
}

static void softscale_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct softscale_render_data *render_data = graphics->render_data;
  uint32_t amask = render_data->sdl.texture_amask;
  uint32_t *pixels;
  unsigned int pitch;
  unsigned int bpp;

  softscale_lock_texture(render_data, false, &pixels, &pitch, &bpp);

  if(bpp == 16)
    amask |= amask << 16;

  render_mouse(pixels, pitch, bpp, x, y, 0xFFFFFFFF, amask, w, h);
}

static void softscale_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct softscale_render_data *render_data = graphics->render_data;
  SDL_Renderer *renderer = render_data->sdl.renderer;
  SDL_Texture *texture = render_data->sdl.texture[0];
  SDL_Rect *texture_rect = &(render_data->texture_rect);
  SDL_Rect_mzx src_rect;
  SDL_Rect_mzx dest_rect;

  src_rect = sdl_render_rect(texture_rect->x, texture_rect->y,
   texture_rect->w, texture_rect->h);

  dest_rect.x = window->viewport_x;
  dest_rect.y = window->viewport_y;
  dest_rect.w = window->viewport_width;
  dest_rect.h = window->viewport_height;

  softscale_unlock_texture(render_data);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(renderer);

  SDL_RenderTexture(renderer, texture, &src_rect, &dest_rect);
  SDL_RenderPresent(renderer);
}

static const char * const softscale_aliases[] =
{
  "overlay1",
  "overlay2",
  NULL
};

const struct renderer renderer_softscale =
{
  "softscale",
  softscale_aliases,
  softscale_init_video,
  softscale_free_video,
  softscale_create_window,
  sdl_resize_window,
  softscale_resize_callback,
  set_window_viewport_scaled,
  sdl_set_window_caption,
  sdl_set_window_icon,
  NULL,
  sdl_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  softscale_render_graph,
  softscale_render_layer,
  softscale_render_cursor,
  NULL,
  softscale_render_mouse,
  softscale_sync_screen,
  NULL,
};

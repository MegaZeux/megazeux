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

#include "render_sdl.h"
#include "../render.h"
#include "../renderers.h"

#include "../../platform/platform.h"

#include <stdlib.h>
#include <string.h>

struct gp2x_render_data
{
  struct sdl_render_data sdl;
  uint32_t halfmask;
  uint16_t buffer[320*350];
};

static SDL_Surface *
gp2x_get_screen_surface(struct gp2x_render_data *render_data)
{
  return render_data->sdl.shadow ?
   render_data->sdl.shadow : render_data->sdl.screen;
}

static void gp2x_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  uint32_t cb_bg, cb_fg, cb_mx;

  cb_bg = graphics->flat_intensity_palette[bg];
  cb_fg = graphics->flat_intensity_palette[fg];
  if(cb_bg == cb_fg)
    cb_mx = cb_bg;
  else
    cb_mx = ((cb_bg >> 1) & render_data->halfmask) +
     ((cb_fg >> 1) & render_data->halfmask);

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (cb_bg << 16) | cb_bg;
  char_colors[1] = (cb_bg << 16) | cb_mx;
  char_colors[2] = (cb_bg << 16) | cb_mx;
  char_colors[3] = (cb_bg << 16) | cb_fg;
  char_colors[4] = (cb_mx << 16) | cb_bg;
  char_colors[5] = (cb_mx << 16) | cb_mx;
  char_colors[6] = (cb_mx << 16) | cb_mx;
  char_colors[7] = (cb_mx << 16) | cb_fg;
  char_colors[8] = (cb_mx << 16) | cb_bg;
  char_colors[9] = (cb_mx << 16) | cb_mx;
  char_colors[10] = (cb_mx << 16) | cb_mx;
  char_colors[11] = (cb_mx << 16) | cb_fg;
  char_colors[12] = (cb_fg << 16) | cb_bg;
  char_colors[13] = (cb_fg << 16) | cb_mx;
  char_colors[14] = (cb_fg << 16) | cb_mx;
  char_colors[15] = (cb_fg << 16) | cb_fg;
#else
  char_colors[0] = (cb_bg << 16) | cb_bg;
  char_colors[1] = (cb_mx << 16) | cb_bg;
  char_colors[2] = (cb_mx << 16) | cb_bg;
  char_colors[3] = (cb_fg << 16) | cb_bg;
  char_colors[4] = (cb_bg << 16) | cb_mx;
  char_colors[5] = (cb_mx << 16) | cb_mx;
  char_colors[6] = (cb_mx << 16) | cb_mx;
  char_colors[7] = (cb_fg << 16) | cb_mx;
  char_colors[8] = (cb_bg << 16) | cb_mx;
  char_colors[9] = (cb_mx << 16) | cb_mx;
  char_colors[10] = (cb_mx << 16) | cb_mx;
  char_colors[11] = (cb_fg << 16) | cb_mx;
  char_colors[12] = (cb_bg << 16) | cb_fg;
  char_colors[13] = (cb_mx << 16) | cb_fg;
  char_colors[14] = (cb_mx << 16) | cb_fg;
  char_colors[15] = (cb_fg << 16) | cb_fg;
#endif
}

static void gp2x_set_colors_smzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg)
{
  const uint8_t *indices = graphics->smzx_indices;
  uint32_t bb, bf, fb, ff;
  bg &= 0x0F;
  fg &= 0x0F;
  indices += ((bg << 4) | fg) << 2;

  bb = graphics->flat_intensity_palette[indices[0]];
  bf = graphics->flat_intensity_palette[indices[1]];
  fb = graphics->flat_intensity_palette[indices[2]];
  ff = graphics->flat_intensity_palette[indices[3]];

#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bb << 16) | bf;
  char_colors[2] = (bb << 16) | fb;
  char_colors[3] = (bb << 16) | ff;
  char_colors[4] = (bf << 16) | bb;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (bf << 16) | fb;
  char_colors[7] = (bf << 16) | ff;
  char_colors[8] = (fb << 16) | bb;
  char_colors[9] = (fb << 16) | bf;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (fb << 16) | ff;
  char_colors[12] = (ff << 16) | bb;
  char_colors[13] = (ff << 16) | bf;
  char_colors[14] = (ff << 16) | fb;
  char_colors[15] = (ff << 16) | ff;
#else
  char_colors[0] = (bb << 16) | bb;
  char_colors[1] = (bf << 16) | bb;
  char_colors[2] = (fb << 16) | bb;
  char_colors[3] = (ff << 16) | bb;
  char_colors[4] = (bb << 16) | bf;
  char_colors[5] = (bf << 16) | bf;
  char_colors[6] = (fb << 16) | bf;
  char_colors[7] = (ff << 16) | bf;
  char_colors[8] = (bb << 16) | fb;
  char_colors[9] = (bf << 16) | fb;
  char_colors[10] = (fb << 16) | fb;
  char_colors[11] = (ff << 16) | fb;
  char_colors[12] = (bb << 16) | ff;
  char_colors[13] = (bf << 16) | ff;
  char_colors[14] = (fb << 16) | ff;
  char_colors[15] = (ff << 16) | ff;
#endif
}

static const set_colors_function gp2x_set_colors[4] =
{
  gp2x_set_colors_mzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx,
  gp2x_set_colors_smzx
};

static boolean gp2x_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct gp2x_render_data *render_data =
   (struct gp2x_render_data *)cmalloc(sizeof(struct gp2x_render_data));

  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct gp2x_render_data));
  graphics->render_data = render_data;

  graphics->allow_resize = 0;

  graphics->bits_per_pixel = 16;

  graphics->resolution_width = 320;
  graphics->resolution_height = 240;
  graphics->window_width = 320;
  graphics->window_height = 240;
  return true;
}

static void gp2x_free_video(struct graphics_data *graphics)
{
  sdl_destruct_window(graphics);

  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean gp2x_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  const SDL_PixelFormatDetails *format;
  uint32_t halfmask;

  if(!sdl_create_window_soft(graphics, window))
    return false;

  format = render_data->sdl.flat_format;
  halfmask = (format->Rmask >> 1) & format->Rmask;
  halfmask |= (format->Gmask >> 1) & format->Gmask;
  halfmask |= (format->Bmask >> 1) & format->Bmask;
  render_data->halfmask = halfmask;

  return true;
}

static void gp2x_set_window_viewport(struct graphics_data *graphics,
 struct video_window *window)
{
  window->viewport_x = 0;
  window->viewport_y = 0;
  window->viewport_width = 320;
  window->viewport_height = 240;
  window->is_integer_scaled = false;
}

#if 0
static void gp2x_get_screen_coords(struct graphics_data *graphics,
 int screen_x, int screen_y, int *x, int *y, int *min_x, int *min_y,
 int *max_x, int *max_y)
{
  *x = screen_x * 2;
  *y = screen_y * 35 / 24;
  *min_x = 0;
  *min_y = 0;
  *max_x = 319;
  *max_y = 239;
}

static void gp2x_set_screen_coords(struct graphics_data *graphics,
 int x, int y, int *screen_x, int *screen_y)
{
  *screen_x = x / 2;
  *screen_y = y * 24 / 35;
}
#endif

static void gp2x_render_graph(struct graphics_data *graphics)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  render_graph8((uint8_t *)render_data->buffer, 640, graphics,
   gp2x_set_colors[graphics->screen_mode]);
}

static void gp2x_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  uint32_t flatcolor = graphics->flat_intensity_palette[color];
  flatcolor |= flatcolor << 16;
  render_cursor((uint32_t *)render_data->buffer, 640, 8, x, y, flatcolor, lines,
   offset);
}

static void gp2x_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  render_mouse((uint32_t *)render_data->buffer, 640, 8, x, y, 0xFFFFFFFF,
   0x0, w, h);
}

static void gp2x_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct gp2x_render_data *render_data = graphics->render_data;
  SDL_Surface *screen = gp2x_get_screen_surface(render_data);

  unsigned int line_advance = screen->pitch / 2;
  uint16_t *dest = (uint16_t *)screen->pixels;
  uint16_t *src = render_data->buffer;
  unsigned int i, j, skip = 0;

  SDL_LockSurface(screen);
  for(i = 0; i < 240; i++)
  {
    skip += 35 - 24;
    if(skip < 24)
      memcpy(dest, src, sizeof(uint16_t) * 320);
    else
    {
      for(j = 0; j < 320; j++)
      {
        if(src[j] == src[j+320])
          dest[j] = src[j];
        else
          dest[j] = ((src[j] >> 1) & render_data->halfmask) +
           ((src[j+320] >> 1) & render_data->halfmask);
      }
      skip -= 24;
      src += 320;
    }
    src += 320;
    dest += line_advance;
  }

  SDL_UnlockSurface(screen);

  if(render_data->sdl.shadow)
  {
    SDL_Rect src_rect;
    SDL_Rect dest_rect;
    SDL_GetSurfaceClipRect(render_data->sdl.shadow, &src_rect);
    SDL_GetSurfaceClipRect(render_data->sdl.screen, &dest_rect);
    SDL_BlitSurface(render_data->sdl.shadow, &src_rect,
     render_data->sdl.screen, &dest_rect);
  }

#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_UpdateWindowSurface(render_data->sdl.window);
#else
  SDL_Flip(render_data->sdl.screen);
#endif
}

const struct renderer renderer_gp2x =
{
  "gp2x",
  NULL,
  gp2x_init_video,
  gp2x_free_video,
  gp2x_create_window,
  gp2x_create_window,
  NULL,
  gp2x_set_window_viewport,
  sdl_set_window_caption,
  sdl_set_window_icon,
  NULL,
  sdl_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  gp2x_render_graph,
  NULL,
  gp2x_render_cursor,
  NULL,
  gp2x_render_mouse,
  gp2x_sync_screen,
  NULL,
};

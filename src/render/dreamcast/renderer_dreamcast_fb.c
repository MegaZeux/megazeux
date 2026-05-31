/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018 Adrian Siekierka <kontakt@asie.pl>
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
#include "../render_layer.h"
#include "../renderers.h"
#include "../../util.h"

#include <kos.h>

struct dc_fb_render_data
{
};

static boolean dc_fb_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct dc_fb_render_data render_data;

  vid_init(DM_640x480 | DM_MULTIBUFFER, PM_RGB565);

  memset(&render_data, 0, sizeof(struct dc_fb_render_data));
  graphics->render_data = &render_data;

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 16;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;
  return true;
}

static inline uint16_t *dc_fb_vram_ptr()
{
  return vram_s + (640 * 65); // 0,65 - 639,415
}

static void dc_fb_free_video(struct graphics_data *graphics)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
}

static boolean dc_fb_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;
}

static void dc_fb_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned count)
{
  unsigned i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
       ((palette[i].r >> 3) << 11)
     | ((palette[i].g >> 2) << 5)
     | (palette[i].b >> 3);
  }
}

static void dc_fb_render_graph(struct graphics_data *graphics)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
  render_graph16(dc_fb_vram_ptr(), 640 * 2, graphics,
   set_colors16[graphics->screen_mode]);
}

static void dc_fb_render_layer(struct graphics_data *graphics,
 struct video_layer *vlayer)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
  render_layer(dc_fb_vram_ptr(), 640, 350, 640 * 2, 16, graphics, vlayer);
}

static void dc_fb_render_cursor(struct graphics_data *graphics,
 unsigned x, unsigned y, uint16_t color, unsigned lines, unsigned offset)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
  uint32_t flatcolor = graphics->flat_intensity_palette[color] * 0x10001;

  render_cursor((uint32_t *)dc_fb_vram_ptr(), 640 * 2, 16, x, y,
   flatcolor, lines, offset);
}

static void dc_fb_render_mouse(struct graphics_data *graphics,
 unsigned x, unsigned y, unsigned w, unsigned h)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
  render_mouse((uint32_t *)dc_fb_vram_ptr(), 640 * 2, 16, x, y,
   0xFFFFFFFF, 0, w, h);
}

static void dc_fb_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
//  struct dc_fb_render_data *render_data = graphics->render_data;
  vid_flip(-1);
}

const struct renderer renderer_dreamcast_fb =
{
  "dreamcast_fb",
  NULL,
  dc_fb_init_video,
  dc_fb_free_video,
  dc_fb_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  dc_fb_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  dc_fb_render_graph,
  dc_fb_render_layer,
  dc_fb_render_cursor,
  NULL,
  dc_fb_render_mouse,
  dc_fb_sync_screen,
  NULL,
};

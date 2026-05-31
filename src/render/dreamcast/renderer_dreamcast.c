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

/*
 * Eventually, this file will house a PVR-accelerated renderer.
 *
 * TODO
 */

struct dc_render_data
{
  pvr_ptr_t texture;
  pvr_poly_hdr_t header;
};


static pvr_init_params_t pvr_params =
{
  { PVR_BINSIZE_8, PVR_BINSIZE_0, PVR_BINSIZE_8, PVR_BINSIZE_0, PVR_BINSIZE_0 },
  32 * 32768,
  0, 0, 1, 0
};

static boolean dc_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct dc_render_data render_data;
  pvr_poly_cxt_t cxt;

  pvr_init(&pvr_params);
  pvr_set_bg_color(0.0f, 0.0f, 0.0f);

  memset(&render_data, 0, sizeof(struct dc_render_data));
  graphics->render_data = &render_data;

  render_data.texture = pvr_mem_malloc(1024 * 512 * 2);

  // generate polygon header
  pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_NONTWIDDLED | PVR_TXRFMT_RGB565,
   1024, 512, render_data.texture, PVR_FILTER_NEAREST);
  pvr_poly_compile(&(render_data.header), &cxt);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = 16;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;
  return true;
}

static void dc_free_video(struct graphics_data *graphics)
{
  struct dc_render_data *render_data = graphics->render_data;
  pvr_mem_free(render_data->texture);
}

static boolean dc_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;
}

static void dc_update_colors(struct graphics_data *graphics,
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

static void dc_render_graph(struct graphics_data *graphics)
{
  struct dc_render_data *render_data = graphics->render_data;
  render_graph16(render_data->texture, 1024 * 2, graphics,
   set_colors16[graphics->screen_mode]);
}

static void dc_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct dc_render_data *render_data = graphics->render_data;
  render_layer(render_data->texture, SCREEN_PIX_W, SCREEN_PIX_H,
   1024 * 2, 16, graphics, layer);
}

static void dc_render_cursor(struct graphics_data *graphics,
 unsigned x, unsigned y, uint16_t color, unsigned lines, unsigned offset)
{
  struct dc_render_data *render_data = graphics->render_data;
  uint32_t flatcolor = graphics->flat_intensity_palette[color] * 0x10001;

  render_cursor((uint32_t *)render_data->texture, 1024 * 2, 16, x, y,
   flatcolor, lines, offset);
}

static void dc_render_mouse(struct graphics_data *graphics,
 unsigned x, unsigned y, unsigned w, unsigned h)
{
  struct dc_render_data *render_data = graphics->render_data;
  render_mouse((uint32_t *)render_data->texture, 1024 * 2, 16, x, y,
   0xFFFFFFFF, 0, w, h);
}

static void dc_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct dc_render_data *render_data = graphics->render_data;
  pvr_vertex_t vtx;

  pvr_wait_ready();
  pvr_scene_begin();
  pvr_list_begin(PVR_LIST_OP_POLY);

  pvr_prim(&(render_data->header), sizeof(pvr_poly_hdr_t));

  vtx.argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
  vtx.oargb = 0;
  vtx.flags = PVR_CMD_VERTEX;
  vtx.z = 1;

  vtx.x = 1;
  vtx.y = 1;
  vtx.u = 0;
  vtx.v = 0;
  pvr_prim(&vtx, sizeof(vtx));

  vtx.x = 640;
  vtx.y = 1;
  vtx.u = 640.0 / 1024.0;
  vtx.v = 0;
  pvr_prim(&vtx, sizeof(vtx));

  vtx.x = 1;
  vtx.y = 480;
  vtx.u = 0;
  vtx.v = 350.0 / 512.0;
  pvr_prim(&vtx, sizeof(vtx));

  vtx.x = 640;
  vtx.y = 480;
  vtx.u = 640.0 / 1024.0;
  vtx.v = 350.0 / 512.0;
  vtx.flags = PVR_CMD_VERTEX_EOL;
  pvr_prim(&vtx, sizeof(vtx));

  pvr_list_finish();
  pvr_scene_finish();
}

const struct renderer renderer_dreamcast =
{
  "dreamcast",
  NULL,
  dc_init_video,
  dc_free_video,
  dc_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  dc_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  dc_render_graph,
  dc_render_layer,
  dc_render_cursor,
  NULL,
  dc_render_mouse,
  dc_sync_screen,
  NULL,
};

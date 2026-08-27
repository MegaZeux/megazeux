/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018, 2019 Adrian Siekierka <kontakt@asie.pl>
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#define delay delay_dos
#include <sys/nearptr.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#undef delay

#include "render_djgpp.h"
#include "../render.h"
#include "../render_layer.h"
#include "../renderers.h"

#include "../../platform/log.h"
#include "../../platform/djgpp/platform_djgpp.h"

struct svga_render_data
{
  __dpmi_meminfo mapping;
  int mapping_type;
  uint8_t *ptr;
  uint8_t *ptr2;
  uint16_t line;
  uint16_t line2;
  uint16_t mode;
  uint16_t pitch;
  uint8_t display;
  boolean page_flip_ok;
  int update_colors;
};

static boolean svga_try_mode(struct graphics_data *graphics, uint16_t mode,
 uint16_t width, uint16_t height, uint16_t bpp)
{
  struct svga_render_data *render_data = graphics->render_data;
  __dpmi_regs reg;

  reg.x.ax = 0x4F02; // set video mode
  reg.x.bx = mode;
  __dpmi_int(0x10, &reg);

  if(reg.x.ax != 0x004F)
    return false;

  graphics->resolution_width = graphics->window_width = width;
  graphics->resolution_height = graphics->window_height = height;
  graphics->bits_per_pixel = bpp;
  render_data->mode = mode;

  return true;
}

static boolean svga_try_modes(struct graphics_data *graphics)
{
  if(graphics->bits_per_pixel >= 16)
  {
    // 640x480x16 with LFB
    if(svga_try_mode(graphics, 0x4111, 640, 480, 16))
      return true;
  }

  if(graphics->bits_per_pixel >= 8)
  {
    // 640x400x8 with LFB
    if(svga_try_mode(graphics, 0x4100, 640, 400, 8))
      return true;
    // 640x480x8 with LFB
    if(svga_try_mode(graphics, 0x4101, 640, 480, 8))
      return true;
  }
  return false;
}

static boolean svga_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct svga_render_data render_data;
  struct vbe_mode_info vbe;
  int x_offset, y_offset;
  __dpmi_regs reg;
  uint32_t size;

  memset(&render_data, 0, sizeof(struct svga_render_data));
  graphics->render_data = &render_data;

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = conf->force_bpp;
  if(graphics->bits_per_pixel != 8 && graphics->bits_per_pixel != 16)
    graphics->bits_per_pixel = 16;

  render_data.display = djgpp_display_adapter_detect();
  if(render_data.display < DISPLAY_ADAPTER_VBE20)
  {
    warn("Could not find VBE 2.0+-compatible graphics card!\n");
    return false;
  }

  if(!svga_try_modes(graphics))
  {
    warn("Could not find supported VESA graphics mode!\n");
    return false;
  }

  reg.x.ax = 0x4F01; // get video mode information
  reg.x.cx = render_data.mode;
  reg.x.di = __tb & 0xF; // transfer block
  reg.x.es = (__tb >> 4);
  __dpmi_int(0x10, &reg);

  if(reg.x.ax != 0x004F)
  {
    warn("Could not query VESA graphics mode!\n");
    return false;
  }

  dosmemget(__tb, sizeof(struct vbe_mode_info), &vbe);
  djgpp_print_vbe_mode_info(&vbe);

  /* VBE image pages field indicates the total number, minus 1, of complete
   * frames that will fit into video memory. If it's >=1, at least 2 images
   * will fit and double buffering can be enabled. */
  if(vbe.image_pages >= 1)
    render_data.page_flip_ok = true;

  size = graphics->resolution_height * vbe.pitch;
  if(render_data.page_flip_ok)
    size <<= 1;

  render_data.ptr = (uint8_t *)djgpp_map_physical_memory(
   vbe.linear_ptr, size, &render_data.mapping, &render_data.mapping_type);
  if(!render_data.ptr)
  {
    warn("Could not map VESA video memory! (requested %ld bytes at %08lX for mode %04X)\n",
     render_data.mapping.size, render_data.mapping.address, render_data.mode);
    return false;
  }
  memset(render_data.ptr, 0, size);

  x_offset = (graphics->resolution_width - SCREEN_PIX_W) / 2;
  y_offset = (graphics->resolution_height - SCREEN_PIX_H) / 2;

  render_data.ptr += (vbe.pitch * y_offset) + ((vbe.bpp >> 3) * x_offset);

  render_data.pitch = vbe.pitch;
  if(render_data.page_flip_ok)
  {
    render_data.ptr2 = render_data.ptr + (graphics->resolution_height * vbe.pitch);
    render_data.line = 0;
    render_data.line2 = graphics->resolution_height;
  }

  return true;
}

static void svga_free_video(struct graphics_data *graphics)
{
  struct svga_render_data *render_data = graphics->render_data;
  djgpp_unmap_physical_memory(&render_data->mapping, &render_data->mapping_type);
  /* Screen mode will be cleaned up on exit. */
}

static boolean svga_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;
}

static void svga_upload_colors(uint32_t *palette, uint32_t count)
{
  __dpmi_regs reg;

  if(count > 256)
    count = 256;
  dosmemput(palette, count << 2, __tb);

  reg.x.ax = 0x4F08; // DAC palette width
  reg.x.bx = 0x0600; // ... set 6 bits per color
  __dpmi_int(0x10, &reg);

  reg.x.ax = 0x4F09; // DAC palette entries
  reg.x.bx = 0x0080; // ... set on next retrace
  reg.x.cx = count;
  reg.x.dx = 0;
  reg.x.di = __tb & 0xF;
  reg.x.es = (__tb >> 4);
  __dpmi_int(0x10, &reg);

  if(reg.x.ax != 0x004F)
  {
    reg.h.bl = 0x00;
    __dpmi_int(0x10, &reg);
  }
}

static void svga_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct svga_render_data *render_data = graphics->render_data;
  uint32_t i;

  if(graphics->bits_per_pixel == 16)
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
          ((palette[i].r & 0xF8) << 8)
        | ((palette[i].g & 0xFC) << 3)
        | ((palette[i].b & 0xF8) >> 3);
    }
  }
  else
  {
    for(i = 0; i < count; i++)
    {
      graphics->flat_intensity_palette[i] =
          ((palette[i].r & 0xFC) << 14)
        | ((palette[i].g & 0xFC) << 6)
        | ((palette[i].b & 0xFC) >> 2);
    }
    render_data->update_colors = count;
  }
}

static void svga_render_graph(struct graphics_data *graphics)
{
  struct svga_render_data *render_data = graphics->render_data;
  if(graphics->bits_per_pixel == 16)
  {
    render_graph16((uint16_t *)render_data->ptr, render_data->pitch, graphics,
     set_colors16[graphics->screen_mode]);
  }
  else

  if(graphics->bits_per_pixel == 8)
  {
    render_graph8((uint8_t *)render_data->ptr, render_data->pitch, graphics,
     set_colors8[graphics->screen_mode]);
  }
}

static void svga_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct svga_render_data *render_data = graphics->render_data;
  render_layer(render_data->ptr, SCREEN_PIX_W, SCREEN_PIX_H,
   render_data->pitch, graphics->bits_per_pixel, graphics, layer);
}

static void svga_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct svga_render_data *render_data = graphics->render_data;
  uint32_t flatcolor = 0;

  if(graphics->bits_per_pixel == 16)
  {
    flatcolor = graphics->flat_intensity_palette[color] * 0x00010001;
  }
  else

  if(graphics->bits_per_pixel == 8)
    flatcolor = color * 0x01010101;

  render_cursor((uint32_t *)render_data->ptr, render_data->pitch,
   graphics->bits_per_pixel, x, y, flatcolor, lines, offset);
}

static void svga_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct svga_render_data *render_data = graphics->render_data;
  uint32_t mask;

  if((graphics->bits_per_pixel == 8) && !graphics->screen_mode)
    mask = 0x0F0F0F0F;
  else
    mask = 0xFFFFFFFF;

  render_mouse((uint32_t *)render_data->ptr, render_data->pitch,
   graphics->bits_per_pixel, x, y, mask, 0, w, h);
}

static void svga_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct svga_render_data *render_data = graphics->render_data;
  __dpmi_regs reg;
  uint8_t *ptr;
  uint16_t line;

  if(render_data->update_colors > 0)
  {
    svga_upload_colors(graphics->flat_intensity_palette, render_data->update_colors);
    render_data->update_colors = 0;
  }

  if(render_data->page_flip_ok)
  {
    reg.x.ax = 0x4F07; // set display start
    reg.x.bx = 0x0080; // ...during vertical retrace
    reg.x.cx = 0;
    reg.x.dx = render_data->line;

    __dpmi_int(0x10, &reg);
    if(reg.x.ax != 0x004F)
    {
      render_data->page_flip_ok = false;
      return;
    }

    ptr = render_data->ptr;
    render_data->ptr = render_data->ptr2;
    render_data->ptr2 = ptr;

    line = render_data->line;
    render_data->line = render_data->line2;
    render_data->line2 = line;
  }
}

const struct renderer renderer_svga =
{
  "svga",
  NULL,
  svga_init_video,
  svga_free_video,
  svga_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  svga_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  svga_render_graph,
  svga_render_layer,
  svga_render_cursor,
  NULL,
  svga_render_mouse,
  svga_sync_screen,
  NULL,
};

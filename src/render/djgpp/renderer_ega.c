/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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
#include <stdlib.h>
#include <string.h>
#include <pc.h>
#include <dos.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <sys/segments.h>
#include <sys/movedata.h>
#undef delay

#include "../render.h"
#include "../renderers.h"

#include "../../platform/log.h"
#include "../../platform/djgpp/platform_djgpp.h"

// Address: 0x000B8000
// Limit  : 0x03FFF
// Type   : 0x3
// Flags  : Application, Ring 3, Present, 32-bit
static uint8_t ega_vb_desc[8] =
{
  0xFF, 0x3F, 0x00, 0x80, 0x0B, 0xF3, 0x40, 0x00
};

/*
// Address: 0x000A0000
// Limit  : 0x0FFFF
// Type   : 0x3
// Flags  : Application, Ring 3, Present, 32-bit
static const uint8_t ega_vb_desc[8] =
{
  0xFF, 0xFF, 0x00, 0x00, 0x0A, 0xF3, 0x40, 0x00
}
*/

static const unsigned long ega_vb_page[4] =
{
  0x0000,
  0x1000,
  0x2000,
  0x3000
};

#define TEXT_FLAGS_CHR 1
#define TEXT_FLAGS_VGA 2

struct ega_render_data
{
  unsigned char page;
  unsigned char smzx_swap_nibbles;
  unsigned char flags;
  unsigned char oldmode;
  unsigned short vbsel;
  unsigned char curpages;
  boolean is_ati_card;
  uint8_t lines;
  uint8_t offset;
  uint32_t x;
  uint32_t y;
};

static void ega_set_14p(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1201;
  reg.h.bl = 0x30;
  __dpmi_int(0x10, &reg);
}

static void ega_set_page(int page)
{
  __dpmi_regs reg;
  reg.x.ax = 0x0500 | page;
  __dpmi_int(0x10, &reg);
}

static void ega_set_smzx(boolean is_ati)
{
  // Super MegaZeux mode:
  // In a nutshell, this sets bit 6 of the VGA Mode Control Register.
  // Bit 6 controls the pixel width - if 1, the pixel width is doubled,
  // creating one 8-bit pixel instead of two 4-bit pixels. HOWEVER,
  // normally, this is only done in Mode 13h.
  //
  // nVidia and some Cirrus Logic cards support this; ATI cards
  // also "support" it, but swap the order of joining the pixels
  // and require a weird horizontal pixel shift value - see below.
  outportb(0x3C0, 0x10);
  outportb(0x3C0, 0x4C);

  if(is_ati)
  {
    // set horizontal pixel shift to Undefined (0.5 pixels, in theory)
    outportb(0x3C0, 0x13);
    outportb(0x3C0, 0x01);
  }
}

static void ega_set_16p(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1202;
  reg.h.bl = 0x30;
  __dpmi_int(0x10, &reg);
}

static void ega_blink_on(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1003;
  reg.h.bl = 0x01;
  __dpmi_int(0x10, &reg);
}

static void ega_blink_off(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1003;
  reg.h.bl = 0x00;
  __dpmi_int(0x10, &reg);
}

static void ega_cursor_off(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x0103;
  reg.x.cx = 0x3F00;
  __dpmi_int(0x10, &reg);
}

static void ega_set_cursor_shape(uint8_t lines, uint8_t offset)
{
  __dpmi_regs reg;
  if(!lines)
  {
    ega_cursor_off();
    return;
  }
  reg.x.ax = 0x0103;
  reg.h.ch = offset * 8 / 14;
  reg.h.cl = (offset + lines) * 8 / 14 - 1;
  __dpmi_int(0x10, &reg);
}

static void ega_set_cursor_pos(int page, uint32_t x, uint32_t y)
{
  __dpmi_regs reg;
  reg.h.ah = 0x02;
  reg.h.bh = page;
  reg.h.dh = y;
  reg.h.dl = x;
  __dpmi_int(0x10, &reg);
}

static unsigned char ega_get_mode(void)
{
  __dpmi_regs reg;

  reg.h.ah = 0x0F;
  __dpmi_int(0x10, &reg);
  return reg.h.al & 0x7F;
}

static void ega_set_mode(unsigned char mode)
{
  __dpmi_regs reg;
  reg.h.ah = 0x00;
  reg.h.al = mode;
  __dpmi_int(0x10, &reg);
}

static void ega_bank_char(void)
{
  outportb(0x03CE, 0x05);
  outportb(0x03CF, 0x00);
  outportb(0x03CE, 0x06);
  outportb(0x03CF, 0x0C);
  outportb(0x03C4, 0x04);
  outportb(0x03C5, 0x06);
  outportb(0x03C4, 0x02);
  outportb(0x03C5, 0x04);
  outportb(0x03CE, 0x04);
  outportb(0x03CF, 0x02);
}

static void ega_bank_text(void)
{
  outportb(0x03CE, 0x05);
  outportb(0x03CF, 0x10);
  outportb(0x03CE, 0x06);
  outportb(0x03CF, 0x0E);
  outportb(0x03C4, 0x04);
  outportb(0x03C5, 0x02);
  outportb(0x03C4, 0x02);
  outportb(0x03C5, 0x03);
  outportb(0x03CE, 0x04);
  outportb(0x03CF, 0x00);
}

static void ega_vsync(void)
{
  while(inportb(0x03DA) & 0x08)
    ;
  while(!(inportb(0x03DA) & 0x08))
    ;
}

static boolean ega_is_ati_card(void)
{
  // TODO: untested.
  char ati_magic[9];
  dosmemget(0xC0031, 9, ati_magic);
  return memcmp("761295520", ati_magic, 9) == 0;
}

static boolean ega_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct ega_render_data *render_data =
   (struct ega_render_data *)cmalloc(sizeof(struct ega_render_data));
  int display, sel;

  if(!render_data)
    return false;

  display = djgpp_display_adapter_detect();
  if(display == DISPLAY_ADAPTER_UNSUPPORTED)
  {
    warn("Could not find EGA-compatible graphics card!");
    free(render_data);
    return false;
  }

  sel = __dpmi_allocate_ldt_descriptors(1);
  if(__dpmi_set_descriptor(sel, ega_vb_desc) < 0)
  {
    warn("Failed to create VRAM selector.");
    free(render_data);
    return false;
  }
  render_data->vbsel = sel;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;
  graphics->bits_per_pixel = 1;

  if(display >= DISPLAY_ADAPTER_VGA)
    render_data->flags = TEXT_FLAGS_VGA;
  else
    render_data->flags = 0;

  render_data->oldmode = ega_get_mode();
  render_data->is_ati_card = ega_is_ati_card();
  render_data->smzx_swap_nibbles = render_data->is_ati_card;

  graphics->render_data = render_data;
  return true;
}

static void ega_free_video(struct graphics_data *graphics)
{
  struct ega_render_data *render_data = graphics->render_data;
  if(render_data->flags & TEXT_FLAGS_VGA)
    ega_set_16p();
  ega_set_mode(render_data->oldmode);
  ega_blink_on();
  __dpmi_free_ldt_descriptor(render_data->vbsel);
  free(render_data);
}

static boolean ega_set_screen_mode(struct graphics_data *graphics, unsigned mode)
{
  struct ega_render_data *render_data = graphics->render_data;
  int i;

  render_data->page = 0;
  render_data->lines = 255;
  render_data->offset = 255;
  render_data->x = 65535;
  render_data->y = 65535;

  if(render_data->flags & TEXT_FLAGS_VGA)
    ega_set_14p();

  ega_set_mode(0x03);
  if(mode > 0)
    ega_set_smzx(render_data->is_ati_card);
  ega_blink_off();
  ega_cursor_off();

  // If VGA, set the EGA palette to point to first 16 VGA palette entries
  if(render_data->flags & TEXT_FLAGS_VGA)
  {
    ega_vsync();
    for(i = 0; i < 16; i++)
    {
      outportb(0x03C0, i);
      outportb(0x03C0, i);
    }
    // Get attribute controller back to normal
    outportb(0x03C0, 0x20);
  }

  render_data->flags |= TEXT_FLAGS_CHR;

  // Unless someone makes a cursed VGA card specifically for MegaZeux,
  // mode 3 does not exist in hardware and never has.
  if(mode > 2)
    return false;

  return true;
}

static boolean ega_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  ega_set_screen_mode(graphics, graphics->screen_mode);
  return true;
}

static void ega_remap_char_range(struct graphics_data *graphics, uint16_t first, uint16_t count)
{
  struct ega_render_data *render_data = graphics->render_data;
  render_data->flags |= TEXT_FLAGS_CHR;
}

static void ega_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct ega_render_data *render_data = graphics->render_data;
  render_data->flags |= TEXT_FLAGS_CHR;
}

static void ega_remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  struct ega_render_data *render_data = graphics->render_data;
  render_data->flags |= TEXT_FLAGS_CHR;
}

static void ega_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct ega_render_data *render_data = graphics->render_data;
  unsigned int i, j, c, step;

  if(render_data->flags & TEXT_FLAGS_VGA)
  {
    if(graphics->screen_mode && render_data->smzx_swap_nibbles)
    {
      for(i = 0; i < count; i++)
      {
        outportb(0x03C8, (i >> 4) | ((i & 0x0F) << 4));
        outportb(0x03C9, palette[i].r >> 2);
        outportb(0x03C9, palette[i].g >> 2);
        outportb(0x03C9, palette[i].b >> 2);
      }
    }
    else
    {
      for(i = 0; i < count; i++)
      {
        outportb(0x03C8, i);
        outportb(0x03C9, palette[i].r >> 2);
        outportb(0x03C9, palette[i].g >> 2);
        outportb(0x03C9, palette[i].b >> 2);
      }
    }
  }
  else
  {
    if(graphics->screen_mode)
      step = 17;
    else
      step = 1;

    // Reset index/data flip-flop
    inportb(0x03DA);
    for(i = j = 0; i < 16 && j < count; i++, j += step)
    {
      c = (palette[j].b >> 7) & 0x01;
      c |= (palette[j].g >> 6) & 0x02;
      c |= (palette[j].r >> 5) & 0x04;
      c |= (palette[j].b >> 3) & 0x08;
      c |= (palette[j].g >> 2) & 0x10;
      c |= (palette[j].r >> 1) & 0x20;
      outportb(0x03C0, i);
      outportb(0x03C0, c);
    }
    // Get attribute controller back to normal
    outportb(0x03C0, 0x20);
  }
}

static void ega_render_graph(struct graphics_data *graphics)
{
  struct ega_render_data *render_data = graphics->render_data;
  struct char_element *src = graphics->text_video;
  unsigned long dest = ega_vb_page[render_data->page];
  int i;

  _farsetsel(render_data->vbsel);

  for(i = 0; i < (SCREEN_W * SCREEN_H); i++, src++, dest += 2)
  {
    _farnspokew(dest, (src->bg_color << 12) | ((src->fg_color & 0x0F) << 8)
                 | (src->char_value & 0xFF));
  }
}

static void ega_hardware_cursor(struct graphics_data *graphics,
 unsigned x, unsigned y, uint16_t color, unsigned lines, unsigned offset, boolean enable)
{
  struct ega_render_data *render_data = graphics->render_data;
  unsigned long dest = ega_vb_page[render_data->page];

  dest += x * 2 + 1;
  dest += y * 160;

  if((lines != render_data->lines) || (offset != render_data->offset))
  {
    ega_set_cursor_shape(lines, offset);
    render_data->lines = lines;
    render_data->offset = offset;
    render_data->curpages = 4;
  }

  if(enable)
  {
    if((x != render_data->x) || (y != render_data->y))
    {
      render_data->x = x;
      render_data->y = y;
      render_data->curpages = 4;
    }
    if(render_data->curpages)
    {
      ega_set_cursor_pos(render_data->page, x, y);
      render_data->curpages--;
    }
    /**
     * Disable setting the cursor color for now. This changes the color of the
     * color of the entire char which is generally not desirable; the point of
     * having a different cursor color than the char color is to make it more
     * visible.
     */
    //_farsetsel(render_data->vbsel);
    //_farnspokeb(dest, (_farnspeekb(dest) & 0xF0) | (color & 0x0F));
  }
}

static void ega_render_mouse(struct graphics_data *graphics,
 unsigned x, unsigned y, unsigned w, unsigned h)
{
  struct ega_render_data *render_data = graphics->render_data;
  unsigned long dest = ega_vb_page[render_data->page];
  dest += x / 8 * 2 + 1;
  dest += y / 14 * 160;
  _farsetsel(render_data->vbsel);
  _farnspokeb(dest, _farnspeekb(dest) ^ 0xFF);
}

static void ega_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct ega_render_data *render_data = graphics->render_data;
  uint8_t *src = graphics->charset;
  unsigned int dest = 0;
  int i;

  if(render_data->flags & TEXT_FLAGS_CHR)
  {
    ega_bank_char();
    for(i = 0; i < 256; i++, src += 14, dest += 32)
      movedata(_my_ds(), (unsigned int)src, render_data->vbsel, dest, 14);
    ega_bank_text();
    render_data->flags &= ~TEXT_FLAGS_CHR;
  }

  // TODO: Character set page flips.
  ega_set_page(render_data->page);
  render_data->page = (render_data->page + 1) & 3;
}

const struct renderer renderer_ega =
{
  "ega",
  NULL,
  ega_init_video,
  ega_free_video,
  ega_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  ega_set_screen_mode,
  ega_update_colors,
  ega_remap_char_range,
  ega_remap_char,
  ega_remap_charbyte,
  NULL,
  ega_render_graph,
  NULL,
  NULL,
  ega_hardware_cursor,
  ega_render_mouse,
  ega_sync_screen,
  NULL,
};

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

#ifndef MEGAZEUX_RENDER_DJGPP_EGA_H
#define MEGAZEUX_RENDER_DJGPP_EGA_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>
#include <dpmi.h>
#include <pc.h>

static inline void ega_set_16p(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1202;
  reg.h.bl = 0x30;
  __dpmi_int(0x10, &reg);
}

static inline void ega_set_14p(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1201;
  reg.h.bl = 0x30;
  __dpmi_int(0x10, &reg);
}

static inline void ega_set_page(int page)
{
  __dpmi_regs reg;
  reg.x.ax = 0x0500 | page;
  __dpmi_int(0x10, &reg);
}

static inline void ega_blink_on(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1003;
  reg.h.bl = 0x01;
  __dpmi_int(0x10, &reg);
}

static inline void ega_blink_off(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x1003;
  reg.h.bl = 0x00;
  __dpmi_int(0x10, &reg);
}

static inline void ega_cursor_off(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x0103;
  reg.x.cx = 0x3F00;
  __dpmi_int(0x10, &reg);
}

static inline void ega_set_cursor_shape(uint8_t lines, uint8_t offset)
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

static inline void ega_set_cursor_pos(int page, uint32_t x, uint32_t y)
{
  __dpmi_regs reg;
  reg.h.ah = 0x02;
  reg.h.bh = page;
  reg.h.dh = y;
  reg.h.dl = x;
  __dpmi_int(0x10, &reg);
}

static inline void ega_reset_cursor(void)
{
  __dpmi_regs reg;
  reg.x.ax = 0x0103;
  reg.h.ch = 11;
  reg.h.cl = 12;
  __dpmi_int(0x10, &reg);
}

static inline unsigned char ega_get_mode(void)
{
  __dpmi_regs reg;

  reg.h.ah = 0x0F;
  __dpmi_int(0x10, &reg);
  return reg.h.al & 0x7F;
}

static inline void ega_set_mode(unsigned char mode)
{
  __dpmi_regs reg;
  reg.h.ah = 0x00;
  reg.h.al = mode;
  __dpmi_int(0x10, &reg);
}

/**
 * Enable Super MegaZeux mode.
 *
 * In a nutshell, this sets bit 6 of the VGA Mode Control Register.
 * Bit 6 controls the pixel width: if 1, the pixel width is doubled,
 * creating one 8-bit pixel from two adjacent 4-bit pixels. HOWEVER,
 * normally, this is only done in Mode 13h.
 *
 * C&T, NVIDIA, early Trident cards, and VIA S3 chips support this;
 * ATI and Oak Technology cards support it but store adjacent 4-bit pixels
 * in little endian order, requiring the palette colors to be written in a
 * different order; additionally, ATI cards shift text mode horizontally and
 * need to be "corrected" with the horizontal shift register.
 *
 * This should be performed immediately after setting the mode, otherwise
 * it will not take effect.
 *
 * @param horizontal_shift_hack  Set horizontal pixel shift to 1 instead of the
 *                               default 0. Only required for ATI cards.
 */
static inline void ega_set_smzx(boolean horizontal_shift_hack)
{
  outportb(0x3C0, 0x10);
  outportb(0x3C0, 0x4C);

  if(horizontal_shift_hack)
  {
    // set horizontal pixel shift to Undefined (0.5 pixels, in theory)
    outportb(0x3C0, 0x13);
    outportb(0x3C0, 0x01);
  }
}

static inline void ega_bank_char(void)
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

static inline void ega_bank_text(void)
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

static inline void ega_vsync(void)
{
  while(inportb(0x03DA) & 0x08)
    ;
  while(!(inportb(0x03DA) & 0x08))
    ;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_RENDER_DJGPP_EGA_H */

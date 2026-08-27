/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
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

#include <inttypes.h>
#include <string.h>

#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>

#include "ega.h"
#include "render_djgpp.h"
#include "../../platform/log.h"

static int old_mode;

static enum display_adapter_type display_adapter_detect(void)
{
  __dpmi_regs reg;

  // VESA SuperVGA BIOS (VBE) - GET SuperVGA INFORMATION
  // Generally supported by SVGA cards
  reg.x.ax = 0x4F00;
  reg.x.di = __tb & 0xF;
  reg.x.es = (__tb >> 4);
  __dpmi_int(0x10, &reg);

  if(reg.x.ax == 0x004F)
  {
    struct vbe_info vbe;
    dosmemget(__tb, sizeof(struct vbe_info), &vbe);
    if(memcmp(vbe.signature, "VESA", 4) == 0)
    {
      if(vbe.version >= 0x300)
        return DISPLAY_ADAPTER_VBE30;
      else

      if(vbe.version >= 0x200)
        return DISPLAY_ADAPTER_VBE20;

      return DISPLAY_ADAPTER_SVGA;
    }
  }

  // VIDEO - GET DISPLAY COMBINATION CODE (PS,VGA/MCGA)
  // Generally supported by VGA cards
  reg.x.ax = 0x1A00;
  __dpmi_int(0x10, &reg);

  if(reg.h.al == 0x1A)
  {
    switch(reg.h.bl) {
      case 0x04:
      case 0x05:
        return DISPLAY_ADAPTER_EGA;
      case 0x07:
      case 0x08:
        return DISPLAY_ADAPTER_VGA;
      default:
        return DISPLAY_ADAPTER_UNSUPPORTED;
    }
  }

  // VIDEO - ALTERNATE FUNCTION SELECT (PS, EGA, VGA, MCGA) - GET EGA INFO
  // Generally supported by EGA cards
  reg.h.ah = 0x12;
  reg.x.bx = 0xFF10;
  __dpmi_int(0x10, &reg);

  if(reg.h.bh != 0xFF)
    return DISPLAY_ADAPTER_EGA;

  return DISPLAY_ADAPTER_UNSUPPORTED;
}

enum display_adapter_type djgpp_display_adapter_detect(void)
{
  static enum display_adapter_type detected_type = DISPLAY_ADAPTER_MAX;

  if(detected_type >= DISPLAY_ADAPTER_MAX)
    detected_type = display_adapter_detect();

  return detected_type;
}

const char *djgpp_display_adapter_name(enum display_adapter_type adapter)
{
  static const char *disp_adapter_names[] =
  {
    "Unsupported",
    "EGA",
    "VGA",
    "SVGA",
    "SVGA (VBE 2.0)",
    "SVGA (VBE 3.0+)",
  };

  if((unsigned)adapter >= DISPLAY_ADAPTER_MAX)
    return "Unknown";

  return disp_adapter_names[adapter];
}

boolean djgpp_display_is_ati(void)
{
  char ati_magic[9];
  dosmemget(0xC0031, 9, ati_magic);

  if(memcmp(ati_magic, "761295520", 9) == 0)
  {
    debug("Detected ATI video adapter.\n");
    return true;
  }
  return false;
}

boolean djgpp_display_is_oak_technology(void)
{
  char oak_magic[7];
  dosmemget(0xC0008, 7, oak_magic);

  if(memcmp(oak_magic, "OAK VGA", 7) == 0)
  {
    debug("Detected Oak Technology video adapter.\n");
    return true;
  }
  return false;
}

void djgpp_display_save_old_mode(void)
{
  old_mode = ega_get_mode();
}

void djgpp_display_restore_old_mode(void)
{
  if(djgpp_display_adapter_detect() >= DISPLAY_ADAPTER_VGA)
    ega_set_16p();

  ega_set_mode(old_mode);
  ega_blink_on();
  ega_reset_cursor();
}

void djgpp_print_vbe_mode_info(const struct vbe_mode_info *vbe)
{
  (void)vbe; /* nop if debug prints are not enabled */
  debug("VBE mode info:\n"
    "VBE 1.0:\n"
    "  ModeAttributes:      %04" PRIx16 "h\n"
    "  WinAAttributes:      %02" PRIx8 "h\n"
    "  WinBAttributes:      %02" PRIx8 "h\n"
    "  WinGranularity:      %" PRIu16 "\n"
    "  WinSize:             %" PRIu16 "\n"
    "  WinASegment:         %04" PRIx16 "h\n"
    "  WinBSegment:         %04" PRIx16 "h\n"
    "  WinFuncPtr:          %08" PRIx32 "h\n"
    "  BytesPerScanline:    %" PRIu16 "\n"
    "VBE 1.2:\n"
    "  XResolution (px):    %" PRIu16 "\n"
    "  YResolution (px):    %" PRIu16 "\n"
    "  XCharSize:           %" PRIu8 "\n"
    "  YCharSize:           %" PRIu8 "\n"
    "  NumberOfPlanes:      %" PRIu8 "\n"
    "  BitsPerPixel:        %" PRIu8 "\n"
    "  NumberOfBanks:       %" PRIu8 "\n"
    "  MemoryModel:         %02" PRIx8 "h\n"
    "  BankSize (KiB):      %" PRIu8 "\n"
    "  NumberOfImagePages:  %" PRIu8 "\n"
    "  Reserved:            %" PRIu8 "\n"
    "Direct color:\n"
    "  RedMaskSize:         %" PRIu8 "\n"
    "  RedFieldPosition:    %" PRIu8 "\n"
    "  GreenMaskSize:       %" PRIu8 "\n"
    "  GreenFieldPosition:  %" PRIu8 "\n"
    "  BlueMaskSize:        %" PRIu8 "\n"
    "  BlueFieldPosition:   %" PRIu8 "\n"
    "  RsvdMaskSize:        %" PRIu8 "\n"
    "  RsvdFieldPosition:   %" PRIu8 "\n"
    "  DirectColorModeInfo: %02" PRIx8 "h\n"
    "VBE 2.0:\n"
    "  PhysBasePtr:         %08" PRIx32 "h\n"
    "  OffScreenMemOffset:  %08" PRIx32 "h\n"
    "  OffScreenMemSize:    %" PRIu16 "\n",

    vbe->attr, vbe->window_a_attr, vbe->window_b_attr, vbe->window_granularity,
    vbe->window_size, vbe->window_a_start, vbe->window_b_start,
    vbe->window_positioning_func, vbe->pitch,

    vbe->width, vbe->height, vbe->char_width, vbe->char_height,
    vbe->memory_planes, vbe->bpp, vbe->memory_banks, vbe->memory_model_type,
    vbe->bank_size, vbe->image_pages, vbe->reserved1,

    vbe->red_mask_size, vbe->red_field_position,
    vbe->green_mask_size, vbe->green_field_position,
    vbe->blue_mask_size, vbe->blue_field_position,
    vbe->rsvd_mask_size, vbe->rsvd_field_position,
    vbe->direct_color_mode_info,

    vbe->linear_ptr, vbe->offscreen_ptr, vbe->offscreen_size
  );
}

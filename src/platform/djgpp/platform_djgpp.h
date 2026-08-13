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

#ifndef MEGAZEUX_PLATFORM_DJGPP_H
#define MEGAZEUX_PLATFORM_DJGPP_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

#define DMA_BOUNDARY  0x10000 /* For djgpp_malloc_boundary */

#define DMA_AUTOINIT  0x10
#define DMA_READ      0x44
#define DMA_WRITE     0x48

enum
{
  DISPLAY_ADAPTER_UNSUPPORTED,
  DISPLAY_ADAPTER_EGA,
  DISPLAY_ADAPTER_VGA,
  DISPLAY_ADAPTER_SVGA,
  DISPLAY_ADAPTER_VBE20,
  DISPLAY_ADAPTER_VBE30
};

struct vbe_info
{
  char signature[4];
  uint16_t version;
  uint32_t oem_name;
  uint32_t capabilities;
  uint32_t modes;
  uint16_t memory_size;
  uint16_t oem_version;
  uint32_t vendor_name;
  uint32_t product_name;
  uint32_t product_revision;
} __attribute__((packed));

struct vbe_mode_info
{
  uint16_t attr;
  uint8_t window_a_attr;
  uint8_t window_b_attr;
  uint16_t window_granularity;
  uint16_t window_size;
  uint16_t window_a_start;
  uint16_t window_b_start;
  uint32_t window_positioning_func;
  uint16_t pitch;
  uint16_t width;
  uint16_t height;
  uint8_t char_width;
  uint8_t char_height;
  uint8_t memory_planes;
  uint8_t bpp;
  uint8_t memory_banks;
  uint8_t memory_model_type;
  uint8_t bank_size;
  uint8_t image_pages;
  uint8_t reserved1;
  uint8_t red_mask_size;
  uint8_t red_field_size;
  uint8_t green_mask_size;
  uint8_t green_field_size;
  uint8_t blue_mask_size;
  uint8_t blue_field_size;
  uint16_t reserved2;
  uint8_t direct_color_mode_info;
  uint32_t linear_ptr;
  uint32_t offscreen_ptr;
  uint16_t offscreen_size;
} __attribute__((packed));

struct irq_state
{
  int port_21h;
  int port_A1h;
};

int djgpp_display_adapter_detect(void);
const char *djgpp_display_adapter_name(int adapter);
int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector);
boolean djgpp_push_enable_nearptr(void);
boolean djgpp_pop_enable_nearptr(void);
void djgpp_enable_dma(uint8_t port, uint8_t mode, int offset, int bytes);
void djgpp_disable_dma(uint8_t port);

void djgpp_irq_enable(int irq, struct irq_state *old_state);
void djgpp_irq_restore(struct irq_state *old_state);
void djgpp_irq_ack(int irq);
int djgpp_irq_vector(int irq);

/* Because multiple sound engines rely on floating point, the x87 FPU
 * state needs to be saved at the beginning of and reloaded at the end
 * of every audio driver callback. Otherwise, these engines will clobber
 * floating point values from normal execution (especially noticeable
 * in stb_vorbis streams corrupted during their loading process).
 *
 * Affected engines: libxmp, libmodplug, libopenmpt, stb_vorbis.
 * Also affected: libvorbis, which is unusable for other reasons.
 */
static inline void djgpp_save_x87(uint8_t fpustate[108])
{
  __asm__("fsave %0" : "=m"(fpustate));
}
static inline void djgpp_restore_x87(const uint8_t fpustate[108])
{
  __asm__("fwait\n\t"
          "frstor %0" : : "m"(fpustate));
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_DJGPP_H */

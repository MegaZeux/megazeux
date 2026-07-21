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
#include <dpmi.h>

#define DMA_BOUNDARY  0x10000 /* For djgpp_malloc_boundary */

#define DMA_AUTOINIT  0x10
#define DMA_READ      0x44
#define DMA_WRITE     0x48

struct irq_state
{
  int port_21h;
  int port_A1h;
};

int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector);
void *djgpp_map_physical_memory(uint32_t address, size_t len_bytes,
 __dpmi_meminfo *mi, int *type);
void djgpp_unmap_physical_memory(__dpmi_meminfo *mi, int *type);
void djgpp_enable_dma(uint8_t port, uint8_t mode, int offset, int bytes);
void djgpp_disable_dma(uint8_t port);

void djgpp_irq_enable(int irq, struct irq_state *old_state);
void djgpp_irq_restore(struct irq_state *old_state);
void djgpp_irq_ack(int irq);
int djgpp_irq_vector(int irq);

void djgpp_set_irq8_handler(double rate, void *priv, void (*callback)(void *));

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

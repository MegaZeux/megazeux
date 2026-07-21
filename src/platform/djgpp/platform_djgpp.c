/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <pc.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <crt0.h>
#include <sys/exceptn.h>
#include <sys/nearptr.h>
#include <sys/segments.h>
#undef delay
#include "../../util.h"
#include "../platform.h"
#include "platform_djgpp.h"

/* TODO: Most of the audio callback code can't currently be locked or moved
 * outside of the callback, which causes CWSDPMI to crash during paging.
 * Disable paging altogether for now.
 *
 * Non-moving sbrk() (default, specify anyway) is required for nearptr hacks.
 */
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY | _CRT0_FLAG_NONMOVE_SBRK;

static int djgpp_nearptr_cnt = 0;

static boolean djgpp_push_enable_nearptr(void)
{
  if(djgpp_nearptr_cnt == 0)
    if(!__djgpp_nearptr_enable())
      return false;

  djgpp_nearptr_cnt++;
  return true;
}

static boolean djgpp_pop_enable_nearptr(void)
{
  if(djgpp_nearptr_cnt <= 0)
    return false;
  if(djgpp_nearptr_cnt == 1)
    __djgpp_nearptr_disable();
  djgpp_nearptr_cnt--;
  return true;
}

/**
 * Allocate a buffer in DOS memory. If boundary_bytes is larger than or equal
 * to len_bytes, this function will guarantee the returned segment contains
 * at least len_bytes constrained within a boundary of size boundary_bytes.
 * This is done by allocating twice as much memory as is required and shifting
 * the returned segment to the start of the next boundary (if needed).
 *
 * @param len_bytes       size of allocation. If this value is less than 0 or
 *                        too large to fit in DOS memory, this call will fail.
 * @param boundary_bytes  size of boundary to constrain the allocation within.
 *                        For DMA, this should be 65536. If this value is less
 *                        than 0 or smaller than len_bytes, this call will fail.
 * @return                a segment pointing to an allocated region of
 *                        len_bytes on success, otherwise -1.
 */
int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector)
{
  int len_paragraphs;
  int boundary_mask;
  int segment;

  if(len_bytes < 0 || boundary_bytes < len_bytes || len_bytes > (1 << 19))
    return -1;

  len_paragraphs = (len_bytes + 15) >> 4;
  boundary_mask = ~((boundary_bytes - 1) >> 4);

  segment = __dpmi_allocate_dos_memory(len_paragraphs << 1, selector);
  if(segment < 0)
    return -1;

  if(((segment + len_paragraphs - 1) & boundary_mask) != (segment & boundary_mask))
    segment = (segment + len_paragraphs) & boundary_mask;

  return segment;
}

/* Method 1: Physical Address Mapping 800h
 *
 * Some DPMI clients support directly mapping below 1MiB despite this being
 * against the specification. CWSDPMI does not support this, but Windows does.
 * This should always work for addresses over 1MiB, so try it first.
 */
static void *map_physical_memory_over_1mb(uint32_t address, size_t len_bytes,
 __dpmi_meminfo *mi, int *type)
{
  mi->address = address;
  mi->size = len_bytes;
  if(__dpmi_physical_address_mapping(mi))
    return NULL;

  debug("--NEARPTR-- %08" PRIx32 "h %zd (physical mapping)\n", address, len_bytes);
  *type = 1;
  return (void *)(mi->address + __djgpp_conventional_base);
}

/* Method 2: Map Device Memory in Memory Block 508h
 *           Map Conventional Memory in Memory Block 509h
 *
 * TODO: this method is non-trivial and has few benefits over methods 1 and 3,
 * so it hasn't been implemented.
 */
//void *map_physical_memory_dpmi_10() {}

/* Method 3: Manually derive nearptr offset.
 *
 * Don't bother with the physical address mapping, as
 * "most DPMI servers map the first Megabyte 1:1 anyway"
 * and nearptr permits access to the entire address space.
 * This is confirmed to work with CWSDPMI, PMODE/DJ, and 386MAX/DPMIONE.
 */
static void *map_physical_memory_manual(uint32_t address, size_t len_bytes,
 __dpmi_meminfo *mi, int *type)
{
  /* Only support first 1MiB of memory */
  if(address >= 1024 * 1024 || address + len_bytes > 1024 * 1024)
    return NULL;

  debug("--NEARPTR-- %08" PRIx32 "h %zd (manual)\n", address, len_bytes);
  *type = 3;
  return (void *)(address + __djgpp_conventional_base);
}

/**
 * Use to nearptr map an allocation in conventional or device memory e.g.
 * allocated by djgpp_malloc_boundary or __dpmi_allocate_dos_memory.
 *
 * @param address     absolute address of the area to be mapped.
 * @param len_bytes   length of the area to be mapped, in bytes.
 * @param mi          __dpmi_meminfo required to unmap this buffer.
 * @param type        the type of mapping is stored here to allow unmapping.
 * @return            a pointer usable by the caller addressing the requested
 *                    area of conventional memory on success, otherwise NULL.
 */
void *djgpp_map_physical_memory(uint32_t address, size_t len_bytes,
 __dpmi_meminfo *mi, int *type)
{
  void *ret;

  *type = 0;
  if((uint64_t)address + len_bytes > UINT32_MAX)
    return NULL;

  if(!djgpp_push_enable_nearptr())
    return NULL;

  ret = map_physical_memory_over_1mb(address, len_bytes, mi, type);
  if(ret)
    return ret;

  /*
  ret = map_physical_memory_dpmi_10(address, len_bytes, mi, type);
  if(ret)
    return ret;
  */

  ret = map_physical_memory_manual(address, len_bytes, mi, type);
  if(ret)
    return ret;

  djgpp_pop_enable_nearptr();
  return NULL;
}

/**
 * Unmap djgpp_map_physical_memory.
 *
 * @param mi          __dpmi_meminfo containing the original mapping info.
 * @param type        type pointer that was provided to the original mapping.
 */
void djgpp_unmap_physical_memory(__dpmi_meminfo *mi, int *type)
{
  switch(*type)
  {
    case 1:
      /* This is a DPMI 1.0 function but supported by CWSDPMI and PMODE/DJ */
      __dpmi_free_physical_address_mapping(mi);
      /* fall-through */

    case 3:
      djgpp_pop_enable_nearptr();
      break;
  }
  *type = 0;
}

static void djgpp_enable_dma16(uint8_t port, uint8_t mode, int offset, int bytes)
{
  int words = (bytes + 1) >> 1;
  outportb(0xD4, 0x04 | (port & 3));
  outportb(0xD8, 0x00);
  outportb(0xD6, (mode & (~3)) | (port & 3));
  outportb(0xC0 + ((port & 3) << 2), (offset >> 1) & 0xFF);
  outportb(0xC0 + ((port & 3) << 2), (offset >> 9) & 0xFF);
  outportb(0xC2 + ((port & 3) << 2), (words - 1) & 0xFF);
  outportb(0xC2 + ((port & 3) << 2), (words - 1) >> 8);
  switch(port & 3)
  {
    case 1:
      outportb(0x8B, (offset >> 16));
      break;
    case 2:
      outportb(0x89, (offset >> 16));
      break;
    case 3:
      outportb(0x8A, (offset >> 16));
      break;
  }
  outportb(0xD4, (port & 3));
}

static void djgpp_enable_dma8(uint8_t port, uint8_t mode, int offset, int bytes)
{
  outportb(0x0A, 0x04 | (port & 3));
  outportb(0x0C, 0x00);
  outportb(0x0B, (mode & (~3)) | (port & 3));
  outportb(0x00 + ((port & 3) << 1), (offset) & 0xFF);
  outportb(0x00 + ((port & 3) << 1), (offset >> 8) & 0xFF);
  outportb(0x01 + ((port & 3) << 1), (bytes - 1) & 0xFF);
  outportb(0x01 + ((port & 3) << 1), (bytes - 1) >> 8);
  switch(port & 3)
  {
    case 0:
      outportb(0x87, (offset >> 16));
      break;
    case 1:
      outportb(0x83, (offset >> 16));
      break;
    case 2:
      outportb(0x81, (offset >> 16));
      break;
    case 3:
      outportb(0x82, (offset >> 16));
      break;
  }
  outportb(0x0A, (port & 3));
}

void djgpp_enable_dma(uint8_t port, uint8_t mode, int offset, int bytes)
{
  if(port >= 4)
    djgpp_enable_dma16(port, mode, offset, bytes);
  else
    djgpp_enable_dma8(port, mode, offset, bytes);
}

void djgpp_disable_dma(uint8_t port)
{
  if(port >= 4)
    outportb(0xD4, 0x04 | (port & 3));
  else
    outportb(0x0A, 0x04 | (port & 3));
}

void djgpp_irq_enable(int irq, struct irq_state *old_state)
{
  old_state->port_21h = inportb(0x21);
  old_state->port_A1h = -1;
  if(irq >= 8)
  {
    old_state->port_A1h = inportb(0xA1);
    outportb(0x21, old_state->port_21h & (~(1 << 2)));
    outportb(0xA1, old_state->port_A1h & (~(1 << (irq & 7))));
  }
  else
    outportb(0x21, old_state->port_21h & (~(1 << irq)));
}

void djgpp_irq_restore(struct irq_state *old_state)
{
  outportb(0x21, old_state->port_21h);
  if(old_state->port_A1h >= 0)
    outportb(0xA1, old_state->port_A1h);
}

void djgpp_irq_ack(int irq)
{
  if(irq >= 8)
    outportb(0xA0, 0x20);
  outportb(0x20, 0x20);
}

int djgpp_irq_vector(int irq)
{
  if(irq >= 8)
    return 0x70 + (irq - 8);
  else
    return 0x08 + irq;
}

void djgpp_set_irq8_handler(double rate, void *priv, void (*callback)(void *))
{
  // FIXME:
}

#define TIMER_CLOCK  3579545
#define TIMER_LENGTH 8
#define TIMER_COUNT  (TIMER_LENGTH * TIMER_CLOCK / 3000)
#define TIMER_NORMAL 65536

// Defined in interrupt.S
extern int int_lock_start, int_lock_end;
extern unsigned short int_ds;

extern int timer_handler;
extern __dpmi_paddr timer_old_handler;
extern volatile uint32_t timer_ticks;
extern volatile uint32_t timer_offset;
extern uint32_t timer_length;
extern uint32_t timer_count;
extern uint32_t timer_normal;

extern __dpmi_paddr kbd_old_handler;

static boolean yieldable;

void delay(uint32_t ms)
{
  ms += timer_ticks;
  while(timer_ticks < ms)
  {
    if(yieldable)
      __dpmi_yield();
  }
}

uint64_t get_ticks(void)
{
  return timer_ticks;
}

static void set_timer(uint32_t count)
{
  outportb(0x43, 0x34);
  outportb(0x40, count & 0xFF);
  outportb(0x40, count >> 8);
}

static void fix_timezone(void)
{
  // DJGPP, unlike normal SDKs, will return -1 for time functions
  // unless TZ is initialized. Use UTC as a default.
  if(!getenv("TZ"))
    setenv("TZ", "UTC", 1);
}

boolean platform_init(void)
{
  __dpmi_meminfo region;
  __dpmi_paddr handler;
  unsigned long base;
  int flags;
  char vendor[128];

  // Disable exception on Ctrl-C
  __djgpp_set_ctrl_c(0);

  // Print DPMI vendor and capabilities if supported
  if(__dpmi_get_capabilities(&flags, vendor) == 0)
  {
    info("DPMI vendor: %s %d.%d (%02xh)\n",
     vendor + 2, vendor[0], vendor[1], flags);
  }
  else
    info("DPMI vendor: unknown\n");

  // Check if DPMI yield function is supported
  errno = 0;
  __dpmi_yield();
  yieldable = (errno != ENOSYS);

  int_ds = _my_ds();

  if(__dpmi_get_segment_base_address(_my_ds(), &base))
  {
    warn("Failed to get segment base address.");
    return false;
  }

  region.address = base + (unsigned long)&int_lock_start;
  region.size = (unsigned long)&int_lock_end - (unsigned long)&int_lock_start;
  if(__dpmi_lock_linear_region(&region))
  {
    warn("Failed to lock interrupt handler region.");
    return false;
  }

  timer_length = TIMER_LENGTH;
  timer_count = TIMER_COUNT;
  timer_normal = TIMER_NORMAL;
  __dpmi_get_protected_mode_interrupt_vector(0x08, &timer_old_handler);

  handler.offset32 = (unsigned long)&timer_handler;
  handler.selector = _my_cs();

  disable();
  if(__dpmi_set_protected_mode_interrupt_vector(0x08, &handler))
  {
    enable();
    warn("Failed to hook timer interrupt.");
    return false;
  }
  set_timer(timer_count);
  enable();

  __dpmi_get_protected_mode_interrupt_vector(0x09, &kbd_old_handler);
  fix_timezone();
  return true;
}

void platform_quit(void)
{
  __dpmi_regs reg;

  // TODO: Add deinit function for event system
  // Unhook keyboard interrupt
  if(__dpmi_set_protected_mode_interrupt_vector(0x09, &kbd_old_handler))
    warn("Failed to unhook keyboard interrupt.");
  // Reset mouse driver
  reg.x.ax = 0;
  __dpmi_int(0x33, &reg);

  disable();
  if(__dpmi_set_protected_mode_interrupt_vector(0x08, &timer_old_handler))
    warn("Failed to unhook timer interrupt.");
  set_timer(timer_normal);
  enable();

  while(djgpp_pop_enable_nearptr())
    ;
}

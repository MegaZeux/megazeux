/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2019 Adrian Siekierka <kontakt@asie.pl>
 * Copyright (C) 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <math.h>
#include <pc.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <crt0.h>
#include <sys/exceptn.h>
#include <sys/farptr.h>
#include <sys/nearptr.h>
#include <sys/segments.h>
#undef delay
#include "../../util.h"
#include "../platform.h"
#include "platform_djgpp.h"
#include "interrupt.h"

/* TODO: Most of the audio callback code can't currently be locked or moved
 * outside of the callback, which causes CWSDPMI to crash during paging.
 * Disable paging altogether for now.
 *
 * Non-moving sbrk() (default, specify anyway) is required for nearptr hacks.
 */
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY | _CRT0_FLAG_NONMOVE_SBRK;

static __dpmi_paddr rtc_old_handler;
static struct irq_state rtc_old_state;
static _go32_dpmi_seginfo rtc_handler;
static int rtc_old_register_a;
static int rtc_old_register_b;
static boolean have_rtc_interrupt;

static _go32_dpmi_seginfo mouse_callback_info;
static boolean have_mouse_callback;

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

static void pit_set_divider(uint16_t div)
{
  outportb(0x43, 0x34);
  outportb(0x40, div & 0xFF);
  outportb(0x40, div >> 8);
  current_div = div;
}

static void pit_save_handler(void)
{
  __dpmi_get_protected_mode_interrupt_vector(IRQ_VECTOR(0), &timer_old_handler);
}

static void pit_restore_handler(void)
{
  disable();

  if(__dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(0), &timer_old_handler))
    warn("Failed to unhook timer interrupt.");

  pit_set_divider((uint16_t)PIT_DEFAULT_DIVIDER);
  enable();
}

static boolean pit_set_handler(uint16_t divider, __dpmi_paddr *handler)
{
  disable();

  if(__dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(0), handler))
  {
    enable();
    warn("Failed to hook timer interrupt.");
    return false;
  }

  pit_set_divider(divider);
  enable();
  return true;
}

static void kbd_save_handler(void)
{
  __dpmi_get_protected_mode_interrupt_vector(IRQ_VECTOR(1), &kbd_old_handler);
}

static void kbd_restore_handler(void)
{
  disable();

  if(__dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(1), &kbd_old_handler))
    warn("Failed to unhook keyboard interrupt.");

  enable();
}

static boolean kbd_set_handler(__dpmi_paddr *handler)
{
  disable();

  if(__dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(1), handler))
  {
    enable();
    warn("Failed to hook keyboard interrupt.");
    return false;
  }

  enable();
  return true;
}

/* Enable non-maskable interrupts */
static void nmi_enable(void)
{
  outportb(0x70, inportb(0x70) & 0x7f);
  inportb(0x71);
}

/* Disable non-maskable interrupts */
static void nmi_disable(void)
{
  outportb(0x70, inportb(0x70) | 0x80);
  inportb(0x71);
}

static void rtc_save_handler(void)
{
  __dpmi_get_protected_mode_interrupt_vector(IRQ_VECTOR(8), &rtc_old_handler);
  have_rtc_interrupt = false;

  disable();
  outportb(0x70, 0x8A);
  rtc_old_register_a = inportb(0x71);
  outportb(0x70, 0x8B);
  rtc_old_register_b = inportb(0x71);
  nmi_enable();
  enable();
}

static void rtc_restore_handler(void)
{
  disable();
  nmi_disable();

  if(__dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(8), &rtc_old_handler))
    warn("Failed to unhook RTC interrupt.");

  outportb(0x70, 0x8A);
  outportb(0x71, rtc_old_register_a);
  outportb(0x70, 0x8B);
  outportb(0x71, rtc_old_register_b);

  if(have_rtc_interrupt)
    djgpp_irq_restore(&rtc_old_state);

  nmi_enable();
  enable();

  if(have_rtc_interrupt)
  {
    _go32_dpmi_free_iret_wrapper(&rtc_handler);
    have_rtc_interrupt = false;
  }
}

static boolean rtc_set_handler(int divider, _go32_dpmi_seginfo *handler)
{
  int prev_a, prev_b;

  /* Dividers <2 cause hardware issues, see OSDev Wiki. */
  divider = CLAMP(divider, 2, 15);

  /* If present, release and free the old wrapper. */
  if(have_rtc_interrupt)
    rtc_restore_handler();

  if(_go32_dpmi_allocate_iret_wrapper(handler))
  {
    warn("Failed to wrap RTC interrupt.");
    return false;
  }

  disable();
  nmi_disable();

  if(_go32_dpmi_set_protected_mode_interrupt_vector(IRQ_VECTOR(8), handler))
  {
    nmi_enable();
    enable();
    _go32_dpmi_free_iret_wrapper(handler);
    warn("Failed to hook RTC interrupt.");
    return false;
  }

  /* Set divider */
  outportb(0x70, 0x8A);
  prev_a = inportb(0x71);
  outportb(0x70, 0x8A);
  outportb(0x71, (prev_a & 0xf0) | divider);

  /* Enable interrupt */
  outportb(0x70, 0x8B);
  prev_b = inportb(0x71);
  outportb(0x70, 0x8B);
  outportb(0x71, prev_b | 0x40);

  /* ack prior interrupt */
  outportb(0x70, 0x8C);
  inportb(0x71);

  /* Unmask interrupt */
  djgpp_irq_enable(8, &rtc_old_state);

  nmi_enable();
  enable();

  rtc_handler = *handler;
  have_rtc_interrupt = true;
  return true;
}

static boolean mouse_reset_driver(void)
{
  __dpmi_regs reg;
  __dpmi_raddr mouse_vector;

  /* Verify mouse driver is installed (interrupt vector 0x33 exists). */
  if(__dpmi_get_real_mode_interrupt_vector(MOUSE_VECTOR, &mouse_vector))
    return false;
  if(mouse_vector.offset16 == 0 && mouse_vector.segment == 0)
    return false;

  /* Reset mouse driver
   *
   * Returns:
   *
   * ax 0x0000: hardware/driver not installed
   *    0xffff: hardware/driver installed
   * bx 0x0000: unknown button count
   *    0x0002: two buttons
   *    0x0003: Mouse Systems/Logitech 3 button mouse
   *    0xffff: two buttons
   */
  reg.x.ax = 0;
  __dpmi_int(MOUSE_VECTOR, &reg);

  return reg.x.ax == 0xffff ? true : false;
}

static void mouse_quit_driver(void)
{
  mouse_reset_driver();

  if(have_mouse_callback)
  {
    _go32_dpmi_free_real_mode_callback(&mouse_callback_info);
    have_mouse_callback = false;
  }
}

static boolean mouse_init_driver(void)
{
  __dpmi_regs reg;

  if(have_mouse_callback)
    mouse_quit_driver();

  if(!mouse_reset_driver())
    return false;

  memset(&mouse_callback_info, 0, sizeof(mouse_callback_info));

  mouse_callback_info.pm_offset = (unsigned long)mouse_handler;
  if(_go32_dpmi_allocate_real_mode_callback_retf(&mouse_callback_info, &mouse_regs))
    return false;

  have_mouse_callback = true;

  /* Define interrupt subroutine parameters
   *
   * cx:    call mask (0: move 1:left press 2:left release 3: right press
   *                   4: right release 5: middle press 6: middle release
   *                   7+: undefined)
   * es:dx: real mode callback address
   *
   * No documented return values.
   *
   * Callback registers:
   * ax:    condition mask (same bits as call mask)
   * bx:    button state
   * cx:    cursor column
   * dx:    cursor row
   * si:    horizontal mickey count
   * di:    vertical mickey count
   */
  reg.x.ax = 0x000C;
  reg.x.cx = 0x007F;
  reg.x.dx = mouse_callback_info.rm_offset;
  reg.x.es = mouse_callback_info.rm_segment;
  __dpmi_int(MOUSE_VECTOR, &reg);

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
 * @param selector        Pointer to write allocation selector to on success.
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

void djgpp_rtc_ack(void)
{
  /* Separate from the PIC ack, the RTC needs this register to be read
   * before it will send further IRQ8 interrupts: */
  outportb(0x70, 0x0C);
  inportb(0x71);
}

boolean djgpp_reset_irq0_handler(void)
{
  __dpmi_paddr handler;
  handler.offset32 = (unsigned long)&timer_handler;
  handler.selector = _my_cs();

  return pit_set_handler(TIMER_DIVIDER, &handler);
}

boolean djgpp_reset_irq8_handler(void)
{
  rtc_restore_handler();
  return true;
}

boolean djgpp_set_irq0_handler(uint16_t rate_hz, const int *irq_handler)
{
  __dpmi_paddr handler;
  handler.offset32 = (unsigned long)irq_handler;
  handler.selector = _my_cs();

  rate_hz = rate_hz ? rate_hz : 1;
  return pit_set_handler(PIT_DIVIDER(rate_hz), &handler);
}

boolean djgpp_set_irq8_handler(uint16_t rate_hz, void (*callback)(void))
{
  _go32_dpmi_seginfo handler;
  int divider;

  /* IRQ8 Hz = RTC_BASE_CLOCK >> (divider - 1)
   * To get the closest divider, floor(log2(Base / Hz)) and add 1. */
  rate_hz = rate_hz ? rate_hz : 1;
  divider = (int)(log((double)RTC_BASE_CLOCK / (double)rate_hz) / M_LN2) + 1;

  handler.pm_offset = (unsigned long)callback;
  handler.pm_selector = _go32_my_cs();

  return rtc_set_handler(divider, &handler);
}

/**
 * Get the LPT base port for LPT1-3. Extended LPTs must have their base port
 * manually specified elsewhere.
 *
 * @param lpt   LPT to get the base port for. If this isn't in the range of
 *              1 to 3, or if this LPT has no base port, this call will fail.
 * @return      the base port of this LPT on success, otherwise 0.
 */
uint16_t djgpp_get_lpt_base_port(int lpt)
{
  if(lpt < 1 || lpt > 3)
    return 0;

  /* LPT base ports are stored sequentially in BIOS memory starting at 0x408.
   * This memory is not mapped in protected mode. */
  return _farpeekw(_dos_ds, 0x408 + (lpt - 1) * 2);
}


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

  //timer_prev_div = PIT_DEFAULT_DIVIDER;

  handler.offset32 = (unsigned long)&timer_handler;
  handler.selector = _my_cs();

  pit_save_handler();
  if(!pit_set_handler(TIMER_DIVIDER, &handler))
    return false;

  handler.offset32 = (unsigned long)&kbd_handler;
  handler.selector = _my_cs();

  kbd_save_handler();
  if(!kbd_set_handler(&handler))
  {
    pit_restore_handler();
    return false;
  }

  if(!mouse_init_driver())
    warn("Failed to initialize mouse driver.\n");

  /* RTC interrupt is optional, only used for audio callbacks in some drivers.
   * The base frequency can't be set without causing the CMOS clock to lose
   * track of time, so there are only a small number of usable frequencies. */
  have_rtc_interrupt = false;
  rtc_save_handler();

  fix_timezone();
  return true;
}

void platform_quit(void)
{
  mouse_quit_driver();

  if(have_rtc_interrupt)
  {
    rtc_restore_handler();
    have_rtc_interrupt = false;
  }
  kbd_restore_handler();
  pit_restore_handler();

  while(djgpp_pop_enable_nearptr())
    ;
}

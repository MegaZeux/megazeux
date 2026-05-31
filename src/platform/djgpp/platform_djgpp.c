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
 * outside of the callback, which causes CWSDMPI to crash during paging.
 * Disable paging altogether for now.
 */
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

static int djgpp_nearptr_cnt = 0;

boolean djgpp_push_enable_nearptr(void)
{
  if(djgpp_nearptr_cnt > 0)
    return true;
  if(!__djgpp_nearptr_enable())
    return false;
  djgpp_nearptr_cnt++;
  return true;
}

boolean djgpp_pop_enable_nearptr(void)
{
  if(djgpp_nearptr_cnt <= 0)
    return false;
  if(djgpp_nearptr_cnt == 1)
    __djgpp_nearptr_disable();
  djgpp_nearptr_cnt--;
  return true;
}

int djgpp_display_adapter_detect(void)
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

const char *disp_adapter_names[] =
{
  "Unsupported",
  "EGA",
  "VGA",
  "SVGA",
  "SVGA (VBE 2.0+)"
};

const char *djgpp_display_adapter_name(int adapter)
{
  return disp_adapter_names[adapter];
}

int djgpp_malloc_boundary(int len_bytes, int boundary_bytes, int *selector)
{
  int len_segment = (len_bytes + 15) >> 4;
  int boundary_mask = ~((boundary_bytes - 1) >> 4);
  int segment = __dpmi_allocate_dos_memory((len_bytes + 7) >> 3, selector);
  if(((segment + len_segment - 1) & boundary_mask) != (segment & boundary_mask))
    segment += len_segment;
  return segment;
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
      outportb(0x8B, (offset >> 17));
      break;
    case 2:
      outportb(0x89, (offset >> 17));
      break;
    case 3:
      outportb(0x8A, (offset >> 17));
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

  // Disable exception on Ctrl-C
  __djgpp_set_ctrl_c(0);

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

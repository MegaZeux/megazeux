/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_DJGPP_INTERRUPT_H
#define MEGAZEUX_PLATFORM_DJGPP_INTERRUPT_H

#include "../../compat.h"

#include <dpmi.h>
#include <stdint.h>

MEGAZEUX_BEGIN_DECLS

#define IRQ_VECTOR(x)         (((x) >= 0x08) ? (0x70 + (x) - 0x08) : (0x08 + (x)))
#define MOUSE_VECTOR          0x33

#define PIT_BASE_CLOCK        14318180
#define PIT_BASE_DIVIDER      12
#define PIT_DIVIDER(x)        (PIT_BASE_CLOCK / ((x) * PIT_BASE_DIVIDER))
#define PIT_DEFAULT_DIVIDER   65536
#define RTC_BASE_CLOCK        32768

/* Slightly less accurate than 125 Hz (divider 9545), but required for PCS.
 * This is also near the rate that the DSS is intended to be updated at. */
#define TIMER_HZ              500
#define TIMER_MS              (1000 / TIMER_HZ)
#define TIMER_DIVIDER         PIT_DIVIDER(TIMER_HZ) /* 2386 */

#define LPT_8K_HZ             8007 /* Divider 149 is near (2386 / 16) */
#define LPT_10K_HZ            10026 /* Divider 119 is near (2386 / 20) */
#define LPT_11K_HZ            11256 /* Divider 106 is near (2386 / 22.5) */
#define LPT_15K_HZ            15495 /* Divider 77 is near (2386 / 31) */
#define LPT_17K_HZ            17045 /* Divider 70 is near (2386 / 34) */
#define LPT_22K_HZ            22512 /* Divider 53 is near (2386 / 45) */
#define LPT_26K_HZ            26515 /* Divider 45 is near (2386 / 53) */
#define LPT_28K_HZ            27748 /* Divider 43 is near (2386 / 55.5) */
#define LPT_32K_HZ            32248 /* Divider 37 is near (2386 / 64.5) */
#define LPT_38K_HZ            38489 /* Divider 31 is near (2386 / 77) */
#define LPT_44K_HZ            44191 /* Divider 27 gets closest to 44100 */
#define LPT_48K_HZ            47727 /* Divider 25 gets closest to 48000 */
#define LPT_DSS_HZ            7000 /* DSS outputs a fixed rate */

extern const int int_lock_start;
extern const int int_lock_end;
extern unsigned short int_ds;

/* Timer interrupt variables */
extern const int timer_handler;
extern __dpmi_paddr timer_old_handler;
extern volatile uint32_t timer_ticks;
extern volatile int32_t timer_pos;
extern volatile int32_t timer_prev_pos;
//extern uint32_t timer_prev_div;
extern uint32_t current_div;

/* Audio interrupt variables */
extern const int lpt_mono_handler;
extern const int lpt_stereo1_handler;
extern const int lpt_stereo2_handler;
extern const int lpt_dss_handler;
extern uint8_t *dos_audio_buffer;
extern uint8_t *dos_audio_buffer_end;
extern volatile uint8_t *dos_audio_buffer_pos; /* <size -> first, >=size -> second */
extern uint32_t dos_audio_buffer_size;
extern uint32_t dos_audio_buffer_next; /* != pos buffer -> need callback */
extern uint32_t dos_audio_buffer_frames;
extern uint32_t dos_audio_channels;
extern int16_t lpt_left_port; /* port for mono/stereo-on-1/stereo-on-2 left LPT */
extern int16_t lpt_right_port; /* port for stereo-on-2 right LPT */

/* Keyboard interrupt variables */
extern const int kbd_handler;
extern __dpmi_paddr kbd_old_handler;
extern volatile uint8_t kbd_buffer[256];
extern volatile uint8_t kbd_read;
extern volatile uint8_t kbd_write;

/* Mouse callback variables */
struct mouse_event
{
  uint16_t cond;
  uint16_t button;
  int16_t dx;
  int16_t dy;
};

extern void mouse_handler(__dpmi_regs *reg);
extern _go32_dpmi_registers mouse_regs;
extern volatile struct mouse_event mouse_buffer[256];
extern volatile uint8_t mouse_read;
extern volatile uint8_t mouse_write;

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_DJGPP_INTERUPT_H */

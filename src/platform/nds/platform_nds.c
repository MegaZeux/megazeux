/* MegaZeux
 *
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

#include <stdarg.h>

#include "../platform.h"
#undef main

#include "../../util.h"

#include <fat.h>
#include "extmem_nds.h"
#include "../dlmalloc.h"
#include "../../render/nds/renderer_nds.h"

// from arch/nds/event.c
extern void nds_update_input(void);

// Timer code stolen from SDL (LGPL)

#define timers2ms(tlow,thigh) ((tlow) | ((thigh)<<16)) >> 7
#ifdef BUILD_PROFILING
#define timers2ticks(tlow,thigh) ((tlow) | ((thigh)<<16))
#endif

void delay(uint32_t ms)
{
  uint32_t now;
  now = timers2ms(TIMER0_DATA, TIMER1_DATA);
  while((uint32_t)timers2ms(TIMER0_DATA, TIMER1_DATA) < now + ms)
    ;
}

uint64_t get_ticks(void)
{
  return timers2ms(TIMER0_DATA, TIMER1_DATA);
}

static void timer_init(void)
{
  // TIMER0, TIMER1: Tick timer
  TIMER0_DATA = 0;
  TIMER1_DATA = 0;
  TIMER0_CR = TIMER_ENABLE | TIMER_DIV_256;
  TIMER1_CR = TIMER_ENABLE | TIMER_CASCADE;

  // TIMER2: Input handler IRQ
  // We could do this in vblank, but sometimes we need to block new input
  // events when the event queue is in use.  Using a hardware timer, we can
  // keep the graphics flicker running.
  TIMER2_DATA = TIMER_FREQ_1024(60);
  TIMER2_CR = TIMER_ENABLE | TIMER_DIV_1024 | TIMER_IRQ_REQ;
  irqSet(IRQ_TIMER2, nds_update_input);
  irqEnable(IRQ_TIMER2);
}

#ifdef BUILD_PROFILING

#define PROFILE_QUEUE_DEPTH 4

static const char *profile_names[PROFILE_QUEUE_DEPTH];
static u32 profile_times[PROFILE_QUEUE_DEPTH];
static int profile_pos = 0;

ITCM_CODE
void profile_start(const char *name)
{
  if(profile_pos < PROFILE_QUEUE_DEPTH)
  {
    profile_names[profile_pos] = name;
    profile_times[profile_pos] = timers2ticks(TIMER0_DATA, TIMER1_DATA);
  }

  profile_pos++;
}

ITCM_CODE
void profile_end(void)
{
  u32 ctime;

  if(profile_pos <= 0)
    return;

  if(--profile_pos < PROFILE_QUEUE_DEPTH)
  {
    ctime = timers2ticks(TIMER0_DATA, TIMER1_DATA);
    printf("%s: %lld cycles\n", profile_names[profile_pos], (u64) (ctime - profile_times[profile_pos]) << 8);
  }
}

#endif

// pulled in from libnds - we need to wrap it to undo some of our
// graphics changes
extern void guruMeditationDump(void);

static void mzxExceptionHandler(void)
{
  // stop vblank handler
  irqClear(IRQ_VBLANK);

  // clear sub screen (incl. DMA to it)
  DMA0_CR = 0;
  DMA1_CR = 0;
  DMA2_CR = 0;
  DMA3_CR = 0;
  REG_BG0HOFS_SUB = 0;
  REG_BG0VOFS_SUB = 0;

  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
  vramSetBankC(VRAM_C_SUB_BG);

#ifdef CONFIG_STDIO_REDIRECT
  // The log files can get corrupted if they aren't explicitly closed here.
  redirect_stdio_exit();
#endif

  guruMeditationDump();
  while(1)
  {
  }
}

boolean platform_init(void)
{
  powerOn(POWER_ALL_2D);
  setExceptionHandler(mzxExceptionHandler);

  if(!fatInitDefault())
  {
    printf("Unable to initialize FAT.\n"
            "Check your DLDI driver.\n");
    return false;
  }

#ifdef CONFIG_EXTRAM
  if(!isDSiMode())
    nds_ram_init();
#endif
  timer_init();

  // Enable vblank interrupts, but don't install the handler until the
  // graphics have initialized.
  irqEnable(IRQ_VBLANK);

  return true;
}

void platform_quit(void)
{
}

// argc/argv is unreliable on NDS and varies between cards/launchers.

int main(int argc, char *argv[])
{
  static char _argv0[] = SHAREDIR "/mzxrun.nds";
  static char *_argv[] = {_argv0};
  consoleDemoInit();

  if(argc < 1 || argv == NULL || argv[0] == NULL)
  {
    printf("argv[0]: not found.\n"
            "using '%s'\n"
            "WARNING: Use of a loader that supports argv[0] is recommended.\n",
            _argv0);

    real_main(1, _argv);
  }
  else
  {
    printf("argv[0]: '%s'\n", argv[0]);
    real_main(argc, argv);
  }

  return 0;
}

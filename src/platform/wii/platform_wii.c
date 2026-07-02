/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#include "../platform.h"
#undef main

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/iosupport.h>

#include <ogc/system.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>

#define STACKSIZE 8192

static lwpq_t reset_queue;
static lwp_t reset_thread;
static u8 reset_stack[STACKSIZE];

//void c_default_exceptionhandler(frame_context *pCtx);

// Emergency exit
static void *wii_reset_thread(void *dud)
{
  LWP_ThreadSleep(reset_queue);
  platform_quit();
  delay(1000);
  //c_default_exceptionhandler(&_thr_main->context);
  exit(0);
  return 0;
}

static void reset_callback(u32 irq, void *ctx)
{
  static volatile int callcount = 0;
  callcount++;
  if(callcount > 5)
    *(int *)NULL = 0xDEADCAFE; // Try to cause an exception if pressed 6 times
  LWP_ThreadSignal(reset_queue);
}

static u64 timebase_offset;

void delay(uint32_t ms)
{
  // TODO use nanosleep instead?
  usleep(ms * 1000);
}

uint64_t get_ticks(void)
{
  return ticks_to_millisecs(gettime() - timebase_offset);
}

boolean platform_init(void)
{
  timebase_offset = gettime();

  LWP_InitQueue(&reset_queue);
  LWP_CreateThread(&reset_thread, wii_reset_thread, NULL, reset_stack,
   STACKSIZE, 127);
  SYS_SetResetCallback(reset_callback);

  if(!fatInitDefault())
    return false;

  return true;
}

void platform_quit(void)
{
  int i;

  for(i = 0; i < STD_MAX; i++)
    if(devoptab_list[i] && devoptab_list[i]->chdir_r)
      fatUnmount(devoptab_list[i]->name);
}

// argc may be 0 if the loader doesn't set args
int main(int argc, char *argv[])
{
  static char _argv0[] = SHAREDIR "megazeux";
  static char *_argv[] = {_argv0};

  if(!argc)
    real_main(1, _argv);
  else
    real_main(argc, argv);
}

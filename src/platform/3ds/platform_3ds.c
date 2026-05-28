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

#include "platform_3ds.h"

#include "../platform.h"
#undef main

#include "../../util.h"
#include "../../error.h"
#include "../../graphics.h"
#include "../../event/event.h"

#include <unistd.h>
#include <stdio.h>

#include <3ds.h>
#include <citro3d.h>

#include "../../event/3ds/keyboard.h"

static u8 isNot2DS, consoleModelId;

FILE *popen(const char *command, const char *type)
{
  return NULL;
}

int pclose(FILE *stream)
{
  return 0;
}

void delay(uint32_t ms)
{
  if(ms > 0)
  {
    svcSleepThread(1e6 * ms);
  }
}

boolean ctr_is_2d(void)
{
  return isNot2DS == 0;
}

boolean ctr_supports_wide(void)
{
  return consoleModelId != 3 /* O2DS */;
}

uint64_t get_ticks(void)
{
  return osGetTime();
}

boolean platform_init(void)
{
  cfguInit();
  romfsInit();
  osSetSpeedupEnable(1);
  APT_SetAppCpuTimeLimit(30);

  gfxInitDefault();
  gfxSet3D(false);

  CFGU_GetSystemModel(&consoleModelId);
  CFGU_GetModelNintendo2DS(&isNot2DS);
  return true;
}

void platform_quit(void)
{
  gfxExit();

  romfsExit();
  cfguExit();
}

#ifdef CONFIG_CHECK_ALLOC

static void out_of_linear_memory_check(void *p, const char *file, int line)
{
  char msgbuf[128];
  if(!p)
  {
    snprintf(msgbuf, sizeof(msgbuf), "Out of linear memory in %s:%d",
     file, line);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    error(msgbuf, 2, 4, 0);
  }
}

void *check_linearAlloc(size_t size, size_t alignment, const char *file,
 int line)
{
  void *result = linearMemAlign(size, alignment);
  out_of_linear_memory_check(result, file, line);
  return result;
}

#endif

/**
 * argv[0] will either not exist (cia) or be the location of the 3dsx.
 * For the cia case we can't really do anything, so assume a SHAREDIR
 * startup location.
 */

int main(int argc, char *argv[])
{
  static char _argv0[] = SHAREDIR "/mzxrun.3dsx";
  static char *_argv[] = { _argv0 };

  if(argc < 1 || argv == NULL || argv[0] == NULL)
  {
    iprintf("argv[0]: not found.\n"
            "using '%s'\n"
            "WARNING: Use of a loader that supports argv[0] is recommended.\n",
            _argv0);

    chdir(SHAREDIR);
    real_main(1, _argv);
  }
  else
  {
    iprintf("argv[0]: '%s'\n", argv[0]);
    real_main(argc, argv);
  }

  return 0;
}

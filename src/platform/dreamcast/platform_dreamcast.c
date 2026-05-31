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

#include <sys/time.h>
#include <kos.h>
#include "../platform.h"
#undef main

#include "../../util.h"

KOS_INIT_FLAGS(INIT_DEFAULT);
KOS_INIT_ROMDISK(KOS_INIT_ROMDISK_NONE);

// extern uint8 romdisk[];
// KOS_INIT_ROMDISK(romdisk);

void delay(uint32_t ms)
{
  thd_sleep(ms);
}

uint64_t get_ticks(void)
{
  return timer_ms_gettime64();
}

boolean platform_init(void)
{
  return true;
}

void platform_quit(void)
{
}

int main(int argc, char *argv[])
{
  static char _argv0[] = "/cd/megazeux/megazeux.dummy";
  static char *_argv[] = {_argv0};
  dbgio_enable();
  chdir("/cd");
  real_main(1, _argv);
  return 0;
}

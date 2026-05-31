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

// This isn't really a mutex. Audio is handled via interrupt.

#ifndef MEGAZEUX_THREAD_DJGPP_H
#define MEGAZEUX_THREAD_DJGPP_H

#define delay delay_dos
#include <dos.h>
#undef delay
#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#define PLATFORM_NO_THREADING
#define THREAD_RES void *
#define THREAD_RETURN do { return NULL; } while(0)

typedef int platform_mutex;
typedef int platform_thread_id;

static inline void platform_mutex_init(platform_mutex *mutex)
{
  *mutex = 0;
}

static inline void platform_mutex_destroy(platform_mutex *mutex)
{
  *mutex = 0;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  *mutex = disable();
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  if(*mutex)
  {
    enable();
    *mutex = 0;
  }
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return 0;
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return true;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_THREAD_DJGPP_H */

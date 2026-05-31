/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Functions for debugging mutexes.
 */

#ifndef MEGAZEUX_THREAD_DEBUG_H
#define MEGAZEUX_THREAD_DEBUG_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "platform.h"
#include "../util.h"

struct platform_mutex_debug
{
  char message[32];
  platform_thread_id lock_thread;
  boolean locked;
};

typedef struct platform_mutex_debug platform_mutex_debug;

static inline void platform_mutex_lock_debug(platform_mutex *mutex,
 platform_mutex *debug_mutex, platform_mutex_debug *dinfo,
 const char *file, int line)
{
  // lock may be held here, but it shouldn't be held by the current thread.
  platform_thread_id cur_thread = platform_get_thread_id();

  platform_mutex_lock(debug_mutex);

  if(dinfo->locked)
  {
    if(dinfo->lock_thread == cur_thread)
      warn("%s:%d (TID %zu): locked at %s (TID %zu) already!\n",
       file, line, (size_t)cur_thread, dinfo->message, (size_t)dinfo->lock_thread);
    else
      trace("%s:%d (TID %zu): waiting on mutex locked by %s (TID %zu)\n",
       file, line, (size_t)cur_thread, dinfo->message, (size_t)dinfo->lock_thread);
  }

  platform_mutex_unlock(debug_mutex);

  // acquire the mutex
  platform_mutex_lock(mutex);

  platform_mutex_lock(debug_mutex);

  // store information on this lock
  snprintf((char *)dinfo->message, 32, "%s:%d", file, line);
  dinfo->message[31] = '\0';

  dinfo->lock_thread = cur_thread;
  dinfo->locked = true;

  platform_mutex_unlock(debug_mutex);
}

static inline void platform_mutex_unlock_debug(platform_mutex *mutex,
 platform_mutex *debug_mutex, platform_mutex_debug *dinfo,
 const char *file, int line)
{
  platform_thread_id cur_thread = platform_get_thread_id();

  platform_mutex_lock(debug_mutex);

  // lock should be held here
  if(!dinfo->locked)
  {
    warn("%s:%d (TID %zu): tried to unlock when not locked!\n",
     file, line, (size_t)cur_thread);
  }
  else

  // ...but not by another thread.
  if(dinfo->lock_thread != cur_thread)
    warn("%s:%d (TID %zu): tried to unlock mutex held by %s (TID %zu)!\n",
     file, line, (size_t)cur_thread, dinfo->message, (size_t)dinfo->lock_thread);

  dinfo->locked = false;

  platform_mutex_unlock(debug_mutex);

  platform_mutex_unlock(mutex);
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_THREAD_DEBUG_H */

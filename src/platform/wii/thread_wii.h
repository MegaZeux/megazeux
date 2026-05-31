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

#ifndef MEGAZEUX_THREAD_WII_H
#define MEGAZEUX_THREAD_WII_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <limits.h>
#include <time.h>

#define BOOL _BOOL
#include <gctypes.h>
#include <ogc/mutex.h>
#include <ogc/cond.h>
#include <ogc/semaphore.h>
#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#undef BOOL

#define THREAD_RES void *
#define THREAD_RETURN do { return NULL; } while(0)

typedef cond_t platform_cond;
typedef mutex_t platform_mutex;
typedef sem_t platform_sem;
typedef lwp_t platform_thread;
typedef lwp_t platform_thread_id;
typedef THREAD_RES (*platform_thread_fn)(void *);

static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  if(LWP_MutexInit(mutex, false))
    return false;
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  if(LWP_MutexDestroy(*mutex))
    return false;
  return true;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  if(LWP_MutexLock(*mutex))
    return false;
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  if(LWP_MutexUnlock(*mutex))
    return false;
  return true;
}

static inline boolean platform_cond_init(platform_cond *cond)
{
  if(LWP_CondInit(cond))
    return false;
  return true;
}

static inline boolean platform_cond_destroy(platform_cond *cond)
{
  if(LWP_CondDestroy(*cond))
    return false;
  return true;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(LWP_CondWait(*cond, *mutex))
    return false;
  return true;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  struct timespec timeout;

  /**
   * FIXME: LWP_CondTimedWait, despite its documentation, currently takes a
   * relative timeout. Apparently this behavior is going to get changed in
   * libogc at some point to match the documentation, so until then, the
   * correct handling of this is commented out.
   *
   * https://github.com/devkitPro/libogc/issues/101
   */
  /*
  u64 ticks = gettime() + millisecs_to_ticks(timeout_ms);

  timeout.tv_sec = ticks_to_secs(ticks);
  timeout.tv_nsec = ticks_to_nanosecs(ticks) % 1000000000;
  */
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_nsec = (timeout_ms % 1000) / 1000000;

  if(LWP_CondTimedWait(*cond, *mutex, &timeout))
    return false;
  return true;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  if(LWP_CondSignal(*cond))
    return false;
  return true;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  if(LWP_CondBroadcast(*cond))
    return false;
  return true;
}

static inline boolean platform_sem_init(platform_sem *sem, unsigned init_value)
{
  if(LWP_SemInit(sem, init_value, INT_MAX))
    return false;
  return true;
}

static inline boolean platform_sem_destroy(platform_sem *sem)
{
  if(LWP_SemDestroy(*sem))
    return false;
  return true;
}

static inline boolean platform_sem_wait(platform_sem *sem)
{
  if(LWP_SemWait(*sem))
    return false;
  return true;
}

static inline boolean platform_sem_post(platform_sem *sem)
{
  if(LWP_SemPost(*sem))
    return false;
  return true;
}

static inline boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  if(LWP_CreateThread(thread, start_function, data, NULL, 0, 64))
    return false;
  return true;
}

static inline boolean platform_thread_join(platform_thread *thread)
{
  if(LWP_JoinThread(*thread, NULL))
    return false;
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return LWP_GetSelf();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return a == b;
}

static inline void platform_yield(void)
{
  LWP_YieldThread();
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_THREAD_WII_H */

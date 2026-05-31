/* MegaZeux
 *
 * Copyright (C) 2016, 2018 Adrian Siekierka <asiekierka@gmail.com>
 * Copyright (C) 2022 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_THREAD_DREAMCAST_H
#define MEGAZEUX_THREAD_DREAMCAST_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <kos.h>
#include <stdbool.h>

#define THREAD_RES void *
#define THREAD_RETURN do { return NULL; } while(0)

typedef condvar_t platform_cond;
typedef mutex_t platform_mutex;
typedef semaphore_t platform_sem;
typedef kthread_t *platform_thread;
typedef kthread_t *platform_thread_id;
typedef void *(*platform_thread_fn)(void *);

static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  if(mutex_init(mutex, MUTEX_TYPE_NORMAL))
    return false;
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  if(mutex_destroy(mutex))
    return false;
  return true;
}

static inline bool platform_mutex_lock(platform_mutex *mutex)
{
  if(mutex_trylock(mutex))
    return false;
  return true;
}

static inline bool platform_mutex_unlock(platform_mutex *mutex)
{
  if(mutex_unlock(mutex))
    return false;
  return true;
}

static inline boolean platform_cond_init(platform_cond *cond)
{
  if(cond_init(cond))
    return false;
  return true;
}

static inline boolean platform_cond_destroy(platform_cond *cond)
{
  if(cond_destroy(cond))
    return false;
  return true;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(cond_wait(cond, mutex))
    return false;
  return true;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  if(cond_wait_timed(cond, mutex, (int)timeout_ms))
    return false;
  return true;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  if(cond_signal(cond))
    return false;
  return true;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  if(cond_broadcast(cond))
    return false;
  return true;
}

static inline boolean platform_sem_init(platform_sem *sem, unsigned init_value)
{
  if(sem_init(sem, init_value))
    return false;
  return true;
}

static inline boolean platform_sem_destroy(platform_sem *sem)
{
  if(sem_destroy(sem))
    return false;
  return true;
}

static inline boolean platform_sem_wait(platform_sem *sem)
{
  if(sem_wait(sem))
    return false;
  return true;
}

static inline boolean platform_sem_post(platform_sem *sem)
{
  if(sem_signal(sem))
    return false;
  return true;
}

static inline boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  platform_thread ret = thd_create(0, start_function, data);
  if(ret)
  {
    *thread = ret;
    return true;
  }
  return false;
}

static inline boolean platform_thread_join(platform_thread *thread)
{
  if(thd_join(*thread, NULL))
    return false;
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return thd_get_current();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return a == b;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_THREAD_DREAMCAST_H */

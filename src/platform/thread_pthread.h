/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef __THREAD_PTHREAD_H
#define __THREAD_PTHREAD_H

#include "../compat.h"

__M_BEGIN_DECLS

#include <sched.h>
#include <semaphore.h>
#include <time.h>
#include "pthread.h"

#define THREAD_RES void *
#define THREAD_RETURN do { return NULL; } while(0)

typedef pthread_cond_t platform_cond;
typedef pthread_mutex_t platform_mutex;
typedef sem_t platform_sem;
typedef pthread_t platform_thread;
typedef pthread_t platform_thread_id;
typedef THREAD_RES (*platform_thread_fn)(void *);

static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  if(pthread_mutex_init(mutex, NULL))
    return false;
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  if(pthread_mutex_destroy(mutex))
    return false;
  return true;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  if(pthread_mutex_lock(mutex))
    return false;
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  if(pthread_mutex_unlock(mutex))
    return false;
  return true;
}

static inline boolean platform_cond_init(platform_cond *cond)
{
  if(pthread_cond_init(cond, NULL))
    return false;
  return true;
}

static inline boolean platform_cond_destroy(platform_cond *cond)
{
  if(pthread_cond_destroy(cond))
    return false;
  return true;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  if(pthread_cond_wait(cond, mutex))
    return false;
  return true;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned int timeout_ms)
{
  struct timespec timeout;

  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec  += (timeout_ms / 1000);
  timeout.tv_nsec += (timeout_ms % 1000) * 1000000;

  if(pthread_cond_timedwait(cond, mutex, &timeout))
    return false;
  return true;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  if(pthread_cond_signal(cond))
    return false;
  return true;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  if(pthread_cond_broadcast(cond))
    return false;
  return true;
}

static inline boolean platform_sem_init(platform_sem *sem, unsigned init_value)
{
  if(sem_init(sem, 0, init_value))
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
  if(sem_post(sem))
    return false;
  return true;
}

static inline boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  if(pthread_create(thread, NULL, start_function, data))
    return false;
  return true;
}

static inline boolean platform_thread_join(platform_thread *thread)
{
  if(pthread_join(*thread, NULL))
    return false;
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return pthread_self();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return pthread_equal(a, b) != 0;
}

static inline void platform_yield(void)
{
  sched_yield();
}

__M_END_DECLS

#endif // __THREAD_PTHREAD_H

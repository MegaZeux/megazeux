/* MegaZeux
 *
 * Copyright (C) 2020-2025 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Dummy threading functions to avoid ifdefs in the check alloc functions.
 * If your platform needs actual threading functions and SDL or pthread are
 * not available, see platform.h.
 */

#ifndef __THREAD_DUMMY_H
#define __THREAD_DUMMY_H

#include "../compat.h"

__M_BEGIN_DECLS

#define PLATFORM_NO_THREADING

#define THREAD_ERROR_MSG "Provide a valid thread.h implementation for this platform!"

#if defined(__clang__) && defined(__has_extension) && !defined(THREAD_ERROR)
#if __has_extension(attribute_unavailable_with_message)
#define THREAD_ERROR __attribute__((unavailable(THREAD_ERROR_MSG)))
#endif
#endif

#if defined(__GNUC__) && !defined(THREAD_ERROR) && \
 (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define THREAD_ERROR __attribute__((error(THREAD_ERROR_MSG)))
#endif

#ifndef THREAD_ERROR
// Just let it fail to link...
#define THREAD_ERROR
#endif

#define THREAD_RES void
#define THREAD_RETURN do { return; } while(0)

typedef int platform_cond;
typedef int platform_mutex;
typedef int platform_sem;
typedef int platform_thread;
typedef int platform_thread_id;
typedef THREAD_RES (*platform_thread_fn)(void *);

/**
 * This should pretty much only be used to get audio.c to build for platforms
 * that don't have proper mutexes. Don't add hacks like this for the other
 * functions in this file; anything relying on those functions WILL NOT WORK
 * without a proper threading/synchronization implementation.
 */
#ifdef THREAD_DUMMY_ALLOW_MUTEX
static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  *mutex = 1;
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  *mutex = 0;
  return true;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  return true;
}
#else
THREAD_ERROR
boolean platform_mutex_init(platform_mutex *mutex);

THREAD_ERROR
boolean platform_mutex_destroy(platform_mutex *mutex);

THREAD_ERROR
boolean platform_mutex_lock(platform_mutex *mutex);

THREAD_ERROR
boolean platform_mutex_unlock(platform_mutex *mutex);
#endif

THREAD_ERROR
boolean platform_cond_init(platform_cond *cond);

THREAD_ERROR
boolean platform_cond_destroy(platform_cond *cond);

THREAD_ERROR
boolean platform_cond_wait(platform_cond *cond, platform_mutex *mutex);

THREAD_ERROR
boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *muted, unsigned int timeout_ms);

THREAD_ERROR
boolean platform_cond_signal(platform_cond *cond);

THREAD_ERROR
boolean platform_cond_broadcast(platform_cond *cond);

THREAD_ERROR
boolean platform_sem_init(platform_sem *sem, unsigned init_value);

THREAD_ERROR
boolean platform_sem_destroy(platform_sem *sem);

THREAD_ERROR
boolean platform_sem_wait(platform_sem *sem);

THREAD_ERROR
boolean platform_sem_post(platform_sem *sem);

THREAD_ERROR
boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data);

THREAD_ERROR
boolean platform_thread_join(platform_thread *thread);

/**
 * Safe to use with no thread implementation (required by util.c).
 */
static inline platform_thread_id platform_get_thread_id(void)
{
  return 0;
}

/**
 * Safe to use with no thread implementation (required by util.c).
 */
static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return true;
}

__M_END_DECLS

#endif /* __THREAD_DUMMY_H */

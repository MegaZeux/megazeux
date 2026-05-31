/* MegaZeux
 *
 * Copyright (C) 2023 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Fallback Windows threading implementation. This uses Win32 function calls
 * which should work all the way back to Windows 95. Since Win32 didn't have
 * condition variables until Vista, this is pretty hacky and terrible!
 * Only use this when SDL isn't available, e.g. for the utils.
 */

#ifndef MEGAZEUX_THREAD_WIN32_H
#define MEGAZEUX_THREAD_WIN32_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define THREAD_RES DWORD WINAPI
#define THREAD_RETURN do { return 0; } while(0)

typedef DWORD platform_cond;
typedef CRITICAL_SECTION platform_mutex;
typedef HANDLE platform_sem;
typedef HANDLE platform_thread;
typedef DWORD platform_thread_id;
typedef THREAD_RES (*platform_thread_fn)(void *);

static inline boolean platform_mutex_init(platform_mutex *mutex)
{
  InitializeCriticalSection(mutex);
  return true;
}

static inline boolean platform_mutex_destroy(platform_mutex *mutex)
{
  DeleteCriticalSection(mutex);
  return true;
}

static inline boolean platform_mutex_lock(platform_mutex *mutex)
{
  EnterCriticalSection(mutex);
  return true;
}

static inline boolean platform_mutex_unlock(platform_mutex *mutex)
{
  LeaveCriticalSection(mutex);
  return true;
}

// TODO: Dynamically link Vista condvars or approximate with semaphores...
static inline boolean platform_cond_init(platform_cond *cond)
{
  return false;
}

static inline boolean platform_cond_destroy(platform_cond *cond)
{
  return false;
}

static inline boolean platform_cond_timedwait(platform_cond *cond,
 platform_mutex *mutex, unsigned time)
{
  abort();
  return false;
}

static inline boolean platform_cond_wait(platform_cond *cond,
 platform_mutex *mutex)
{
  abort();
  return false;
}

static inline boolean platform_cond_signal(platform_cond *cond)
{
  abort();
  return false;
}

static inline boolean platform_cond_broadcast(platform_cond *cond)
{
  abort();
  return false;
}

static inline boolean platform_sem_init(platform_sem *sem, unsigned init_value)
{
  HANDLE s = CreateSemaphore(NULL, init_value, LONG_MAX, NULL);
  if(s)
  {
    *sem = s;
    return true;
  }
  return false;
}

static inline boolean platform_sem_destroy(platform_sem *sem)
{
  return CloseHandle(*sem);
}

static inline boolean platform_sem_wait(platform_sem *sem)
{
  if(WaitForSingleObject(*sem, INFINITE) != WAIT_OBJECT_0)
    return false;
  return true;
}

static inline boolean platform_sem_post(platform_sem *sem)
{
  if(ReleaseSemaphore(*sem, 1, NULL) == 0)
    return false;
  return true;
}

static inline boolean platform_thread_create(platform_thread *thread,
 platform_thread_fn start_function, void *data)
{
  HANDLE t = CreateThread(NULL, 0, start_function, data, 0, NULL);
  if(t)
  {
    *thread = t;
    return true;
  }
  return false;
}

static inline boolean platform_thread_join(platform_thread *thread)
{
  if(WaitForSingleObject(*thread, INFINITE) != WAIT_OBJECT_0)
    return false;
  return true;
}

static inline platform_thread_id platform_get_thread_id(void)
{
  return GetCurrentThreadId();
}

static inline boolean platform_is_same_thread(platform_thread_id a,
 platform_thread_id b)
{
  return a == b;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_THREAD_WIN32_H */

/* MegaZeux
 *
 * Copyright (C) 2023-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "platform_time.h"
#include "../util.h"

#include <time.h>

#ifndef _MSC_VER
#include <unistd.h> /* _POSIX_TIMERS */
#include <sys/time.h> /* gettimeofday */
#endif

#if defined(CONFIG_DREAMCAST) && defined(_POSIX_TIMERS)
// KallistiOS may incorrectly define this to nothing when
// it should leave it undefined.
#undef _POSIX_TIMERS
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "platform_dso.h"

#define WINDOWS_TO_UNIX_SECONDS 11644473600LL
#define WINDOWS_TO_UNIX_100NS   10000000LL

static void convert_timestamp(int64_t *epoch, int32_t *nano,
 DWORD high, DWORD low)
{
  uint64_t full = ((uint64_t)high << 32) | (uint64_t)low;

  *epoch = (full / WINDOWS_TO_UNIX_100NS) - WINDOWS_TO_UNIX_SECONDS;
  *nano = (full % WINDOWS_TO_UNIX_100NS) * 100;
}

/**
 * Get the system timestamp using Win32 function calls directly.
 */
static boolean system_time_win32(int64_t *epoch, int32_t *nano)
{
  static struct dso_library *dll;
  static void (WINAPI *_GetSystemTimePreciseAsFileTime)(LPFILETIME) = NULL;
  static const struct dso_syms_map sym =
   { "GetSystemTimePreciseAsFileTime", { &_GetSystemTimePreciseAsFileTime } };
  static boolean init = false;
  FILETIME ft;

  if(!init)
  {
    // Note: can't unload, or the function pointer will no longer be valid.
    dll = platform_load_library("Kernel32.dll");
    if(dll)
      platform_load_function(dll, &sym);

    init = true;
  }

  if(_GetSystemTimePreciseAsFileTime)
  {
    /* Windows 8, precise to 100ns. */
    _GetSystemTimePreciseAsFileTime(&ft);
  }
  else
  {
    /* Windows 95, NT 3.5; precision not well defined. */
    GetSystemTimeAsFileTime(&ft);
  }

  convert_timestamp(epoch, nano, ft.dwHighDateTime, ft.dwLowDateTime);
  return true;
}
#endif /* _WIN32 */

/**
 * Get the system timestamp via `clock_gettime(CLOCK_REALTIME)`.
 * Disable for MinGW (implemented by winpthread); not supported by MSVC.
 * TODO: glibc prior to 2.17 requires manually linking librt, which can't
 * be linked safely currently (no equivalent to autoconf to check for it).
 */
static boolean system_time_clock_gettime(int64_t *epoch, int32_t *nano)
{
#if !defined(_WIN32) && defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && \
 (!defined(__GLIBC__) || !defined(__GLIBC_MINOR__) || __GLIBC__ >= 3 || \
  (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 17))
  struct timespec tp;

  if(clock_gettime(CLOCK_REALTIME, &tp))
    return false;

  *epoch = tp.tv_sec;
  *nano = (unsigned long)tp.tv_nsec % 1000000000;
  return true;
#else
  return false;
#endif
}

/**
 * Get the system timestamp via `gettimeofday`, which exists in most libc
 * implementations. Safe for MinGW (implemented by MinGW), not for MSVC.
 */
static boolean system_time_gettimeofday(int64_t *epoch, int32_t *nano)
{
#ifndef _MSC_VER
  struct timeval tv;

  if(gettimeofday(&tv, NULL))
    return false;

  *epoch = tv.tv_sec;
  *nano = ((unsigned long)tv.tv_usec % 1000000) * 1000;
  return true;
#else
  return false;
#endif
}

/**
 * Use `time` as a fallback :(. Modern implementations, even for 32-bit
 * platforms, typically use a 64-bit `time_t` now.
 */
static void system_time_fallback(int64_t *epoch, int32_t *nano)
{
  *epoch = time(NULL);
  *nano = 0;
}

/**
 * Get the system clock time in the system clock timezone.
 * All parameters must be present and will be written to.
 *
 * @param tm    destination for `localtime` info. May be 0-filled on failure.
 * @param epoch destination for seconds since Jan 1 1970.
 * @param nano  destination for nanoseconds since Jan 1 1970.
 * @return      `true` if all fields are filled and have microsecond precision,
 *              otherwise `false`. Nanosecond precision is not reliable.
 */
boolean platform_system_time(struct tm *tm, int64_t *epoch, int32_t *nano)
{
  struct tm *local;
  boolean ret;
  time_t tmp;

#ifdef _WIN32
  if(system_time_win32(epoch, nano))
  {
    ret = true;
  }
  else
#endif

  if(system_time_clock_gettime(epoch, nano))
  {
    ret = true;
  }
  else

  if(system_time_gettimeofday(epoch, nano))
  {
    ret = true;
  }
  else
  {
    system_time_fallback(epoch, nano);
    ret = false;
  }

  tmp = *epoch;
  local = localtime(&tmp);
  if(local)
  {
    memcpy(tm, local, sizeof(struct tm));
    return ret;
  }
  else
  {
    memset(tm, 0, sizeof(struct tm));
    return false;
  }
}

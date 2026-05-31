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

#ifndef MEGAZEUX_PLATFORM_THREAD_H
#define MEGAZEUX_PLATFORM_THREAD_H

/**
 * Audio, networking, and other misc. optional features require threading
 * support. Include a platform-specific thread header here if it's available.
 * If not, a dummy implementation will be included.
 *
 * Most of the dummy functions will emit errors when used.
 * If this happens, implement proper threading functions for that platform.
 */
#ifdef CONFIG_PTHREAD
#include "thread_pthread.h"
#elif defined(CONFIG_3DS)
#include "3ds/thread_3ds.h"
#elif defined(CONFIG_DJGPP)
#include "djgpp/thread_djgpp.h"
#elif defined(CONFIG_DREAMCAST)
#include "dreamcast/thread_dreamcast.h"
#elif defined(CONFIG_WII)
#include "wii/thread_wii.h"
#elif defined(CONFIG_SDL) && !defined(SKIP_SDL)
#include "sdl/thread_sdl.h"
#elif defined(_WIN32) /* Fallback, prefer SDL when possible. */
#include "win32/thread_win32.h"
#else
#if defined(CONFIG_NDS)
#define THREAD_DUMMY_ALLOW_MUTEX
#endif
#include "thread_dummy.h"
#endif

#endif /* MEGAZEUX_PLATFORM_THREAD_H */

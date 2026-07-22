/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_ANDROID_LOG_ANDROID_H
#define MEGAZEUX_PLATFORM_ANDROID_LOG_ANDROID_H

#include "../../compat.h"

#include <android/log.h>

#define info(...)  __android_log_print(ANDROID_LOG_INFO, "MegaZeux", __VA_ARGS__)
#define warn(...)  __android_log_print(ANDROID_LOG_WARN, "MegaZeux", __VA_ARGS__)

#ifdef DEBUG
#define debug(...) __android_log_print(ANDROID_LOG_DEBUG, "MegaZeux", __VA_ARGS__)
#else
#define debug(...) do { } while(0)
#endif

#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...) __android_log_print(ANDROID_LOG_VERBOSE, "MegaZeux", __VA_ARGS__)
#else
#define trace(...) do { } while(0)
#endif

#endif /* MEGAZEUX_PLATFORM_ANDROID_LOG_ANDROID_H */

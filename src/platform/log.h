/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2019-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_LOG_H
#define MEGAZEUX_PLATFORM_LOG_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_STDIO_REDIRECT
CORE_LIBSPEC extern FILE *mzxout_h;
CORE_LIBSPEC extern FILE *mzxerr_h;
#define mzxout (mzxout_h ? mzxout_h : stdout)
#define mzxerr (mzxerr_h ? mzxerr_h : stderr)
#else
#define mzxout stdout
#define mzxerr stderr
#endif

CORE_LIBSPEC boolean redirect_stdio_init(const char *base_path, boolean require_conf);
CORE_LIBSPEC void redirect_stdio_exit(void);

#ifdef DEBUG_TRACE
#define LOG_LINE_STR2(x) #x
#define LOG_LINE_STR(x) LOG_LINE_STR2(x)
#define LOG_PREFIX(s) s " " __FILE__ ":" LOG_LINE_STR(__LINE__) ": "
#else
#define LOG_PREFIX(s) s ": "
#endif

#if defined(ANDROID)
#include "android/log_android.h"
#elif defined(CONFIG_DREAMCAST)
#include "dreamcast/log_dreamcast.h"
#else

#include <stdio.h>

#define info(...) \
 do { \
   fprintf(mzxout, LOG_PREFIX("INFO") __VA_ARGS__); \
   fflush(mzxout); \
 } while(0)

#define warn(...) \
 do { \
   fprintf(mzxerr, LOG_PREFIX("WARNING") __VA_ARGS__); \
   fflush(mzxerr); \
 } while(0)

#ifdef DEBUG
#define debug(...) \
 do { \
   fprintf(mzxerr, LOG_PREFIX("DEBUG") __VA_ARGS__); \
   fflush(mzxerr); \
 } while(0)
#else
#define debug(...) do { } while(0)
#endif

#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...) \
 do { \
    fprintf(mzxerr, LOG_PREFIX("TRACE") __VA_ARGS__); \
    fflush(mzxerr); \
 } while(0)
#else
#define trace(...) do { } while(0)
#endif

#endif /* !ANDROID && !KOS */

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_LOG_H */

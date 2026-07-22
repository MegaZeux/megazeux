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

#ifndef MEGAZEUX_PLATFORM_DREAMCAST_LOG_DREAMCAST_H
#define MEGAZEUX_PLATFORM_DREAMCAST_LOG_DREAMCAST_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

/* KallistiOS has some of the messiest public headers of any console SDK,
 * and of course, they didn't bother providing a va_list version of
 * dbgio_printf, so it can't be isolated to its own compilation unit.
 * Redeclare these instead of letting their mess into every compilation unit
 * that needs logging. This may break in the future.
 */
//#include <kos.h>

#include "../platform_attribute.h"

int dbgio_printf(const char *fmt, ...) ATTRIBUTE_PRINTF(1,2);
int dbgio_flush(void);

#define info(...) \
 do { \
   dbgio_printf(LOG_PREFIX("INFO") __VA_ARGS__); \
   dbgio_flush(); \
 } while(0)

#define warn(...) \
 do { \
   dbgio_printf(LOG_PREFIX("WARNING") __VA_ARGS__); \
   dbgio_flush(); \
 } while(0)

#ifdef DEBUG
#define debug(...) \
 do { \
   dbgio_printf(LOG_PREFIX("DEBUG") __VA_ARGS__); \
   dbgio_flush(); \
 } while(0)
#else
#define debug(...) do { } while(0)
#endif

#if defined(DEBUG) && defined(DEBUG_TRACE)
#define trace(...) \
  do { \
    dbgio_printf(LOG_PREFIX("TRACE") __VA_ARGS__); \
    dbgio_flush(); \
  } while(0)
#else
#define trace(...) do { } while(0)
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_DREAMCAST_LOG_DREAMCAST_H */

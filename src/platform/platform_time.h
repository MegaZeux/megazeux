/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_TIME_H
#define MEGAZEUX_PLATFORM_TIME_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>
#include <time.h>

/* Defined in platform_[platform].c */
CORE_LIBSPEC void delay(uint32_t ms);
CORE_LIBSPEC uint64_t get_ticks(void);

/* Defined in platform_time.c */
CORE_LIBSPEC boolean platform_system_time(struct tm *tm,
 int64_t *epoch, int32_t *nano);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_TIME_H */

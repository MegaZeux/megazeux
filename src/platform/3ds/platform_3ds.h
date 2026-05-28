/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __3DS_PLATFORM_H__
#define __3DS_PLATFORM_H__

#include "../../compat.h"

#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>

__M_BEGIN_DECLS

boolean ctr_is_2d(void);
boolean ctr_supports_wide(void);

#ifdef CONFIG_CHECK_ALLOC

#include <stddef.h>

#define clinearAlloc(size, alignment) \
 check_linearAlloc(size, alignment, __FILE__, __LINE__)

void *check_linearAlloc(size_t size, size_t alignment, const char *file,
 int line);

#else

static inline void *clinearAlloc(size_t size)
{
  return linearAlloc(size);
}

#endif /* CONFIG_CHECK_ALLOC */

__M_END_DECLS

#endif /* __3DS_PLATFORM_H__ */

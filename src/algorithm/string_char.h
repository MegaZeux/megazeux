/* MegaZeux
 *
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

#ifndef MEGAZEUX_ALGORITHM_STRING_CHAR_H
#define MEGAZEUX_ALGORITHM_STRING_CHAR_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../platform/platform_attribute.h"

UTILS_LIBSPEC extern const unsigned char mzx_tolower_table[256];

#define mzx_tolower(d)  mzx_tolower_table[(unsigned char)(d)]
#define memcasechr      mzx_memcasechr

/* memchr implementations in some C libraries are slow enough that
 * reimplementation is a significant performance boost. This problem
 * exists in MSVCRT, UCRT, and probably the other Windows CRTs. */
#if defined(_WIN32)
#define memchr          mzx_memchr
#endif

UTILS_LIBSPEC const void *mzx_memchr(const void *haystack,
 unsigned char needle, size_t len) ATTRIBUTE_NONNULL;
UTILS_LIBSPEC const void *mzx_memcasechr(const void *haystack,
 unsigned char needle, size_t len) ATTRIBUTE_NONNULL;

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_STRING_CHAR_H */

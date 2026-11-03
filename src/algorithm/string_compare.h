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

#ifndef MEGAZEUX_ALGORITHM_STRING_COMPARE_H
#define MEGAZEUX_ALGORITHM_STRING_COMPARE_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

UTILS_LIBSPEC extern const unsigned char mzx_tolower_table[256];

#define mzx_tolower(d)  mzx_tolower_table[(unsigned char)(d)]
#define memcasecmp      mzx_memcasecmp
#define memcasecmp32    mzx_memcasecmp32

//#if defined(_WIN32) && defined(__STRICT_ANSI__)
#define strcasecmp      mzx_strcasecmp
#define strncasecmp     mzx_strncasecmp
//#endif

UTILS_LIBSPEC int mzx_strcasecmp(const char *a, const char *b);
UTILS_LIBSPEC int mzx_strncasecmp(const char *a, const char *b, size_t n);
UTILS_LIBSPEC int mzx_memcasecmp(const void *a, const void *b, size_t n);
UTILS_LIBSPEC int mzx_memcasecmp32(const void *a, const void *b, size_t n);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_STRING_COMPARE_H */

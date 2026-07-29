/* MegaZeux
 *
 * Copyright (C) 2022-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_IMAGE_COMMON_H
#define MEGAZEUX_IO_IMAGE_COMMON_H

/* Force dependency tracking even though this doesn't use core MZX headers. */
#ifndef MEGAZEUX_IO_STANDALONE
#include "../compat.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char image_bool;

enum image_bool_values
{
  IMAGE_FALSE,
  IMAGE_TRUE
};

#if defined(_MSC_VER) && _MSC_VER < 1400
static void imagedbg(...) {}
static void imagenodbg(...) {}
#else
#define imagedbg(...) \
 do { fprintf(stderr, __VA_ARGS__); fflush(stderr); } while(0)
#define imagenodbg(...) do {} while(0)
#endif

#ifndef RESTRICT
#if (defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1400)) || \
    (defined(__WATCOMC__) && (__WATCOMC__ >= 1250) && !defined(__cplusplus))
#define RESTRICT __restrict
#else
#define RESTRICT
#endif
#endif

#undef ARRAY_SIZE
#undef IMAGE_MAX
#undef IMAGE_MIN
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define IMAGE_MAX(a,b) ((a) > (b) ? (a) : (b))
#define IMAGE_MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef __cplusplus
}
#endif

#endif /* MEGAZEUX_IO_IMAGE_COMMON_H */

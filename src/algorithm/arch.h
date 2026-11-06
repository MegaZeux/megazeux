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

#ifndef MEGAZEUX_ALGORITHM_ARCH_H
#define MEGAZEUX_ALGORITHM_ARCH_H

/* TODO: replace */
#include "../platform/platform_endian.h"

#include <stdint.h>

/* Fast integer type defines. */
#if ARCHITECTURE_BITS >= 64
typedef uint64_t arch_uint;
#define ARCH_UINT_MODULO        ALIGN_64_MODULO
#define ARCH_32_OR_64(x32,x64)  (x64)
#else
typedef uint32_t arch_uint;
#define ARCH_UINT_MODULO        ALIGN_32_MODULO
#define ARCH_32_OR_64(x32,x64)  (x32)
#endif

#define ARCH_UINT_TRUNC_CAST(p) ((arch_uint *)((size_t)(p) & ~(sizeof(arch_uint) - 1)))
#define ARCH_UINT_UNSAFE_CAST(p) ((arch_uint *)(p))

#endif /* MEGAZEUX_ALGORITHM_ARCH_H */

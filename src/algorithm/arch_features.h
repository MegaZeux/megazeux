/* MegaZeux
 *
 * Copyright (C) 2024-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_ALGORITHM_ARCH_FEATURES_H
#define MEGAZEUX_ALGORITHM_ARCH_FEATURES_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

MZX_LIBIO boolean mzx_architecture_has_sse2(void);
MZX_LIBIO boolean mzx_architecture_has_avx(void);
MZX_LIBIO boolean mzx_architecture_has_avx2(void);
MZX_LIBIO boolean mzx_architecture_has_neon(void);
MZX_LIBIO boolean mzx_architecture_has_sve(void);
MZX_LIBIO boolean mzx_architecture_has_rvv(void);
MZX_LIBIO boolean mzx_architecture_has_altivec(void);
MZX_LIBIO boolean mzx_architecture_has_altivec_vsx(void);
MZX_LIBIO boolean mzx_architecture_has_lsx(void);
MZX_LIBIO boolean mzx_architecture_has_lasx(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_ARCH_FEATURES_H */

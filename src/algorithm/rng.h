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

#ifndef MEGAZEUX_ALGORITHM_RNG_H
#define MEGAZEUX_ALGORITHM_RNG_H

#include "../compat.h"
#include "../platform/platform_attribute.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

#define MZX_RNG_MAX ((uint64_t)UINT32_MAX + 1)

struct mzx_rng_state
{
  uint64_t state;
};

UTILS_LIBSPEC void mzx_rng_init(struct mzx_rng_state *rng) ATTRIBUTE_NONNULL;
UTILS_LIBSPEC uint64_t mzx_rng_get_seed(const struct mzx_rng_state *rng)
 ATTRIBUTE_NONNULL;
UTILS_LIBSPEC void mzx_rng_set_seed(struct mzx_rng_state *rng, uint64_t value)
 ATTRIBUTE_NONNULL;
UTILS_LIBSPEC uint32_t mzx_rng_get_random(struct mzx_rng_state *rng, uint64_t range)
 ATTRIBUTE_NONNULL;

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_RNG_H */

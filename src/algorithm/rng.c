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

#include "rng.h"

#include <time.h>

void mzx_rng_init(struct mzx_rng_state *rng)
{
  rng->state = ((uint64_t)time(NULL) << 32) | clock();
}

uint64_t mzx_rng_get_seed(const struct mzx_rng_state *rng)
{
  return rng->state;
}

void mzx_rng_set_seed(struct mzx_rng_state *rng, uint64_t value)
{
  rng->state = value;
}

/**
 * Get a random value between 0 and (range - 1). This function steps the
 * pRNG state once.
 *
 * @param rng     pRNG state.
 * @param range   maximum value plus 1. Valid values are between 0 and
 *                (UINT32_MAX + 1); higher values do not work.
 * @return        a random value between 0 and (range - 1).
 */
uint32_t mzx_rng_get_random(struct mzx_rng_state *rng, uint64_t range)
{
  /* via Sebastiano Vigna (2016):
   * https://vigna.di.unimi.it/ftp/papers/xorshift.pdf
   */
  uint64_t x = rng->state;
  x += (x == 0); /* Do not allow the PRNG to get stuck */
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  rng->state = x;

  return (((x * 0x2545f4914f6cdd1dull) >> 32) * range) >> 32;
}

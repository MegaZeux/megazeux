/* MegaZeux
 *
 * Copyright (C) 2021-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "arch.h"
#include "string_xor.h"

/**
 * XOR every byte in a block of memory by the same 8-bit value.
 *
 * "memxor" is already a GNU function for XORing two blocks of memory, hence
 * "memxorchr". Argument order is deliberately the same as memset().
 *
 * @param dest      pointer to block of memory to XOR.
 * @param xor_chr   value to XOR the block of memory by.
 * @param dest_len  length of block of memory to XOR.
 */
void mzx_memxorchr(void *dest, uint8_t xor_chr, size_t dest_len)
{
  uint8_t *pos = (uint8_t *)dest;
  uint8_t *end = pos + dest_len;

  while(pos < end && ARCH_PTR_MODULO(pos, ARCH_UINT_MODULO))
    *(pos++) ^= xor_chr;

  if(pos < end)
  {
    arch_uint xor_w = xor_chr * ARCH_32_OR_64(0x01010101u, 0x0101010101010101ull);
    arch_uint *pos_w = (arch_uint *)pos;
    arch_uint *end_w = ARCH_UINT_TRUNC_CAST(end);

    while(pos_w < end_w)
      *(pos_w++) ^= xor_w;

    pos = (unsigned char *)pos_w;
  }

  while(pos < end)
    *(pos++) ^= xor_chr;
}

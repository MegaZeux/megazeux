/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_BITSTREAM_H
#define MEGAZEUX_IO_BITSTREAM_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <inttypes.h>
#include <stdio.h>

#include "../algorithm/arch.h"

/**
 * Most zip compression methods require treating the input as a stream of
 * bits rather than bytes.
 */

#define BS_CAPACITY ARCHITECTURE_BITS

struct bitstream
{
  arch_uint buf;
  arch_uint buf_left;
  const uint8_t *input;
  size_t input_left;
};

static inline boolean bs_fill(struct bitstream *b)
{
  if(b->input_left)
  {
#if ARCH_BYTE_ORDER == ARCH_LIL_ENDIAN

#if ARCHITECTURE_BITS >= 64
    if(ARCH_PTR_MODULO(b->input, ARCH_INT32_MODULO) == 0 &&
     b->buf_left <= (BS_CAPACITY - 32) && b->input_left >= 4)
    {
      b->buf |= ((arch_uint)*((uint32_t *)b->input)) << b->buf_left;
      b->buf_left += 32;
      b->input += 4;
      b->input_left -= 4;
    }
    else
#endif

    if(ARCH_PTR_MODULO(b->input, ARCH_INT16_MODULO) == 0 &&
     b->buf_left <= (BS_CAPACITY - 16) && b->input_left >= 2)
    {
      b->buf |= ((arch_uint)*((uint16_t *)b->input)) << b->buf_left;
      b->buf_left += 16;
      b->input += 2;
      b->input_left -= 2;
    }
    else
#endif
    while(b->buf_left <= (BS_CAPACITY - 8) && b->input_left)
    {
      b->buf |= ((arch_uint)*(b->input)) << b->buf_left;
      b->buf_left += 8;
      b->input++;
      b->input_left--;
    }

    if(!b->input_left)
      b->input = NULL;

    return true;
  }
  return false;
}

static inline int bs_read(struct bitstream *b, arch_uint mask, arch_uint bits)
{
  arch_uint tmp;
  if(b->buf_left < bits)
    if(!bs_fill(b) || b->buf_left < bits)
      return EOF;

  tmp = (b->buf & mask);
  b->buf >>= bits;
  b->buf_left -= bits;
  return tmp;
}

#define BS_READ(b,bits) bs_read(b, (0xFFFF >> (16 - bits)), bits)

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_BITSTREAM_H */

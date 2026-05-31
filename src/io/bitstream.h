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

#include "../platform/platform_endian.h"

/**
 * Most zip compression methods require treating the input as a stream of
 * bits rather than bytes.
 */

#if ARCHITECTURE_BITS >= 64
#define BS_CAPACITY 64
typedef uint64_t BS_BUFTYPE;
#else
#define BS_CAPACITY 32
typedef uint32_t BS_BUFTYPE;
#endif

struct bitstream
{
  BS_BUFTYPE buf;
  BS_BUFTYPE buf_left;
  const uint8_t *input;
  size_t input_left;
};

static inline boolean bs_fill(struct bitstream *b)
{
  if(b->input_left)
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
    size_t align = (size_t)b->input % ALIGN_64_MODULO;

#if ARCHITECTURE_BITS >= 64
    if(!align && b->buf_left == 0 && b->input_left >= 8)
    {
      b->buf = ((BS_BUFTYPE)*((uint64_t *)b->input));
      b->buf_left += 64;
      b->input += 8;
      b->input_left -= 8;
    }
    else
#endif
    if(!(align % ALIGN_32_MODULO) && b->buf_left <= (BS_CAPACITY - 32) && b->input_left >= 4)
    {
      b->buf |= ((BS_BUFTYPE)*((uint32_t *)b->input)) << b->buf_left;
      b->buf_left += 32;
      b->input += 4;
      b->input_left -= 4;
    }
    else

    if(!(align % ALIGN_16_MODULO) && b->buf_left <= (BS_CAPACITY - 16) && b->input_left >= 2)
    {
      b->buf |= ((BS_BUFTYPE)*((uint16_t *)b->input)) << b->buf_left;
      b->buf_left += 16;
      b->input += 2;
      b->input_left -= 2;
    }
    else
#endif
    while(b->buf_left <= (BS_CAPACITY - 8) && b->input_left)
    {
      b->buf |= ((BS_BUFTYPE)*(b->input)) << b->buf_left;
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

static inline int bs_read(struct bitstream *b, BS_BUFTYPE mask, BS_BUFTYPE bits)
{
  BS_BUFTYPE tmp;
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

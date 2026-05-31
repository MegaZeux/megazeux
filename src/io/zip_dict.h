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

#ifndef MEGAZEUX_ZIP_DICT_H
#define MEGAZEUX_ZIP_DICT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../util.h"

/**
 * Copy a block of memory from an earlier portion of an output stream to the
 * current position in the output stream. If the specified distance would
 * result in a negative offset relative to the start pointer, the bytes at
 * negative offsets are treated as 0. No other bounds checks are performed here.
 */
static inline void sliding_dictionary_copy(uint8_t *start, uint8_t **_pos,
 uint32_t distance, uint32_t length)
{
  uint8_t *pos = *_pos;
  ptrdiff_t copy_offset = (pos - distance) - start;
  uint32_t i;

  if(copy_offset < 0)
  {
    // Treat bytes at negative offsets as having value 0...
    uint32_t fill_amount = MIN((uint32_t)(-copy_offset), length);

    for(i = 0; i < fill_amount; i++)
      pos[i] = 0;

    pos += fill_amount;
    length -= fill_amount;
    copy_offset = 0;
  }

  if(length > 0)
  {
    // NOTE: These algorithms rely on moving dictionary values that might not
    // exist until mid-copy, so memcpy is not safe to use here.
    for(i = 0; i < length; i++)
      pos[i] = start[copy_offset + i];

    pos += length;
  }
  *_pos = pos;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_DICT_H */

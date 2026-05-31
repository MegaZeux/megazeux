/* MegaZeux
 *
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Reduce (zip compression methods 2-5) decompressor.
 */

#ifndef MEGAZEUX_ZIP_REDUCE_H
#define MEGAZEUX_ZIP_REDUCE_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <inttypes.h>

#include "bitstream.h"
#include "zip_dict.h"
#include "zip_stream.h"

#include "../util.h"

struct reduce_stream_data
{
  struct zip_stream_data zs;
  struct bitstream b;
  uint8_t factor;
};

/**
 * Allocate an expanding stream.
 */
static inline struct zip_stream_data *reduce_ex_create(void)
{
  return cmalloc(sizeof(struct reduce_stream_data));
}

/**
 * Open an expanding stream.
 */
static inline void reduce_ex_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  struct reduce_stream_data *rs = ((struct reduce_stream_data *)zs);
  memset(zs, 0, sizeof(struct reduce_stream_data));
  rs->factor = CLAMP((method - 2), 0, 3);
}

/**
 * Return the number of bits required to store an index for a follower set of
 * a given length, or 0 if the follower set is of length 0.
 */
static inline uint8_t follower_set_bits_required(int length)
{
  int i;
  if(length < 1)
    return 0;

  length--;
  for(i = 1; i < 8; i++)
    if(length < (1 << i))
      return i;
  return i;
}

#define REDUCE_DLE (144)
#define REDUCE_BUFFER_SIZE (8192)
#define SET(ch) (sets + (ch * 32))

/**
 * Expand the entire input buffer into the output buffer as a complete file.
 */
static inline enum zip_error reduce_ex_file(struct zip_stream_data *zs)
{
  struct reduce_stream_data *rs = ((struct reduce_stream_data *)zs);
  struct bitstream *b = &(rs->b);
  uint8_t factor = rs->factor;
  uint8_t n[256];
  uint8_t *buffer = cmalloc((REDUCE_BUFFER_SIZE + 32 * 256) * sizeof(uint8_t));
  uint8_t *sets = buffer + REDUCE_BUFFER_SIZE;
  uint8_t *set;

  uint8_t *start = zs->output_buffer;
  uint8_t *pos = zs->output_buffer;
  uint8_t *end = zs->output_buffer + zs->output_length;

  uint32_t buffer_pos;
  uint32_t buffer_stop;
  uint32_t length = 0;
  uint32_t distance = 0;
  uint8_t last_character;
  uint8_t state;
  boolean eof = false;
  int value;
  int i;
  int j;

  if(zs->finished)
    return ZIP_STREAM_FINISHED;

  if(!zs->output_buffer)
    return ZIP_OUTPUT_FULL;

  if(!zs->input_buffer)
    return ZIP_INPUT_EMPTY;

  b->input = zs->input_buffer;
  b->input_left = zs->input_length;
  zs->final_input_length = zs->input_length;
  zs->input_buffer = zs->output_buffer = NULL;
  zs->input_length = zs->output_length = 0;

  // Read the follower sets from the input buffer.
  for(i = 255; i >= 0; i--)
  {
    value = BS_READ(b, 6);
    if(value < 0)
      goto err_free;

    n[i] = follower_set_bits_required(value);
    set = SET(i);
    for(j = 0; j < value; j++)
      set[j] = BS_READ(b, 8);
    for(j = value; j < 32; j++)
      set[j] = 0;
  }

  last_character = 0;
  state = 0;
  while(pos < end)
  {
    // Abort if this is the end of the stream...
    if(eof)
      break;

    // Stage 1: probabilistic decompression using follower sets.
    buffer_pos = 0;
    while(buffer_pos < REDUCE_BUFFER_SIZE)
    {
      if(!n[last_character])
      {
        value = BS_READ(b, 8);
        if(value < 0)
        {
          eof = true;
          break;
        }

        last_character = value;
      }
      else
      {
        value = BS_READ(b, 1);
        if(value < 0)
        {
          eof = true;
          break;
        }

        if(value)
        {
          value = BS_READ(b, 8);
          if(value < 0)
          {
            eof = true;
            break;
          }

          last_character = value;
        }
        else
        {
          value = BS_READ(b, n[last_character]);
          if(value < 0)
          {
            eof = true;
            break;
          }

          last_character = SET(last_character)[value];
        }
      }
      buffer[buffer_pos++] = last_character;
    }

    // Stage 2: expand intermediate buffer.
    buffer_stop = buffer_pos;
    buffer_pos = 0;
    while(pos < end && buffer_pos < buffer_stop)
    {
      uint8_t ch = buffer[buffer_pos++];
      switch(state)
      {
        case 0:
        {
          if(ch != REDUCE_DLE)
          {
            *(pos++) = ch;
          }
          else
            state = 1;
          break;
        }

        case 1:
        {
          if(ch)
          {
            uint8_t lower_mask = 0x7F >> factor;
            uint8_t upper_mask = 0xFF ^ lower_mask;

            distance = (uint32_t)(ch & upper_mask) << (factor + 1);
            length = (ch & lower_mask);
            state = (length == lower_mask) ? 2 : 3;
          }
          else
          {
            *(pos++) = REDUCE_DLE;
            state = 0;
          }
          break;
        }

        case 2:
        {
          length += ch;
          state = 3;
          break;
        }

        case 3:
        {
          distance += ch + 1;
          length += 3;

          if(pos + length > end)
            goto err_free;

          sliding_dictionary_copy(start, &pos, distance, length);
          state = 0;
          break;
        }
      }
    }
  }
  free(buffer);

  zs->final_output_length = pos - start;
  zs->finished = true;
  return ZIP_STREAM_FINISHED;

err_free:
  free(buffer);
  return ZIP_DECOMPRESS_FAILED;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_REDUCE_H */

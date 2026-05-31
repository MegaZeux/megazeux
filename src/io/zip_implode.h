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
 * Implode (zip compression method 6) decompressor.
 */

#ifndef MEGAZEUX_ZIP_IMPLODE_H
#define MEGAZEUX_ZIP_IMPLODE_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <inttypes.h>

#include "bitstream.h"
#include "zip.h"
#include "zip_dict.h"
#include "zip_stream.h"

struct explode_stream_data
{
  struct zip_stream_data zs;
  struct bitstream b;
  struct SF_tree *literal_tree;
  struct SF_tree *length_tree;
  struct SF_tree *distance_tree;
  uint8_t minimum_match_length;
  boolean has_8k_dictionary;
  boolean has_literal_tree;
};

#define REVERSE_BITS(u_16) \
( \
  ((u_16 & 0x8000) >> 15) | \
  ((u_16 & 0x4000) >> 13) | \
  ((u_16 & 0x2000) >> 11) | \
  ((u_16 & 0x1000) >>  9) | \
  ((u_16 & 0x0800) >>  7) | \
  ((u_16 & 0x0400) >>  5) | \
  ((u_16 & 0x0200) >>  3) | \
  ((u_16 & 0x0100) >>  1) | \
  ((u_16 & 0x0080) <<  1) | \
  ((u_16 & 0x0040) <<  3) | \
  ((u_16 & 0x0020) <<  5) | \
  ((u_16 & 0x0010) <<  7) | \
  ((u_16 & 0x0008) <<  9) | \
  ((u_16 & 0x0004) << 11) | \
  ((u_16 & 0x0002) << 13) | \
  ((u_16 & 0x0001) << 15)   \
)

// Struct for Shannon-Fano compressed tree pairs.
struct SF_pair
{
  uint8_t length;
  uint8_t count;
};

// Struct for expanded Shannon-Fano tree leaves when constructing the table.
struct SF_value
{
  uint8_t length;
  uint16_t value;
};

// Final tree struct for the Shannon-Fano tree.
struct SF_tree
{
  union
  {
    uint16_t next[2];
    struct
    {
      uint16_t is_value;
      uint16_t value;
    } v;
  } data;
};

#define SF_TREE_VALUE 0xFFFF
#define IS_VALUE(sft) (sft.data.v.is_value == SF_TREE_VALUE)

static inline int sort_sf_list(const void *A, const void *B)
{
  const struct SF_value *a = (const struct SF_value *)A;
  const struct SF_value *b = (const struct SF_value *)B;
  return (a->length != b->length) ? a->length - b->length : a->value - b->value;
}

/**
 * Read an imploded Shannon-Fano tree from a bitstream.
 * NOTE: this will fail unless the entire tree is buffered in the bitstream.
 */
static inline enum zip_error expl_SF_read_tree(struct bitstream *b,
 struct SF_tree **output)
{
  struct SF_pair *data = NULL;
  struct SF_value *list = NULL;
  struct SF_tree *tree = NULL;
  uint16_t *codes = NULL;

  int tree_data_len = BS_READ(b, 8) + 1;
  int tree_list_len = 0;
  int i, j;

  if(tree_data_len == 0 || tree_data_len > 256)
    goto err_free;

  // Read the raw tree data.
  data = cmalloc(sizeof(struct SF_pair) * tree_data_len);
  for(i = 0; i < tree_data_len; i++)
  {
    int value = BS_READ(b, 8);
    if(value == EOF)
      goto err_free;

    data[i].length = (value & 0x0F) + 1;
    data[i].count  = ((value & 0xF0) >> 4) + 1;
    tree_list_len += data[i].count;
  }

  list = cmalloc(sizeof(struct SF_value) * tree_list_len);

  // Build the initial node list.
  {
    uint16_t value = 0;
    uint16_t pos = 0;
    for(i = 0; i < tree_data_len; i++)
    {
      for(j = 0; j < (int)data[i].count; j++)
      {
        list[pos].length = data[i].length;
        list[pos].value = value++;
        pos++;
      }
    }
  }
  free(data);
  data = NULL;

  qsort(list, tree_list_len, sizeof(struct SF_value), sort_sf_list);

  codes = cmalloc(sizeof(uint16_t) * tree_list_len);

  // Calculate the SF codes.
  {
    uint16_t code = 0;
    uint16_t code_inc = 0;
    uint8_t last_length = 0;

    for(i = tree_list_len - 1; i >= 0; i--)
    {
      code += code_inc;
      if(list[i].length != last_length)
      {
        last_length = list[i].length;
        code_inc = 1 << (16 - last_length);
      }
      codes[i] = REVERSE_BITS(code);
    }
  }

  // Allocate twice as many nodes as there are leaves for now...
  tree = ccalloc(tree_list_len * 2, sizeof(struct SF_tree));

  // Construct the searchable tree using the list.
  {
    uint16_t count = 1;

    for(i = 0; i < tree_list_len; i++)
    {
      uint16_t pos = 0;
      uint16_t next_pos;

      for(j = 0; j < list[i].length; j++)
      {
        uint8_t bit = (codes[i] >> j) & 1;

        if(IS_VALUE(tree[pos]))
          goto err_free;

        next_pos = tree[pos].data.next[bit];

        if(!next_pos)
        {
          if(count >= tree_list_len * 2)
            goto err_free;

          tree[pos].data.next[bit] = count;
          next_pos = count;
          count++;
        }
        pos = next_pos;
      }

      if(IS_VALUE(tree[pos]))
        goto err_free;

      tree[pos].data.v.is_value = SF_TREE_VALUE;
      tree[pos].data.v.value = list[i].value;
    }

    // Validate the tree -- make sure all child indexes are set for non-leaves.
    for(i = 0; i < count; i++)
    {
      if(!IS_VALUE(tree[i]))
        if(!tree[i].data.next[0] || !tree[i].data.next[1])
          goto err_free;
    }

    // Shrink the tree...
    tree = crealloc(tree, count * sizeof(struct SF_tree));
  }
  free(list);
  free(codes);
  list = NULL;
  codes = NULL;

  *output = tree;
  return ZIP_SUCCESS;

err_free:
  free(data);
  free(list);
  free(tree);
  free(codes);
  *output = NULL;
  return ZIP_DECOMPRESS_FAILED;
}

/**
 * Read a variable length encoded Shannon-Fano tree value from a bitstream and
 * return the decoded value. Returns -1 on failure.
 */
static inline int expl_SF_decode(struct bitstream *b, struct SF_tree *tree)
{
  uint16_t pos = 0;
  int bit;

  while(true)
  {
    if(IS_VALUE(tree[pos]))
      return tree[pos].data.v.value;

    bit = BS_READ(b, 1);
    if(bit < 0 || bit > 1)
      return -1;

    pos = tree[pos].data.next[bit];
  }
}

/**
 * Free a Shannon-Fano tree. Currently, this just only needs one free()...
 */
static inline void expl_SF_free(struct SF_tree *tree)
{
  free(tree);
}

/**
 * Allocate an explode stream.
 */
static inline struct zip_stream_data *expl_create(void)
{
  return cmalloc(sizeof(struct explode_stream_data));
}

/**
 * Open an explode stream.
 */
static inline void expl_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  struct explode_stream_data *xs = ((struct explode_stream_data *)zs);

  memset(zs, 0, sizeof(struct explode_stream_data));
  xs->has_8k_dictionary = !!(flags & ZIP_F_COMPRESSION_1);
  xs->has_literal_tree = !!(flags & ZIP_F_COMPRESSION_2);
  xs->minimum_match_length = xs->has_literal_tree ? 3 : 2;
}

/**
 * Close an explode stream.
 */
static inline void expl_close(struct zip_stream_data *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  struct explode_stream_data *xs = ((struct explode_stream_data *)zs);
  expl_SF_free(xs->literal_tree);
  expl_SF_free(xs->length_tree);
  expl_SF_free(xs->distance_tree);

  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

/**
 * Explode the input stream into the output buffer as a single file.
 */
static inline enum zip_error expl_file(struct zip_stream_data *zs)
{
  struct explode_stream_data *xs = ((struct explode_stream_data *)zs);
  struct bitstream *b = &(xs->b);

  uint8_t *start = zs->output_buffer;
  uint8_t *pos = zs->output_buffer;
  uint8_t *end = zs->output_buffer + zs->output_length;

  enum zip_error result;

  if(zs->finished)
    return ZIP_STREAM_FINISHED;

  if(!start)
    return ZIP_OUTPUT_FULL;

  if(!b->input)
  {
    if(!zs->input_buffer)
      return ZIP_INPUT_EMPTY;

    b->input = zs->input_buffer;
    b->input_left = zs->input_length;
    zs->final_input_length = zs->input_length;
    zs->input_buffer = NULL;
    zs->input_length = 0;
  }

  zs->output_buffer = NULL;
  zs->output_length = 0;

  if(!xs->length_tree)
  {
    if(!xs->literal_tree && xs->has_literal_tree)
    {
      result = expl_SF_read_tree(b, &(xs->literal_tree));
      if(result != ZIP_SUCCESS)
        return result;
    }

    result = expl_SF_read_tree(b, &(xs->length_tree));
    if(result != ZIP_SUCCESS)
      return result;
  }

  if(!xs->distance_tree)
  {
    result = expl_SF_read_tree(b, &(xs->distance_tree));
    if(result != ZIP_SUCCESS)
      return result;
  }

  while(pos < end)
  {
    int value = BS_READ(b, 1);

    if(value < 0)
      break;

    if(value)
    {
      // Literal data.
      if(xs->literal_tree)
      {
        value = expl_SF_decode(b, xs->literal_tree);
      }
      else
        value = BS_READ(b, 8);

      if(value < 0)
        break;

      *(pos++) = value;
    }
    else
    {
      // Sliding dictionary match.
      uint16_t distance;
      uint16_t length;

      // Distance
      if(xs->has_8k_dictionary)
      {
        distance = BS_READ(b, 7);
        value = expl_SF_decode(b, xs->distance_tree);
        if(value < 0)
          break;

        distance |= value << 7;
      }
      else
      {
        distance = BS_READ(b, 6);
        value = expl_SF_decode(b, xs->distance_tree);
        if(value < 0)
          break;

        distance |= value << 6;
      }

      // Length
      value = expl_SF_decode(b, xs->length_tree);
      if(value < 0)
        break;

      length = value + xs->minimum_match_length;
      if(value == 63)
      {
        value = BS_READ(b, 8);
        if(value < 0)
          break;

        length += value;
      }

      // Must be able to write this amount to the buffer.
      if(pos + length > end)
        return ZIP_DECOMPRESS_FAILED;

      sliding_dictionary_copy(start, &pos, distance + 1, length);
    }
  }
  zs->final_output_length = pos - start;
  zs->finished = true;
  return ZIP_STREAM_FINISHED;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_IMPLODE_H */

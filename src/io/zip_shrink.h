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
 * Shrink (zip compression method 1) decompressor.
 */

#ifndef MEGAZEUX_ZIP_SHRINK_H
#define MEGAZEUX_ZIP_SHRINK_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <inttypes.h>

#include "bitstream.h"
#include "zip.h"

#include "../util.h"

struct LZW_node
{
  uint16_t pr_st; // State (0x6000) and previous node index (0x1FFF)
  uint8_t length;
  uint8_t value;
};

struct LZW_tree
{
  struct LZW_node *nodes;
  uint16_t next;
  uint16_t length;
  uint16_t alloc;
  uint16_t previous_code;
  uint8_t previous_first_char;
};

struct shrink_stream_data
{
  struct zip_stream_data zs;
  struct bitstream b;
  struct LZW_tree tree;
  uint8_t bit_width;
};

#define LZW_NO_CODE 0xFFFF
#define LZW_COMMAND_CODE 0x100
#define LZW_NUM_CODES_DEFAULT 257
#define LZW_TREE_ALLOC_DEFAULT 1024
#define LZW_TREE_ALLOC_MAX 8192
#define LZW_BIT_WIDTH_DEFAULT 9
#define LZW_BIT_WIDTH_MAX     13

#define LZW_AVAILABLE   0
#define LZW_LEAF        1
#define LZW_BRANCH      2

#define LZW_GET_PREV(n)    ((n)->pr_st & 0x1FFF)
#define LZW_SET_PREV(n,p)  ((n)->pr_st = ((n)->pr_st & ~0x1FFF)|(p & 0x1FFF))
#define LZW_GET_STATE(n)   (((n)->pr_st >> 13) & 3)
#define LZW_SET_STATE(n,s) ((n)->pr_st = ((n)->pr_st & 0x1FFF)|((s & 3)<<13));

/**
 * Add an LZW code.
 */
static inline void LZW_add(struct LZW_tree *tree)
{
  struct LZW_node *current = tree->nodes + tree->next;
  uint8_t prev_length;

  if(tree->next == tree->length)
  {
    if(tree->length == tree->alloc)
    {
      if(tree->alloc >= LZW_TREE_ALLOC_MAX)
        return;

      tree->alloc *= 2;
      tree->nodes = crealloc(tree->nodes, tree->alloc * sizeof(struct LZW_node));
      current = tree->nodes + tree->next;
    }
    tree->length++;
    tree->next++;
  }
  else
  {
    // Find the next available code for the next time this function is called.
    uint16_t i;
    //assert(tree->next != tree->previous_code);
    assert(LZW_GET_STATE(current) == LZW_AVAILABLE);
    for(i = tree->next + 1; i < tree->length; i++)
      if(LZW_GET_STATE(tree->nodes + i) == LZW_AVAILABLE)
        break;
    tree->next = i;
  }

  LZW_SET_PREV(current, tree->previous_code);
  LZW_SET_STATE(current, LZW_LEAF);
  current->value = tree->previous_first_char;

  // NOTE: may intentionally overflow to 0, in which case the length will be
  // computed as-needed by iterating the tree. The other case this might be 0
  // is with nodes marked "available", as they may be spontaneously overwritten,
  // invalidating the lengths of their children as well (see below).
  prev_length = tree->nodes[tree->previous_code].length;
  current->length = prev_length ? prev_length + 1 : 0;
}

/**
 * Partially clear the LZW tree.
 * In practice what this means is marking leaf nodes as available and resetting
 * the next available code. NOTE: available codes can still be referenced,
 * branched from, and then overwritten, altering their child strings too.
 */
static inline void LZW_partial_clear(struct LZW_tree *tree)
{
  struct LZW_node *current;
  uint16_t first_available = tree->length;
  uint16_t i;

  // Pass 1: all nodes should be marked as "leaves" or "available".
  // Either of these could actually be branches...
  for(i = LZW_NUM_CODES_DEFAULT; i < tree->length; i++)
  {
    current = tree->nodes + i;
    LZW_SET_STATE(tree->nodes + LZW_GET_PREV(current), LZW_BRANCH);
  }

  // Pass 2: chain leaves/available and mark available, mark others as leaves.
  for(i = tree->length - 1; i >= LZW_NUM_CODES_DEFAULT; i--)
  {
    current = tree->nodes + i;

    switch(LZW_GET_STATE(current))
    {
      case LZW_AVAILABLE:
      case LZW_LEAF:
        LZW_SET_STATE(current, LZW_AVAILABLE);
        current->length = 0;
        first_available = i;
        break;

      case LZW_BRANCH:
        LZW_SET_STATE(current, LZW_LEAF);
        break;
    }
  }
  tree->next = first_available;
}

/**
 * Get the length of an LZW code, or compute it if it isn't currently stored.
 * This happens when one or mode codes in the sequence are marked for reuse.
 */
static inline uint16_t LZW_get_length(struct LZW_tree *tree, struct LZW_node *c)
{
  uint16_t code;
  uint16_t length = 1;

  if(c->length)
    return c->length;

  do
  {
    // Shouldn't happen, but...
    if(length >= (1<<13))
      return 0;

    length++;
    code = LZW_GET_PREV(c);
    c = tree->nodes + code;
  }
  while(code >= LZW_NUM_CODES_DEFAULT);
  return length;
}

/**
 * Output an LZW code.
 */
static inline enum zip_error LZW_output(struct LZW_tree *tree, uint16_t code,
 uint8_t **_pos, uint8_t *end)
{
  uint8_t *pos = *_pos;

  if(code >= LZW_NUM_CODES_DEFAULT)
  {
    struct LZW_node *nodes = tree->nodes;
    struct LZW_node *current = nodes + code;
    uint16_t length = LZW_get_length(tree, current);
    uint16_t i;

    if(pos + length > end)
    {
      trace("--UNSHRINK-- out of buffer in LZW_output!\n");
      return ZIP_OUTPUT_FULL;
    }

    if(length == 0)
    {
      trace("--UNSHRINK-- encountered infinite cycle at %u! aborting\n", code);
      return ZIP_DECOMPRESS_FAILED;
    }

    for(i = length - 1; i > 0; i--)
    {
      pos[i] = current->value;
      code = LZW_GET_PREV(current);
      current = nodes + code;
    }
    *pos = code;
    pos += length;
  }
  else

  if(pos < end)
  {
    *(pos++) = code;
  }
  else
  {
    trace("--UNSHRINK-- out of buffer in LZW_output!\n");
    return ZIP_OUTPUT_FULL;
  }

  *_pos = pos;
  tree->previous_first_char = code;
  return ZIP_SUCCESS;
}

/**
 * Decode an LZW code and create the next code from known data.
 */
static inline enum zip_error LZW_decode(struct LZW_tree *tree, uint16_t code,
 uint8_t **_pos, uint8_t *end)
{
  enum zip_error result;

  if(code > tree->length)
  {
    trace("--UNSHRINK-- invalid code %u\n", code);
    return ZIP_DECOMPRESS_FAILED;
  }

  if(code == tree->next)
  {
    // This is a special case--the current code is the previous code with the
    // first character of the previous code appended, and needs to be added
    // before the output occurs (instead of after).
    if(tree->previous_code == LZW_NO_CODE)
    {
      trace("--UNSHRINK-- invalid code %u (in special case)\n", code);
      return ZIP_DECOMPRESS_FAILED;
    }

    LZW_add(tree);
    result = LZW_output(tree, code, _pos, end);
    tree->previous_code = code;
    return result;
  }

  // Otherwise, output first, and then add a new code, which is the previous
  // code with the first character of the current code appended.
  result = LZW_output(tree, code, _pos, end);
  if(result == ZIP_SUCCESS)
  {
    if(tree->previous_code != LZW_NO_CODE)
      LZW_add(tree);

    tree->previous_code = code;
  }
  return result;
}

/**
 * Allocate an unshrink stream.
 */
static inline struct zip_stream_data *unshrink_create(void)
{
  return cmalloc(sizeof(struct shrink_stream_data));
}

/**
 * Open a unshrink stream.
 */
static inline void unshrink_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  struct shrink_stream_data *ss = ((struct shrink_stream_data *)zs);
  struct LZW_tree *tree = &(ss->tree);
  int i;

  memset(zs, 0, sizeof(struct shrink_stream_data));

  tree->nodes = cmalloc(LZW_TREE_ALLOC_DEFAULT * sizeof(struct LZW_node));
  tree->alloc = LZW_TREE_ALLOC_DEFAULT;
  tree->next = LZW_NUM_CODES_DEFAULT;
  tree->length = LZW_NUM_CODES_DEFAULT;
  tree->previous_code = LZW_NO_CODE;
  tree->previous_first_char = 0;

  for(i = 0; i < LZW_NUM_CODES_DEFAULT; i++)
  {
    tree->nodes[i].length = 1;
    LZW_SET_STATE(tree->nodes + i, LZW_BRANCH);
  }

  ss->bit_width = LZW_BIT_WIDTH_DEFAULT;
}

/**
 * Free extra allocated data for a (un)shrink stream.
 */
static inline void unshrink_close(struct zip_stream_data *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  struct shrink_stream_data *ss = ((struct shrink_stream_data *)zs);
  free(ss->tree.nodes);

  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

/**
 * Unshrink the input buffer into the output buffer, treating it as a file.
 */
static inline enum zip_error unshrink_file(struct zip_stream_data *zs)
{
  struct shrink_stream_data *ss = ((struct shrink_stream_data *)zs);
  struct LZW_tree *tree = &(ss->tree);
  struct bitstream *b = &(ss->b);
  enum zip_error result;

  uint8_t *start = zs->output_buffer;
  uint8_t *pos = zs->output_buffer;
  uint8_t *end = zs->output_buffer + zs->output_length;

  uint8_t bit_width = ss->bit_width;

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

  while(pos < end)
  {
    int code = BS_READ(b, bit_width);
    if(code < 0)
      break;

    if(code == LZW_COMMAND_CODE)
    {
      code = BS_READ(b, bit_width);
      if(code < 0)
        break;

      if(code == 1)
      {
        if(bit_width >= LZW_BIT_WIDTH_MAX)
        {
          trace("--UNSHRINK-- can't expand bit width beyond 13!\n");
          return ZIP_DECOMPRESS_FAILED;
        }

        bit_width++;
        ss->bit_width = bit_width;
      }
      else

      if(code == 2)
      {
        LZW_partial_clear(tree);
      }
      else
      {
        trace("--UNSHRINK-- invalid shrink command code %u!\n", code);
        return ZIP_DECOMPRESS_FAILED;
      }
      continue;
    }

    result = LZW_decode(tree, code, &pos, end);
    if(result)
      return result;
  }
  zs->final_output_length = pos - start;
  zs->finished = true;
  return ZIP_STREAM_FINISHED;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_SHRINK_H */

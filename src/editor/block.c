/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

/* Block actions */
/* For the selection dialogs that used to be called "block.c", see select.c */

#include <string.h>

#include "ansi.h"
#include "block.h"
#include "undo.h"

#include "../block.h"
#include "../core.h"
#include "../data.h"
#include "../extmem.h"
#include "../mzm.h"
#include "../robot.h"
#include "../window.h"
#include "../world_struct.h"

#include "../event/event.h"

static void clear_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int dest_skip = dest_width - block_width;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_char[dest_offset] = 32;
      dest_color[dest_offset] = 7;
      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

static void clear_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *level_id = dest_board->level_id;
  char *level_param = dest_board->level_param;
  char *level_color = dest_board->level_color;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_param = dest_board->level_under_param;
  char *level_under_color = dest_board->level_under_color;

  int dest_width = dest_board->board_width;
  int dest_skip = dest_width - block_width;

  enum thing dest_id;
  int dest_param;
  int i, i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_id = (enum thing)level_id[dest_offset];
      if(dest_id != PLAYER)
      {
        dest_param = level_param[dest_offset];

        if(dest_id == SENSOR)
        {
          clear_sensor_id(dest_board, dest_param);
        }
        else

        if(is_signscroll(dest_id))
        {
          clear_scroll_id(dest_board, dest_param);
        }
        else

        if(is_robot(dest_id))
        {
          clear_robot_id(dest_board, dest_param);
        }

        level_id[dest_offset] = (char)SPACE;
        level_param[dest_offset] = 0;
        level_color[dest_offset] = 7;
      }

      level_under_id[dest_offset] = (char)SPACE;
      level_under_param[dest_offset] = 0;
      level_under_color[dest_offset] = 7;

      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

static void flip_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  char *buffer = cmalloc(sizeof(char) * block_width);

  int start_offset = dest_offset;
  int end_offset = dest_offset + dest_width * (block_height - 1);

  while(start_offset < end_offset)
  {
    memcpy(buffer, dest_char + start_offset,
     block_width);
    memcpy(dest_char + start_offset, dest_char + end_offset,
     block_width);
    memcpy(dest_char + end_offset, buffer,
     block_width);

    memcpy(buffer, dest_color + start_offset,
     block_width);
    memcpy(dest_color + start_offset, dest_color + end_offset,
     block_width);
    memcpy(dest_color + end_offset, buffer,
     block_width);

    start_offset += dest_width;
    end_offset -= dest_width;
  }

  free(buffer);
}

static void flip_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *buffer = cmalloc(sizeof(char) * block_width);

  char *level_id = dest_board->level_id;
  char *level_color = dest_board->level_color;
  char *level_param = dest_board->level_param;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_color = dest_board->level_under_color;
  char *level_under_param = dest_board->level_under_param;

  int dest_width = dest_board->board_width;

  int start_offset = dest_offset;
  int end_offset = dest_offset + dest_width * (block_height - 1);

  while(start_offset < end_offset)
  {
    memcpy(buffer, level_id + start_offset,
     block_width);
    memcpy(level_id + start_offset, level_id + end_offset,
     block_width);
    memcpy(level_id + end_offset, buffer,
     block_width);

    memcpy(buffer, level_color + start_offset,
     block_width);
    memcpy(level_color + start_offset, level_color + end_offset,
     block_width);
    memcpy(level_color + end_offset, buffer,
     block_width);

    memcpy(buffer, level_param + start_offset,
     block_width);
    memcpy(level_param + start_offset,
     level_param + end_offset, block_width);
    memcpy(level_param + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_id + start_offset,
     block_width);
    memcpy(level_under_id + start_offset, level_under_id + end_offset,
     block_width);
    memcpy(level_under_id + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_color + start_offset,
     block_width);
    memcpy(level_under_color + start_offset, level_under_color + end_offset,
     block_width);
    memcpy(level_under_color + end_offset, buffer,
     block_width);

    memcpy(buffer, level_under_param + start_offset,
     block_width);
    memcpy(level_under_param + start_offset, level_under_param + end_offset,
     block_width);
    memcpy(level_under_param + end_offset, buffer,
     block_width);

    start_offset += dest_width;
    end_offset -= dest_width;
  }

  free(buffer);
}

static void mirror_layer_block(
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  char t;
  int i;
  int a;
  int b;

  for(i = 0; i < block_height; i++)
  {
    a = dest_offset;
    b = dest_offset + block_width - 1;

    while(a < b)
    {
      t = dest_char[a];
      dest_char[a] = dest_char[b];
      dest_char[b] = t;

      t = dest_color[a];
      dest_color[a] = dest_color[b];
      dest_color[b] = t;

      a++;
      b--;
    }

    dest_offset += dest_width;
  }
}

static void mirror_board_block(struct board *dest_board, int dest_offset,
 int block_width, int block_height)
{
  char *level_id = dest_board->level_id;
  char *level_color = dest_board->level_color;
  char *level_param = dest_board->level_param;
  char *level_under_id = dest_board->level_under_id;
  char *level_under_color = dest_board->level_under_color;
  char *level_under_param = dest_board->level_under_param;

  int dest_width = dest_board->board_width;

  char t;
  int i;
  int a;
  int b;

  for(i = 0; i < block_height; i++)
  {
    a = dest_offset;
    b = dest_offset + block_width - 1;

    while(a < b)
    {
      t = level_id[a];
      level_id[a] = level_id[b];
      level_id[b] = t;

      t = level_color[a];
      level_color[a] = level_color[b];
      level_color[b] = t;

      t = level_param[a];
      level_param[a] = level_param[b];
      level_param[b] = t;

      t = level_under_id[a];
      level_under_id[a] = level_under_id[b];
      level_under_id[b] = t;

      t = level_under_color[a];
      level_under_color[a] = level_under_color[b];
      level_under_color[b] = t;

      t = level_under_param[a];
      level_under_param[a] = level_under_param[b];
      level_under_param[b] = t;

      a++;
      b--;
    }

    dest_offset += dest_width;
  }
}

static void paint_layer_block(char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height, int paint_color)
{
  int dest_skip = dest_width - block_width;
  int i;
  int i2;

  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_color[dest_offset] = paint_color;
      dest_offset++;
    }

    dest_offset += dest_skip;
  }
}

void copy_layer_buffer_to_buffer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height)
{
  int src_skip = src_width - block_width;
  int dest_skip = dest_width - block_width;
  int i, i2;

  // Like a direct layer-to-layer copy, but without the char 32 checks
  for(i = 0; i < block_height; i++)
  {
    for(i2 = 0; i2 < block_width; i2++)
    {
      dest_char[dest_offset] = src_char[src_offset];
      dest_color[dest_offset] = src_color[src_offset];

      src_offset++;
      dest_offset++;
    }

    src_offset += src_skip;
    dest_offset += dest_skip;
  }
}

static void move_layer_block(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height,
 int clear_width, int clear_height)
{
  // Similar to copy_layer_to_layer_buffered, but deletes the source
  // after copying to the buffer.

  char *buffer_char = cmalloc(block_width * block_height);
  char *buffer_color = cmalloc(block_width * block_height);

  // Copy source to buffer
  copy_layer_buffer_to_buffer(
   src_char, src_color, src_width, src_offset,
   buffer_char, buffer_color, block_width, 0,
   block_width, block_height);

  // Clear the source
  clear_layer_block(
   src_char, src_color, src_width, src_offset,
   clear_width, clear_height);

  // Copy buffer to destination
  copy_layer_buffer_to_buffer(
   buffer_char, buffer_color, block_width, 0,
   dest_char, dest_color, dest_width, dest_offset,
   block_width, block_height);

  free(buffer_char);
  free(buffer_color);
}

void do_block_command(struct world *mzx_world, struct block_info *block,
 struct undo_history *dest_history, char *mzm_name_buffer, int current_color)
{
  // Source is optional and mostly only matters for copy/move
  struct board *src_board = NULL;
  char *src_char = NULL;
  char *src_color = NULL;
  int src_width = 0;
  int src_offset;
  int src_x = block->src_x;
  int src_y = block->src_y;

  struct board *dest_board;
  char *dest_char;
  char *dest_color;
  int dest_width;
  int dest_height;
  int dest_offset;
  int dest_x = block->dest_x;
  int dest_y = block->dest_y;

  // History size is generally the block size, but move block adds complications
  int history_x = block->dest_x;
  int history_y = block->dest_y;
  int history_width;
  int history_height;
  int history_offset;

  // Trim the block size to fit the destination, but keep the old
  // size so the old block can be cleared during the move command
  int block_width = block->width;
  int block_height = block->height;
  int dest_block_width = block_width;
  int dest_block_height = block_height;
  boolean loaded_src_board = false;

  if(block->command == BLOCK_CMD_NONE)
    return;

  // Set up source
  switch(block->src_mode)
  {
    case EDIT_BOARD:
      if(block->src_board == NULL) break;
      if(block->src_board != mzx_world->current_board)
      {
        retrieve_board_from_extram(block->src_board);
        loaded_src_board = true;
      }
      src_board = block->src_board;
      src_char = NULL;
      src_color = src_board->level_color;
      src_width = src_board->board_width;
      break;

    case EDIT_OVERLAY:
      if(block->src_board == NULL) break;
      src_board = block->src_board;
      src_char = src_board->overlay;
      src_color = src_board->overlay_color;
      src_width = src_board->board_width;
      break;

    case EDIT_VLAYER:
      src_board = NULL;
      src_char = mzx_world->vlayer_chars;
      src_color = mzx_world->vlayer_colors;
      src_width = mzx_world->vlayer_width;
      break;

    default:
      return;
  }

  // Set up destination
  switch(block->dest_mode)
  {
    case EDIT_BOARD:
      dest_board = mzx_world->current_board;
      dest_char = NULL;
      dest_color = dest_board->level_color;
      dest_width = dest_board->board_width;
      dest_height = dest_board->board_height;
      break;

    case EDIT_OVERLAY:
      dest_board = mzx_world->current_board;
      dest_char = dest_board->overlay;
      dest_color = dest_board->overlay_color;
      dest_width = dest_board->board_width;
      dest_height = dest_board->board_height;
      break;

    case EDIT_VLAYER:
      dest_board = NULL;
      dest_char = mzx_world->vlayer_chars;
      dest_color = mzx_world->vlayer_colors;
      dest_width = mzx_world->vlayer_width;
      dest_height = mzx_world->vlayer_height;
      break;

    default:
      return;
  }

  // Trim the block for the destination
  if((dest_x + dest_block_width) > dest_width)
    dest_block_width = dest_width - dest_x;

  if((dest_y + dest_block_height) > dest_height)
    dest_block_height = dest_height - dest_y;

  history_width = dest_block_width;
  history_height = dest_block_height;

  // In-place move block adds some complications to history.
  // For now, just save the smallest possible space containing both blocks.
  if((block->command == BLOCK_CMD_MOVE) &&
   (block->src_mode == block->dest_mode) &&
   ((block->src_mode == EDIT_VLAYER) || (src_board == dest_board)))
  {
    if(src_x > dest_x)
    {
      history_x = dest_x;
      history_width = dest_block_width + src_x - dest_x;
    }
    else
    {
      history_x = src_x;
      history_width = dest_block_width + dest_x - src_x;
    }

    if(src_y > dest_y)
    {
      history_y = dest_y;
      history_height = dest_block_height + src_y - dest_y;
    }
    else
    {
      history_y = src_y;
      history_height = dest_block_height + dest_y - src_y;
    }
  }

  src_offset = src_x + (src_y * src_width);
  dest_offset = dest_x + (dest_y * dest_width);
  history_offset = history_x + (history_y * dest_width);

  // Perform the block action
  if(block->dest_mode == EDIT_BOARD)
  {
    add_block_undo_frame(mzx_world, dest_history, dest_board,
     history_offset, history_width, history_height);

    switch(block->command)
    {
      case BLOCK_CMD_NONE:
        return;

      case BLOCK_CMD_COPY:
      case BLOCK_CMD_COPY_REPEATED:
      {
        switch(block->src_mode)
        {
          case EDIT_BOARD:
            copy_board_to_board(mzx_world,
             src_board, src_offset,
             dest_board, dest_offset,
             dest_block_width, dest_block_height);
            break;

          case EDIT_OVERLAY:
          case EDIT_VLAYER:
          {
            copy_layer_to_board(
             src_char, src_color, src_width, src_offset,
             dest_board, dest_offset,
             dest_block_width, dest_block_height, block->convert_id);
            break;
          }
        }
        break;
      }

      case BLOCK_CMD_MOVE:
      {
        // Has to take x and y so it can manipulate the player position.
        move_board_block(mzx_world,
         src_board, src_x, src_y,
         dest_board, dest_x, dest_y,
         dest_block_width, dest_block_height,
         block_width, block_height); // Dimensions to clear old block
        break;
      }

      case BLOCK_CMD_CLEAR:
      {
        clear_board_block(dest_board, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_FLIP:
      {
        flip_board_block(dest_board, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_MIRROR:
      {
        mirror_board_block(dest_board, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_PAINT:
      {
        paint_layer_block(
         dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height, current_color);
        break;
      }

      case BLOCK_CMD_SAVE_MZM:
      case BLOCK_CMD_SAVE_ANSI:
      {
        // not handled here
        break;
      }

      case BLOCK_CMD_LOAD_MZM:
      {
        load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, block->dest_mode,
         false, block->convert_id);
        break;
      }

      case BLOCK_CMD_LOAD_ANSI:
      {
        clear_board_block(dest_board, dest_offset,
         dest_block_width, dest_block_height);
        // This function takes the original block size, not the clipped size.
        import_ansi(mzx_world, mzm_name_buffer, block->dest_mode, dest_x,
         dest_y, block_width, block_height, block->convert_id);
        break;
      }
    }

    update_undo_frame(dest_history);
  }
  else

  if((block->dest_mode == EDIT_OVERLAY) || (block->dest_mode == EDIT_VLAYER))
  {
    add_layer_undo_frame(dest_history, dest_char, dest_color, dest_width,
     history_offset, history_width, history_height);

    switch(block->command)
    {
      case BLOCK_CMD_NONE:
        break;

      case BLOCK_CMD_COPY:
      case BLOCK_CMD_COPY_REPEATED:
      {
        switch(block->src_mode)
        {
          case EDIT_BOARD:
            copy_board_to_layer(
             src_board, src_offset,
             dest_char, dest_color, dest_width, dest_offset,
             dest_block_width, dest_block_height);
            break;

          case EDIT_OVERLAY:
          case EDIT_VLAYER:
            copy_layer_to_layer(
             src_char, src_color, src_width, src_offset,
             dest_char, dest_color, dest_width, dest_offset,
             dest_block_width, dest_block_height);
            break;
        }
        break;
      }

      case BLOCK_CMD_MOVE:
      {
        move_layer_block(
         src_char, src_color, src_width, src_offset,
         dest_char, dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height,
         block_width, block_height); // Dimensions to clear old block
        break;
      }

      case BLOCK_CMD_CLEAR:
      {
        clear_layer_block(
         dest_char, dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_FLIP:
      {
        flip_layer_block(
         dest_char, dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_MIRROR:
      {
        mirror_layer_block(
         dest_char, dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height);
        break;
      }

      case BLOCK_CMD_PAINT:
      {
        paint_layer_block(
         dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height, current_color);
        break;
      }

      case BLOCK_CMD_SAVE_MZM:
      case BLOCK_CMD_SAVE_ANSI:
      {
        // not handled here
        break;
      }

      case BLOCK_CMD_LOAD_MZM:
      {
        load_mzm(mzx_world, mzm_name_buffer, dest_x, dest_y, block->dest_mode,
         false, NO_ID);
        break;
      }

      case BLOCK_CMD_LOAD_ANSI:
      {
        clear_layer_block(
         dest_char, dest_color, dest_width, dest_offset,
         dest_block_width, dest_block_height);
        // This function takes the original block size, not the clipped size.
        import_ansi(mzx_world, mzm_name_buffer, block->dest_mode, dest_x,
         dest_y, block_width, block_height, NO_ID);
        break;
      }
    }

    update_undo_frame(dest_history);
  }

  // TODO: it might be better to keep it in regular memory for repeat copies.
  if(loaded_src_board)
    store_board_to_extram(src_board);
}

//--------------------------
//
// ( ) Copy block
// ( ) Copy block (repeated)
// ( ) Move block
// ( ) Clear block
// ( ) Flip block
// ( ) Mirror block
// ( ) Paint block
// ( ) Copy to...
// ( ) Copy to...
// ( ) Save as ANSi
// ( ) Save as MZM
//
//    _OK_      _Cancel_
//
//--------------------------
boolean select_block_command(struct world *mzx_world, struct block_info *block,
 enum editor_mode mode)
{
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  int selection = 0;
  const char *radio_button_strings[] =
  {
    "Copy block",
    "Copy block (repeated)",
    "Move block",
    "Clear block",
    "Flip block",
    "Mirror block",
    "Paint block",
    NULL,
    NULL,
    "Save as ANSi/TXT",
    "Save as MZM"
  };

  switch(mode)
  {
    default:
    case EDIT_BOARD:
      radio_button_strings[7] = "Copy to overlay";
      radio_button_strings[8] = "Copy to vlayer";
      break;

    case EDIT_OVERLAY:
      radio_button_strings[7] = "Copy to board";
      radio_button_strings[8] = "Copy to vlayer";
      break;

    case EDIT_VLAYER:
      radio_button_strings[7] = "Copy to board";
      radio_button_strings[8] = "Copy to overlay";
      break;
  }

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BLOCK_CMD);
  elements[0] = construct_radio_button(2, 2, radio_button_strings,
   11, 21, &selection);
  elements[1] = construct_button(5, 14, "OK", 0);
  elements[2] = construct_button(15, 14, "Cancel", -1);

  construct_dialog(&di, "Choose block command", 26, 3, 29, 17,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);
  pop_context();

  destruct_dialog(&di);

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
  {
    block->selected = false;
    return false;
  }

  // Translate selection to a real block command.
  block->selected = true;
  block->command = BLOCK_CMD_NONE;
  block->src_mode = mode;
  block->dest_mode = mode;

  switch(selection)
  {
    case 0:
      block->command = BLOCK_CMD_COPY;
      break;

    case 1:
      block->command = BLOCK_CMD_COPY_REPEATED;
      break;

    case 2:
      block->command = BLOCK_CMD_MOVE;
      break;

    case 3:
      block->command = BLOCK_CMD_CLEAR;
      break;

    case 4:
      block->command = BLOCK_CMD_FLIP;
      break;

    case 5:
      block->command = BLOCK_CMD_MIRROR;
      break;

    case 6:
      block->command = BLOCK_CMD_PAINT;
      break;

    case 7:
    {
      block->command = BLOCK_CMD_COPY;
      switch(mode)
      {
        case EDIT_BOARD:
          block->dest_mode = EDIT_OVERLAY;
          break;

        case EDIT_OVERLAY:
        case EDIT_VLAYER:
          block->dest_mode = EDIT_BOARD;
          break;
      }
      break;
    }

    case 8:
    {
      block->command = BLOCK_CMD_COPY;
      switch(mode)
      {
        case EDIT_BOARD:
        case EDIT_OVERLAY:
          block->dest_mode = EDIT_VLAYER;
          break;

        case EDIT_VLAYER:
          block->dest_mode = EDIT_OVERLAY;
          break;
      }
      break;
    }

    case 9:
      block->command = BLOCK_CMD_SAVE_ANSI;
      break;

    case 10:
      block->command = BLOCK_CMD_SAVE_MZM;
      break;
  }
  return true;
}

enum thing layer_to_board_object_type(struct world *mzx_world)
{
  int dialog_result;
  struct element *elements[3];
  struct dialog di;
  int object_type = 0;
  const char *radio_button_strings[] =
  {
    "Custom Block",
    "Custom Floor",
    "Text"
  };

  // Prevent previous keys from carrying through.
  force_release_all_keys();

  set_context(CTX_BLOCK_TYPE);
  elements[0] = construct_radio_button(6, 4, radio_button_strings,
   3, 12, &object_type);
  elements[1] = construct_button(5, 11, "OK", 0);
  elements[2] = construct_button(15, 11, "Cancel", -1);

  construct_dialog(&di, "Object type", 26, 4, 28, 14,
   elements, 3, 0);

  dialog_result = run_dialog(mzx_world, &di);

  destruct_dialog(&di);
  pop_context();

  // Prevent UI keys from carrying through.
  force_release_all_keys();

  if(dialog_result)
    return NO_ID;

  switch(object_type)
  {
    case 0: return CUSTOM_BLOCK;
    case 1: return CUSTOM_FLOOR;
    case 2: return TEXT_ID;
  }

  return NO_ID;
}

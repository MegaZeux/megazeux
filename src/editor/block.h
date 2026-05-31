/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_EDITOR_BLOCK_H
#define MEGAZEUX_EDITOR_BLOCK_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"
#include "edit.h"
#include "undo.h"

enum block_command
{
  BLOCK_CMD_NONE,
  BLOCK_CMD_COPY,
  BLOCK_CMD_COPY_REPEATED,
  BLOCK_CMD_MOVE,
  BLOCK_CMD_CLEAR,
  BLOCK_CMD_FLIP,
  BLOCK_CMD_MIRROR,
  BLOCK_CMD_PAINT,
  BLOCK_CMD_SAVE_MZM,
  BLOCK_CMD_LOAD_MZM,
  BLOCK_CMD_SAVE_ANSI,
  BLOCK_CMD_LOAD_ANSI,
};

struct block_info
{
  enum block_command command;
  enum editor_mode src_mode;
  enum editor_mode dest_mode;
  int src_x;
  int src_y;
  int dest_x;
  int dest_y;
  int width;
  int height;
  enum thing convert_id;
  struct board *src_board;
  struct board *dest_board;
  boolean selected;
};

void copy_layer_buffer_to_buffer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);

void do_block_command(struct world *mzx_world, struct block_info *block,
 struct undo_history *dest_history, char *mzm_name_buffer, int current_color);

boolean select_block_command(struct world *mzx_world, struct block_info *block,
 enum editor_mode mode);
enum thing layer_to_board_object_type(struct world *mzx_world);

MEGAZEUX_END_DECLS

#endif  /* MEGAZEUX_EDITOR_BLOCK_H */

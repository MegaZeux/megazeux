/* MegaZeux
 *
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

#ifndef MEGAZEUX_EDITOR_UNDO_H
#define MEGAZEUX_EDITOR_UNDO_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"
#include "buffer_struct.h"

struct undo_history;

boolean apply_undo(struct undo_history *h);
boolean apply_redo(struct undo_history *h);
void add_undo_position(struct undo_history *h, int x, int y);
void update_undo_frame(struct undo_history *h);
void destruct_undo_history(struct undo_history *h);

struct undo_history *construct_charset_undo_history(int max_size);
struct undo_history *construct_board_undo_history(int max_size);
struct undo_history *construct_layer_undo_history(int max_size);

void add_charset_undo_frame(struct undo_history *h, int charset, int first_char,
 int width, int height);

void add_board_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct buffer_info *buffer, int x, int y);

void add_block_undo_frame(struct world *mzx_world, struct undo_history *h,
 struct board *src_board, int src_offset, int width, int height);

void add_layer_undo_pos_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, struct buffer_info *buffer, int x, int y);

void add_layer_undo_frame(struct undo_history *h, char *layer_chars,
 char *layer_colors, int layer_width, int layer_offset, int width, int height);

enum text_undo_line_type
{
  TX_INVALID_LINE_TYPE,
  TX_OLD_LINE,
  TX_NEW_LINE,
  TX_SAME_LINE,
  TX_OLD_BUFFER,
  TX_SAME_BUFFER,
};

struct robot_editor_context;
struct undo_history *construct_robot_editor_undo_history(int max_size);
void add_robot_editor_undo_frame(struct undo_history *h, struct robot_editor_context *rstate);
void add_robot_editor_undo_line(struct undo_history *h, enum text_undo_line_type type,
 int line, int pos, char *value, size_t length);
enum text_undo_line_type robot_editor_undo_frame_type(struct undo_history *h);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_UNDO_H */

/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2019 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_BLOCK_H
#define MEGAZEUX_BLOCK_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "data.h"
#include "world_struct.h"

CORE_LIBSPEC void copy_board_to_board(struct world *mzx_world,
 struct board *src_board, int src_offset,
 struct board *dest_board, int dest_offset,
 int block_width, int block_height);

CORE_LIBSPEC void copy_layer_to_layer(
 char *src_char, char *src_color, int src_width, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);

CORE_LIBSPEC void copy_board_to_layer(
 struct board *src_board, int src_offset,
 char *dest_char, char *dest_color, int dest_width, int dest_offset,
 int block_width, int block_height);

CORE_LIBSPEC void copy_layer_to_board(
 char *src_char, char *src_color, int src_width, int src_offset,
 struct board *dest_board, int dest_offset,
 int block_width, int block_height, enum thing convert_id);

#ifdef CONFIG_EDITOR

CORE_LIBSPEC void copy_replace_player(struct world *mzx_world, int x, int y);

CORE_LIBSPEC void move_board_block(struct world *mzx_world,
 struct board *src_board, int src_x, int src_y,
 struct board *dest_board, int dest_x, int dest_y,
 int block_width, int block_height,
 int clear_width, int clear_height);

#endif //CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_BLOCK_H */

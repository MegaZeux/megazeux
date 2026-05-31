/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_BOARD_H
#define MEGAZEUX_BOARD_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

struct zip_archive;

CORE_LIBSPEC int save_board(struct world *mzx_world, struct board *cur_board,
 struct zip_archive *zp, int savegame, int file_version, int board_id);

CORE_LIBSPEC struct board *load_board_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version, unsigned int board_id);

CORE_LIBSPEC void board_set_input_string(struct board *cur_board,
 const char *input, size_t len);
CORE_LIBSPEC void clear_board(struct board *cur_board);

struct board *duplicate_board(struct world *mzx_world,
 struct board *src_board);

void default_board_settings(struct world *mzx_world, struct board *cur_board);
void dummy_board(struct world *mzx_world, struct board *cur_board);

int find_board(struct world *mzx_world, char *name);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC int load_board_direct(struct world *mzx_world,
 struct board *cur_board,  struct zip_archive *zp, int savegame,
 int file_version, unsigned int board_id);

CORE_LIBSPEC void board_set_charset_path(struct board *cur_board,
 const char *path, size_t path_len);
CORE_LIBSPEC void board_set_palette_path(struct board *cur_board,
 const char *path, size_t path_len);
#endif // CONFIG_EDITOR

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_BOARD_H */

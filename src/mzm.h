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

#ifndef MEGAZEUX_MZM_H
#define MEGAZEUX_MZM_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

enum mzm_save_mode
{
  MZM_BOARD_TO_BOARD_STORAGE    = 0,
  MZM_OVERLAY_TO_LAYER_STORAGE  = 1,
  MZM_VLAYER_TO_LAYER_STORAGE   = 2,
  MZM_BOARD_TO_LAYER_STORAGE    = 3,
};

enum mzm_load_mode
{
  MZM_LOAD_TO_BOARD   = 0,
  MZM_LOAD_TO_OVERLAY = 1,
  MZM_LOAD_TO_VLAYER  = 2,
};

enum mzm_storage_mode
{
  MZM_STORAGE_MODE_BOARD = 0,
  MZM_STORAGE_MODE_LAYER = 1,
};

struct mzm_header
{
  int width;
  int height;
  int robots_location;
  int num_robots;
  enum mzm_storage_mode storage_mode;
  int savegame_mode;
  int world_version;
};

CORE_LIBSPEC void save_mzm(struct world *mzx_world, char *name,
 int start_x, int start_y, int width, int height, int mode, int savegame);
CORE_LIBSPEC void save_mzm_string(struct world *mzx_world, const char *name,
 int start_x, int start_y, int width, int height, int mode, int savegame, int id);
CORE_LIBSPEC int load_mzm(struct world *mzx_world, char *name,
 int start_x, int start_y, int mode, int savegame, enum thing layer_convert_id);
CORE_LIBSPEC int load_mzm_memory(struct world *mzx_world, char *name, int start_x,
 int start_y, int mode, int savegame, enum thing layer_convert_id,
 const void *buffer, size_t length);
CORE_LIBSPEC boolean load_mzm_header(char *name, struct mzm_header *mzm_header);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_MZM_H */

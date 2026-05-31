/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2020 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_LEGACY_WORLD_SAVE_H
#define MEGAZEUX_LEGACY_WORLD_SAVE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "const.h"
#include "world_struct.h"
#include "io/memfile.h"

size_t legacy_save_robot_calculate_size(struct world *mzx_world,
 struct robot *cur_robot, int savegame, int version);

void legacy_save_robot_to_memory(struct robot *cur_robot, struct memfile *mf,
 int savegame, int version);

void legacy_save_robot(struct world *mzx_world, struct robot *cur_robot,
 FILE *fp, int savegame, int version);


void legacy_save_scroll(struct scroll *cur_scroll, FILE *fp, int savegame);

void legacy_save_sensor(struct sensor *cur_sensor, FILE *fp, int savegame);

int legacy_save_board(struct world *mzx_world,
 struct board *cur_board, FILE *fp, int savegame, int version);

int legacy_save_world(struct world *mzx_world, const char *file, int savegame);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_LEGACY_WORLD_SAVE_H */

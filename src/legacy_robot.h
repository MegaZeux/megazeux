/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_LEGACY_ROBOT_H
#define MEGAZEUX_LEGACY_ROBOT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "const.h"
#include "world_struct.h"
#include "io/vfile.h"

struct robot *legacy_load_robot_allocate(struct world *mzx_world, vfile *vf,
 int savegame, int file_version, boolean *truncated);

void legacy_load_robot_from_memory(struct world *mzx_world,
 struct robot *cur_robot, struct memfile *mf, int savegame, int version,
 int robot_location);

size_t legacy_calculate_partial_robot_size(int savegame, int version);

size_t legacy_load_robot_calculate_size(const void *buffer, int savegame,
 int version);

boolean legacy_load_robot(struct world *mzx_world, struct robot *cur_robot,
 vfile *vf, int savegame, int version);

struct scroll *legacy_load_scroll_allocate(vfile *vf, int file_version);

struct sensor *legacy_load_sensor_allocate(vfile *vf, int file_version);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_LEGACY_ROBOT_H */

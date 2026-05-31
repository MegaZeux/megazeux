/* MegaZeux
 *
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

#ifndef MEGAZEUX_LEGACY_WORLD_H
#define MEGAZEUX_LEGACY_WORLD_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world.h"
#include "world_struct.h"
#include "io/vfile.h"

/****************************
 * LEGACY WORLD FORMAT INFO *
 ****************************/

/* world:
 * 25 name
 * 1  protected?
 * 3  magic
 *
 * 4129 block 1
 * 3584 char set
 *  455 charid table
 *   90 status counters
 *
 *   72 block 2
 *   24 global info settings
 *   48 palette
 *
 *@4230:
 * ???? block 3 -- board and robot data, and custom sfx
 *    4 global robot offset
 *+   1 number of boards?  if that byte was actually 0:
 *    {
 *      2 sfx size
 *+    50 sfx strings up to 68 chars long each
 *      1 number of boards
 *    }
 *+  25 times the number of boards -- board names
 *    8 times the number of boards -- size, offset
 */

/* save (2.84):
 *    5 save magic
 *    2 world magic
 *    1 current board
 *
 * (block 1)
 *
 *+  73 save block 1
 *   71 keys, potion timers, save player position data, misc
 *+   2 real_mod_playing, up to 514
 *
 * (block 2)
 *
 *+7508 save block 2
 *   24 intensity, faded, coords, player under
 *+   4 counters, up to 2^31 * counter size
 *+   4 strings, up to 2^31 * max string size
 * 4612 sprites
 *(4096   data
 *    4   info
 *  512   collision list)
 *   12 misc settings
 *+   2 fread filename, up to 514
 *    4 fread pos
 *+   2 fwrite filename, up to 514
 *    4 fwrite pos
 *    2 screen mode
 *  768 smzx palette
 *    4 commands
 *+   8 vlayer size, w, h; then data: vlayer size (up to 2^31) * 2
 *
 * (block 3)
 */

void legacy_load_world(struct world *mzx_world, vfile *vf, const char *file,
 boolean savegame, int file_version, char *name, boolean *faded);

vfile *validate_legacy_world_file(struct world *mzx_world,
 const char *file, boolean savegame);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_LEGACY_WORLD_H */

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

#ifndef MEGAZEUX_EDITOR_WORLD_H
#define MEGAZEUX_EDITOR_WORLD_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"

boolean append_world(struct world *mzx_world, const char *file);
void create_blank_world(struct world *mzx_world);
void set_update_done_current(struct world *mzx_world);

void move_current_board(struct world *mzx_world, int new_position);

char get_default_id_char(int id);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_WORLD_H */

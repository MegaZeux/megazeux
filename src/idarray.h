/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

/* Declarations for IDARRAY.ASM */

#ifndef MEGAZEUX_IDARRAY_H
#define MEGAZEUX_IDARRAY_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "data.h"
#include "world_struct.h"

CORE_LIBSPEC void id_remove_top(struct world *mzx_world,
 int array_x, int array_y);

void id_place(struct world *mzx_world, int array_x, int array_y,
 enum thing id, char color, char param);
void offs_place_id(struct world *mzx_world, unsigned int offset,
 enum thing id, char color, char param);
void offs_remove_id(struct world *mzx_world, unsigned int offset);
void id_remove_under(struct world *mzx_world, int array_x, int array_y);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IDARRAY_H */

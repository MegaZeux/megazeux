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

#ifndef MEGAZEUX_EDITOR_FILL_H
#define MEGAZEUX_EDITOR_FILL_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "buffer.h"
#include "edit.h"
#include "undo.h"

#include "../world_struct.h"

void fill_area(struct world *mzx_world, struct buffer_info *buffer,
 int x, int y, enum editor_mode mode, struct undo_history *history);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_FILL_H */

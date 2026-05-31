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

/* Declarations */

#ifndef MEGAZEUX_EDITOR_SELECT_H
#define MEGAZEUX_EDITOR_SELECT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"

int select_screen_mode(struct world *mzx_world);
int choose_char_set(struct world *mzx_world);
int export_type(struct world *mzx_world);
int import_type(struct world *mzx_world);

MEGAZEUX_END_DECLS

#endif  /* MEGAZEUX_EDITOR_SELECT_H */

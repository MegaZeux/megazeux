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

#ifndef MEGAZEUX_EDITOR_PARAM_H
#define MEGAZEUX_EDITOR_PARAM_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"

int edit_param(struct world *mzx_world, int id, int param);
int edit_scroll(struct world *mzx_world, struct scroll *cur_scroll);
int edit_sensor(struct world *mzx_world, struct sensor *cur_sensor);
void edit_robot(context *ctx, struct robot *cur_robot, int *ret_value);
void edit_global_robot(context *ctx);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_PARAM_H */

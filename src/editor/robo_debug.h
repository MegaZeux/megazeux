/* MegaZeux
 *
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

#ifndef MEGAZEUX_EDITOR_ROBO_DEBUG_H
#define MEGAZEUX_EDITOR_ROBO_DEBUG_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"

EDITOR_LIBSPEC void __debug_robot_config(struct world *mzx_world);
EDITOR_LIBSPEC void __debug_robot_reset(struct world *mzx_world);

EDITOR_LIBSPEC int __debug_robot_break(context *ctx,
 struct robot *cur_robot, int id, int lines_run);
EDITOR_LIBSPEC int __debug_robot_watch(context *ctx,
 struct robot *cur_robot, int id, int lines_run);

EDITOR_LIBSPEC void free_breakpoints(void);

void update_watchpoint_last_values(struct world *mzx_world);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_ROBO_DEBUG_H */

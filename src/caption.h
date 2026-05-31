/* MegaZeux
 *
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_CAPTION_H
#define MEGAZEUX_CAPTION_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

/**
 * Set the current world to display in the caption.
 * Clears all editor-related caption values.
 *
 * @param mzx_world   The active world.
 */

CORE_LIBSPEC void caption_set_world(struct world *mzx_world);

#ifdef CONFIG_EDITOR

/**
 * Set the current board to display in the caption in the editor.
 * Clears the caption robot value.
 *
 * @param mzx_world   The active world.
 * @param board       The active board, or NULL for no board.
 */

CORE_LIBSPEC void caption_set_board(struct world *mzx_world,
 struct board *board);

/**
 * Set the current robot to display in the caption in the editor.
 *
 * @param mzx_world   The active world.
 * @param robot       The active robot, or NULL for no robot.
 */

CORE_LIBSPEC void caption_set_robot(struct world *mzx_world,
 struct robot *robot);

/**
 * Set the modified value of the current world.
 *
 * @param modified    True if the world has been modified.
 */

CORE_LIBSPEC void caption_set_modified(boolean modified);

#endif

/**
 * Set the updates available value for the caption.
 *
 * @param available   True if updates are available.
 */

#ifdef CONFIG_UPDATER
CORE_LIBSPEC void caption_set_updates_available(boolean available);
#endif

/**
 * Set the current FPS value.
 *
 * @param fps         The current FPS of MegaZeux, or <=0.0 to hide the display.
 */

#ifdef CONFIG_FPS
void caption_set_fps(double fps);
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_CAPTION_H */

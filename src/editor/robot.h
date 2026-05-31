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

#ifndef MEGAZEUX_EDITOR_ROBOT_H
#define MEGAZEUX_EDITOR_ROBOT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../world_struct.h"

void create_blank_robot_direct(struct robot *cur_robot, int x, int y);
void create_blank_scroll_direct(struct scroll *cur_croll);
void create_blank_sensor_direct(struct sensor *cur_sensor);
void clear_scroll_contents(struct scroll *cur_scroll);
void replace_scroll(struct board *src_board, struct scroll *src_scroll,
 int dest_id);
void replace_sensor(struct board *src_board, struct sensor *src_sensor,
 int dest_id);

#ifdef CONFIG_DEBYTECODE

void duplicate_robot_direct_source(struct world *mzx_world,
 struct robot *cur_robot, struct robot *copy_robot, int x, int y);
int duplicate_robot_source(struct world *mzx_world,
 struct board *src_board, struct robot *cur_robot, int x, int y);
void replace_robot_source(struct world *mzx_world,
 struct board *src_board, struct robot *src_robot, int dest_id);

#else // !CONFIG_DEBYTECODE

#include "../robot.h"

static inline void duplicate_robot_direct_source(struct world *mzx_world,
 struct robot *cur_robot, struct robot *copy_robot, int x, int y)
{
  duplicate_robot_direct(mzx_world, cur_robot, copy_robot, x, y, 0);
}

static inline int duplicate_robot_source(struct world *mzx_world,
 struct board *src_board, struct robot *cur_robot, int x, int y)
{
  return duplicate_robot(mzx_world, src_board, cur_robot, x, y, 0);
}

static inline void replace_robot_source(struct world *mzx_world,
 struct board *src_board, struct robot *src_robot, int dest_id)
{
  replace_robot(mzx_world, src_board, src_robot, dest_id);
}

#endif // !CONFIG_DEBYTECODE

#ifndef CONFIG_DEBYTECODE
void prepare_robot_source(struct robot *cur_robot);
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_ROBOT_H */

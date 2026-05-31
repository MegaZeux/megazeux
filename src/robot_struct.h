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

#ifndef MEGAZEUX_ROBOT_STRUCT_H
#define MEGAZEUX_ROBOT_STRUCT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "data.h"
#include "legacy_rasm.h"

struct label
{
  // Point this to the name in the robot
  char *name;
  int position;
  // Used for zapping.
  int cmd_position;
  // Set to true if zapped
  boolean zapped;
};

struct scroll
{
  int num_lines;

  // Pointer to scroll's message
  char *mesg;
  size_t mesg_size;

  char used;
};

struct sensor
{
  char sensor_name[ROBOT_NAME_SIZE];
  char sensor_char;
  char robot_to_mesg[ROBOT_NAME_SIZE];

  char used;
};

struct robot
{
  int world_version;
  int program_source_length;
  char *program_source;
  int program_bytecode_length;
  char *program_bytecode;         // Pointer to robot's program
  char robot_name[ROBOT_NAME_SIZE];
  unsigned char robot_char;
  // Location of start of line (pt to FF for none)
  int cur_prog_line;
  int pos_within_line;            // Countdown for GO and WAIT
  int robot_cycle;
  int cycle_count;
  char bullet_type;
  char is_locked;
  char can_lavawalk;              // Can always travel on fire
  char can_goopwalk;
  enum dir walk_dir;
  enum dir last_touch_dir;
  enum dir last_shot_dir;

  // Used for IF ALIGNED "robot", THISX/THISY, PLAYERDIST,
  // HORIZPLD, VERTPLD, and others. Keep updated at all
  // times. Set to -1/-1 for global robot.
  int xpos, ypos;

  // Yeah, that note about keeping xpos/ypos "udpated" at all times?
  // Versions prior to 2.92 didn't take that seriously, causing
  // all of the features using those values to break in several cases.
  // The easiest way to deal with this is to have compatibility
  // position values for features that rely on xpos/ypos.
  int compat_xpos, compat_ypos;

  // 0 = Un-run yet, 1=Was run but only END'd, WAIT'd, or was
  // inactive, 2=To be re-run on a second robot-run due to a
  // rec'd message
  char status;

  // This is deprecated. It's only there for legacy reasons.
  char used;

  // Loop count. Loops go back to first seen LOOP
  // START, loop at first seen LOOP #, and an ABORT
  // LOOP jumps to right after first seen LOOP #.
  int loop_count;

  int num_labels;
  struct label **label_list;

  int stack_size;
  int stack_pointer;
  int *stack;

  // Local counters - store in save file
  int local[32];

#ifdef CONFIG_EDITOR
  // A mapping of bytecode lines to source lines.
  struct command_mapping *command_map;
  int command_map_length;

  // Total commands run; commands run in cycle; commands seen by debugger
  int commands_total;
  int commands_cycle;
  int commands_caught;
#endif
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ROBOT_STRUCT_H */

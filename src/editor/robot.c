/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#include "robot.h"

#include "../board_struct.h"
#include "../legacy_rasm.h"
#include "../robot.h"

#include <string.h>

// TODO: This might have to be something different.
void create_blank_robot_direct(struct robot *cur_robot, int x, int y)
{
  memset(cur_robot, 0, sizeof(struct robot));
  cur_robot->robot_name[0] = 0;

#ifdef CONFIG_DEBYTECODE
  {
    const char empty_robot_program[] = "";
    int program_source_length = strlen(empty_robot_program) + 1;

    cur_robot->program_source = cmalloc(program_source_length);
    cur_robot->program_source_length = program_source_length;

    memcpy(cur_robot->program_source,
     empty_robot_program, program_source_length);
  }
#else
  cur_robot->program_bytecode = cmalloc(2);
  cur_robot->program_bytecode_length = 2;
  cur_robot->program_bytecode[0] = 0xFF;
  cur_robot->program_bytecode[1] = 0x00;
#endif

  cur_robot->xpos = x;
  cur_robot->ypos = y;
  cur_robot->robot_char = 2;
  cur_robot->bullet_type = 1;
  cur_robot->used = 1;
  cur_robot->cur_prog_line = 1;

  cur_robot->commands_total = 0;
  cur_robot->commands_cycle = 0;
}

void create_blank_scroll_direct(struct scroll *cur_scroll)
{
  char *message = cmalloc(3);

  memset(cur_scroll, 0, sizeof(struct scroll));

  cur_scroll->num_lines = 1;
  cur_scroll->mesg_size = 3;
  cur_scroll->mesg = message;
  cur_scroll->used = 1;
  message[0] = 0x01;
  message[1] = '\n';
  message[2] = 0x00;
}

void create_blank_sensor_direct(struct sensor *cur_sensor)
{
  memset(cur_sensor, 0, sizeof(struct sensor));
  cur_sensor->used = 1;
}

void clear_scroll_contents(struct scroll *cur_scroll)
{
  free(cur_scroll->mesg);
  cur_scroll->mesg = NULL;
  cur_scroll->used = 0;
}

void replace_scroll(struct board *src_board, struct scroll *src_scroll,
 int dest_id)
{
  struct scroll *cur_scroll = src_board->scroll_list[dest_id];
  clear_scroll_contents(cur_scroll);
  duplicate_scroll_direct(src_scroll, cur_scroll);
}

void replace_sensor(struct board *src_board, struct sensor *src_sensor,
 int dest_id)
{
  struct sensor *cur_sensor = src_board->sensor_list[dest_id];
  duplicate_sensor_direct(src_sensor, cur_sensor);
}

#ifdef CONFIG_DEBYTECODE

// For the editor: only copies the source and static parameters, doesn't
// worry about program or labels, but it needs the runtime stuff,
// for now (this should change later)

void duplicate_robot_direct_source(struct world *mzx_world,
 struct robot *cur_robot, struct robot *copy_robot, int x, int y)
{
  int program_source_length = cur_robot->program_source_length;

  // Copy all the base contents.
  memcpy(copy_robot, cur_robot, sizeof(struct robot));

  copy_robot->program_source = cmalloc(program_source_length);
  memcpy(copy_robot->program_source, cur_robot->program_source,
   program_source_length);

  // Bytecode starts as NULL.
  copy_robot->program_bytecode = NULL;

  // Give the robot a new, fresh stack
  copy_robot->stack = NULL;
  copy_robot->stack_size = 0;
  copy_robot->stack_pointer = 0;
  copy_robot->xpos = x;
  copy_robot->ypos = y;

  copy_robot->pos_within_line = 0;
  copy_robot->status = 0;
}

void replace_robot_source(struct world *mzx_world, struct board *src_board,
 struct robot *src_robot, int dest_id)
{
  char old_name[64];
  int x = (src_board->robot_list[dest_id])->xpos;
  int y = (src_board->robot_list[dest_id])->ypos;
  struct robot *cur_robot = src_board->robot_list[dest_id];

  strcpy(old_name, cur_robot->robot_name);

  clear_robot_contents(cur_robot);
  duplicate_robot_direct_source(mzx_world, src_robot, cur_robot, x, y);
  strcpy(cur_robot->robot_name, old_name);

  if(dest_id)
    change_robot_name(src_board, cur_robot, src_robot->robot_name);
}

int duplicate_robot_source(struct world *mzx_world, struct board *src_board,
 struct robot *cur_robot, int x, int y)
{
  int dest_id = find_free_robot(src_board);
  if(dest_id != -1)
  {
    struct robot *copy_robot = cmalloc(sizeof(struct robot));
    duplicate_robot_direct_source(mzx_world, cur_robot, copy_robot, x, y);
    add_robot_name_entry(src_board, copy_robot, copy_robot->robot_name);
    src_board->robot_list[dest_id] = copy_robot;
  }

  return dest_id;
}

#else // !CONFIG_DEBYTECODE

void prepare_robot_source(struct robot *cur_robot)
{
  // Disassemble the robot to source. Used by the robot debugger.
  if(!cur_robot->program_source)
  {
    disassemble_program(
     cur_robot->program_bytecode, cur_robot->program_bytecode_length,
     &cur_robot->program_source, &cur_robot->program_source_length,
     &cur_robot->command_map, &cur_robot->command_map_length
    );
  }
}

#endif // !CONFIG_DEBYTECODE

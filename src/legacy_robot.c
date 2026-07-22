/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "legacy_robot.h"

#include "const.h"
#include "error.h"
#include "legacy_rasm.h"
#include "rasm.h"
#include "robot.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"
#include "io/memfile.h"
#include "io/vio.h"
#include "platform/log.h"

/* This is the largest allowed stack size in MZX 2.84X, in case the maximum
 * stack size ever gets increased for some reason. */
#define LEGACY_ROBOT_MAX_STACK 65536

static boolean legacy_load_robot_v1(struct world *mzx_world, struct robot *cur_robot,
 vfile *vf, int savegame, int file_version)
{
  int robot_location = vftell(vf);
  char *program_v1 = NULL;
  char *program_v2 = NULL;
  int program_v1_length;
  int program_v2_length;

  create_blank_robot(cur_robot);
  cur_robot->world_version = mzx_world->version;

  program_v1_length = vfgetw(vf);
  vfgetw(vf); // Unused high bytes
  vfgetd(vf); // Junk

  if(!vfread(cur_robot->robot_name, LEGACY_ROBOT_NAME_SIZE, 1, vf))
    return false;

  cur_robot->robot_name[LEGACY_ROBOT_NAME_SIZE - 1] = '\0';

  cur_robot->robot_char = vfgetc(vf);
  cur_robot->cur_prog_line = vfgetw(vf);
  vfgetw(vf); // Unused high bytes
  cur_robot->pos_within_line = vfgetc(vf);
  cur_robot->robot_cycle = vfgetc(vf);
  cur_robot->cycle_count = vfgetc(vf);
  cur_robot->bullet_type = vfgetc(vf);
  cur_robot->is_locked = vfgetc(vf);
  cur_robot->can_lavawalk = vfgetc(vf);
  cur_robot->walk_dir = (enum dir)vfgetc(vf);
  cur_robot->last_touch_dir = (enum dir)vfgetc(vf);
  cur_robot->last_shot_dir = (enum dir)vfgetc(vf);

  if(!program_v1_length)
  {
    create_blank_robot_program(cur_robot);
    return true;
  }

  program_v1 = (char *)cmalloc(program_v1_length);
  if(!vfread(program_v1, program_v1_length, 1, vf))
  {
    trace("failed to read v1 program (len %d) @ %d\n", program_v1_length, robot_location);
    goto err;
  }

  // Convert v1 program to v2.
  if(!legacy_convert_v1_program(&program_v2, &program_v2_length,
   &cur_robot->cur_prog_line, program_v1, program_v1_length))
  {
    trace("legacy convert v1 program failed @ %d\n", robot_location);
    goto err;
  }

  free(program_v1);
  program_v1 = NULL;

  if(!validate_legacy_bytecode(&program_v2, &program_v2_length,
   &cur_robot->cur_prog_line))
  {
    trace("validate legacy bytecode failed @ %d\n", robot_location);
    goto err;
  }

#ifdef CONFIG_DEBYTECODE

  // TODO: now convert v2 to source. This multi-step conversion
  // mess should be replaced with a v1 bytecode frontend.
  cur_robot->program_source =
   legacy_disassemble_program(program_v2, program_v2_length,
   &(cur_robot->program_source_length), true, 10);

  if(cur_robot->cur_prog_line)
  {
    // Translate the legacy current bytecode offset and stack bytecode offsets
    // into usable new bytecode offsets. This may compile the robot program.
    translate_robot_bytecode_offsets(mzx_world, cur_robot, file_version);
  }
  free(program_v2);
  program_v2 = NULL;

#else

  cur_robot->program_bytecode = program_v2;
  cur_robot->program_bytecode_length = program_v2_length;
  cache_robot_labels(cur_robot);

#endif

  return true;

err:
  error_message(E_BOARD_ROBOT_CORRUPT, robot_location, NULL);
  free(program_v1);
  free(program_v2);
  cur_robot->used = 0;
  return false;
}

struct robot *legacy_load_robot_allocate(struct world *mzx_world, vfile *vf,
 int savegame, int file_version, boolean *truncated)
{
  struct robot *cur_robot = (struct robot *)cmalloc(sizeof(struct robot));

  cur_robot->stack = NULL;
  cur_robot->label_list = NULL;
  cur_robot->program_bytecode = NULL;
  cur_robot->program_source = NULL;
  cur_robot->num_labels = 0;

  if(file_version < V200)
  {
    if(!legacy_load_robot_v1(mzx_world, cur_robot, vf, savegame, file_version))
      *truncated = true;
  }
  else

  if(!legacy_load_robot(mzx_world, cur_robot, vf, savegame, file_version))
    *truncated = true;

  return cur_robot;
}

// Most of this stuff does not have to be loaded unless savegame
// is set.
void legacy_load_robot_from_memory(struct world *mzx_world,
 struct robot *cur_robot, struct memfile *mf, int savegame, int version,
 int robot_location)
{
  int program_length;
  int xpos, ypos;
  int i;

  cur_robot->world_version = mzx_world->version;

  cur_robot->stack = NULL;
  cur_robot->label_list = NULL;
  cur_robot->program_bytecode = NULL;
  cur_robot->program_source = NULL;
  cur_robot->num_labels = 0;

  program_length = mfgetw(mf);
  // Skip DOS program pointer field.
  mf->current += 2;

#ifdef CONFIG_EDITOR
  cur_robot->command_map = NULL;
  cur_robot->command_map_length = 0;

  cur_robot->commands_total = 0;
  cur_robot->commands_cycle = 0;
  cur_robot->commands_caught = 0;
#endif

  mfread(cur_robot->robot_name, LEGACY_ROBOT_NAME_SIZE, 1, mf);
  cur_robot->robot_name[LEGACY_ROBOT_NAME_SIZE - 1] = '\0';

  cur_robot->robot_char = mfgetc(mf);
  cur_robot->cur_prog_line = mfgetw(mf);
  cur_robot->pos_within_line = mfgetc(mf);
  cur_robot->robot_cycle = mfgetc(mf);
  cur_robot->cycle_count = mfgetc(mf);
  cur_robot->bullet_type = mfgetc(mf);
  cur_robot->is_locked = mfgetc(mf);
  cur_robot->can_lavawalk = mfgetc(mf);
  cur_robot->walk_dir = (enum dir)mfgetc(mf);
  cur_robot->last_touch_dir = (enum dir)mfgetc(mf);
  cur_robot->last_shot_dir = (enum dir)mfgetc(mf);
  xpos = mfgetw(mf);
  ypos = mfgetw(mf);
  cur_robot->status = mfgetc(mf);
  // Skip local - these are in the save files now
  mf->current += 2;
  cur_robot->used = mfgetc(mf);
  if(version <= V283)
    cur_robot->loop_count = mfgetw(mf);
  else
    mf->current += 2;

  // Fix xpos and ypos for global robot
  xpos = (xpos >= 32768) ? -1 : xpos;
  ypos = (ypos >= 32768) ? -1 : ypos;
  cur_robot->xpos = xpos;
  cur_robot->ypos = ypos;
  cur_robot->compat_xpos = xpos;
  cur_robot->compat_ypos = ypos;

  // If savegame, there's some additional information to get
  if(savegame)
  {
    int stack_skip = 0;
    int stack_size;

    if(version >= V284)
      cur_robot->loop_count = mfgetd(mf);

    for(i = 0; i < 32; i++)
      cur_robot->local[i] = mfgetd(mf);

    // Double so we can fit pos_within_line values in.
    stack_size = mfgetd(mf) * 2;
    cur_robot->stack_pointer = mfgetd(mf) * 2;

    // Don't allow the stack size to be loaded over the maximum.
    if(stack_size > LEGACY_ROBOT_MAX_STACK)
    {
      stack_skip = LEGACY_ROBOT_MAX_STACK - stack_size;
      stack_size = LEGACY_ROBOT_MAX_STACK;
    }
    else

    if(stack_size < 0)
      stack_size = 0;

    // Also don't allow stack positions beyond the size of the stack.
    cur_robot->stack_pointer = CLAMP(cur_robot->stack_pointer, 0, stack_size);

    cur_robot->stack = (int *)ccalloc(stack_size, sizeof(int));
    for(i = 0; i < stack_size; i += 2)
    {
      cur_robot->stack[i] = mfgetd(mf);
      cur_robot->stack[i+1] = 0;
    }

    // In the (unlikely) event the saved stack size was larger than the maximum,
    // the unused stack values need to be skipped...
    mf->current += 4 * (stack_skip / 2);

    cur_robot->stack_size = stack_size;
  }
  else
  {
    // Otherwise, allocate some stuff; local counters are 0
    cur_robot->loop_count = 0;
    memset(cur_robot->local, 0, sizeof(int) * 32);

    // Give an empty stack.
    cur_robot->stack_size = 0;
    cur_robot->stack = NULL;

    // Initialize the stack pointer to the bottom
    cur_robot->stack_pointer = 0;
  }

#ifdef CONFIG_DEBYTECODE

  // The program is legacy bytecode and we have to convert it to source.
  cur_robot->program_bytecode = NULL;
  cur_robot->program_bytecode_length = 0;

  if((cur_robot->used) || (program_length >= 2))
  {
    char *program_legacy_bytecode = (char *)cmalloc(program_length);
    mfread(program_legacy_bytecode, program_length, 1, mf);

    if(!validate_legacy_bytecode(&program_legacy_bytecode, &program_length,
     &(cur_robot->cur_prog_line)))
    {
      free(program_legacy_bytecode);
      goto err_invalid;
    }

    cur_robot->program_source =
     legacy_disassemble_program(program_legacy_bytecode, program_length,
     &(cur_robot->program_source_length), true, 10);

    free(program_legacy_bytecode);

    // Translate the legacy current bytecode offset and stack bytecode offsets
    // into usable new bytecode offsets. This may compile the robot program.
    translate_robot_bytecode_offsets(mzx_world, cur_robot, version);
  }
  else
  {
    // Skip over program, we're not going to use it.
    mf->current += program_length;

    create_blank_robot_program(cur_robot);
    cur_robot->cur_prog_line = 0;
  }

#else /* !CONFIG_DEBYTECODE */

  cur_robot->program_bytecode_length = program_length;
  if(program_length > 0)
  {
    cur_robot->program_bytecode = (char *)cmalloc(program_length);
    mfread(cur_robot->program_bytecode, program_length, 1, mf);

    if(!validate_legacy_bytecode(&cur_robot->program_bytecode, &program_length,
     &(cur_robot->cur_prog_line)))
    {
      // Only error for used robots; unused robots don't really matter and
      // in some games (Slave Pit, Wes) may have garbage programs.
      if(cur_robot->used)
        goto err_invalid;

      debug("silently ignoring unused corrupt robot @ %4.4X\n", robot_location);
      clear_robot_contents(cur_robot);
      create_blank_robot(cur_robot);
      create_blank_robot_program(cur_robot);
      cur_robot->cur_prog_line = 0;
    }
    else
      cur_robot->program_bytecode_length = program_length;

    // The stack must also be checked for invalid offsets.
    fix_robot_stack_offsets(cur_robot);

    // Now create a label cache IF the robot is in use
    if(cur_robot->used)
      cache_robot_labels(cur_robot);
  }
#endif /* !CONFIG_DEBYTECODE */

  return;

err_invalid:
  // Don't worry about the unused source and bytecode
  // that were allocated, they'll be cleared later.
  error_message(E_BOARD_ROBOT_CORRUPT, robot_location, NULL);

  clear_robot_contents(cur_robot);
  create_blank_robot(cur_robot);
  create_blank_robot_program(cur_robot);
  strcpy(cur_robot->robot_name, "<<error>>");
  cur_robot->cur_prog_line = 0;
}

/**
 * Calculate the amount of a robot that needs to be read in order to
 * calculate its size.
 */
size_t legacy_calculate_partial_robot_size(int savegame, int version)
{
  size_t partial_robot_size = 41;

  if(savegame)
  {
    if(version >= V284)
      partial_robot_size += 4; // loopcount
    partial_robot_size += 32 * 4; // 32 local counters
    partial_robot_size += 2 * 4; // stack size and position
  }
  return partial_robot_size;
}

/**
 * This function is used to calculate a robot's total size based on an
 * incomplete load of that robot. This allows the robot size to be
 * calculated before the entire thing is loaded.
 */
size_t legacy_load_robot_calculate_size(const void *buffer, int savegame,
 int version)
{
  struct memfile mf;
  int program_length;
  int stack_size;

  // Base robot size
  size_t robot_size = legacy_calculate_partial_robot_size(savegame, version);

  // First, read the program length (0 bytes in)
  mfopen(buffer, robot_size, &mf);
  program_length = mfgetw(&mf);
  mfgetw(&mf);

  // Next, if this is a savegame robot, read the stack size
  if(savegame)
  {
    mfseek(&mf, robot_size - 8, SEEK_SET);
    stack_size = mfgetd(&mf);

    // Don't allow the stack size to be loaded over the maximum.
    // Nothing valid should ever be higher than this and since the full robot
    // is being allocated based on this, invalid robots need to be truncated.
    if(stack_size > LEGACY_ROBOT_MAX_STACK)
    {
      return robot_size;
    }
    else

    if(stack_size < 0)
      stack_size = 0;

    robot_size += stack_size * 4;
  }

  robot_size += program_length;
  return robot_size;
}

boolean legacy_load_robot(struct world *mzx_world, struct robot *cur_robot,
 vfile *vf, int savegame, int version)
{
  int robot_location = vftell(vf);
  size_t partial_size = legacy_calculate_partial_robot_size(savegame, version);
  char *buffer = (char *)cmalloc(partial_size);
  boolean truncated = true;
  size_t full_size;

  if(vfread(buffer, partial_size, 1, vf))
  {
    full_size = legacy_load_robot_calculate_size(buffer, savegame, version);
    buffer = crealloc(buffer, full_size);

    if((partial_size == full_size) ||
     vfread(buffer + partial_size, full_size - partial_size, 1, vf))
      truncated = false;
  }

  create_blank_robot(cur_robot);

  if(truncated)
  {
    error_message(E_BOARD_ROBOT_CORRUPT, robot_location, NULL);
    create_blank_robot_program(cur_robot);
    strcpy(cur_robot->robot_name, "<<error>>");
    vfseek(vf, 0, SEEK_END);
  }
  else
  {
    struct memfile mf;
    mfopen(buffer, full_size, &mf);
    legacy_load_robot_from_memory(mzx_world, cur_robot, &mf, savegame,
     version, robot_location);
  }
  free(buffer);
  return !truncated;
}

/**
 * Convert v1 scroll text to scroll text with color code support.
 *
 * 1.x uses a bizarre color code hack where lines can start with ![byte].
 * The entire line will be colored [byte] (including non-text areas) and the
 * color code will be displayed as two spaces.
 *
 * Convert these to Robotic ~@-style color codes, pad their affected lines with
 * spaces so they display correctly, and escape existing ~@s.
 */
static boolean legacy_convert_scroll_v1(struct scroll *cur_scroll)
{
  char *old_text = cur_scroll->mesg;
  char *new_text;
  size_t old_size = cur_scroll->mesg_size;
  size_t new_size;
  size_t new_pos;
  size_t i;

  new_text = (char *)cmalloc(old_size * 2);
  new_size = old_size * 2;
  if(!new_text)
    goto err;

  new_text[0] = 0x01;
  new_pos = 1;

  for(i = 1; i < old_size && old_text[i];)
  {
    // Start of line.
    char linebuf[256];
    int pad = 0;
    size_t j = 0;

    if(old_text[i] == '!')
    {
      // Legacy v1 color code.
      static const char hex[] = "0123456789abcdef";
      uint8_t color = old_text[i + 1];
      if(!color)
        goto err;

      linebuf[j++] = '~';
      linebuf[j++] = hex[color & 0xf];
      linebuf[j++] = '@';
      linebuf[j++] = hex[color >> 4];
      linebuf[j++] = ' ';
      linebuf[j++] = ' ';
      i += 2;
      pad = 62;
    }

    // Direct line copy with escapes for ~ and @.
    while(old_text[i])
    {
      if(j >= sizeof(linebuf) - 3)
        goto err;

      if(old_text[i] == '\n')
      {
        i++;
        break;
      }

      if(old_text[i] == '~')
        linebuf[j++] = '~';

      if(old_text[i] == '@')
        linebuf[j++] = '@';

      linebuf[j++] = old_text[i++];
      pad--;
    }
    for(; pad > 0; pad--)
      linebuf[j++] = ' ';

    linebuf[j++] = '\n';

    // Append line (+1 for scroll terminator).
    if(j + new_pos + 1 > new_size)
    {
      char *t;
      while(j + new_pos + 1 > new_size)
      {
        if(new_size > INT_MAX)
          goto err;

        new_size *= 2;
      }
      t = (char *)crealloc(new_text, new_size);
      if(!t)
        goto err;

      new_text = t;
    }
    memcpy(new_text + new_pos, linebuf, j);
    new_pos += j;
  }
  new_text[new_pos++] = '\0';

  cur_scroll->mesg = (char *)crealloc(new_text, new_pos);
  cur_scroll->mesg_size = new_pos;
  if(!cur_scroll->mesg)
    goto err;

  free(old_text);
  return true;

err:
  free(old_text);
  free(new_text);
  cur_scroll->mesg = NULL;
  return false;
}

static void legacy_load_scroll(struct scroll *cur_scroll, vfile *vf, int file_version)
{
  int scroll_size;

  cur_scroll->mesg = NULL;
  cur_scroll->num_lines = vfgetw(vf);

  if(file_version >= V200)
  {
    vfgetw(vf); // Skip junk
    scroll_size = vfgetw(vf);
    cur_scroll->used = vfgetc(vf);
  }
  else
  {
    vfgetd(vf); // Skip junk
    scroll_size = vfgetd(vf);
    cur_scroll->used = 1;
  }

  if(scroll_size < 0 || scroll_size > 65535)
    goto scroll_err;

  cur_scroll->mesg_size = scroll_size;
  cur_scroll->mesg = (char *)cmalloc(scroll_size);
  if(!vfread(cur_scroll->mesg, scroll_size, 1, vf))
    goto scroll_err;

  if(scroll_size < 3)
    goto scroll_err;

  if(file_version < V200)
  {
    if(!legacy_convert_scroll_v1(cur_scroll))
      goto scroll_err;
  }
  else
    cur_scroll->mesg[scroll_size - 1] = '\0';

  return;

scroll_err:
  // Something screwed up, slip in an empty scroll.
  cur_scroll->num_lines = 1;
  cur_scroll->mesg_size = 3;
  cur_scroll->used = 1;

  free(cur_scroll->mesg);
  cur_scroll->mesg = (char *)cmalloc(3);
  strcpy(cur_scroll->mesg, "\x01\x0A");
}

struct scroll *legacy_load_scroll_allocate(vfile *vf, int file_version)
{
  struct scroll *cur_scroll = (struct scroll *)cmalloc(sizeof(struct scroll));
  legacy_load_scroll(cur_scroll, vf, file_version);
  return cur_scroll;
}

static void legacy_load_sensor(struct sensor *cur_sensor, vfile *vf, int file_version)
{
  if(!vfread(cur_sensor->sensor_name, LEGACY_ROBOT_NAME_SIZE, 1, vf))
    goto sensor_err;

  cur_sensor->sensor_char = vfgetc(vf);

  if(!vfread(cur_sensor->robot_to_mesg, LEGACY_ROBOT_NAME_SIZE, 1, vf))
    goto sensor_err;

  cur_sensor->sensor_name[LEGACY_ROBOT_NAME_SIZE - 1] = '\0';
  cur_sensor->robot_to_mesg[LEGACY_ROBOT_NAME_SIZE - 1] = '\0';

  if(file_version >= V200)
    cur_sensor->used = vfgetc(vf);
  else
    cur_sensor->used = 1;

  return;

sensor_err:
  // Something screwed up, insert filler
  strcpy(cur_sensor->sensor_name, "");
  strcpy(cur_sensor->robot_to_mesg, "");
  cur_sensor->sensor_char = 'S';
  cur_sensor->used = 1;
}

struct sensor *legacy_load_sensor_allocate(vfile *vf, int file_version)
{
  struct sensor *cur_sensor = (struct sensor *)cmalloc(sizeof(struct sensor));
  legacy_load_sensor(cur_sensor, vf, file_version);
  return cur_sensor;
}

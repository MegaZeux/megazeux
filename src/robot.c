/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "board.h"
#include "const.h"
#include "counter.h"
#include "error.h"
#include "expr.h"
#include "extmem.h"
#include "game_ops.h"
#include "game_player.h"
#include "graphics.h"
#include "idarray.h"
#include "legacy_rasm.h"
#include "memcasecmp.h"
#include "robot.h"
#include "rasm.h"
#include "scrdisp.h"
#include "str.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "world_format.h"
#include "world_struct.h"
#include "io/memfile.h"
#include "io/zip.h"

#include "event/event.h"
#include "platform/log.h"

void create_blank_robot(struct robot *cur_robot)
{
  int i;

  // Clear the robot before use.
  cur_robot->robot_name[0] = 0;
  cur_robot->robot_char = 'R';

  cur_robot->stack = NULL;
  cur_robot->stack_size = 0;
  cur_robot->stack_pointer = 0;

  cur_robot->label_list = NULL;
  cur_robot->num_labels = 0;

  cur_robot->program_bytecode_length = 0;
  cur_robot->program_bytecode = NULL;
  cur_robot->program_source_length = 0;
  cur_robot->program_source = NULL;

  cur_robot->cur_prog_line = 1;
  cur_robot->pos_within_line = 0;
  cur_robot->robot_cycle = 0;
  cur_robot->cycle_count = 0;
  cur_robot->bullet_type = 1;
  cur_robot->is_locked = 0;
  cur_robot->can_lavawalk = 0;
  cur_robot->can_goopwalk = 0;
  cur_robot->walk_dir = 0;
  cur_robot->last_touch_dir = 0;
  cur_robot->last_shot_dir = 0;
  cur_robot->xpos = -1;
  cur_robot->ypos = -1;
  cur_robot->compat_xpos = -1;
  cur_robot->compat_ypos = -1;
  cur_robot->status = 0;
  cur_robot->used = 1;

  cur_robot->loop_count = 0;

  for(i = 0; i<32; i++)
    cur_robot->local[i] = 0;

#ifdef CONFIG_EDITOR
  cur_robot->command_map = NULL;
  cur_robot->command_map_length = 0;

  cur_robot->commands_total = 0;
  cur_robot->commands_cycle = 0;
#endif
}

void create_blank_robot_program(struct robot *cur_robot)
{
  // Add a blank robot program to a robot.

#ifdef CONFIG_DEBYTECODE
  cur_robot->program_source_length = 2;
  cur_robot->program_source = cmalloc(2);
  strcpy(cur_robot->program_source, "\n");
#else
  cur_robot->program_bytecode_length = 2;
  cur_robot->program_bytecode = cmalloc(2);
  strcpy(cur_robot->program_bytecode, "\xFF");
#endif
}

#define err_if_skipped(idn) if(last_ident < idn) { goto err_invalid; }

static int load_robot_from_memory(struct world *mzx_world, struct robot *cur_robot,
 struct memfile *mf, int savegame, int file_version)
{
#ifdef CONFIG_DEBYTECODE
  char *program_legacy_bytecode = NULL;
  size_t program_legacy_bytecode_length = 0;
  char *saved_label_zaps = NULL;
  int num_label_zaps = 0;
#endif
  struct memfile prop;
  int last_ident = -1;
  int v_size;
  int ident;
  int size;
  int i;

  create_blank_robot(cur_robot);

  while(next_prop(&prop, &ident, &size, mf))
  {
    switch(ident)
    {
      case RPROP_EOF:
        mfseek(mf, 0, SEEK_END);
        break;

      case RPROP_ROBOT_NAME:
        size = MIN(size, ROBOT_NAME_SIZE - 1);
        mfread(cur_robot->robot_name, size, 1, &prop);
        cur_robot->robot_name[size] = '\0';
        break;

      case RPROP_ROBOT_CHAR:
        err_if_skipped(RPROP_ROBOT_NAME);
        cur_robot->robot_char = load_prop_int(&prop);
        break;

      case RPROP_XPOS:
        err_if_skipped(RPROP_ROBOT_CHAR);
        cur_robot->xpos = load_prop_int_s(&prop, -1, 32767);
        cur_robot->compat_xpos = cur_robot->xpos;
        break;

      case RPROP_YPOS:
        err_if_skipped(RPROP_XPOS);
        cur_robot->ypos = load_prop_int_s(&prop, -1, 32767);
        cur_robot->compat_ypos = cur_robot->ypos;
        break;

      // Legacy world, now save
      case RPROP_CUR_PROG_LINE:
        cur_robot->cur_prog_line = load_prop_int(&prop);
        break;

      case RPROP_POS_WITHIN_LINE:
        cur_robot->pos_within_line = load_prop_int(&prop);
        break;

      case RPROP_ROBOT_CYCLE:
        cur_robot->robot_cycle = load_prop_int(&prop);
        break;

      case RPROP_CYCLE_COUNT:
        cur_robot->cycle_count = load_prop_int(&prop);
        break;

      case RPROP_BULLET_TYPE:
        cur_robot->bullet_type = load_prop_int(&prop) & 255;
        break;

      case RPROP_IS_LOCKED:
        cur_robot->is_locked = load_prop_boolean(&prop);
        break;

      case RPROP_CAN_LAVAWALK:
        cur_robot->can_lavawalk = load_prop_int(&prop) & 255;
        break;

      case RPROP_WALK_DIR:
        cur_robot->walk_dir = load_prop_int(&prop);
        break;

      case RPROP_LAST_TOUCH_DIR:
        cur_robot->last_touch_dir = load_prop_int(&prop);
        break;

      case RPROP_LAST_SHOT_DIR:
        cur_robot->last_shot_dir = load_prop_int(&prop);
        break;

      case RPROP_STATUS:
        cur_robot->status = load_prop_int(&prop);
        break;

      // Legacy save
      case RPROP_LOOP_COUNT:
        cur_robot->loop_count = load_prop_int(&prop);
        break;

      case RPROP_LOCALS:
        for(i = 0; i < 32; i++)
          cur_robot->local[i] = mfgetd(&prop);
        break;

      case RPROP_STACK_POINTER:
        cur_robot->stack_pointer = load_prop_int(&prop) & ~1;
        break;

      case RPROP_STACK:
        // # of stack values is file size /4.
        // Furthermore, there should be an even number of values, and a count
        // over ROBOT_MAX_STACK is invalid.
        if(cur_robot->stack)
          break;
        size = MIN((size / 4) & ~1, ROBOT_MAX_STACK);
        cur_robot->stack_size = size;
        cur_robot->stack = cmalloc(size * sizeof(int));
        for(i = 0; i < size; i++)
          cur_robot->stack[i] = mfgetd(&prop);
        break;

      // New
      case RPROP_CAN_GOOPWALK:
        cur_robot->can_goopwalk = load_prop_int(&prop) & 255;
        break;

      // Source/bytecode are slated for separation from these files.
      // When that happens, it might also be good to merge all robots into
      // one file per board.
#ifdef CONFIG_DEBYTECODE

      case RPROP_PROGRAM_LABEL_ZAPS:
      {
        err_if_skipped(RPROP_YPOS);

        // This does not need to be supported in the world format--zaps
        // in world files should be expressed directly in the source code.
        if(!savegame)
          break;

        // These have to be handled after loading the program.
        saved_label_zaps = (char *)prop.start;
        num_label_zaps = size;
        break;
      }

      case RPROP_PROGRAM_SOURCE:
      {
        err_if_skipped(RPROP_YPOS);

        if(cur_robot->program_source || program_legacy_bytecode)
          break;

        if(size > MAX_OBJ_SIZE)
          goto err_invalid;

        // This program is source.
        cur_robot->program_source = cmalloc(size + 1);
        cur_robot->program_source_length = size;

        mfread(cur_robot->program_source, size, 1, &prop);
        cur_robot->program_source[size] = 0;
        break;
      }

      case RPROP_PROGRAM_BYTECODE:
      {
        err_if_skipped(RPROP_YPOS);

        if(cur_robot->program_source || program_legacy_bytecode)
          break;

        if(size > MAX_OBJ_SIZE)
          goto err_invalid;

        // This field should never be found in files saved in >= VERSION_SOURCE.
        if(file_version < VERSION_SOURCE)
        {
          program_legacy_bytecode = cmalloc(size);
          program_legacy_bytecode_length = size;

          mfread(program_legacy_bytecode, size, 1, &prop);
        }
        break;
      }

#else /* !CONFIG_DEBYTECODE */

      case RPROP_PROGRAM_BYTECODE:
      {
        err_if_skipped(RPROP_YPOS);

        if(cur_robot->program_bytecode)
          break;

        if(size > MAX_OBJ_SIZE)
          goto err_invalid;

        if(size > 0)
        {
          cur_robot->program_bytecode = cmalloc(size);
          cur_robot->program_bytecode_length = size;

          mfread(cur_robot->program_bytecode, size, 1, &prop);
        }
        break;
      }

#endif /* !CONFIG_DEBYTECODE */

      default:
        break;
    }
    last_ident = ident;
  }

  // Check the stack for invalid states...
  if(savegame && cur_robot->stack_pointer)
  {
    if(!cur_robot->stack || !cur_robot->stack_size || cur_robot->stack_pointer < 0)
    {
      cur_robot->stack_pointer = 0;
    }
    else

    if(cur_robot->stack_pointer > cur_robot->stack_size)
      cur_robot->stack_pointer = cur_robot->stack_size;
  }

#ifdef CONFIG_DEBYTECODE

  if(program_legacy_bytecode)
  {
    // Load and translate legacy bytecode to modern source.
    v_size = (int)program_legacy_bytecode_length;
    if(!validate_legacy_bytecode(&program_legacy_bytecode, &v_size,
     &cur_robot->cur_prog_line))
    {
      free(program_legacy_bytecode);
      goto err_invalid;
    }

    cur_robot->program_source =
     legacy_disassemble_program(program_legacy_bytecode, v_size,
      &(cur_robot->program_source_length), true, 10);
  }

  if(!cur_robot->program_source)
  {
    // Saved without a program? Give it a blank program.
    create_blank_robot_program(cur_robot);
    cur_robot->cur_prog_line = 0;
  }
  else
  {
    // The saved robot bytecode offsets are actually command numbers (3.00+)
    // or legacy bytecode offsets (<3.00). Translate the loaded offsets into
    // usable bytecode offsets. This may compile the robot program.
    translate_robot_bytecode_offsets(mzx_world, cur_robot, file_version);

    if(saved_label_zaps)
    {
      // Apply saved label zap data.
      prepare_robot_bytecode(mzx_world, cur_robot);

      if(num_label_zaps == cur_robot->num_labels)
      {
        for(i = 0; i < num_label_zaps; i++)
          cur_robot->label_list[i]->zapped = saved_label_zaps[i];
      }
    }
  }

  free(program_legacy_bytecode);

#else /* !CONFIG_DEBYTECODE */

  if(!cur_robot->program_bytecode)
    goto err_invalid;

  // Validate to fix trash programs or out-of-bounds program offsets.
  v_size = cur_robot->program_bytecode_length;
  if(!validate_legacy_bytecode(&cur_robot->program_bytecode, &v_size,
   &(cur_robot->cur_prog_line)))
    goto err_invalid;

  cur_robot->program_bytecode_length = v_size;

  // The stack must also be checked for invalid offsets.
  fix_robot_stack_offsets(cur_robot);

  // Create the label cache for this robot
  cache_robot_labels(cur_robot);

#endif

  return 0;

err_invalid:
  // One of the essential fields of the robot is missing altogether.

  // Nuke the robot
  clear_robot_contents(cur_robot);
  create_blank_robot(cur_robot);
  create_blank_robot_program(cur_robot);
  strcpy(cur_robot->robot_name, "<<error>>");
  cur_robot->cur_prog_line = 0;

#ifdef CONFIG_DEBYTECODE
  free(program_legacy_bytecode);
#endif

  // Trigger error message
  return -1;
}

void load_robot(struct world *mzx_world, struct robot *cur_robot,
 struct zip_archive *zp, int savegame, int file_version)
{
  char *buffer = NULL;
  size_t actual_size;
  struct memfile mf;

  unsigned int method;
  unsigned int board_id;
  unsigned int id;
  boolean is_stream = false;

  zip_get_next_mzx_file_id(zp, NULL, &board_id, &id);
  zip_get_next_method(zp, &method);

  // We aren't saving or loading null robots.
  cur_robot->world_version = mzx_world->version;
  cur_robot->used = 1;

  // If this is an uncompressed memory zip, we can read the memory directly.
  if(zp->is_memory && method == ZIP_M_NONE)
  {
    zip_read_open_mem_stream(zp, &mf);
    is_stream = true;
  }
  else
  {
    zip_get_next_uncompressed_size(zp, &actual_size);
    buffer = cmalloc(actual_size);
    zip_read_file(zp, buffer, actual_size, &actual_size);

    mfopen(buffer, actual_size, &mf);
  }

  if(load_robot_from_memory(mzx_world, cur_robot, &mf, savegame, file_version))
  {
    error_message(E_BOARD_ROBOT_CORRUPT, (board_id << 8)|id, NULL);
  }

  if(is_stream)
    zip_read_close_stream(zp);

  free(buffer);
}

struct robot *load_robot_allocate(struct world *mzx_world,
 struct zip_archive *zp, int savegame, int file_version)
{
  struct robot *cur_robot = cmalloc(sizeof(struct robot));

  load_robot(mzx_world, cur_robot, zp, savegame, file_version);

  return cur_robot;
}

struct scroll *load_scroll_allocate(struct zip_archive *zp)
{
  struct scroll *cur_scroll = ccalloc(1, sizeof(struct scroll));

  size_t actual_size;
  void *buffer;
  struct memfile mf;
  struct memfile prop;
  int ident;
  int size;

  zip_get_next_uncompressed_size(zp, &actual_size);
  if(actual_size > SCROLL_PROPS_SIZE + MAX_OBJ_SIZE)
    actual_size = SCROLL_PROPS_SIZE + MAX_OBJ_SIZE;

  buffer = cmalloc(actual_size);
  if(!buffer)
  {
    zip_skip_file(zp);
    goto err;
  }

  // We aren't saving or loading null scrolls.
  cur_scroll->used = 1;

  zip_read_file(zp, buffer, actual_size, &actual_size);

  mfopen(buffer, actual_size, &mf);

  while(next_prop(&prop, &ident, &size, &mf))
  {
    switch(ident)
    {
      case SCRPROP_EOF:
        mfseek(&mf, 0, SEEK_END);
        break;

      case SCRPROP_NUM_LINES:
        cur_scroll->num_lines = load_prop_int(&prop);
        break;

      case SCRPROP_MESG:
        if(cur_scroll->mesg)
          break;
        if(size < 3)
          goto err;

        // TODO: return value (scroll display doesn't support NULL here)
        size = MIN(size, MAX_OBJ_SIZE);
        cur_scroll->mesg_size = size;
        cur_scroll->mesg = (char *)cmalloc(size);
        mfread(cur_scroll->mesg, size, 1, &prop);
        if(size > 0)
          cur_scroll->mesg[size - 1] = '\0';
        break;

      default:
        break;
    }
  }

  if(cur_scroll->mesg_size < 3)
  {
err:
    // We have an incomplete scroll, so slip in an empty scroll.
    cur_scroll->num_lines = 1;
    cur_scroll->mesg_size = 3;

    // TODO: return value (scroll display doesn't support NULL here)
    free(cur_scroll->mesg);
    cur_scroll->mesg = (char *)cmalloc(3);
    strcpy(cur_scroll->mesg, "\x01\x0A");
  }

  free(buffer);
  return cur_scroll;
}

struct sensor *load_sensor_allocate(struct zip_archive *zp)
{
  struct sensor *cur_sensor = ccalloc(1, sizeof(struct sensor));

  size_t actual_size;
  void *buffer;
  struct memfile mf;
  struct memfile prop;
  int ident;
  int size;

  zip_get_next_uncompressed_size(zp, &actual_size);
  if(actual_size > MAX_OBJ_SIZE) // SENSOR_PROPS_SIZE, but just in case...
    actual_size = MAX_OBJ_SIZE;

  buffer = cmalloc(actual_size);
  if(!buffer)
  {
    zip_skip_file(zp);
    return cur_sensor;
  }

  // We aren't saving or loading null sensors.
  cur_sensor->used = 1;

  zip_read_file(zp, buffer, actual_size, &actual_size);

  mfopen(buffer, actual_size, &mf);

  while(next_prop(&prop, &ident, &size, &mf))
  {
    switch(ident)
    {
      case SENPROP_EOF:
        mfseek(&mf, 0, SEEK_END);
        break;

      case SENPROP_SENSOR_NAME:
        size = MIN(size, ROBOT_NAME_SIZE - 1);
        mfread(cur_sensor->sensor_name, size, 1, &prop);
        cur_sensor->sensor_name[size] = '\0';
        break;

      case SENPROP_SENSOR_CHAR:
        cur_sensor->sensor_char = load_prop_int(&prop);
        break;

      case SENPROP_ROBOT_TO_MESG:
        size = MIN(size, ROBOT_NAME_SIZE - 1);
        mfread(cur_sensor->robot_to_mesg, size, 1, &prop);
        cur_sensor->robot_to_mesg[size] = '\0';
        break;

      default:
        break;
    }
  }

  // Zeros work fine as filler for incomplete sensors here

  free(buffer);
  return cur_sensor;
}

size_t save_robot_calculate_size(struct world *mzx_world,
 struct robot *cur_robot, int savegame, int file_version)
{
  int size = ROBOT_PROPS_SIZE;

  /* Saved name field size is version dependent, see save_prop_s_293. */
  size += (file_version >= V293) ? strlen(cur_robot->robot_name) : ROBOT_NAME_SIZE;

  if(savegame)
  {
    size += ROBOT_SAVE_PROPS_SIZE;
    size += 4 * cur_robot->stack_size;
  }

#ifdef CONFIG_DEBYTECODE

  size += cur_robot->program_source_length;

  if(savegame)
    size += PROP_HEADER_SIZE + cur_robot->num_labels;

#else // !CONFIG_DEBYTECODE

  size += cur_robot->program_bytecode_length;

#endif // !CONFIG_DEBYTECODE

  return size;
}

static void save_robot_to_memory(struct robot *cur_robot,
 struct memfile *mf, int savegame, int file_version)
{
  struct memfile prop;

  save_prop_s_293(RPROP_ROBOT_NAME, cur_robot->robot_name, ROBOT_NAME_SIZE, mf);
  save_prop_c(RPROP_ROBOT_CHAR, cur_robot->robot_char, mf);
  save_prop_w(RPROP_XPOS, cur_robot->xpos, mf);
  save_prop_w(RPROP_YPOS, cur_robot->ypos, mf);

#ifdef CONFIG_DEBYTECODE

  if(cur_robot->program_source)
  {
    int src_len = cur_robot->program_source_length;

    save_prop_v(RPROP_PROGRAM_SOURCE, src_len, &prop, mf);

    mfwrite(cur_robot->program_source, src_len, 1, &prop);
  }

#else // !CONFIG_DEBYTECODE

  if(cur_robot->program_bytecode)
  {
    int bc_len = cur_robot->program_bytecode_length;

    save_prop_v(RPROP_PROGRAM_BYTECODE, bc_len, &prop, mf);

    mfwrite(cur_robot->program_bytecode, bc_len, 1, &prop);
  }

#endif // !CONFIG_DEBYTECODE

  if(savegame)
  {
    int stack_size = cur_robot->stack_size;
    int program_line = cur_robot->cur_prog_line;
    int i;

#ifdef CONFIG_DEBYTECODE

    // The current bytecode offset isn't very useful when saved with
    // source code. Save the command number within the program instead.
    program_line = get_program_command_num(cur_robot, program_line);

    // Label zaps.
    // TODO only do this if anything has actually been zapped/restored from the
    // default so the robot doesn't have to be compiled when it's loaded.
    save_prop_v(RPROP_PROGRAM_LABEL_ZAPS, cur_robot->num_labels, &prop, mf);
    for(i = 0; i < cur_robot->num_labels; i++)
    {
      struct label *cur = cur_robot->label_list[i];
      prop.start[i] = cur->zapped;
    }

#endif // CONFIG_DEBYTECODE

    save_prop_d(RPROP_CUR_PROG_LINE, program_line, mf);
    save_prop_d(RPROP_POS_WITHIN_LINE, cur_robot->pos_within_line, mf);
    save_prop_c(RPROP_ROBOT_CYCLE, cur_robot->robot_cycle, mf);
    save_prop_c(RPROP_CYCLE_COUNT, cur_robot->cycle_count, mf);
    save_prop_c(RPROP_BULLET_TYPE, cur_robot->bullet_type, mf);
    save_prop_c(RPROP_IS_LOCKED, cur_robot->is_locked, mf);
    save_prop_c(RPROP_CAN_LAVAWALK, cur_robot->can_lavawalk, mf);
    save_prop_c(RPROP_WALK_DIR, cur_robot->walk_dir, mf);
    save_prop_c(RPROP_LAST_TOUCH_DIR, cur_robot->last_touch_dir, mf);
    save_prop_c(RPROP_LAST_SHOT_DIR, cur_robot->last_shot_dir, mf);
    save_prop_c(RPROP_STATUS, cur_robot->status, mf);

    save_prop_d(RPROP_LOOP_COUNT, cur_robot->loop_count, mf);
    save_prop_v(RPROP_LOCALS, 4*32, &prop, mf);

    for(i = 0; i < 32; i++)
      mfputd(cur_robot->local[i], &prop);

    save_prop_d(RPROP_STACK_POINTER, cur_robot->stack_pointer, mf);
    save_prop_v(RPROP_STACK, stack_size * 4, &prop, mf);

    for(i = 0; i < stack_size; i += 2)
    {
      program_line = cur_robot->stack[i];
#ifdef CONFIG_DEBYTECODE
      // The stack bytecode offsets have the same problem as the current
      // bytecode offset above, so convert these to line numbers too.
      program_line = get_program_command_num(cur_robot, program_line);
#endif
      mfputd(program_line, &prop);
      mfputd(cur_robot->stack[i + 1], &prop);
    }

    save_prop_c(RPROP_CAN_GOOPWALK, cur_robot->can_goopwalk, mf);
  }

  save_prop_eof(mf);
}

void save_robot(struct world *mzx_world, struct robot *cur_robot,
 struct zip_archive *zp, int savegame, int file_version, const char *name)
{
  struct memfile mf;
  void *buffer = NULL;
  size_t actual_size = 0;
  boolean is_stream = false;

  if(cur_robot->used)
  {
#ifdef CONFIG_DEBYTECODE
    // This will generally be done when calculating the size.
    // Since memory zips are expecting this to be done beforehand, do it
    // again anyway just in case.

    if(savegame)
      prepare_robot_bytecode(mzx_world, cur_robot);
#endif

    actual_size = save_robot_calculate_size(mzx_world, cur_robot, savegame,
     file_version);

    if(zp->is_memory)
    {
      // The regular way works with memory zips too, but this is faster.
      if(zip_write_open_mem_stream(zp, &mf, name, actual_size) == ZIP_SUCCESS)
        is_stream = true;
    }

    if(!is_stream)
    {
      buffer = cmalloc(actual_size);

      mfopen_wr(buffer, actual_size, &mf);
    }

    save_robot_to_memory(cur_robot, &mf, savegame, file_version);

    if(is_stream)
    {
      zip_write_close_mem_stream(zp, &mf);
    }
    else
    {
      zip_write_file(zp, name, buffer, actual_size, ZIP_M_NONE);

      free(buffer);
    }
  }
}

void save_scroll(struct scroll *cur_scroll, struct zip_archive *zp,
 int file_version, const char *name)
{
  void *buffer;
  struct memfile mf;
  size_t scroll_size;
  size_t actual_size;

  if(cur_scroll->used)
  {
    scroll_size = cur_scroll->mesg_size;
    actual_size = scroll_size + SCROLL_PROPS_SIZE;

    buffer = cmalloc(actual_size);

    mfopen_wr(buffer, actual_size, &mf);

    save_prop_w(SCRPROP_NUM_LINES, cur_scroll->num_lines, &mf);
    save_prop_a(SCRPROP_MESG, cur_scroll->mesg, scroll_size, 1, &mf);
    save_prop_eof(&mf);

    zip_write_file(zp, name, buffer, actual_size, ZIP_M_NONE);

    free(buffer);
  }
}

void save_sensor(struct sensor *cur_sensor, struct zip_archive *zp,
 int file_version, const char *name)
{
  char buffer[SENSOR_PROPS_SIZE];
  struct memfile mf;

  if(cur_sensor->used)
  {
    mfopen_wr(buffer, SENSOR_PROPS_SIZE, &mf);

    save_prop_s_293(SENPROP_SENSOR_NAME, cur_sensor->sensor_name, ROBOT_NAME_SIZE, &mf);
    save_prop_c(SENPROP_SENSOR_CHAR, cur_sensor->sensor_char, &mf);
    save_prop_s_293(SENPROP_ROBOT_TO_MESG, cur_sensor->robot_to_mesg, ROBOT_NAME_SIZE, &mf);
    save_prop_eof(&mf);

    zip_write_file(zp, name, buffer, mftell(&mf), ZIP_M_NONE);
  }
}


static int cmp_labels(const void *dest, const void *src)
{
  struct label *lsrc = *((struct label **)src);
  struct label *ldest = *((struct label **)dest);
  int cmp_primary = strcasecmp(ldest->name, lsrc->name);

  // A match needs to go on a secondary criteria.
  if(!cmp_primary)
  {
    if(ldest->position == 0)
      return 1;
    if(lsrc->position == 0)
      return -1;

    return ldest->position - lsrc->position;
  }
  else
  {
    return cmp_primary;
  }
}

// TODO: If bytecode isn't valid then this is done at a bad time. It should
// really be done when robots are assembled, rather than when they're loaded.
// So it's bundled with the function for that.

void cache_robot_labels(struct robot *cur_robot)
{
  int labels_allocated = 16;
  int labels_found = 0;
  int cmd;
  int next;
  int i;

  char *robot_program = cur_robot->program_bytecode;
  struct label **label_list;
  struct label *current_label;

  cur_robot->label_list = NULL;
  cur_robot->num_labels = 0;

  if(!robot_program)
    return;

  label_list = ccalloc(16, sizeof(struct label *));

  for(i = 1; i < (cur_robot->program_bytecode_length - 1); i++)
  {
    // Is it a label?
    cmd = robot_program[i + 1];
    next = i + robot_program[i] + 1;

    if((cmd == ROBOTIC_CMD_LABEL) || (cmd == ROBOTIC_CMD_ZAPPED_LABEL))
    {
      current_label = cmalloc(sizeof(struct label));
      current_label->name = robot_program + i + 3;

      current_label->cmd_position = i + 1;

      if(next >= (cur_robot->program_bytecode_length - 2))
        current_label->position = 0;
      else
      {
        //compatibility fix for 2.80 to 2.83
        if(cur_robot->world_version >= V280 && cur_robot->world_version <= V283)
          current_label->position = next + 1;
        else
          current_label->position = i;
      }

      if(cmd == ROBOTIC_CMD_ZAPPED_LABEL)
        current_label->zapped = true;
      else
        current_label->zapped = false;

      // Do we need more room?
      if(labels_found == labels_allocated)
      {
        labels_allocated *= 2;
        label_list = crealloc(label_list,
         sizeof(struct label *) * labels_allocated);
      }
      label_list[labels_found] = current_label;
      labels_found++;
    }

    // Go to next command
    i = next;
  }

  if(!labels_found)
  {
    free(label_list);
    return;
  }

  if(labels_found != labels_allocated)
  {
    label_list =
     crealloc(label_list, sizeof(struct label *) * labels_found);
  }

  // Now sort the list
  qsort(label_list, labels_found, sizeof(struct label *), cmp_labels);

  cur_robot->label_list = label_list;
  cur_robot->num_labels = labels_found;
  return;
}

void clear_label_cache(struct robot *cur_robot)
{
  int i;

  if(cur_robot->label_list)
  {
    for(i = 0; i < cur_robot->num_labels; i++)
    {
      free(cur_robot->label_list[i]);
    }

    free(cur_robot->label_list);
  }

  cur_robot->label_list = NULL;
  cur_robot->num_labels = 0;
}

void clear_robot_contents(struct robot *cur_robot)
{
  free(cur_robot->stack);
  cur_robot->stack = NULL;

#ifdef CONFIG_EDITOR
  free(cur_robot->command_map);
  cur_robot->command_map = NULL;
  cur_robot->command_map_length = 0;
#endif

  // If it was created by the game or loaded via a save file
  // then it won't have source code.
  free(cur_robot->program_source);
  cur_robot->program_source = NULL;
  cur_robot->program_source_length = 0;

  // It could be in the editor, or possibly it was never executed.
  if(cur_robot->program_bytecode)
  {
    clear_label_cache(cur_robot);
    free(cur_robot->program_bytecode);
    cur_robot->program_bytecode = NULL;
    cur_robot->program_bytecode_length = 0;
  }
}

void clear_robot(struct robot *cur_robot)
{
  clear_robot_contents(cur_robot);
  free(cur_robot);
}

void clear_scroll(struct scroll *cur_scroll)
{
  free(cur_scroll->mesg);
  free(cur_scroll);
}

// Does not remove entry from the normal list
static void remove_robot_name_entry(struct board *src_board,
 struct robot *cur_robot, char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  struct robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);

  // Find the one that matches the robot
  while(name_list[first] != cur_robot)
    first++;

  // Remove from name list
  active--;

  if(first != active)
  {
    memmove(name_list + first, name_list + first + 1,
     (active - first) * sizeof(struct robot *));
  }
  src_board->num_robots_active = active;
}

void clear_robot_id(struct board *src_board, int id)
{
  struct robot *cur_robot = src_board->robot_list[id];

  if(id)
  {
    remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
    clear_robot(cur_robot);
    src_board->robot_list[id] = NULL;
  }
  else
  {
    clear_robot_contents(cur_robot);
    cur_robot->used = 0;
  }
}

void clear_scroll_id(struct board *src_board, int id)
{
  clear_scroll(src_board->scroll_list[id]);
  src_board->scroll_list[id] = NULL;
}

void clear_sensor_id(struct board *src_board, int id)
{
  clear_sensor(src_board->sensor_list[id]);
  src_board->sensor_list[id] = NULL;
}

void clear_sensor(struct sensor *cur_sensor)
{
  free(cur_sensor);
}

#ifndef CONFIG_DEBYTECODE

void reallocate_robot(struct robot *robot, int size)
{
  robot->program_bytecode = crealloc(robot->program_bytecode, size);
  robot->program_bytecode_length = size;
}

#endif /* CONFIG_DEBYTECODE */

void reallocate_scroll(struct scroll *scroll, size_t size)
{
  scroll->mesg = crealloc(scroll->mesg, size);
  scroll->mesg_size = size;
}

/**
 * Due to MZX bugs in older versions the xpos/ypos for in-game features
 * may not be the robot's actual position on the board. This function
 * will return compatibility positions for the affected versions or
 * the robot's real board position otherwise.
 */
void get_robot_position(struct robot *cur_robot, int *xpos, int *ypos)
{
  if(cur_robot)
  {
    if(cur_robot->world_version >= V292)
    {
      *xpos = cur_robot->xpos;
      *ypos = cur_robot->ypos;
    }
    else
    {
      *xpos = cur_robot->compat_xpos;
      *ypos = cur_robot->compat_ypos;
    }
  }
}

int get_robot_id(struct board *src_board, const char *name)
{
  int first, last;

  if(find_robot(src_board, name, &first, &last))
  {
    struct robot *cur_robot = src_board->robot_list_name_sorted[first];
    // This is a cheap trick for now since robots don't have
    // a back-reference for IDs
    enum thing d_id;
    int offset;
    int thisx = 0;
    int thisy = 0;
    get_robot_position(cur_robot, &thisx, &thisy);

    offset = thisx + (thisy * src_board->board_width);
    d_id = (enum thing)src_board->level_id[offset];

    if(is_robot(d_id))
    {
      return src_board->level_param[offset];
    }
    else
    {
      int i;
      for(i = 1; i <= src_board->num_robots; i++)
      {
        if(!src_board->robot_list[i])
          continue;
        if(!strcasecmp(name, (src_board->robot_list[i])->robot_name))
          return i;
      }
    }
  }

  return -1;
}

static struct label *find_label(struct robot *cur_robot, const char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct label **base = cur_robot->label_list;
  struct label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];

    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Found a match, see if there's an earlier one
        while(middle)
        {
          current = base[middle - 1];
          if(!strcasecmp(current->name, name))
          {
            middle--;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a non-zapped one
        while(middle <= total)
        {
          if(current->zapped)
          {
            if(middle == total)
              return NULL;

            middle++;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

static int find_label_position(struct robot *cur_robot, const char *name)
{
  struct label *cur_label = find_label(cur_robot, name);

  if(cur_label)
  {
    return cur_label->position;
  }
  else
  {
    return -1;
  }
}

// This should return the last zapped label found, for restore label
// to work correctly.

static struct label *find_zapped_label(struct robot *cur_robot, char *name)
{
  int total = cur_robot->num_labels - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct label **base = cur_robot->label_list;
  struct label *current;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
       // Found a match, see if there's a later one
        while(middle < total)
        {
          current = base[middle + 1];
          if(!strcasecmp(current->name, name))
          {
            middle++;
          }
          else
          {
            break;
          }
        }

        current = base[middle];

        // Now find a zapped one
        while(middle >= 0)
        {
          if(!current->zapped)
          {
            if(!middle)
              return NULL;

            middle--;
            current = base[middle];
            if(strcasecmp(current->name, name))
            {
              return NULL;
            }
          }
          else
          {
            break;
          }
        }

        return current;
      }
    }
  }

  return NULL;
}

// Returns 1 if found, first is the first robot in the list,
// last is the last. If not found, first and last are the position to place
// into.

int find_robot(struct board *src_board, const char *name,
 int *first, int *last)
{
  int total = src_board->num_robots_active - 1;
  int bottom = 0, top = total, middle = 0;
  int cmpval = 0;
  struct robot **base = src_board->robot_list_name_sorted;
  struct robot *current;
  int f, l;

  while(bottom <= top)
  {
    middle = (top + bottom) / 2;
    current = base[middle];
    cmpval = strcasecmp(name, current->robot_name);

    if(cmpval > 0)
    {
      bottom = middle + 1;
    }
    else
    {
      if(cmpval < 0)
      {
        top = middle - 1;
      }
      else
      {
        // Find any prior occurances
        f = middle;
        l = middle;

        while((f > 0) && (!strcasecmp(name, (base[f - 1])->robot_name)))
        {
          f--;
        }

        *first = f;

        while((l < total) && (!strcasecmp(name, (base[l + 1])->robot_name)))
        {
          l++;
        }

        *last = l;
        return 1;
      }
    }
  }

  if(cmpval > 0)
  {
    *first = middle + 1;
    *last = middle + 1;
  }
  else
  {
    *first = middle;
    *last = middle;
  }

  return 0;
}

/* Built-in label only wrappers for send_robot_id and send_robot_all */
int send_robot_id_def(struct world *mzx_world, int robot_id, const char *mesg,
 int ignore_lock)
{
  char submesg[ROBOT_MAX_TR];
  int result = send_robot_id(mzx_world, robot_id, mesg, ignore_lock), subresult;

  //now, attempt to send the subroutine version of it
  if(mzx_world->version >= V284)
  {
    snprintf(submesg, ROBOT_MAX_TR, "#%s", mesg);
    submesg[ROBOT_MAX_TR - 1] = '\0';
    subresult = send_robot_id(mzx_world, robot_id, submesg, ignore_lock);
    if(result && !subresult)
      result = subresult;
  }
  return result;
}

void send_robot_all_def(struct world *mzx_world, const char *mesg)
{
  char submesg[ROBOT_MAX_TR];
  send_robot_all(mzx_world, mesg, 0);

  //now, attempt to send the subroutine version of it
  if(mzx_world->version >= V284)
  {
    snprintf(submesg, ROBOT_MAX_TR, "#%s", mesg);
    submesg[ROBOT_MAX_TR - 1] = '\0';
    send_robot_all(mzx_world, submesg, 0);
  }
}

void send_robot_def(struct world *mzx_world, int robot_id,
 enum builtin_label mesg_id)
{
  switch(mesg_id)
  {
    case LABEL_TOUCH:
      send_robot_id_def(mzx_world, robot_id, "TOUCH", 0);
      break;

    case LABEL_BOMBED:
      send_robot_id_def(mzx_world, robot_id, "BOMBED", 0);
      break;

    case LABEL_INVINCO:
      send_robot_all_def(mzx_world, "INVINCO");
      break;

    case LABEL_PUSHED:
      send_robot_id_def(mzx_world, robot_id, "PUSHED", 0);
      break;

    case LABEL_PLAYERSHOT:
      if(send_robot_id_def(mzx_world, robot_id, "PLAYERSHOT", 0))
      {
        send_robot_id_def(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case LABEL_NEUTRALSHOT:
      if(send_robot_id_def(mzx_world, robot_id, "NEUTRALSHOT", 0))
      {
        send_robot_id_def(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case LABEL_ENEMYSHOT:
      if(send_robot_id_def(mzx_world, robot_id, "ENEMYSHOT", 0))
      {
        send_robot_id_def(mzx_world, robot_id, "SHOT", 0);
      }
      break;

    case LABEL_PLAYERHIT:
      send_robot_all_def(mzx_world, "PLAYERHIT");
      break;

    case LABEL_LAZER:
      send_robot_id_def(mzx_world, robot_id, "LAZER", 0);
      break;

    case LABEL_SPITFIRE:
      send_robot_id_def(mzx_world, robot_id, "SPITFIRE", 0);
      break;

    case LABEL_JUSTLOADED: //no subroutine version
      send_robot_all(mzx_world, "JUSTLOADED", 0);
      break;

    case LABEL_JUSTENTERED: //no subroutine version
      send_robot_all(mzx_world, "JUSTENTERED", 0);
      break;

    case LABEL_GOOPTOUCHED:
      send_robot_all_def(mzx_world, "GOOPTOUCHED");
      break;

    case LABEL_PLAYERHURT:
      send_robot_all_def(mzx_world, "PLAYERHURT");
      break;

    case LABEL_PLAYERDIED:
      send_robot_all_def(mzx_world, "PLAYERDIED");
      break;
  }
}

static void send_sensor_command(struct world *mzx_world, int id, int command)
{
  struct board *src_board = mzx_world->current_board;
  struct sensor *cur_sensor = src_board->sensor_list[id];
  int x = -1, y = -1, under = -1;
  char *level_under_id = src_board->level_under_id;
  char *level_under_param = src_board->level_under_param;
  char *level_under_color = src_board->level_under_color;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  char *level_color = src_board->level_color;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int player_x = mzx_world->player_x;
  int player_y = mzx_world->player_y;
  int player_offset = player_x + (player_y * board_width);
  int offset;
  int move_status;

  // Find sensor
  if(!(command & 0x100))
  {
    // Don't bother for a char cmd
    under = 0;
    if((level_under_id[player_offset] == 122) &&
     (level_under_param[player_offset] == id))
    {
      under = 1;
      x = player_x;
      y = player_y;
    }
    else
    {
      int found = 0;
      for(y = 0, offset = 0; y < board_height; y++)
      {
        for(x = 0; x < board_width; x++, offset++)
        {
          if((level_id[offset] == 122) &&
           (level_param[offset] == id))
          {
            found = 1;
            break;
          }
        }
        if(found)
          break;
      }
    }
  }

  // Okay, this means the sensor is unlinked. Due to some
  // errors in world 1 format files or ver1to2 or something
  // this can happen (see Caverns). It's very unpleasant.

  if(y == board_height)
    return;

  // Cmd
  switch(command)
  {
    case 0:
    case 1:
    case 2:
    case 3:
    {
      move_status = 0;
      if(under)
      {
        // Attempt to move player, then if ok,
        // put sensor underneath and delete from
        // original space.
        move_status = move(mzx_world, x, y, command, CAN_PUSH |
         CAN_TRANSPORT | CAN_LAVAWALK | CAN_FIREWALK | CAN_WATERWALK);

        if(move_status == NO_HIT)
        {
          // Find player...
          find_player(mzx_world);
          player_x = mzx_world->player_x;
          player_y = mzx_world->player_y;
          player_offset = player_x + (player_y * board_width);

          move_status = HIT_PLAYER;
        }
        else
        {
          send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
        }
      }
      else
      {
        // Attempt to move sensor.
        move_status = move(mzx_world, x, y, command,
         CAN_PUSH | CAN_TRANSPORT | CAN_LAVAWALK | CAN_FIREWALK |
         CAN_WATERWALK | REACT_PLAYER);

        if(move_status == HIT_PLAYER)
        {
          step_sensor(mzx_world, id);
        }
      }

      if(move_status == HIT_PLAYER)
      {
        // Met player- so put under player!
        mzx_world->under_player_id = level_under_id[player_offset];
        mzx_world->under_player_param = level_under_param[player_offset];
        mzx_world->under_player_color = level_under_color[player_offset];
        level_under_id[player_offset] = 122;
        level_under_param[player_offset] = id;
        level_under_color[player_offset] = level_color[x + (y * board_width)];
        id_remove_top(mzx_world, x, y);
      }
      else

      if(move_status != NO_HIT)
      {
        // Sensorthud!
        send_robot(mzx_world, cur_sensor->robot_to_mesg, "SENSORTHUD", 0);
      }
      break;
    }

    case 4:
    {
      if(under)
      {
        id_remove_under(mzx_world, x, y);
        clear_sensor_id(src_board, id);
      }
      else
      {
        id_remove_top(mzx_world, x, y);
        clear_sensor_id(src_board, id);
      }
      break;
    }

    default:
    {
      if(command & 0x100)
      {
        cur_sensor->sensor_char = command - 256;
      }
      else
      {
        if(under)
        {
          level_under_color[player_offset] = command - 512;
        }
        else
        {
          src_board->level_color[x + (y * board_width)] =
           command - 512;
        }
      }
      break;
    }
  }
}

static void send_sensors(struct world *mzx_world, char *name, const char *mesg)
{
  struct board *src_board = mzx_world->current_board;

  if(src_board->num_sensors)
  {
    // Sensors
    // Set command- 0-3 move, 4 die, 256 | # char, 512 | # color (hex)
    int command = -1; // No command yet

    // Check movement commands
    if(mesg[1] == 0)
    {
      char first_letter = mesg[0];
      if((first_letter >= 'a') && (first_letter <= 'z'))
        first_letter -= 32;

      switch(first_letter)
      {
        case 'N':
          command = 0;
          break;

        case 'S':
          command = 1;
          break;

        case 'E':
          command = 2;
          break;

        case 'W':
          command = 3;
          break;
      }
    }

    // Die?
    if(!strcasecmp("DIE", mesg))
      command = 4;

    // Char___? (___ can be ### or 'c')
    if(!strncasecmp("CHAR", mesg, 4))
    {
     command = 256;
      if(mesg[4] == '\'')
        command = 0x100 | mesg[5];
      else
        command = 0x100 | (strtol(mesg + 4, NULL, 10) & 0xFF);
    }

    // Color__? (__ is hex)
    if(!strncasecmp("COLOR", mesg, 5))
    {
      command = 512 | (strtol(mesg + 5, NULL, 16) & 0xFF);
    }

    if(command != -1)
    {
      struct sensor **sensor_list = src_board->sensor_list;
      struct sensor *current_sensor;
      int i;

      if(!strcasecmp(name, "ALL"))
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];

          if(current_sensor)
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
      else
      {
        for(i = 1; i <= src_board->num_sensors; i++)
        {
          current_sensor = sensor_list[i];
          if((current_sensor != NULL) &&
           !strcasecmp(name, current_sensor->sensor_name))
          {
            send_sensor_command(mzx_world, i, command);
          }
        }
      }
    }
  }
}

static void robot_stack_push(struct robot *cur_robot, int position,
 int position_in_line)
{
  int stack_pointer = cur_robot->stack_pointer;
  int stack_size = cur_robot->stack_size;
  int *stack = cur_robot->stack;

  if((stack_pointer + 1) >= stack_size)
  {
    // Initialize or double the stack. Don't let it get too large though!
    if(stack_size == 0)
      stack_size = 2;
    else
      stack_size *= 2;

    if(stack_size > ROBOT_MAX_STACK)
      return;

    cur_robot->stack = crealloc(stack, stack_size * sizeof(int));
    stack = cur_robot->stack;
    cur_robot->stack_size = stack_size;
  }

  stack[stack_pointer++] = position;
  stack[stack_pointer++] = position_in_line;
  cur_robot->stack_pointer = stack_pointer;
}

static int robot_stack_pop(struct robot *cur_robot, int *position_in_line)
{
  int stack_pointer = cur_robot->stack_pointer;
  int *stack = cur_robot->stack;

  if(stack_pointer)
  {
    stack_pointer--;
    *position_in_line = stack[stack_pointer];

    stack_pointer--;
    cur_robot->stack_pointer = stack_pointer;
    return stack[stack_pointer];
  }
  else
  {
    *position_in_line = 0;
    return -1;
  }
}

static void set_robot_position(struct world *mzx_world,
 struct robot *cur_robot, int position, int position_in_line)
{
  cur_robot->cur_prog_line = position;
  cur_robot->pos_within_line = position_in_line;
  cur_robot->cycle_count = cur_robot->robot_cycle - 1;

  // Popping a subroutine's retval from the stack may legitimately
  // try to position another robot's code beyond the end of its
  // program. Ensure that if this happens, the robot's program
  // will terminate in the next cycle. We need -2 here to ignore
  // the initial 0xff and terminal 0x00 signature bytes.

  if(cur_robot->cur_prog_line > cur_robot->program_bytecode_length - 2)
    cur_robot->cur_prog_line = 0;

  // MegaZeux 1.x always sets its equivalent flag here.
  if(cur_robot->status == 1 || mzx_world->version < V200)
    cur_robot->status = 2;
}

static int send_robot_direct(struct world *mzx_world, struct robot *cur_robot,
 const char *mesg, int ignore_lock, int send_self)
{
  char *robot_program;
  int new_position;

#ifdef CONFIG_DEBYTECODE
  prepare_robot_bytecode(mzx_world, cur_robot);
#endif
  robot_program = cur_robot->program_bytecode;

  if((cur_robot->is_locked) && (!ignore_lock))
    return 1; // Locked

  if(cur_robot->program_bytecode_length < 3)
    return 2; // No program!

  // Are we going to a subroutine? Returning?
  if(mesg[0] == '#')
  {
    // returning?
    if(!strcasecmp(mesg + 1, "return"))
    {
      if(cur_robot->stack_pointer)
      {
        int return_pos_in_line;
        int return_pos = robot_stack_pop(cur_robot, &return_pos_in_line);
        set_robot_position(mzx_world, cur_robot, return_pos, return_pos_in_line);
      }
      else
      {
        // Contextually invalid return; treat like an invalid label
        return 2;
      }
    }
    else

    // returning to the TOP?
    if(!strcasecmp(mesg + 1, "top"))
    {
      if(cur_robot->stack_pointer)
      {
        set_robot_position(mzx_world, cur_robot, cur_robot->stack[0],
         cur_robot->stack[1]);

        cur_robot->stack_pointer = 0;
      }
      else
      {
        // Contextually invalid top; treat like an invalid label
        return 2;
      }
    }
    else
    {
      new_position = find_label_position(cur_robot, mesg);

      if(new_position != -1)
      {
        // Push the current address onto the stack
        // If a maximum overflow happened, this simply won't work.
        int robot_position = cur_robot->cur_prog_line;
        int return_position_in_line = 0;
        int return_position;

        if(robot_position)
        {
          return_position = robot_position;

          // 2.90+: we want the pos_within_line too.
          if(cur_robot->world_version >= V290)
            return_position_in_line = cur_robot->pos_within_line;

          // In DOS versions, ALL subroutine sends returned to the next command.
          // Self sends in any version should also return to the next command.
          if(send_self || cur_robot->world_version < VERSION_PORT)
            return_position += robot_program[robot_position] + 2;
        }
        else
        {
          return_position = 0;
        }

        robot_stack_push(cur_robot, return_position,
         return_position_in_line);

        // Do the jump
        set_robot_position(mzx_world, cur_robot, new_position, 0);
        return 0;
      }

      return 2;
    }
  }
  else
  {
    new_position = find_label_position(cur_robot, mesg);

    if(new_position != -1)
    {
      set_robot_position(mzx_world, cur_robot, new_position, 0);
      return 0;
    }

    return 2;
  }

  return 0;
}

void send_robot(struct world *mzx_world, char *name, const char *mesg,
 int ignore_lock)
{
  struct board *src_board = mzx_world->current_board;
  int first, last;

  if(!strcasecmp(name, "all"))
  {
    send_robot_all(mzx_world, mesg, ignore_lock);
  }
  else
  {
    // See if it's the global robot
    if(!strcasecmp(name, mzx_world->global_robot.robot_name) &&
     mzx_world->global_robot.used)
    {
      send_robot_direct(mzx_world, &mzx_world->global_robot, mesg,
       ignore_lock, 0);
    }

    if(find_robot(src_board, name, &first, &last))
    {
      while(first <= last)
      {
        send_robot_direct(mzx_world, src_board->robot_list_name_sorted[first],
         mesg, ignore_lock, 0);
        first++;
      }
    }
  }

  send_sensors(mzx_world, name, mesg);
}

int send_robot_id(struct world *mzx_world, int id, const char *mesg,
 int ignore_lock)
{
  struct robot *cur_robot = mzx_world->current_board->robot_list[id];
  return send_robot_direct(mzx_world, cur_robot, mesg, ignore_lock, 0);
}

int send_robot_self(struct world *mzx_world, struct robot *src_robot,
 const char *mesg, int ignore_lock)
{
  return send_robot_direct(mzx_world, src_robot, mesg, ignore_lock, 1);
}

void send_robot_all(struct world *mzx_world, const char *mesg, int ignore_lock)
{
  struct board *src_board = mzx_world->current_board;
  int i;

  if(mzx_world->global_robot.used)
  {
    send_robot_direct(mzx_world, &mzx_world->global_robot,
     mesg, ignore_lock, 0);
  }

  for(i = 0; i < src_board->num_robots_active; i++)
  {
    send_robot_direct(mzx_world, src_board->robot_list_name_sorted[i],
     mesg, ignore_lock, 0);
  }
}

// Run a set of x/y pairs through the prefixes
void prefix_first_last_xy(struct world *mzx_world, int *fx, int *fy,
 int *lx, int *ly, int robotx, int roboty)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tfx = *fx;
  int tfy = *fy;
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->first_prefix)
  {
    case REL_TO_SELF:
    case REL_TO_SELF_FIRST_OR_LAST:
    {
      tfx += robotx;
      tfy += roboty;
      break;
    }

    case REL_TO_PLAYER:
    case REL_TO_PLAYER_FIRST_OR_LAST:
    {
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS_FIRST_OR_LAST:
    {
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  switch(mzx_world->last_prefix)
  {
    case REL_TO_SELF:
    case REL_TO_SELF_FIRST_OR_LAST:
    {
      tlx += robotx;
      tly += roboty;
      break;
    }

    case REL_TO_PLAYER:
    case REL_TO_PLAYER_FIRST_OR_LAST:
    {
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS_FIRST_OR_LAST:
    {
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tfx < 0)
    tfx = 0;

  if(tfy < 0)
    tfy = 0;

  if(tlx < 0)
    tlx = 0;

  if(tly < 0)
    tly = 0;

  if(tfx >= board_width)
    tfx = board_width - 1;

  if(tfy >= board_height)
    tfy = board_height - 1;

  if(tlx >= board_width)
    tlx = board_width - 1;

  if(tly >= board_height)
    tly = board_height - 1;

  *fx = tfx;
  *fy = tfy;
  *lx = tlx;
  *ly = tly;
}

// These are primarily for copy {overlay} block. Allows a variable
// width/height so that coordinates can be fixed against more than
// simply the board (for instance vlayer). In the future this could
// also be used to copy inbetween boards, perhaps.

void prefix_first_xy_var(struct world *mzx_world, int *fx, int *fy,
 int robotx, int roboty, int width, int height)
{
  int tfx = *fx;
  int tfy = *fy;

  switch(mzx_world->first_prefix)
  {
    case REL_TO_SELF:
    case REL_TO_SELF_FIRST_OR_LAST:
    {
      tfx += robotx;
      tfy += roboty;
      break;
    }

    case REL_TO_PLAYER:
    case REL_TO_PLAYER_FIRST_OR_LAST:
    {
      find_player(mzx_world);
      tfx += mzx_world->player_x;
      tfy += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS_FIRST_OR_LAST:
    {
      tfx += get_counter(mzx_world, "FIRSTXPOS", 0);
      tfy += get_counter(mzx_world, "FIRSTYPOS", 0);
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tfx += get_counter(mzx_world, "XPOS", 0);
      tfy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tfx < 0)
    tfx = 0;

  if(tfy < 0)
    tfy = 0;

  if(tfx >= width)
    tfx = width - 1;

  if(tfy >= height)
    tfy = height - 1;

  *fx = tfx;
  *fy = tfy;
}

void prefix_last_xy_var(struct world *mzx_world, int *lx, int *ly,
 int robotx, int roboty, int width, int height)
{
  int tlx = *lx;
  int tly = *ly;

  switch(mzx_world->last_prefix)
  {
    case REL_TO_SELF:
    case REL_TO_SELF_FIRST_OR_LAST:
    {
      tlx += robotx;
      tly += roboty;
      break;
    }

    case REL_TO_PLAYER:
    case REL_TO_PLAYER_FIRST_OR_LAST:
    {
      find_player(mzx_world);
      tlx += mzx_world->player_x;
      tly += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS_FIRST_OR_LAST:
    {
      tlx += get_counter(mzx_world, "LASTXPOS", 0);
      tly += get_counter(mzx_world, "LASTYPOS", 0);
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tlx += get_counter(mzx_world, "XPOS", 0);
      tly += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tlx < 0)
    tlx = 0;

  if(tly < 0)
    tly = 0;

  if(tlx >= width)
    tlx = width - 1;

  if(tly >= height)
    tly = height - 1;

  *lx = tlx;
  *ly = tly;
}

void prefix_mid_xy_var(struct world *mzx_world, int *mx, int *my,
 int robotx, int roboty, int width, int height)
{
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->mid_prefix)
  {
    case REL_TO_SELF:
    {
      tmx += robotx;
      tmy += roboty;
      break;
    }

    case REL_TO_PLAYER:
    {
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  if(tmx < 0)
    tmx = 0;

  if(tmy < 0)
    tmy = 0;

  if(tmx >= width)
    tmx = width - 1;

  if(tmy >= height)
    tmy = height - 1;

  *mx = tmx;
  *my = tmy;
}

// Unbounded version.
void prefix_mid_xy_unbound(struct world *mzx_world, int *mx, int *my, int x, int y)
{
  int tmx = *mx;
  int tmy = *my;

  switch(mzx_world->mid_prefix)
  {
    case REL_TO_SELF:
    {
      tmx += x;
      tmy += y;
      break;
    }

    case REL_TO_PLAYER:
    {
      find_player(mzx_world);
      tmx += mzx_world->player_x;
      tmy += mzx_world->player_y;
      break;
    }

    case REL_TO_XPOS_YPOS:
    {
      tmx += get_counter(mzx_world, "XPOS", 0);
      tmy += get_counter(mzx_world, "YPOS", 0);
      break;
    }
  }

  *mx = tmx;
  *my = tmy;
}

// Just does the middle prefixes, since those are all that's usually
// needed...
void prefix_mid_xy(struct world *mzx_world, int *mx, int *my, int x, int y)
{
  struct board *src_board = mzx_world->current_board;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  prefix_mid_xy_unbound(mzx_world, mx, my, x, y);

  if(*mx < 0)
    *mx = 0;
  if(*my < 0)
    *my = 0;
  if(*mx >= board_width)
    *mx = board_width - 1;
  if(*my >= board_height)
    *my = board_height - 1;
}

/**
 * Apply middle prefixes with respect to a different board than the current.
 */
void prefix_mid_xy_ext(struct world *mzx_world, struct board *dest_board,
 int *mx, int *my, int x, int y)
{
  struct board *cur_board = mzx_world->current_board;

  if(!mzx_world->mid_prefix)
    return;

  if(cur_board != dest_board)
  {
    mzx_world->current_board = dest_board;
    retrieve_board_from_extram(dest_board);
  }

  prefix_mid_xy(mzx_world, mx, my, x, y);

  if(cur_board != dest_board)
  {
    store_board_to_extram(dest_board);
    mzx_world->current_board = cur_board;
    find_player(mzx_world);
  }
}

// Move an x/y pair in a given direction. Returns non-0 if edge reached.
int move_dir(struct board *src_board, int *x, int *y, enum dir dir)
{
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int tx = *x;
  int ty = *y;

  switch(dir)
  {
    case NORTH:
    {
      if(ty == 0)
      {
        return 1;
      }
      ty--;
      break;
    }

    case SOUTH:
    {
      if(ty == (board_height - 1))
      {
        return 1;
      }
      ty++;
      break;
    }

    case EAST:
    {
      if(tx == (board_width - 1))
      {
        return 1;
      }
      tx++;
      break;
    }

    case WEST:
    {
      if(tx == 0)
      {
        return 1;
      }
      tx--;
      break;
    }

    default:
      break;
  }

  *x = tx;
  *y = ty;

  return 0;
}

// Internal only. NOTE- IF WE EVER ALLOW ZAPPING OF LABELS NOT IN CURRENT
// ROBOT, USE A COPY OF THE *LABEL BEFORE THE PREPARE_ROBOT_MEM!

// TODO: What we'll want to do is use the label cache instead, so that
// we're not actually modifying the program.

// Both of these can only be done from an actively executing program,
// so they will have valid bytecode.

int restore_label(struct robot *cur_robot, char *label)
{
  struct label *dest_label = find_zapped_label(cur_robot, label);

  if(dest_label)
  {
    cur_robot->program_bytecode[dest_label->cmd_position] = ROBOTIC_CMD_LABEL;
    dest_label->zapped = false;
    return 1;
  }

  return 0;
}

int zap_label(struct robot *cur_robot, char *label)
{
  struct label *dest_label = find_label(cur_robot, label);

  if(dest_label)
  {
    cur_robot->program_bytecode[dest_label->cmd_position] = ROBOTIC_CMD_ZAPPED_LABEL;
    dest_label->zapped = true;
    return 1;
  }

  return 0;
}

/**
 * Return true if this command can be displayed/skipped in the robot box.
 * FIXME this does NOT allow zapped labels; when zapping stops modifying the
 * bytecode, special handling needs to be added for labels. This means access
 * to the robot's label cache HERE.
 */
boolean is_robot_box_command(int cmd)
{
  return
   (cmd == ROBOTIC_CMD_BLANK_LINE) ||
   (cmd == ROBOTIC_CMD_MESSAGE_BOX_LINE) ||
   (cmd == ROBOTIC_CMD_MESSAGE_BOX_OPTION) ||
   (cmd == ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION) ||
   (cmd == ROBOTIC_CMD_LABEL) ||
   (cmd == ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE) ||
   (cmd == ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE) ||
   // While executing, the robot box temporarily replaces inaccessible options
   // with this dummy command.
   (cmd == ROBOTIC_CMD_UNUSED_249);
}

/**
 * Return true if this command should be skipped over when scrolling instead
 * of displaying as a blank line.
 *
 * FIXME this does NOT allow zapped labels; when zapping stops modifying the
 * bytecode, special handling needs to be added for labels. This means access
 * to the robot's label cache HERE.
 */
static boolean is_robot_box_skip_command(int cmd)
{
  return (cmd == ROBOTIC_CMD_LABEL) || (cmd == ROBOTIC_CMD_UNUSED_249);
}

static int robot_box_down(char *program, int pos, int count)
{
  int i, cur_cmd = -1;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go forward a line (if possible)
      pos += program[pos] + 2;
      if(program[pos] == 0)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        cur_cmd = program[pos + 1];
        if(!is_robot_box_command(cur_cmd))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(is_robot_box_skip_command(cur_cmd) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

static int robot_box_up(char *program, int pos, int count)
{
  int i, cur_cmd = -1;
  int old_pos;
  int done = 0;

  for(i = 0; (i < count) && (!done); i++)
  {
    old_pos = pos;
    do
    {
      // Go backwards a line (if possible)
      if(program[pos - 1] == 0xFF)
      {
        pos = old_pos;
        done = 1;
      }
      else
      {
        pos -= program[pos - 1] + 2;
        cur_cmd = program[pos + 1];
        if(!is_robot_box_command(cur_cmd))
        {
          pos = old_pos;
          done = 1;
        }
      }
    } while(is_robot_box_skip_command(cur_cmd) && (!done));

    if(i == 100000)
      i = 99999;
  }

  return pos;
}

static void clip_color_string(char *buf, size_t len, size_t pos)
{
  size_t idx = color_string_index_of(buf, len, pos, '\0');
  buf[idx] = '\0';
  return;
}

static void display_robot_line(struct world *mzx_world, char *program,
 int y, int id)
{
  char ibuff[ROBOT_MAX_TR];
  char *next;
  int scroll_base_color = mzx_world->scroll_base_color;
  int scroll_arrow_color = mzx_world->scroll_arrow_color;
  mzx_world->command_cache = 0;

  switch(program[1])
  {
    case ROBOTIC_CMD_MESSAGE_BOX_LINE: // Normal message
    {
      // On the off-chance something actually relies on this bug...
      int flags = 0;
      if((mzx_world->version >= VERSION_PORT) && (mzx_world->version < V291))
        flags |= WR_TAB;

      tr_msg(mzx_world, program + 3, id, ibuff);
      ibuff[64] = 0; // Clip
      write_string_ext(ibuff, 8, y, scroll_base_color, flags, 0, 0);
      break;
    }

    case ROBOTIC_CMD_MESSAGE_BOX_OPTION: // Option
    {
      // Skip over label...
      // next is pos of string
      next = next_param_pos(program + 2);
      tr_msg(mzx_world, next + 1, id, ibuff);
      clip_color_string(ibuff, ROBOT_MAX_TR, 62); // Clip
      color_string_ext(ibuff, 10, y, scroll_base_color, false, 0, 0);
      draw_char_ext('\x10', scroll_arrow_color, 8, y, 0, 0);
      break;
    }

    case ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION: // Counter-based option
    {
      // Check counter
      int val = parse_param(mzx_world, program + 2, id);
      if(val)
      {
        // Skip over counter and label...
        // next is pos of string
        next = next_param_pos(program + 2);
        next = next_param_pos(next);
        tr_msg(mzx_world, next + 1, id, ibuff);
        clip_color_string(ibuff, ROBOT_MAX_TR, 62); // Clip
        color_string_ext(ibuff, 10, y, scroll_base_color, false, 0, 0);
        draw_char_ext('\x10', scroll_arrow_color, 8, y, 0, 0);
      }
      break;
    }

    case ROBOTIC_CMD_MESSAGE_BOX_COLOR_LINE: // Colored message
    {
      tr_msg(mzx_world, program + 3, id, ibuff);
      clip_color_string(ibuff, ROBOT_MAX_TR, 64); // Clip
      color_string_ext(ibuff, 8, y, scroll_base_color, false, 0, 0);
      break;
    }

    case ROBOTIC_CMD_MESSAGE_BOX_CENTER_LINE: // Centered message
    {
      int length, x_position;
      tr_msg(mzx_world, program + 3, id, ibuff);
      clip_color_string(ibuff, ROBOT_MAX_TR, 64); // Clip
      length = color_string_length(ibuff, ROBOT_MAX_TR);
      x_position = 40 - (length / 2);
      assert(x_position >= 0);
      color_string_ext(ibuff, x_position, y, scroll_base_color, false, 0, 0);
      break;
    }
  }

  // Others, like ROBOTIC_CMD_BLANK_LINE and ROBOTIC_CMD_LABEL, are blank lines
}

static void robot_frame(struct world *mzx_world, char *program, int id)
{
  // Displays one frame of a robot. The scroll edging, arrows, and title
  // must already be shown. Simply prints each line. The pointer points
  // to the center line.
  int scroll_base_color = mzx_world->scroll_base_color;
  int i, pos = 0;
  int old_pos;

  select_layer(GAME_UI_LAYER);

  // Display center line
  fill_line_ext(64, 8, 12, 32, scroll_base_color, 0, 0);
  display_robot_line(mzx_world, program, 12, id);

  // Display lines above center line
  for(i = 11; i >= 6; i--)
  {
    fill_line_ext(64, 8, i, 32, scroll_base_color, 0, 0);
    // Go backward to previous line
    old_pos = pos;
    pos = robot_box_up(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }

  // Display lines below center line
  pos = 0;

  for(i = 13; i <= 18; i++)
  {
    fill_line_ext(64, 8, i, 32, scroll_base_color, 0, 0);
    old_pos = pos;
    pos = robot_box_down(program, pos, 1);
    if(old_pos != pos)
      display_robot_line(mzx_world, program + pos, i, id);
  }

  select_layer(UI_LAYER);
}

void robot_box_display(struct world *mzx_world, char *program,
 char *label_storage, int id)
{
  struct board *src_board = mzx_world->current_board;
  struct robot *cur_robot = src_board->robot_list[id];
  int pos = 0, key, mouse_press;
  int joystick_key;

  label_storage[0] = 0;

  // Draw screen
  save_screen();
  m_show();

  dialog_fadein();

  scroll_edging_ext(mzx_world, 4, false);

  // Write robot name
  select_layer(GAME_UI_LAYER);
  if(!cur_robot->robot_name[0])
  {
    write_string_ext("Interaction", 35, 4,
     mzx_world->scroll_title_color, false, 0, 0);
  }
  else
  {
    write_string_ext(cur_robot->robot_name,
     40 - (unsigned int)strlen(cur_robot->robot_name) / 2, 4,
     mzx_world->scroll_title_color, false, 0, 0);
  }
  select_layer(UI_LAYER);

  // Scan section and mark all invalid counter-controlled options as codes
  // ROBOTIC_CMD_UNUSED_249.

  do
  {
    if(program[pos + 1] == ROBOTIC_CMD_UNUSED_249)
      program[pos + 1] = ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION;

    if(program[pos + 1] == ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = ROBOTIC_CMD_UNUSED_249;
    }

    pos += program[pos] + 2;
  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == ROBOTIC_CMD_UNUSED_249)
      program[pos + 1] = ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION;

    if(program[pos + 1] == ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION)
    {
      if(!parse_param(mzx_world, program + (pos + 2), id))
        program[pos + 1] = ROBOTIC_CMD_UNUSED_249;
    }

    if(program[pos - 1] == 0xFF)
      break;

    pos -= program[pos - 1] + 2;
  } while(1);

  pos = 0;

  // If we're starting on an unavailable option, try to seek down
  if(program[pos + 1] == ROBOTIC_CMD_UNUSED_249)
    pos = robot_box_down(program, pos, 1);

  // Loop
  do
  {
    // Display scroll
    robot_frame(mzx_world, program + pos, id);
    cursor_hint(8, 12);
    update_screen();

    update_event_status_delay();
    key = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      key = joystick_key;

    // Exit event--mimic Escape
    if(get_exit_status())
      key = IKEY_ESCAPE;

    mouse_press = get_mouse_press_ext();

    if(mouse_press && (mouse_press <= MOUSE_BUTTON_RIGHT))
    {
      int mouse_x, mouse_y;
      get_mouse_position(&mouse_x, &mouse_y);

      if((mouse_y >= 6) && (mouse_y <= 18) &&
       (mouse_x >= 8) && (mouse_x <= 71))
      {
        int count = mouse_y - 12;
        if(!count)
        {
          key = IKEY_RETURN;
        }
        else
        {
          if(count > 0)
            pos = robot_box_down(program, pos, count);
          else
            pos = robot_box_up(program, pos, -count);

          warp_mouse(mouse_x, mouse_y - count);
        }
      }
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELUP)
    {
      pos = robot_box_up(program, pos, 3);
    }
    else

    if(mouse_press == MOUSE_BUTTON_WHEELDOWN)
    {
      pos = robot_box_down(program, pos, 3);
    }

    switch(key)
    {
      case IKEY_UP:
        pos = robot_box_up(program, pos, 1);
        break;

      case IKEY_DOWN:
        pos = robot_box_down(program, pos, 1);
        break;

      case IKEY_RETURN:
      {
        key = IKEY_ESCAPE;

        if((program[pos + 1] == ROBOTIC_CMD_MESSAGE_BOX_OPTION) ||
         (program[pos + 1] == ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION))
        {
          char *next;
          if(program[pos + 1] == ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION)
            next = next_param_pos(program + pos + 2) + 1;
          else
            next = program + pos + 3;

          // Goto option! Stores in label_storage
          strcpy(label_storage, next);
        }
        break;
      }

      case IKEY_PAGEDOWN:
        pos = robot_box_down(program, pos, 6);
        break;

      case IKEY_PAGEUP:
        pos = robot_box_up(program, pos, 6);
        break;

      case IKEY_HOME:
        pos = robot_box_up(program, pos, 100000);
        break;

      case IKEY_END:
        pos = robot_box_down(program, pos, 100000);
        break;

      default:
      case IKEY_ESCAPE:
      case 0:
        break;
    }
  } while(key != IKEY_ESCAPE);

  // Scan section and mark all invalid counter-controlled options as codes
  // ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION.

  pos = 0;

  do
  {
    if(program[pos + 1] == ROBOTIC_CMD_UNUSED_249)
    {
      program[pos + 1] = ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION;
    }

    pos += program[pos] + 2;

  } while(program[pos]);

  pos = 0;

  // Backwards
  do
  {
    if(program[pos + 1] == ROBOTIC_CMD_UNUSED_249)
    {
      program[pos + 1] = ROBOTIC_CMD_MESSAGE_BOX_MAYBE_OPTION;
    }
    if(program[pos - 1] == 0xFF)
      break;
    pos -= program[pos - 1] + 2;

  } while(1);

  // Due to a faulty check, 2.83 through 2.91f always stay faded in here.
  if((mzx_world->version < V283) || (mzx_world->version >= V291))
    dialog_fadeout();

  // Restore screen and exit
  m_hide();
  cursor_off();
  restore_screen();
  update_event_status();
}

void push_sensor(struct world *mzx_world, int id)
{
  struct board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORPUSHED", 0);
}

void step_sensor(struct world *mzx_world, int id)
{
  struct board *src_board = mzx_world->current_board;
  send_robot(mzx_world, (src_board->sensor_list[id])->robot_to_mesg,
   "SENSORON", 0);
}

// Don't do this if the entry is not in the normal list
void add_robot_name_entry(struct board *src_board, struct robot *cur_robot,
 char *name)
{
  // Find the position
  int first, last;
  int active = src_board->num_robots_active;
  struct robot **name_list = src_board->robot_list_name_sorted;

  find_robot(src_board, name, &first, &last);
  // Insert into name list, if it's not at the end
  if(first != active)
  {
    memmove(name_list + first + 1, name_list + first,
     (active - first) * sizeof(struct robot *));
  }
  name_list[first] = cur_robot;
  src_board->num_robots_active = active + 1;
}

// This could probably be done in a more efficient manner.
void change_robot_name(struct board *src_board, struct robot *cur_robot,
 char *new_name)
{
  // Remove the old one
  remove_robot_name_entry(src_board, cur_robot, cur_robot->robot_name);
  // Add the new one
  add_robot_name_entry(src_board, cur_robot, new_name);
  // And change the actual name
  strcpy(cur_robot->robot_name, new_name);
}

// Works with the ID-list. Will make room for a new one if there aren't any.
int find_free_robot(struct board *src_board)
{
  int num_robots = src_board->num_robots;
  int i;
  struct robot **robot_list = src_board->robot_list;

  for(i = 1; i <= num_robots; i++)
  {
    if(robot_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_robots)
    {

      int num_robots_allocated = src_board->num_robots_allocated;
      if(num_robots == num_robots_allocated)
      {
        if(num_robots_allocated)
          num_robots_allocated *= 2;
        else
          num_robots_allocated = 1;

        src_board->robot_list = crealloc(robot_list,
         (num_robots_allocated + 1) * sizeof(struct robot *));

        src_board->robot_list_name_sorted =
         crealloc(src_board->robot_list_name_sorted,
         (num_robots_allocated) * sizeof(struct robot *));
        src_board->num_robots_allocated = num_robots_allocated;
      }
      src_board->num_robots = num_robots + 1;
    }
    return i;
  }
  return -1;
}

// Like find_free_robot, but for scrolls. Will also expand the list if
// necessary.
static int find_free_scroll(struct board *src_board)
{
  int num_scrolls = src_board->num_scrolls;
  int i;
  struct scroll **scroll_list = src_board->scroll_list;

  for(i = 1; i <= num_scrolls; i++)
  {
    if(scroll_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_scrolls)
    {
      int num_scrolls_allocated = src_board->num_scrolls_allocated;
      if(num_scrolls == num_scrolls_allocated)
      {
        if(num_scrolls_allocated)
          num_scrolls_allocated *= 2;
        else
          num_scrolls_allocated = 1;

        src_board->scroll_list = crealloc(scroll_list,
         (num_scrolls_allocated + 1) * sizeof(struct scroll *));
        src_board->num_scrolls_allocated = num_scrolls_allocated;
      }
      src_board->num_scrolls = num_scrolls + 1;
    }
    return i;
  }

  return -1;
}

// Like find_free_robot, but for sensors. Will also expand the list if
// necessary.
static int find_free_sensor(struct board *src_board)
{
  int num_sensors = src_board->num_sensors;
  int i;
  struct sensor **sensor_list = src_board->sensor_list;

  for(i = 1; i <= num_sensors; i++)
  {
    if(sensor_list[i] == NULL)
      break;
  }

  if(i < 256)
  {
    // Perhaps make a new one
    if(i > num_sensors)
    {
      int num_sensors_allocated = src_board->num_sensors_allocated;
      if(num_sensors == num_sensors_allocated)
      {
        if(num_sensors_allocated)
          num_sensors_allocated *= 2;
        else
          num_sensors_allocated = 1;

        src_board->sensor_list = crealloc(sensor_list,
         (num_sensors_allocated + 1) * sizeof(struct sensor *));
        src_board->num_sensors_allocated = num_sensors_allocated;
      }
      src_board->num_sensors = num_sensors + 1;
    }
    return i;
  }

  return -1;
}

// Duplicates the contents of one robot to a new one, returning the ID
// of the duplicate. Supply the x/y position of where you want it to be
// placed. Returns the ID of location. Does NOT place the robot on the
// board (so be sure to do that). The given id is the slot to add it in;
// be sure that this is a valid (NULL) entry!

// This function must not be called in the editor because it won't copy
// source code. It's only for runtime (use duplicate_robot_direct_source
// there instead)

void duplicate_robot_direct(struct world *mzx_world, struct robot *cur_robot,
 struct robot *copy_robot, int x, int y, int preserve_state)
{
  char *dest_program_location, *src_program_location;
  struct label *src_label, *dest_label;
  int i, program_length, num_labels;
  ptrdiff_t program_offset;

#ifdef CONFIG_DEBYTECODE
  prepare_robot_bytecode(mzx_world, cur_robot);
#endif
  program_length = cur_robot->program_bytecode_length;
  num_labels = cur_robot->num_labels;

  // Copy all the contents
  memcpy(copy_robot, cur_robot, sizeof(struct robot));
  // We need unique copies of the program and the label cache.
  copy_robot->program_bytecode = cmalloc(program_length);

  src_program_location = cur_robot->program_bytecode;
  dest_program_location = copy_robot->program_bytecode;

  memcpy(dest_program_location, src_program_location, program_length);

  if(num_labels)
    copy_robot->label_list = ccalloc(num_labels, sizeof(struct label *));
  else
    copy_robot->label_list = NULL;

  program_offset = dest_program_location - src_program_location;

  // Copy each individual label pointer over
  for(i = 0; i < num_labels; i++)
  {
    copy_robot->label_list[i] = cmalloc(sizeof(struct label));

    src_label = cur_robot->label_list[i];
    dest_label = copy_robot->label_list[i];

    memcpy(dest_label, src_label, sizeof(struct label));
    // The name pointer actually has to be readjusted to match the new program
    dest_label->name += program_offset;
  }

  copy_robot->program_source = NULL;
  copy_robot->program_source_length = 0;

#ifdef CONFIG_DEBYTECODE
  if(cur_robot->program_source)
  {
    // Copy the source too.
    copy_robot->program_source = cmalloc(cur_robot->program_source_length);
    memcpy(copy_robot->program_source, cur_robot->program_source,
     cur_robot->program_source_length);
    copy_robot->program_source_length = cur_robot->program_source_length;
  }
#endif

#ifdef CONFIG_EDITOR
  copy_robot->command_map = NULL;
  copy_robot->command_map_length = 0;

#ifdef CONFIG_DEBYTECODE
  if(mzx_world->editing && cur_robot->command_map)
  {
    // Duplicate the command map too if this is testing.
    int cmd_map_size =
     cur_robot->command_map_length * sizeof(struct command_mapping);

    // And the command map
    copy_robot->command_map = cmalloc(cmd_map_size);
    memcpy(copy_robot->command_map, cur_robot->command_map, cmd_map_size);
    copy_robot->command_map_length = cur_robot->command_map_length;
  }
#endif
#endif

  if(preserve_state && mzx_world->version < VERSION_PORT)
  {
    // In DOS versions, copy and copy block preserved the current robot state
    // Therefore leave all robot vars alone and copy the stack over
    size_t stack_capacity = copy_robot->stack_size * sizeof(int);
    if(stack_capacity)
    {
      copy_robot->stack = cmalloc(stack_capacity);
      memcpy(copy_robot->stack, cur_robot->stack, stack_capacity);
    }
    else
    {
      copy_robot->stack = NULL;
      copy_robot->stack_pointer = 0;
    }
  }
  else
  {
    // Give the robot an empty stack.
    copy_robot->stack = NULL;
    copy_robot->stack_size = 0;
    copy_robot->stack_pointer = 0;

    if(cur_robot->cur_prog_line)
      copy_robot->cur_prog_line = 1;

    copy_robot->pos_within_line = 0;
    copy_robot->status = 0;
  }
  copy_robot->xpos = x;
  copy_robot->ypos = y;
  copy_robot->compat_xpos = x;
  copy_robot->compat_ypos = y;
}

// Finds a robot ID then duplicates a robot there. Must not be called
// in the editor (use duplicate_robot_source instead).

int duplicate_robot(struct world *mzx_world,
 struct board *src_board, struct robot *cur_robot,
 int x, int y, int preserve_state)
{
  int dest_id = find_free_robot(src_board);
  if(dest_id != -1)
  {
    struct robot *copy_robot = cmalloc(sizeof(struct robot));
    duplicate_robot_direct(mzx_world, cur_robot, copy_robot, x, y,
      preserve_state);
    add_robot_name_entry(src_board, copy_robot, copy_robot->robot_name);
    src_board->robot_list[dest_id] = copy_robot;
  }

  return dest_id;
}

// Makes the dest robot a replication of the source. ID's are given
// so that it can modify the ID table. The dest position is assumed
// to already contain something, and is thus cleared first.
// Will not allow replacing the global robot.
void replace_robot(struct world *mzx_world, struct board *src_board,
 struct robot *src_robot, int dest_id)
{
  char old_name[64];
  int x = (src_board->robot_list[dest_id])->xpos;
  int y = (src_board->robot_list[dest_id])->ypos;
  struct robot *cur_robot = src_board->robot_list[dest_id];

  strcpy(old_name, cur_robot->robot_name);

  clear_robot_contents(cur_robot);
  duplicate_robot_direct(mzx_world, src_robot, cur_robot, x, y, 0);
  strcpy(cur_robot->robot_name, old_name);

  if(dest_id)
    change_robot_name(src_board, cur_robot, src_robot->robot_name);
}

// Like duplicate_robot_direct, but for scrolls.
void duplicate_scroll_direct(struct scroll *cur_scroll,
 struct scroll *copy_scroll)
{
  size_t mesg_size = cur_scroll->mesg_size;

  // Copy all the contents
  memcpy(copy_scroll, cur_scroll, sizeof(struct scroll));
  // We need unique copies of the program and the label cache.
  copy_scroll->mesg = cmalloc(mesg_size);
  memcpy(copy_scroll->mesg, cur_scroll->mesg, mesg_size);
}

// Like duplicate_robot_direct, but for sensors.
void duplicate_sensor_direct(struct sensor *cur_sensor,
 struct sensor *copy_sensor)
{
  // Copy all the contents
  memcpy(copy_sensor, cur_sensor, sizeof(struct sensor));
}

int duplicate_scroll(struct board *src_board, struct scroll *cur_scroll)
{
  int dest_id = find_free_scroll(src_board);
  if(dest_id != -1)
  {
    struct scroll *copy_scroll = cmalloc(sizeof(struct scroll));
    duplicate_scroll_direct(cur_scroll, copy_scroll);
    src_board->scroll_list[dest_id] = copy_scroll;
  }

  return dest_id;
}

int duplicate_sensor(struct board *src_board, struct sensor *cur_sensor)
{
  int dest_id = find_free_sensor(src_board);
  if(dest_id != -1)
  {
    struct sensor *copy_sensor = cmalloc(sizeof(struct sensor));
    duplicate_sensor_direct(cur_sensor, copy_sensor);
    src_board->sensor_list[dest_id] = copy_sensor;
  }

  return dest_id;
}

// This function will remove any null entries in the object lists
// (for robots, scrolls, and sensors), and adjust all of the board
// params to compensate. This should always be used before saving
// the world/game, and ideally when loading too.

void optimize_null_objects(struct board *src_board)
{
  int num_robots = src_board->num_robots;
  int num_scrolls = src_board->num_scrolls;
  int num_sensors = src_board->num_sensors;
  struct robot **robot_list = src_board->robot_list;
  struct scroll **scroll_list = src_board->scroll_list;
  struct sensor **sensor_list = src_board->sensor_list;
  struct robot **optimized_robot_list =
   ccalloc(num_robots + 1, sizeof(struct robot *));
  struct scroll **optimized_scroll_list =
   ccalloc(num_scrolls + 1, sizeof(struct scroll *));
  struct sensor **optimized_sensor_list =
   ccalloc(num_sensors + 1, sizeof(struct sensor *));
  int *robot_id_translation_list = ccalloc(num_robots + 1, sizeof(int));
  int *scroll_id_translation_list = ccalloc(num_scrolls + 1, sizeof(int));
  int *sensor_id_translation_list = ccalloc(num_sensors + 1, sizeof(int));
  struct robot *cur_robot;
  struct scroll *cur_scroll;
  struct sensor *cur_sensor;
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;
  int i, i2;
  int x, y, offset;
  char *level_id = src_board->level_id;
  char *level_param = src_board->level_param;
  enum thing d_id;
  int d_param, d_new_param;
  int do_modify = 0;

  for(i = 1, i2 = 1; i <= num_robots; i++)
  {
    cur_robot = robot_list[i];
    if(cur_robot != NULL)
    {
      optimized_robot_list[i2] = cur_robot;
      robot_id_translation_list[i] = i2;
      i2++;
    }
    else
    {
     // FIXME: Hack, probably bogus!
     robot_id_translation_list[i] = 0;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_robot_list[0] = robot_list[0];
    free(robot_list);
    src_board->robot_list =
     crealloc(optimized_robot_list, sizeof(struct robot *) * i2);
    src_board->num_robots = i2 - 1;
    src_board->num_robots_allocated = i2 - 1;
  }
  else
  {
    free(optimized_robot_list);
  }

  for(i = 1, i2 = 1; i <= num_scrolls; i++)
  {
    cur_scroll = scroll_list[i];
    if(cur_scroll != NULL)
    {
      optimized_scroll_list[i2] = cur_scroll;
      scroll_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_scroll_list[0] = scroll_list[0];
    free(scroll_list);
    src_board->scroll_list =
     crealloc(optimized_scroll_list, sizeof(struct scroll *) * i2);
    src_board->num_scrolls = i2 - 1;
    src_board->num_scrolls_allocated = i2 - 1;
  }
  else
  {
    free(optimized_scroll_list);
  }

  for(i = 1, i2 = 1; i <= num_sensors; i++)
  {
    cur_sensor = sensor_list[i];
    if(cur_sensor != NULL)
    {
      // If there's a gap, fill it up
      optimized_sensor_list[i2] = cur_sensor;
      sensor_id_translation_list[i] = i2;
      i2++;
    }
  }

  if(i2 != i)
  {
    do_modify |= 1;
    optimized_sensor_list[0] = sensor_list[0];
    free(sensor_list);
    src_board->sensor_list =
     crealloc(optimized_sensor_list, sizeof(struct sensor *) * i2);
    src_board->num_sensors = i2 - 1;
    src_board->num_sensors_allocated = i2 - 1;
  }
  else
  {
    free(optimized_sensor_list);
  }

  // Make sure this is up to date
  robot_list = src_board->robot_list;

  if(do_modify)
  {
    // Now, physically modify all references on the board
    for(y = 0, offset = 0; y < board_height; y++)
    {
      for(x = 0; x < board_width; x++, offset++)
      {
        d_id = (enum thing)level_id[offset];
        // Is it a robot?
        if(is_robot(d_id))
        {
          d_param = level_param[offset];
          d_new_param = robot_id_translation_list[d_param];
          level_param[offset] = d_new_param;
          // Also, as a service, set the x/y coordinates, just in case
          // they haven't been initialized (this is a potential pitfall)
          cur_robot = robot_list[d_new_param];
          cur_robot->xpos = x;
          cur_robot->ypos = y;
          cur_robot->compat_xpos = x;
          cur_robot->compat_ypos = y;
        }
        else

        // Is it a scoll?
        if(is_signscroll(d_id))
        {
          d_param = level_param[offset];
          d_new_param = scroll_id_translation_list[d_param];
          level_param[offset] = d_new_param;
        }
        else

        // Is it a sensor?
        if(d_id == SENSOR)
        {
          d_param = level_param[offset];
          d_new_param = sensor_id_translation_list[d_param];
          level_param[offset] = d_new_param;
        }
      }
    }
  }

  // Free the lists
  free(robot_id_translation_list);
  free(scroll_id_translation_list);
  free(sensor_id_translation_list);
}

#ifndef CONFIG_DEBYTECODE
/* Fix nonsense robot stack values, 2.x edition.
 * This should eventually be merged into translate_robot_bytecode_offsets
 * for 3.x.
 */
void fix_robot_stack_offsets(struct robot *cur_robot)
{
  const char *bc;
  int program_length;
  int *stack_start;
  int *stack_pos;
  int *stack;
  int i = 1;

  if(!cur_robot->stack || !cur_robot->stack_pointer)
    return;

  bc = cur_robot->program_bytecode;
  program_length = cur_robot->program_bytecode_length;
  stack = cur_robot->stack;
  stack_start = stack + cur_robot->stack_pointer - 2;

  for(stack_pos = stack_start; stack_pos >= stack; stack_pos -= 2)
  {
    if(*stack_pos < 0 || *stack_pos >= program_length)
    {
      debug("Robot has out-of-bounds stack frame %td: %d (len: %d)\n",
       (stack_pos - stack) >> 1, *stack_pos, program_length);
      *stack_pos = 0;
    }
  }

  while(i < program_length)
  {
    int cmd_pos = i;
    int cmd_next = i + (unsigned char)bc[i] + 2;

    for(stack_pos = stack_start; stack_pos >= stack; stack_pos -= 2)
    {
      if(*stack_pos > cmd_pos && *stack_pos < cmd_next)
      {
        debug("Robot has invalid stack frame %td: %d @ offset %d (len: %d)\n",
         (stack_pos - stack) >> 1, *stack_pos, cmd_pos, program_length);
        *stack_pos = 0;
      }
    }

    i = cmd_next;
  }
}
#endif

#ifdef CONFIG_DEBYTECODE

void prepare_robot_bytecode(struct world *mzx_world, struct robot *cur_robot)
{
  if(cur_robot->program_bytecode == NULL)
  {
#ifdef CONFIG_EDITOR
    if(cur_robot->command_map)
    {
      free(cur_robot->command_map);
      cur_robot->command_map = NULL;
      cur_robot->command_map_length = 0;
    }

    if(mzx_world->editing)
    {
      // Assemble and map the program.
      assemble_program(cur_robot->program_source,
       &cur_robot->program_bytecode, &cur_robot->program_bytecode_length,
       &cur_robot->command_map, &cur_robot->command_map_length);
    }
    else
#endif
    {
      // Assemble the program.
      assemble_program(cur_robot->program_source,
       &cur_robot->program_bytecode, &cur_robot->program_bytecode_length,
       NULL, NULL);
    }

    // This was moved here from load robot - only build up the labels once the
    // robot's actually used. But eventually this should be combined with
    // assemble_program.
    cache_robot_labels(cur_robot);
  }
}

static int command_num_to_program_pos(struct robot *cur_robot, int command_num)
{
  char *bc = cur_robot->program_bytecode;
  int program_length = cur_robot->program_bytecode_length;
  int i = 1;

#ifdef CONFIG_EDITOR
  struct command_mapping *cmd_map = cur_robot->command_map;

  if(cmd_map && command_num < cur_robot->command_map_length)
    return cmd_map[command_num].bc_pos;
#endif

  if(!bc || program_length == 0)
    return 0;

  bc++;
  while(*bc)
  {
    if(i == command_num)
      return (int)(bc - cur_robot->program_bytecode);

    bc += *bc + 2;
    i++;
  }
  return 0;
}

static int get_legacy_bytecode_command_num(char *legacy_bc, int pos_in_bc)
{
  char *end = legacy_bc + pos_in_bc;
  int i = 1;

  if(!legacy_bc || pos_in_bc == 0)
    return 0;

  legacy_bc++;
  while(1)
  {
    // Only exact positions are valid.
    // Offsets in the middle of the command previously executed arbitrary
    // Robotic and could easily run past the end of the program. In newer
    // versions, the robot is simply stopped if this is ever found.
    if(legacy_bc == end)
      return i;

    if(legacy_bc > end)
    {
      debug("invalid legacy bytecode position %d ignored\n", pos_in_bc);
      return 0;
    }

    if(*legacy_bc == 0)
      break;

    legacy_bc += *legacy_bc + 2;
    i++;
  }
  debug("out-of-bounds legacy bytecode position %d ignored\n", pos_in_bc);
  return 0;
}

/**
 * The cur_prog_line value loaded for this robot is pretty much guaranteed to
 * not be a true bytecode position but either a command number (3.00+) or a
 * legacy bytecode position (<3.00). This is also true for all robot positions
 * stored on the robot stack. If present these need to be converted to real
 * bytecode positions in the compiled source so everything works properly.
 *
 * Note that if cur_prog_line is 0 (robot ended) or 1 (always the start of the
 * first command) it does not need to be translated, and if the robot stack
 * doesn't exist or the robot is currently at the bottom of it then the stack
 * doesn't need to be translated. Checking for these cases is necessary to
 * avoid compiling every robot in the entire world/save on load.
 */
void translate_robot_bytecode_offsets(struct world *mzx_world,
 struct robot *cur_robot, int file_version)
{
  if(cur_robot->cur_prog_line > 1 ||
   (cur_robot->stack && cur_robot->stack_pointer))
  {
    int cmd_num = 0;

    prepare_robot_bytecode(mzx_world, cur_robot);

    if(file_version < VERSION_SOURCE)
    {
      // FIXME the following relies upon the new bytecode having the same number
      // of commands as the old bytecode, which SHOULD be true, but it is not.
      // Uncomment this when rasm gets the ability to compile comments and
      // blank lines into NOPs.
      cur_robot->cur_prog_line = 1;
      cur_robot->pos_within_line = 0;
      /*
      if(program_legacy_bytecode)
        cmd_num = get_legacy_bytecode_command_num(program_legacy_bytecode,
          cur_robot->cur_prog_line);
      */
    }
    else
      cmd_num = cur_robot->cur_prog_line;

    if(cmd_num > 1)
      cur_robot->cur_prog_line = command_num_to_program_pos(cur_robot, cmd_num);

    // Also do the stack.
    if(cur_robot->stack && cur_robot->stack_pointer)
    {
      int i;
      for(i = 0; i < cur_robot->stack_pointer; i += 2)
      {
        int *stack_pos = cur_robot->stack + i;

        if(file_version < VERSION_SOURCE)
        {
          // FIXME see above for why this doesn't work right now...
          cur_robot->stack_pointer = 0;
          break;
          /*
          cmd_num = get_legacy_bytecode_command_num(program_legacy_bytecode,
           *stack_pos);
          */
        }
        else
          cmd_num = *stack_pos;

        *stack_pos = command_num_to_program_pos(cur_robot, cmd_num);
      }
    }
  }
}

#endif /* CONFIG_DEBYTECODE */

#if defined(CONFIG_EDITOR) || defined(CONFIG_DEBYTECODE)

int get_program_command_num(struct robot *cur_robot, int program_pos)
{
  int a = 0;

#ifdef CONFIG_EDITOR
  struct command_mapping *cmd_map = cur_robot->command_map;
  int b = cur_robot->command_map_length - 1;
  int i;

  int d;

  // If mapping information is available, do a binary search.
  if(cmd_map && program_pos > 0)
  {
    while(b-a > 1)
    {
      i = (b - a)/2 + a;

      d = cmd_map[i].bc_pos - program_pos;

      if(d >= 0) b = i;
      if(d <= 0) a = i;
    }

    if(program_pos >= cmd_map[b].bc_pos)
      a = b;

    return a;
  }
  else
#endif

  if(program_pos == 0)
    return 0;

  // Otherwise, step through the program line by line.
  if(cur_robot->program_bytecode)
  {
    char *bc = cur_robot->program_bytecode;
    char *end = bc + program_pos;
    a = 1;

    bc++;
    while(*bc)
    {
      if(bc >= end)
        return a;

      bc += *bc + 2;
      a++;
    }
  }
  return 0;
}

#endif /* defined(CONFIG_EDITOR) || defined(CONFIG_DEBYTECODE) */

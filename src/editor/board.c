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

#include "board.h"

#include "../board.h"
#include "../error.h"
#include "../extmem.h"
#include "../legacy_board.h"
#include "../robot.h"
#include "../world.h"
#include "../world_format.h"
#include "../io/vio.h"
#include "../io/zip.h"

#include "world.h"
#include "configure.h"

#include <string.h>


static int board_magic(const char magic_string[4])
{
  if(magic_string[0] == 0xFF)
  {
    if(magic_string[1] == 'M')
    {
      // MZX versions >= 2.00 && <= 2.51S1
      if(magic_string[2] == 'B' && magic_string[3] == '2')
        return V251;

      // MZX versions >= 2.51S2 && <= 9.xx
      if((magic_string[2] > 1) && (magic_string[2] < 10))
        return ((int)magic_string[2] << 8) + (int)magic_string[3];
    }
  }

  // Not a recognized board magic
  return 0;
}

static boolean legacy_check_v1_rle(vfile *vf, int board_width, int board_height)
{
  int size = board_width * board_height;
  int w;
  int h;
  int i;

  w = vfgetc(vf);
  h = vfgetc(vf);
  if(w == 0) // VER1TO2 hack
  {
    w = vfgetc(vf);
    h = vfgetc(vf);
  }
  if(w != board_width || h != board_height)
    return false;

  for(i = 0; i < size;)
  {
    int runsize = vfgetc(vf);
    int current_char = vfgetc(vf);
    if(current_char < 0 || runsize > size - i)
      return false;

    i += runsize;
  }
  return true;
}

static boolean legacy_check_v1_board(vfile *vf)
{
  int w;
  int h;
  int i;

  vrewind(vf);

  w = vfgetc(vf);
  h = vfgetc(vf);
  if(w < 1 || w > 100 || h < 1 || h > 100)
    return false;

  vrewind(vf);
  for(i = 0; i < 6; i++)
    if(!legacy_check_v1_rle(vf, w, h))
      return false;

  // There is at least usable plane data in this file, so allow it to load.
  vrewind(vf);
  return true;
}

void save_board_file(struct world *mzx_world, struct board *cur_board,
 const char *name)
{
  vfile *vf = vfopen_unsafe(name, "wb");
  struct zip_archive *zp;
  boolean success = false;

  if(vf)
  {
    vfputc(0xFF, vf);

    vfputc('M', vf);
    vfputc((MZX_VERSION >> 8) & 0xFF, vf);
    vfputc(MZX_VERSION & 0xFF, vf);

    zp = zip_open_vf_write(vf);
    if(zp)
    {
      if(!save_board(mzx_world, cur_board, zp, 0, MZX_VERSION, 0))
        success = true;

      zip_close(zp, NULL);
    }
    else
      vfclose(vf);
  }

  if(!success)
    error_message(E_WORLD_IO_SAVING, 0, NULL);
}

static struct board *legacy_load_board_allocate_direct(struct world *mzx_world,
 vfile *vf, int version)
{
  struct board *cur_board = cmalloc(sizeof(struct board));
  int board_start, board_end;

  board_start = vftell(vf);
  board_end = vfilelength(vf, false);

  legacy_load_board_direct(mzx_world, cur_board, vf, (board_end - board_start), 0,
   version);

  if(!vfread(cur_board->board_name, 25, 1, vf))
    cur_board->board_name[0] = 0;

  return cur_board;
}

static int find_first_board(struct zip_archive *zp)
{
  unsigned int file_id;

  world_assign_file_ids(zp, false);

  // The first file after sorting should be the board info for ID 0.
  // If it isn't, this isn't a board file.

  if(ZIP_SUCCESS == zip_get_next_mzx_file_id(zp, &file_id, NULL, NULL))
  {
    if(file_id == FILE_ID_BOARD_INFO)
    {
      // Loading board ID 0:
      return 0;
    }
  }

  return -1;
}

void replace_current_board(struct world *mzx_world, const char *name)
{
  int current_board_id = mzx_world->current_board_id;
  struct board *src_board = mzx_world->current_board;

  vfile *vf = vfopen_unsafe(name, "rb");
  struct zip_archive *zp;

  char version_string[4];
  int file_version;
  int success = 0;

  if(vf)
  {
    if(!vfread(version_string, 4, 1, vf))
    {
      error_message(E_IO_READ, 0, NULL);
      vfclose(vf);
      return;
    }

    file_version = board_magic(version_string);

    // 1.x doesn't have a magic string and needs a custom check.
    if(file_version == 0 && legacy_check_v1_board(vf))
      file_version = V100;

    if(file_version > 0 && file_version <= MZX_LEGACY_FORMAT_VERSION)
    {
      // Legacy board.
      clear_board(src_board);

      src_board =
       legacy_load_board_allocate_direct(mzx_world, vf, file_version);

      success = 1;
      vfclose(vf);
    }
    else

    if(file_version <= MZX_VERSION)
    {
      int board_id;

      // Regular board or maybe not a board at all.
      zp = zip_open_vf_read(vf);

      // Make sure it's an actual zip.
      if(zp)
      {
        // Make sure the zip contains a board.
        board_id = find_first_board(zp);

        if(board_id >= 0)
        {
          clear_board(src_board);

          src_board =
           load_board_allocate(mzx_world, zp, 0, file_version, board_id);

          success = 1;
        }
      }

      if(!success)
        error_message(E_BOARD_FILE_INVALID, 0, NULL);

      zip_close(zp, NULL);
    }

    else
    {
      error_message(E_BOARD_FILE_FUTURE_VERSION, file_version, NULL);
      vfclose(vf);
    }

    if(success)
    {
      // Set up the newly allocated board.
      optimize_null_objects(src_board);

      if(src_board->robot_list)
        src_board->robot_list[0] = &mzx_world->global_robot;

      set_update_done_current(mzx_world);

      mzx_world->current_board = NULL;
      set_current_board(mzx_world, src_board);
      mzx_world->board_list[current_board_id] = src_board;
    }
  }
}

struct board *create_blank_board(const struct editor_config_info *conf)
{
  struct board *cur_board = ccalloc(1, sizeof(struct board));
  int layer_size = conf->board_width * conf->board_height;
  int i;

  cur_board->board_width =       conf->board_width;
  cur_board->board_height =      conf->board_height;
  cur_board->overlay_mode =      conf->overlay_enabled;
  cur_board->level_id =          cmalloc(layer_size);
  cur_board->level_param =       cmalloc(layer_size);
  cur_board->level_color =       cmalloc(layer_size);
  cur_board->level_under_id =    cmalloc(layer_size);
  cur_board->level_under_param = cmalloc(layer_size);
  cur_board->level_under_color = cmalloc(layer_size);
  if(cur_board->overlay_mode)
  {
    cur_board->overlay =         cmalloc(layer_size);
    cur_board->overlay_color =   cmalloc(layer_size);
  }
  cur_board->mod_playing[0] = 0;

  cur_board->viewport_x =        conf->viewport_x;
  cur_board->viewport_y =        conf->viewport_y;
  cur_board->viewport_width =    conf->viewport_w;
  cur_board->viewport_height =   conf->viewport_h;
  cur_board->can_shoot =         conf->can_shoot;
  cur_board->can_bomb =          conf->can_bomb;
  cur_board->fire_burn_brown =   conf->fire_burns_brown;
  cur_board->fire_burn_space =   conf->fire_burns_spaces;
  cur_board->fire_burn_fakes =   conf->fire_burns_fakes;
  cur_board->fire_burn_trees =   conf->fire_burns_trees;
  cur_board->explosions_leave =  conf->explosions_leave;
  cur_board->save_mode =         conf->saving_enabled;
  cur_board->forest_becomes =    conf->forest_to_floor;
  cur_board->collect_bombs =     conf->collect_bombs;
  cur_board->fire_burns =        conf->fire_burns_forever;
  cur_board->dragons_can_randomly_move  = conf->dragons_can_randomly_move;

  for(i = 0; i < 4; i++)
  {
    cur_board->board_dir[i] = NO_BOARD;
  }

  cur_board->reset_on_entry =    conf->reset_on_entry;
  cur_board->reset_on_entry_same_board  = conf->reset_on_entry_same_board;
  cur_board->restart_if_zapped = conf->restart_if_hurt;
  cur_board->time_limit =        conf->time_limit;
  cur_board->last_key = '?';
  cur_board->num_input = 0;
  cur_board->input_size = 0;
  cur_board->input_allocated = 0;
  cur_board->input_string = NULL;
  cur_board->player_last_dir = 0x10;
  cur_board->bottom_mesg[0] = 0;
  cur_board->b_mesg_timer = 0;
  cur_board->lazwall_start = 7;
  cur_board->b_mesg_row = 24;
  cur_board->b_mesg_col = -1;
  cur_board->scroll_x = 0;
  cur_board->scroll_y = 0;
  cur_board->locked_x = -1;
  cur_board->locked_y = -1;
  cur_board->player_ns_locked =     conf->player_locked_ns;
  cur_board->player_ew_locked =     conf->player_locked_ew;
  cur_board->player_attack_locked = conf->player_locked_att;
  cur_board->volume = 255;
  cur_board->volume_inc = 0;
  cur_board->volume_target = 255;

  board_set_charset_path(cur_board, conf->charset_path, strlen(conf->charset_path));
  board_set_palette_path(cur_board, conf->palette_path, strlen(conf->palette_path));

  cur_board->num_robots = 0;
  cur_board->num_robots_active = 0;
  cur_board->num_robots_allocated = 0;
  cur_board->robot_list = cmalloc(sizeof(struct robot *));
  cur_board->robot_list_name_sorted = NULL;
  cur_board->num_scrolls = 0;
  cur_board->num_scrolls_allocated = 0;
  cur_board->scroll_list = cmalloc(sizeof(struct scroll *));
  cur_board->num_sensors = 0;
  cur_board->num_sensors_allocated = 0;
  cur_board->sensor_list = cmalloc(sizeof(struct sensor *));

  memset(cur_board->level_id, 0, layer_size);
  memset(cur_board->level_color, 7, layer_size);
  memset(cur_board->level_param, 0, layer_size);
  memset(cur_board->level_under_id, 0, layer_size);
  memset(cur_board->level_under_color, 7, layer_size);
  memset(cur_board->level_under_param, 0, layer_size);
  if(cur_board->overlay_mode)
  {
    memset(cur_board->overlay, 32, layer_size);
    memset(cur_board->overlay_color, 7, layer_size);
  }

  cur_board->level_id[0] = 127;

#ifdef DEBUG
  cur_board->is_extram = false;
#endif

  return cur_board;
}

struct board *create_buffer_board(int width, int height)
{
  // Create a dummy board to be used as a buffer for undo block actions
  struct board *src_board = ccalloc(1, sizeof(struct board));
  int layer_size = width * height;

  src_board->board_width = width;
  src_board->board_height = height;

  src_board->robot_list = ccalloc(1, sizeof(struct robot *));
  src_board->scroll_list = ccalloc(1, sizeof(struct scroll *));
  src_board->sensor_list = ccalloc(1, sizeof(struct sensor *));

  src_board->level_id = ccalloc(1, layer_size);
  src_board->level_color = ccalloc(1, layer_size);
  src_board->level_param = ccalloc(1, layer_size);
  src_board->level_under_id = ccalloc(1, layer_size);
  src_board->level_under_color = ccalloc(1, layer_size);
  src_board->level_under_param = ccalloc(1, layer_size);

  return src_board;
}

void change_board_size(struct board *src_board, int new_width, int new_height)
{
  int board_width = src_board->board_width;
  int board_height = src_board->board_height;

  if(new_width == 0)
    new_width = 1;

  if(new_height == 0)
    new_height = 1;

  if((board_width != new_width) || (board_height != new_height))
  {
    char *level_id = src_board->level_id;
    char *level_color = src_board->level_color;
    char *level_param = src_board->level_param;
    char *level_under_id = src_board->level_under_id;
    char *level_under_color = src_board->level_under_color;
    char *level_under_param = src_board->level_under_param;
    char *overlay = src_board->overlay;
    char *overlay_color = src_board->overlay_color;
    int overlay_mode = src_board->overlay_mode;
    int board_size = board_width * board_height;
    int new_size = new_width * new_height;
    int i;

    // Shrinking height? Remove any robots/scrolls/sensors from excess lines
    if(new_height < board_height)
    {
      int offset;
      int check_id;

      for(offset = new_height * board_width; offset < board_size;
       offset++)
      {
        check_id = level_id[offset];

        if(check_id == 122)
          clear_sensor_id(src_board, level_param[offset]);
        else

        if((check_id == 126) || (check_id == 125))
          clear_scroll_id(src_board, level_param[offset]);
        else

        if((check_id == 123) || (check_id == 124))
          clear_robot_id(src_board, level_param[offset]);
      }
    }

    if(new_width < board_width)
    {
      // Take out pieces from each width, then reallocate.
      // Only do this for the current height or the new height, whichever is
      // less.

      int src_offset, dest_offset;
      int max_height = new_height;
      int check_id, offset;

      if(new_height > board_height)
        max_height = board_height;

      for(i = 0, src_offset = 0, dest_offset = 0; i < max_height;
       i++, src_offset += board_width, dest_offset += new_width)
      {
        // First, remove any robots/scrolls/sensors from excess chunks
        for(offset = src_offset + new_width; offset < src_offset + board_width;
          offset++)
        {
          check_id = level_id[offset];

          if(check_id == 122)
            clear_sensor_id(src_board, level_param[offset]);
          else

          if((check_id == 126) || (check_id == 125))
            clear_scroll_id(src_board, level_param[offset]);
          else

          if((check_id == 123) || (check_id == 124))
            clear_robot_id(src_board, level_param[offset]);
        }

        memmove(level_id + dest_offset, level_id + src_offset, new_width);
        memmove(level_param + dest_offset, level_param + src_offset, new_width);
        memmove(level_color + dest_offset, level_color + src_offset, new_width);
        memmove(level_under_id + dest_offset,
         level_under_id + src_offset, new_width);
        memmove(level_under_param + dest_offset,
         level_under_param + src_offset, new_width);
        memmove(level_under_color + dest_offset,
         level_under_color + src_offset, new_width);
      }

      // Resize layers
      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;

      // Do the overlay too, if it exists
      if(overlay_mode)
      {
        for(i = 0, src_offset = 0, dest_offset = 0; i < max_height;
         i++, src_offset += board_width, dest_offset += new_width)
        {
          memmove(overlay + dest_offset, overlay + src_offset, new_width);
          memmove(overlay_color + dest_offset, overlay_color + src_offset, new_width);
        }

        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);

        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;
      }
    }
    else

    if(new_width > board_width)
    {
      // Reallocate first, then copy over pieces, padding in widths
      // Only do this for the current height or the new height, whichever is
      // less.
      int src_offset, dest_offset;
      int max_height = new_height;
      int width_difference = new_width - board_width;

      if(new_height > board_height)
        max_height = board_height;

      // Resize first this time.
      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);
        overlay = src_board->overlay;
        overlay_color = src_board->overlay_color;
      }

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;

      // And start at the end instead of the beginning
      src_offset = board_width * (max_height - 1);
      dest_offset = new_width * (max_height - 1);

      for(i = 0; i < max_height; i++, src_offset -= board_width, dest_offset -= new_width)
      {
        memmove(level_id + dest_offset, level_id + src_offset, board_width);
        memmove(level_param + dest_offset, level_param + src_offset, board_width);
        memmove(level_color + dest_offset, level_color + src_offset, board_width);
        memmove(level_under_id + dest_offset,
         level_under_id + src_offset, board_width);
        memmove(level_under_param + dest_offset,
         level_under_param + src_offset, board_width);
        memmove(level_under_color + dest_offset,
         level_under_color + src_offset, board_width);
        // And fill in the remainder with blanks
        memset(level_id + dest_offset + board_width, 0, width_difference);
        memset(level_param + dest_offset + board_width, 0, width_difference);
        memset(level_color + dest_offset + board_width, 7, width_difference);
        memset(level_under_id + dest_offset + board_width, 0, width_difference);
        memset(level_under_param + dest_offset + board_width, 0, width_difference);
        memset(level_under_color + dest_offset + board_width, 7, width_difference);
      }

      src_offset = board_width * (max_height - 1);
      dest_offset = new_width * (max_height - 1);

      // Do the overlay too, if it exists
      if(overlay_mode)
      {
        for(i = 0; i < max_height; i++, src_offset -= board_width, dest_offset -= new_width)
        {
          memmove(overlay + dest_offset, overlay + src_offset, board_width);
          memmove(overlay_color + dest_offset, overlay_color + src_offset, board_width);
          // And fill in the remainder with blanks
          memset(overlay + dest_offset + board_width, 32, width_difference);
          memset(overlay_color + dest_offset + board_width, 7, width_difference);
        }
      }
    }
    else
    {
      // Width remains the same, just a straight resize

      src_board->level_id = crealloc(level_id, new_size);
      src_board->level_color = crealloc(level_color, new_size);
      src_board->level_param = crealloc(level_param, new_size);
      src_board->level_under_id = crealloc(level_under_id, new_size);
      src_board->level_under_color = crealloc(level_under_color, new_size);
      src_board->level_under_param = crealloc(level_under_param, new_size);

      // Resize the overlay too, if it exists
      if(overlay_mode)
      {
        src_board->overlay = crealloc(overlay, new_size);
        src_board->overlay_color = crealloc(overlay_color, new_size);
      }

      level_id = src_board->level_id;
      level_color = src_board->level_color;
      level_param = src_board->level_param;
      level_under_id = src_board->level_under_id;
      level_under_color = src_board->level_under_color;
      level_under_param = src_board->level_under_param;
      overlay = src_board->overlay;
      overlay_color = src_board->overlay_color;
    }

    // Fill in any blanks
    if(new_height > board_height)
    {
      int offset = new_width * board_height;
      int size_difference = new_size - offset;

      memset(level_id + offset, 0, size_difference);
      memset(level_param + offset, 0, size_difference);
      memset(level_color + offset, 7, size_difference);
      memset(level_under_id + offset, 0, size_difference);
      memset(level_under_param + offset, 0, size_difference);
      memset(level_under_color + offset, 7, size_difference);

      if(overlay_mode)
      {
        memset(overlay + offset, 32, size_difference);
        memset(overlay_color + offset, 7, size_difference);
      }
    }

    // Set the new sizes
    src_board->board_width = new_width;
    src_board->board_height = new_height;
  }
}

/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mzm.h"

#include "data.h"
#include "error.h"
#include "idput.h"
#include "legacy_robot.h"
#include "legacy_world.h"
#include "robot.h"
#include "str.h"
#include "world.h"
#include "world_format.h"
#include "world_struct.h"
#include "io/memfile.h"
#include "io/vio.h"
#include "io/zip.h"


/* The MZM format hasn't changed a whole lot with zips; only the robots
 * section is really different. This is because MZMs are in general meant to
 * be a raw data format, and furthermore accessing the raw data might be useful.
 */

// This is assumed to not go over the edges.

/**
 * Calculate the total storage size and storage mode of an MZM.
 */
static size_t save_mzm_calculate_size(struct world *mzx_world, int start_x,
 int start_y, int width, int height, int mode, int savegame,
 enum mzm_storage_mode *storage_mode)
{
  size_t mzm_size;
  *storage_mode = mode ? MZM_STORAGE_MODE_LAYER : MZM_STORAGE_MODE_BOARD;

  // Header size.
  mzm_size = 20;

  // Raw tile data storage space.
  mzm_size += (width * height) * (*storage_mode ? 2 : 6);

  // Robot storage space (board to board storage mode only).
  if(mode == MZM_BOARD_TO_BOARD_STORAGE)
  {
    struct board *src_board = mzx_world->current_board;
    struct robot **robot_list = src_board->robot_list_name_sorted;
    int num_robots_active = src_board->num_robots_active;
    int num_robots_alloc = 0;
    int i;

    for(i = 0; i < num_robots_active; i++)
    {
      struct robot *cur_robot = robot_list[i];
      if(cur_robot)
      {
        if(cur_robot->xpos >= start_x && cur_robot->xpos < start_x + width &&
         cur_robot->ypos >= start_y && cur_robot->ypos < start_y + height)
        {
          mzm_size += save_robot_calculate_size(mzx_world, cur_robot,
           savegame, MZX_VERSION);
          num_robots_alloc++;
        }
      }
    }

    // If there aren't any robots, we don't need to bother making this a ZIP.
    if(num_robots_alloc)
    {
      // Add the total overhead of the zip file.
      // File names are all "r##", so the max name length is 3.
      mzm_size += zip_bound_total_header_usage(num_robots_alloc, 3);
    }
  }
  return mzm_size;
}

static size_t save_mzm_common(struct world *mzx_world,
 int start_x, int start_y, int width, int height, int mode, int savegame,
 const enum mzm_storage_mode storage_mode, struct memfile *mf)
{
  if(savegame)
    savegame = 1;

  mfwrite("MZM3", 4, 1, mf);

  mfputw(width, mf);
  mfputw(height, mf);
  // Come back here later if there's robot information.
  mfputd(0, mf); // Robot offset.
  mfputc(0, mf); // Robot count.
  mfputc(storage_mode, mf);
  mfputc(0, mf); // Savegame mode (only set non-zero if there are robots).

  mfputw(MZX_VERSION, mf);
  // Reserved bytes
  mfputc(0, mf);
  mfputc(0, mf);
  mfputc(0, mf);

  switch(mode)
  {
    // Board, raw
    case MZM_BOARD_TO_BOARD_STORAGE:
    {
      struct zip_archive *zp;

      struct board *src_board = mzx_world->current_board;
      int board_width = src_board->board_width;
      char *level_id = src_board->level_id;
      char *level_param = src_board->level_param;
      char *level_color = src_board->level_color;
      char *level_under_id = src_board->level_under_id;
      char *level_under_param = src_board->level_under_param;
      char *level_under_color = src_board->level_under_color;
      int x, y;
      int offset = start_x + (start_y * board_width);
      int line_skip = board_width - width;
      int num_robots = 0;
      int robot_numbers[256];
      int robot_table_position;
      enum thing current_id;
      int i;

      for(y = 0; y < height; y++)
      {
        for(x = 0; x < width; x++, offset++)
        {
          current_id = (enum thing)level_id[offset];

          if(is_robot(current_id))
          {
            // Robot
            robot_numbers[num_robots] = level_param[offset];
            num_robots++;

            mfputc(current_id, mf);
            mfputc(0, mf);
            mfputc(level_color[offset], mf);
            mfputc(level_under_id[offset], mf);
            mfputc(level_under_param[offset], mf);
            mfputc(level_under_color[offset], mf);
          }
          else

          if((current_id == SENSOR) || is_signscroll(current_id) ||
           (current_id == PLAYER))
          {
            // Sensor, scroll, sign, or player
            // Put customblock fake
            mfputc((int)CUSTOM_BLOCK, mf);
            mfputc(get_id_char(src_board, offset), mf);
            mfputc(get_id_color(src_board, offset), mf);
            mfputc(level_under_id[offset], mf);
            mfputc(level_under_param[offset], mf);
            mfputc(level_under_color[offset], mf);
          }
          else
          {
            mfputc(current_id, mf);
            mfputc(level_param[offset], mf);
            mfputc(level_color[offset], mf);
            mfputc(level_under_id[offset], mf);
            mfputc(level_under_param[offset], mf);
            mfputc(level_under_color[offset], mf);
          }
        }
        offset += line_skip;
      }

      if(num_robots)
      {
        struct robot **robot_list = src_board->robot_list;
        uint64_t total_size;
        char name[4];

        // Now we're at the position we want to start writing robots.
        robot_table_position = mftell(mf);

        // Go back to header to put robot table information.
        mfseek(mf, 8, SEEK_SET);
        // Where the robots will be stored
        // (not actually necessary anymore)
        mfputd(robot_table_position, mf);
        // Number of robots
        mfputc(num_robots, mf);
        // Skip board storage mode
        mf->current += 1;
        // Savegame mode for robots
        mfputc(savegame, mf);

        // Open the zip and write our robots into it.
        zp = zip_open_mem_write(mf->start, mf->end - mf->start,
         robot_table_position);

        for(i = 0; i < num_robots; i++)
        {
          sprintf(name, "r%2.2X", (unsigned char) i);

          // Save each robot
          save_robot(mzx_world, robot_list[robot_numbers[i]], zp,
           savegame, MZX_VERSION, name);
        }

        zip_close(zp, &total_size);
        return total_size;
      }
      return mftell(mf);
    }

    // Overlay/Vlayer
    case MZM_OVERLAY_TO_LAYER_STORAGE:
    case MZM_VLAYER_TO_LAYER_STORAGE:
    {
      struct board *src_board = mzx_world->current_board;
      int board_width;
      int x, y;
      int offset;
      int line_skip;
      char *chars;
      char *colors;

      if(mode == MZM_OVERLAY_TO_LAYER_STORAGE)
      {
        // Overlay
        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        chars = src_board->overlay;
        colors = src_board->overlay_color;
        board_width = src_board->board_width;
      }
      else
      {
        // Vlayer
        chars = mzx_world->vlayer_chars;
        colors = mzx_world->vlayer_colors;
        board_width = mzx_world->vlayer_width;
      }

      offset = start_x + (start_y * board_width);
      line_skip = board_width - width;

      for(y = 0; y < height; y++)
      {
        for(x = 0; x < width; x++, offset++)
        {
          mfputc(chars[offset], mf);
          mfputc(colors[offset], mf);
        }
        offset += line_skip;
      }
      return mftell(mf);
    }

    // Board, char based
    case MZM_BOARD_TO_LAYER_STORAGE:
    {
      struct board *src_board = mzx_world->current_board;
      int board_width = src_board->board_width;
      int x, y;
      int offset = start_x + (start_y * board_width);
      int line_skip = board_width - width;

      for(y = 0; y < height; y++)
      {
        for(x = 0; x < width; x++, offset++)
        {
          mfputc(get_id_char(src_board, offset), mf);
          mfputc(get_id_color(src_board, offset), mf);
        }
        offset += line_skip;
      }
      return mftell(mf);
    }
  }
  return 0;
}

void save_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int width, int height, int mode, int savegame)
{
  vfile *output_file = vfopen_unsafe(name, "wb");
  enum mzm_storage_mode storage_mode;
  struct memfile mf;
  void *buffer;
  size_t mzm_size;

  if(output_file)
  {
    mzm_size = save_mzm_calculate_size(mzx_world, start_x, start_y, width,
     height, mode, savegame, &storage_mode);

    buffer = cmalloc(mzm_size);
    mfopen_wr(buffer, mzm_size, &mf);

    mzm_size = save_mzm_common(mzx_world, start_x, start_y, width, height, mode,
     savegame, storage_mode, &mf);

    vfwrite(buffer, mzm_size, 1, output_file);
    free(buffer);
    vfclose(output_file);
  }
}

void save_mzm_string(struct world *mzx_world, const char *name, int start_x,
 int start_y, int width, int height, int mode, int savegame, int id)
{
  enum mzm_storage_mode storage_mode;
  struct memfile mf;
  struct string *str;
  size_t mzm_size;

  mzm_size = save_mzm_calculate_size(mzx_world, start_x, start_y, width,
   height, mode, savegame, &storage_mode);

  str = new_string(mzx_world, name, mzm_size, id);
  if(str)
  {
    mfopen_wr(str->value, mzm_size, &mf);

    mzm_size = save_mzm_common(mzx_world, start_x, start_y, width, height, mode,
     savegame, storage_mode, &mf);

    // Shrink the string length to the final size of the MZM.
    str->length = mzm_size;
  }
}

static boolean read_mzm_header(struct memfile *mf, size_t file_length,
 struct mzm_header *mzm_header)
{
  char magic_string[5];

  if(file_length < 16)
    return false;

  // MegaZeux 2.83 is the last version that won't save the ver,
  // so if we don't have a ver, just act like it's 2.83
  mzm_header->world_version = V283;

  mfread(magic_string, 4, 1, mf);
  magic_string[4] = 0;

  if(!strncmp(magic_string, "MZMX", 4))
  {
    // An MZM1 file is always storage mode 0
    mzm_header->storage_mode = MZM_STORAGE_MODE_BOARD;
    mzm_header->savegame_mode = 0;
    mzm_header->num_robots = 0;
    mzm_header->robots_location = 0;
    mzm_header->width = mfgetc(mf);
    mzm_header->height = mfgetc(mf);
    mf->current += 10; // unused
  }
  else

  if(!strncmp(magic_string, "MZM2", 4))
  {
    mzm_header->width = mfgetw(mf);
    mzm_header->height = mfgetw(mf);
    mzm_header->robots_location = mfgetd(mf);
    mzm_header->num_robots = mfgetc(mf);
    mzm_header->storage_mode = mfgetc(mf);
    mzm_header->savegame_mode = mfgetc(mf);
    mf->current += 1; // unused
  }
  else

  if(!strncmp(magic_string, "MZM3", 4))
  {
    if(file_length < 20)
      return false;

    // MZM3 is like MZM2, except the robots are stored as source code if
    // savegame_mode is 0 and version >= VERSION_SOURCE.
    mzm_header->width = mfgetw(mf);
    mzm_header->height = mfgetw(mf);
    mzm_header->robots_location = mfgetd(mf);
    mzm_header->num_robots = mfgetc(mf);
    mzm_header->storage_mode = mfgetc(mf);
    mzm_header->savegame_mode = mfgetc(mf);
    mzm_header->world_version = mfgetw(mf);
    mf->current += 3; // unused
  }
  else
    return false;

  // Perform minimal validation on the header data to make sure it isn't junk.
  if(mzm_header->savegame_mode < 0 || mzm_header->savegame_mode > 1)
    return false;

  if(mzm_header->storage_mode != MZM_STORAGE_MODE_BOARD
   && mzm_header->storage_mode != MZM_STORAGE_MODE_LAYER)
    return false;

  if(mzm_header->width < 1 || mzm_header->width >= 32768 ||
   mzm_header->height < 1 || mzm_header->height >= 32768 ||
   (mzm_header->width * mzm_header->height) > MAX_BOARD_SIZE)
    return false;

  return true;
}

// This will clip.

static int load_mzm_common(struct world *mzx_world, struct memfile *mf,
 int file_length, int start_x, int start_y, int mode, int savegame,
 enum thing layer_convert_id, char *name)
{
  struct mzm_header mzm;

  int data_start;
  int expected_data_size;

  if(!is_storageless(layer_convert_id))
    layer_convert_id = CUSTOM_BLOCK;

  if(!read_mzm_header(mf, file_length, &mzm))
    goto err_invalid;

  data_start = mftell(mf);
  expected_data_size = (mzm.width * mzm.height) * (mzm.storage_mode ? 2 : 6);

  // Validate MZM size, robots location (the other fields have been validated).
  if((file_length - data_start < expected_data_size) // not enough space
   || (file_length < mzm.robots_location) // The end of file is before the robots
   || (mzm.robots_location && (expected_data_size + data_start > mzm.robots_location)))
    goto err_invalid;

  // If the mzm version is newer than the MZX version, notify
  if(mzm.world_version > MZX_VERSION)
  {
    error_message(E_MZM_FILE_VERSION_TOO_RECENT, mzm.world_version, name);
  }

  // If the MZM is a save MZM but we're not loading at runtime, notify
  if(mzm.savegame_mode > savegame)
  {
    error_message(E_MZM_FILE_FROM_SAVEGAME, 0, name);
  }

  switch(mode)
  {
    // Write to board
    case MZM_LOAD_TO_BOARD:
    {
      struct board *src_board = mzx_world->current_board;
      int board_width = src_board->board_width;
      int board_height = src_board->board_height;
      int effective_width = mzm.width;
      int effective_height = mzm.height;
      int file_line_skip;
      int line_skip;
      int x, y;
      int offset = start_x + (start_y * board_width);
      char *level_id = src_board->level_id;
      char *level_param = src_board->level_param;
      char *level_color = src_board->level_color;
      char *level_under_id = src_board->level_under_id;
      char *level_under_param = src_board->level_under_param;
      char *level_under_color = src_board->level_under_color;
      enum thing src_id;

      // Clip

      if((effective_width + start_x) >= board_width)
        effective_width = board_width - start_x;

      if((effective_height + start_y) >= board_height)
        effective_height = board_height - start_y;

      line_skip = board_width - effective_width;

      switch(mzm.storage_mode)
      {
        case MZM_STORAGE_MODE_BOARD:
        {
          // Board style, write as is
          enum thing current_id;
          int current_robot_loaded = 0;
          int robot_x_locations[256];
          int robot_y_locations[256];
          int width_difference;
          int i;

          width_difference = mzm.width - effective_width;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              current_id = (enum thing)mfgetc(mf);

              if(current_id >= SENSOR)
              {
                if(is_robot(current_id))
                {
                  robot_x_locations[current_robot_loaded] = x + start_x;
                  robot_y_locations[current_robot_loaded] = y + start_y;
                  current_robot_loaded++;
                }
                // Wipe a bunch of crap we don't want in MZMs with spaces
                else
                  current_id = 0;
              }

              src_id = (enum thing)level_id[offset];

              if(src_id >= SENSOR)
              {
                if(src_id == SENSOR)
                  clear_sensor_id(src_board, level_param[offset]);
                else

                if(is_signscroll(src_id))
                  clear_scroll_id(src_board, level_param[offset]);
                else

                if(is_robot(src_id))
                  clear_robot_id(src_board, level_param[offset]);
              }

              // Don't allow the player to be overwritten
              if(src_id != PLAYER)
              {
                level_id[offset] = current_id;
                level_param[offset] = mfgetc(mf);
                level_color[offset] = mfgetc(mf);
                level_under_id[offset] = mfgetc(mf);
                level_under_param[offset] = mfgetc(mf);
                level_under_color[offset] = mfgetc(mf);

                // We don't want this on the under layer, thanks
                if(level_under_id[offset] >= SENSOR)
                {
                  level_under_id[offset] = 0;
                  level_under_param[offset] = 0;
                  level_under_color[offset] = 7;
                }
              }
              else
                mf->current += 5;
            }

            offset += line_skip;

            // Gotta run through and mark the next robots to be skipped
            for(i = 0; i < width_difference; i++)
            {
              current_id = (enum thing)mfgetc(mf);
              mf->current += 5;

              if(is_robot(current_id))
              {
                robot_x_locations[current_robot_loaded] = -1;
                current_robot_loaded++;
              }
            }
          }

          for(i = current_robot_loaded; i < mzm.num_robots; i++)
          {
            robot_x_locations[i] = -1;
          }

          if(mzm.num_robots)
          {
            struct zip_archive *zp;
            unsigned int file_id;
            unsigned int robot_id;
            int result;

            struct robot *cur_robot;
            int current_x, current_y;
            int offset;
            int new_param;
            int robot_calculated_size = 0;
            int robot_partial_size = 0;
            int current_position;
            int dummy = 0;

            // We suppress the errors that will generally occur here and barely
            // error check the zip functions. Why? This needs to run all the way
            // through, regardless of whether it finds errors. Otherwise, we'll
            // get invalid robots with ID=0 littered around the board.

            set_error_suppression(E_WORLD_ROBOT_MISSING, 1);
            set_error_suppression(E_BOARD_ROBOT_CORRUPT, 1);

            // Reset the error count.
            get_and_reset_error_count();

            if(mzm.world_version <= MZX_LEGACY_FORMAT_VERSION)
            {
              robot_partial_size =
               legacy_calculate_partial_robot_size(mzm.savegame_mode,
               mzm.world_version);

              mfseek(mf, mzm.robots_location, SEEK_SET);
              zp = NULL;
            }
            else
            {
              zp = zip_open_mem_read(mf->start, file_length);
              world_assign_file_ids(zp, false);
            }

            // If we're loading a "runtime MZM" then it means that we're loading
            // bytecode. And to do this we must both be in-game and must be
            // running the same version this was made in. But since loading
            // dynamically created MZMs in the editor is still useful, we'll just
            // dummy out the robots.

            if((mzm.savegame_mode > savegame) ||
             (MZX_VERSION < mzm.world_version))
            {
              dummy = 1;
            }

            for(i = 0; i < mzm.num_robots; i++)
            {
              cur_robot = cmalloc(sizeof(struct robot));

              current_x = robot_x_locations[i];
              current_y = robot_y_locations[i];

              // TODO: Skipped legacy robots aren't checked for, so the loaded
              // chars for dummy robots on clipped MZMs might be off. This
              // shouldn't matter too often though.

              if(mzm.world_version <= MZX_LEGACY_FORMAT_VERSION)
              {
                create_blank_robot(cur_robot);

                current_position = mftell(mf);

                // If we fail, we have to continue, or robots won't be dummied
                // correctly. In this case, seek to the end.

                if(current_position + robot_partial_size <= file_length)
                {
                  robot_calculated_size =
                   legacy_load_robot_calculate_size(mf->current, mzm.savegame_mode,
                   mzm.world_version);

                  if(current_position + robot_calculated_size <= file_length)
                  {
                    struct memfile r_mf;
                    mfopen(mf->current, robot_calculated_size, &r_mf);
                    legacy_load_robot_from_memory(mzx_world, cur_robot, &r_mf,
                     mzm.savegame_mode, mzm.world_version, (int)current_position);
                  }
                  else
                  {
                    mfseek(mf, 0, SEEK_END);
                    dummy = 1;
                  }
                }
                else
                {
                  mfseek(mf, 0, SEEK_END);
                  dummy = 1;
                }

                mf->current += robot_calculated_size;
              }

              // Search the zip until a robot is found.
              else do
              {
                result = zip_get_next_mzx_file_id(zp, &file_id, NULL, &robot_id);

                if(result != ZIP_SUCCESS)
                {
                  // We have to continue, or we'll get screwed up robots.
                  create_blank_robot(cur_robot);
                  create_blank_robot_program(cur_robot);
                  dummy = 1;
                  break;
                }
                else

                if(file_id != FILE_ID_ROBOT || (int)robot_id < i)
                {
                  // Not a robot or is a skipped robot
                  zip_skip_file(zp);
                }
                else

                if((int)robot_id > i)
                {
                  // There's a robot missing.
                  create_blank_robot(cur_robot);
                  create_blank_robot_program(cur_robot);
                  break;
                }

                else
                {
                  load_robot(mzx_world, cur_robot, zp, mzm.savegame_mode,
                   mzm.world_version);
                  break;
                }
              }
              while(1);

              if(dummy)
              {
                // Unfortunately, getting the actual character for the robot is
                // kind of a lot of work right now. We have to load it then
                // throw it away.

                // If this is from a futer version and the format changed, we'll
                // just end up with an 'R' char, but this shouldn't happen again
                if(current_x != -1)
                {
                  offset = current_x + (current_y * board_width);
                  level_id[offset] = CUSTOM_BLOCK;
                  level_param[offset] = cur_robot->robot_char;
                }
                clear_robot(cur_robot);
              }

              else
              {
                cur_robot->world_version = mzx_world->version;

#ifdef CONFIG_DEBYTECODE
                // If we're loading source code at runtime, we need to compile it
                if(mzm.savegame_mode < savegame)
                  prepare_robot_bytecode(mzx_world, cur_robot);
#endif

                if(current_x != -1)
                {
                  new_param = find_free_robot(src_board);
                  offset = current_x + (current_y * board_width);

                  if(new_param != -1)
                  {
                    if((enum thing)level_id[offset] != PLAYER)
                    {
                      add_robot_name_entry(src_board, cur_robot,
                        cur_robot->robot_name);
                      src_board->robot_list[new_param] = cur_robot;
                      cur_robot->xpos = current_x;
                      cur_robot->ypos = current_y;
                      cur_robot->compat_xpos = current_x;
                      cur_robot->compat_ypos = current_y;
                      level_param[offset] = new_param;
                    }
                    else
                    {
                      clear_robot(cur_robot);
                    }
                  }
                  else
                  {
                    clear_robot(cur_robot);
                    level_id[offset] = 0;
                    level_param[offset] = 0;
                    level_color[offset] = 7;
                  }
                }
                else
                {
                  clear_robot(cur_robot);
                }
              }
            }

            if(mzm.world_version > MZX_LEGACY_FORMAT_VERSION)
            {
              zip_close(zp, NULL);
            }

            // If any errors were encountered, report
            if(get_and_reset_error_count())
              goto err_robots;
          }
          break;
        }

        case MZM_STORAGE_MODE_LAYER:
        {
          // Compact style; expand to customblocks
          // Board style, write as is

          file_line_skip = (mzm.width - effective_width) * 2;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              src_id = (enum thing)level_id[offset];

              if(src_id == SENSOR)
                clear_sensor_id(src_board, level_param[offset]);
              else

              if(is_signscroll(src_id))
                clear_scroll_id(src_board, level_param[offset]);
              else

              if(is_robot(src_id))
                clear_robot_id(src_board, level_param[offset]);

              // Don't allow the player to be overwritten
              if(src_id != PLAYER)
              {
                level_id[offset] = layer_convert_id;
                level_param[offset] = mfgetc(mf);
                level_color[offset] = mfgetc(mf);
                level_under_id[offset] = 0;
                level_under_param[offset] = 0;
                level_under_color[offset] = 0;
              }
              else
                mf->current += 2;
            }

            offset += line_skip;
            mf->current += file_line_skip;
          }
          break;
        }
      }
      break;
    }

    // Overlay/vlayer
    case MZM_LOAD_TO_OVERLAY:
    case MZM_LOAD_TO_VLAYER:
    {
      struct board *src_board = mzx_world->current_board;
      int dest_width = src_board->board_width;
      int dest_height = src_board->board_height;
      char *dest_chars;
      char *dest_colors;
      int effective_width = mzm.width;
      int effective_height = mzm.height;
      int file_line_skip;
      int line_skip;
      int x, y;
      int offset;

      if(mode == MZM_LOAD_TO_OVERLAY)
      {
        // Overlay
        if(!src_board->overlay_mode)
          setup_overlay(src_board, 3);

        dest_chars = src_board->overlay;
        dest_colors = src_board->overlay_color;
        dest_width = src_board->board_width;
        dest_height = src_board->board_height;
      }
      else
      {
        // Vlayer
        dest_chars = mzx_world->vlayer_chars;
        dest_colors = mzx_world->vlayer_colors;
        dest_width = mzx_world->vlayer_width;
        dest_height = mzx_world->vlayer_height;
      }

      offset = start_x + (start_y * dest_width);

      // Clip

      if((effective_width + start_x) >= dest_width)
        effective_width = dest_width - start_x;

      if((effective_height + start_y) >= dest_height)
        effective_height = dest_height - start_y;

      line_skip = dest_width - effective_width;

      switch(mzm.storage_mode)
      {
        case MZM_STORAGE_MODE_BOARD:
        {
          // Coming from board storage; for now use param as char

          file_line_skip = (mzm.width - effective_width) * 6;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              // Skip ID
              mf->current += 1;
              dest_chars[offset] = mfgetc(mf);
              dest_colors[offset] = mfgetc(mf);
              // Skip under parts
              mf->current += 3;
            }

            offset += line_skip;
            mf->current += file_line_skip;
          }
          break;
        }

        case MZM_STORAGE_MODE_LAYER:
        {
          // Coming from layer storage; transfer directly

          file_line_skip = (mzm.width - effective_width) * 2;

          for(y = 0; y < effective_height; y++)
          {
            for(x = 0; x < effective_width; x++, offset++)
            {
              dest_chars[offset] = mfgetc(mf);
              dest_colors[offset] = mfgetc(mf);
            }

            offset += line_skip;
            mf->current += file_line_skip;
          }
          break;
        }

      }
      break;
    }
  }

  // Clear none of the validation error suppressions:
  // 1) in combination with poor practice, they could lock MZX,
  // 2) they'll get reset after reloading the world.
  return 0;

err_robots:
  // The main file loaded fine, but there was a problem handling robots
  error_message(E_MZM_ROBOT_CORRUPT, 0, name);
  return 0;

err_invalid:
  error_message(E_MZM_FILE_INVALID, 0, name);
  return -1;
}

int load_mzm(struct world *mzx_world, char *name, int start_x, int start_y,
 int mode, int savegame, enum thing layer_convert_id)
{
  vfile *input_file;
  size_t file_size;
  void *buffer;
  int success;
  int count;
  struct memfile mf;
  input_file = vfopen_unsafe(name, "rb");
  if(input_file)
  {
    file_size = vfilelength(input_file, false);
    buffer = cmalloc(file_size);
    count = vfread(buffer, file_size, 1, input_file);
    vfclose(input_file);

    if(!count)
    {
      free(buffer);
      return -1;
    }

    mfopen(buffer, file_size, &mf);
    success = load_mzm_common(mzx_world, &mf, (int)file_size, start_x,
     start_y, mode, savegame, layer_convert_id, name);
    free(buffer);
    return success;
  }
  else
  {
    error_message(E_MZM_DOES_NOT_EXIST, 0, name);
    return -1;
  }
}

int load_mzm_memory(struct world *mzx_world, char *name, int start_x,
 int start_y, int mode, int savegame, enum thing layer_convert_id,
 const void *buffer, size_t length)
{
  struct memfile mf;
  mfopen(buffer, length, &mf);
  return load_mzm_common(mzx_world, &mf, (int)length, start_x, start_y,
   mode, savegame, layer_convert_id, name);
}

boolean load_mzm_header(char *name, struct mzm_header *mzm_header)
{
  vfile *input_file;
  struct memfile mf;
  char buffer[20];
  int read_length;

  mzm_header->width = -1;
  mzm_header->height = -1;

  input_file = vfopen_unsafe(name, "rb");
  if(input_file)
  {
    read_length = vfread(buffer, 1, 20, input_file);
    vfclose(input_file);

    mfopen(buffer, read_length, &mf);
    if(read_mzm_header(&mf, read_length, mzm_header))
      return true;

    error_message(E_MZM_FILE_INVALID, 0, name);
  }
  return false;
}

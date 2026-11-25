/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
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

#include "world.h"

#include "../board.h"
#include "../const.h"
#include "../error.h"
#include "../extmem.h"
#include "../graphics.h"
#include "../robot.h"
#include "../window.h"
#include "../world.h"
#include "../legacy_board.h"
#include "../idput.h"
#include "../io/memfile.h"
#include "../io/vio.h"
#include "../io/zip.h"

#include "../world_format.h"

#include "board.h"
#include "configure.h"
#include "robot.h"

#include <string.h>

static const unsigned char def_id_chars[ID_CHARS_SIZE] =
{
  /* id_chars */
  32,178,219,6,0,255,177,255,233,254,255,254,255,178,177,176, /* 0-15 */
  254,255,0,0,176,24,25,26,27,0,0,160,4,4,3,9,                /* 16-31 */
  150,7,176,0,11,0,177,12,10,0,0,162,161,0,0,22,              /* 32-47 */
  196,0,7,255,255,255,255,159,0,18,29,0,206,0,0,0,            /* 48-63 */
  '?',178,0,151,152,153,154,32,0,42,0,0,255,255,0,0,          /* 64-79 */
  235,236,5,42,2,234,21,224,127,149,5,227,0,0,172,173,        /* 80-95 */
  '?',0, /* 96-97 */
  '?','?','?','?','?','?','?','?', /* 98-121 */
  '?','?','?','?','?','?','?','?',
  '?','?','?','?','?','?','?','?',
  0,0,0,226,232,0, /* 122-127 */
  /* thin_line */
  249,179,179,179, /* None, N, S, NS */
  196,192,218,195, /* E, NE, SE, NSE */
  196,217,191,180, /* W, NW, SW, NSW */
  196,193,194,197, /* EW, NEW, SEW, NSEW */
  /* thick_line */
  249,186,186,186, /* None, N, S, NS */
  205,200,201,204, /* E, NE, SE, NSE */
  205,188,187,185, /* W, NW, SW, NSW */
  205,202,203,206, /* EW, NEW, SEW, NSEW */
  /* ice_anim */
  32,252,253,255, /* Ice animation table */
  /* lava_anim */
  176,177,178, /* Lava animation table */
  /* low_ammo, hi_ammo */
  163, /* < 10 ammunition pic */
  164, /* > 9 ammunition pic */
  /* lit_bomb */
  171,170,169,168,167,166,165, /* Lit bomb animation */
  /* energizer_glow */
  1,9,3,11,15,11,3,9, /* Energizer Glow */
  /* explosion_colors */
  239,206,76,4, /* Explosion color table */
  /* horiz_door, vert_door */
  196, /* Horizontal door pic */
  179, /* Vertical door pic */
  /* cw_anim, ccw_anim */
  47,196,92,179, /* CW animation table */
  47,179,92,196, /* CCW animation table */
  /* open_door */
  47,47, /* Open 1/2 of type 0 */
  92,92, /* Open 1/2 of type 1 */
  92,92, /* Open 1/2 of type 2 */
  47,47, /* Open 1/2 of type 3 */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  179,196,179,196,179,196,179,196, /* Open full of all types */
  47,47, /* Close 1/2 of type 0 */
  92,92, /* Close 1/2 of type 1 */
  92,92, /* Close 1/2 of type 2 */
  47,47, /* Close 1/2 of type 3 */
  /* transport_anims */
  45,94,126,94, /* North */
  86,118,95,118, /* South */
  93,41,62,41, /* East */
  91,40,60,40, /* West */
  94,62,118,60, /* All directions */
  /* thick_arrow, thin_arrow */
  30,31,16,17, /* Thick arrows (pusher-style) NSEW */
  24,25,26,27, /* Thin arrows (gun-style) NSEW */
  /* horiz_lazer, vert_lazer */
  130,196,131,196, /* Horizontal Lazer Anim Table */
  128,179,129,179, /* Vertical Lazer Anim Table */
  /* fire_anim, fire_colors */
  177,178,178,178,177,176, /* Fire animation */
  4,6,12,14,12,6, /* Fire colors */
  /* life_anim, life_colors */
  155,156,157,158, /* Free life animation */
  15,11,3,11, /* Free life colors */
  /* ricochet_panels */
  28,23, /* Ricochet pics */
  /* mine_anim */
  143,144, /* Mine animation */
  /* shooting_fire_anim, shooting_fire_colors */
  15,42, /* Shooting Fire Anim */
  12,14, /* Shooting Fire Colors */
  /* seeker_anim, seeker_colors */
  '/','-','\\','|', /* Seeker animation */
  11,14,10,12, /* Seeker colors */
  /* whirlpool_glow */
  11,3,9,15, /* Whirlpool colors (this ends at 306 bytes, where ver */
  /* 1.02 used to end) */
  /* bullet_char */
  145,146,147,148, /* Player */
  145,146,147,148, /* Neutral */
  145,146,147,148, /* Enemy */
  /* player_char */
  2, 2, 2, 2, /*N S E W */
  /* player_color */
  27,
};

static const unsigned char def_missile_color = 8;

static const unsigned char def_bullet_color[ID_BULLET_COLOR_SIZE] =
{
  15, /* Player */
  15, /* Neutral */
  15, /* Enemy */
};

static const unsigned char def_id_dmg[ID_DMG_SIZE] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0-15 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100,  0, 0, 0, 0, 0, /* 16-31 */
  0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 32-47 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 10, 5, 5, /* 48-63 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 10, 10, /* 64-79 */
  10, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0, 10, 10, /* 80-95 */
  0, 0, /* 96-97 */
  0, 0, 0, 0, 0, 0, 0, 0, /* 98-121 */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0  /* 122-127 */
};


static void append_world_refactor_board(struct world *mzx_world,
 struct board *cur_board, int old_num_boards)
{
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  char *level_id = cur_board->level_id;
  char *level_param = cur_board->level_param;
  int board_dir;
  int offset;
  int d_flag;
  int i;

  // Offset all entrances and exits
  for(offset = 0; offset < board_width * board_height; offset++)
  {
    d_flag = flags[(int)level_id[offset]];
    board_dir = level_param[offset];

    if((d_flag & A_ENTRANCE) && (board_dir != NO_BOARD))
    {
      if(board_dir + old_num_boards < mzx_world->num_boards)
        level_param[offset] += old_num_boards;
      else
        level_param[offset] = NO_BOARD;
    }
  }

  for(i = 0; i < 4; i++)
  {
    board_dir = cur_board->board_dir[i];
    if(board_dir != NO_BOARD)
      board_dir += old_num_boards;

    if(board_dir >= mzx_world->num_boards)
      board_dir = NO_BOARD;

    cur_board->board_dir[i] = board_dir;
  }
}


static boolean append_world_legacy(struct world *mzx_world, vfile *vf,
 int file_version)
{
  int i, j;
  int num_boards, old_num_boards = mzx_world->num_boards;
  int board_names_pos;
  struct board *cur_board;
  unsigned int *board_offsets;
  unsigned int *board_sizes;

  // The board list is 100% compatible between 1.x and 2.x, so the only
  // differences that need to be handled here are the location and custom SFX.
  if(file_version >= V200)
  {
    vfseek(vf, 4234, SEEK_SET);
  }
  else
  {
    int protection_method = 0;

    vfseek(vf, LEGACY_BOARD_NAME_SIZE, SEEK_SET);
    protection_method = vfgetc(vf);

    vfseek(vf, 4009, SEEK_SET);
    if(protection_method)
      vfseek(vf, 16, SEEK_CUR);
  }

  num_boards = vfgetc(vf);

  if(num_boards == 0 && file_version >= V200)
  {
    int sfx_size;
    // Sfx skip word size
    vfseek(vf, 2, SEEK_CUR);

    // Skip sfx
    for(i = 0; i < NUM_BUILTIN_SFX; i++)
    {
      sfx_size = vfgetc(vf);
      vfseek(vf, sfx_size, SEEK_CUR);
    }
    num_boards = vfgetc(vf);
  }

  // Shouldn't happen...
  if(num_boards < 1)
    return false;

  // Skip the names for now
  board_names_pos = vftell(vf);
  vfseek(vf, num_boards * BOARD_NAME_SIZE, SEEK_CUR);

  if(num_boards + old_num_boards >= MAX_BOARDS)
    num_boards = MAX_BOARDS - old_num_boards;

  mzx_world->num_boards += num_boards;
  mzx_world->num_boards_allocated += num_boards;
  mzx_world->board_list =
   (struct board **)crealloc(mzx_world->board_list,
    sizeof(struct board *) * (old_num_boards + num_boards));

  // Read the board offsets/sizes preemptively to reduce the amount of seeking.
  board_offsets = (unsigned int *)cmalloc(sizeof(int) * num_boards);
  board_sizes = (unsigned int *)cmalloc(sizeof(int) * num_boards);
  for(i = 0; i < num_boards; i++)
  {
    board_sizes[i] = vfgetd(vf);
    board_offsets[i] = vfgetd(vf);
  }

  // Append boards
  for(i = 0, j = old_num_boards; j < old_num_boards + num_boards; i++, j++)
  {
    cur_board =
     legacy_load_board_allocate(mzx_world, vf, board_offsets[i], board_sizes[i],
      false, file_version);

    mzx_world->board_list[j] = cur_board;
    if(cur_board)
    {
      // Also patch a pointer to the global robot
      if(cur_board->robot_list)
      {
        cur_board->robot_list[0] = &mzx_world->global_robot;
      }
      // Also optimize out null objects
      optimize_null_objects(cur_board);

      // Fix exits
      append_world_refactor_board(mzx_world, cur_board, old_num_boards);

      store_board_to_extram(cur_board);
    }
  }

  free(board_offsets);
  free(board_sizes);

  // Go back to where the names are
  vfseek(vf, board_names_pos, SEEK_SET);
  for(i = old_num_boards; i < old_num_boards + num_boards; i++)
  {
    char ignore[BOARD_NAME_SIZE];
    char *dest = ignore;

    cur_board = mzx_world->board_list[i];
    if(cur_board)
      dest = cur_board->board_name;

    if(!vfread(dest, BOARD_NAME_SIZE, 1, vf))
      dest[0] = '\0';

    dest[BOARD_NAME_SIZE - 1] = '\0';
  }

  vfclose(vf);
  return true;
}


static int append_world_zip_get_num_boards(const void *buffer, int buf_size)
{
  struct memfile mf;
  struct memfile prop;
  int ident;
  int size;

  mfopen(buffer, buf_size, &mf);

  while(next_prop(&prop, &ident, &size, &mf))
    if(ident == WPROP_NUM_BOARDS)
      return load_prop_int_u(&prop, 0, MAX_BOARDS);

  return 0;
}


static boolean append_world_zip(struct world *mzx_world, struct zip_archive *zp,
 int file_version)
{
  struct board *cur_board;

  unsigned int file_id;
  unsigned int board_id;

  int old_num_boards = mzx_world->num_boards;
  int num_boards;
  int result;
  int i;

  // Since we went through try_load_world, the world info has been loaded
  // and is valid. This is all we need it for, so throw it away afterward.
  num_boards = append_world_zip_get_num_boards(
   mzx_world->raw_world_info, mzx_world->raw_world_info_size);

  free(mzx_world->raw_world_info);
  mzx_world->raw_world_info = NULL;
  mzx_world->raw_world_info_size = 0;

  if(num_boards + old_num_boards >= MAX_BOARDS)
    num_boards = MAX_BOARDS - old_num_boards;

  mzx_world->num_boards += num_boards;
  mzx_world->num_boards_allocated += num_boards;
  mzx_world->board_list =
   (struct board **)crealloc(mzx_world->board_list,
    sizeof(struct board *) * (old_num_boards + num_boards));

  for(i = old_num_boards; i < old_num_boards + num_boards; i++)
  {
    while(1)
    {
      result = zip_get_next_mzx_file_id(zp, &file_id, &board_id, NULL);

      if(result != ZIP_SUCCESS)
      {
        mzx_world->board_list[i] = NULL;
        break;
      }
      else

      if(file_id != FILE_ID_BOARD_INFO)
      {
        zip_skip_file(zp);
        continue;
      }

      else
      {
        cur_board = load_board_allocate(mzx_world, zp, 0,
         file_version, board_id);

        mzx_world->board_list[i] = cur_board;

        if(cur_board)
        {
          // Fix exits.
          append_world_refactor_board(mzx_world, cur_board, old_num_boards);

          store_board_to_extram(cur_board);
        }
        break;
      }
    }
  }

  zip_close(zp, NULL);
  return true;
}


boolean append_world(struct world *mzx_world, const char *file)
{
  char ignore[BOARD_NAME_SIZE];
  int file_version;

  boolean ret = false;

  struct zip_archive *zp;
  vfile *vf;

  try_load_world(mzx_world, &zp, &vf, file, false, &file_version, ignore);

  if(zp)
  {
    ret = append_world_zip(mzx_world, zp, file_version);
  }
  else

  if(vf)
  {
    ret = append_world_legacy(mzx_world, vf, file_version);
  }

  // Remove any null boards
  optimize_null_boards(mzx_world);

  // Make sure update_done is adequately sized
  set_update_done(mzx_world);

  return ret;
}


// Create a new, blank, world, suitable for editing.
void create_blank_world(struct world *mzx_world)
{
  // Make default global data
  // Make a blank board
  int i;

  mzx_world->version = MZX_VERSION;
  mzx_world->active = 1;

  mzx_world->num_boards = 1;
  mzx_world->num_boards_allocated = 1;
  mzx_world->board_list = cmalloc(sizeof(struct board *));
  mzx_world->board_list[0] = create_blank_board(get_editor_config());
  mzx_world->current_board_id = 0;
  mzx_world->current_board = mzx_world->board_list[0];

  mzx_world->edge_color = 8;
  mzx_world->first_board = 0;
  mzx_world->endgame_board = NO_ENDGAME_BOARD;
  mzx_world->death_board = NO_DEATH_BOARD;
  mzx_world->endgame_x = 0;
  mzx_world->endgame_y = 0;
  mzx_world->game_over_sfx = 1;
  mzx_world->death_x = 0;
  mzx_world->death_y = 0;
  mzx_world->starting_lives = 7;
  mzx_world->lives_limit = 99;
  mzx_world->starting_health = 100;
  mzx_world->health_limit = 200;
  mzx_world->enemy_hurt_enemy = 0;
  mzx_world->clear_on_exit = 0;
  mzx_world->only_from_swap = 0;
  memcpy(id_chars, def_id_chars, ID_CHARS_SIZE);
  missile_color = def_missile_color;
  memcpy(bullet_color, def_bullet_color, ID_BULLET_COLOR_SIZE);
  memcpy(id_dmg, def_id_dmg, ID_DMG_SIZE);

  create_blank_robot_direct(&mzx_world->global_robot, -1, -1);
  mzx_world->current_board->robot_list[0] = &mzx_world->global_robot;

  for(i = 0; i < 6; i++)
  {
    mzx_world->status_counters_shown[i][0] = 0;
  }

  memset(mzx_world->name, 0, sizeof(mzx_world->name));

  // Defaults for things set by load_world
  sfx_free(&mzx_world->custom_sfx);
  mzx_world->max_samples = -1;
  mzx_world->joystick_simulate_keys = true;

  set_update_done(mzx_world);

  set_screen_mode(0);
  smzx_palette_loaded(false);
  set_palette_intensity(100);

  ec_clear_set();
  ec_load_mzx();
  default_palette();
  default_vlayer(mzx_world);
  default_global_data(mzx_world);
}

void set_update_done_current(struct world *mzx_world)
{
  struct board *current_board = mzx_world->current_board;
  int size = current_board->board_width * current_board->board_height;

  if(size > mzx_world->update_done_size)
  {
    if(mzx_world->update_done == NULL)
    {
     mzx_world->update_done = cmalloc(size);
    }
    else
    {
      mzx_world->update_done =
       crealloc(mzx_world->update_done, size);
    }

    mzx_world->update_done_size = size;
  }
}

/**
 * Move the current board to a new position in the board list.
 * The current board is updated afterward if necessary.
 */
void move_current_board(struct world *mzx_world, int new_position)
{
  int old_position = mzx_world->current_board_id;

  if(new_position != old_position)
  {
    int num_boards = mzx_world->num_boards;
    struct board **board_list = mzx_world->board_list;
    struct board **new_board_list = ccalloc(num_boards, sizeof(struct board *));
    int *board_id_translation_list = ccalloc(num_boards, sizeof(int));
    int i;
    int j;

    // Copy the list and shift all boards necessary.
    for(i = 0; i < num_boards; i++)
    {
      j = i;

      if(new_position > old_position)
      {
        // Move the board back.
        if(i > old_position && i <= new_position)
          j = i - 1;
      }
      else

      if(new_position < old_position)
      {
        // Move the board forward.
        if(i >= new_position && i < old_position)
          j = i + 1;
      }

      // Ignore the current board for now...
      if(i != old_position)
      {
        board_id_translation_list[i] = j;
        new_board_list[j] = board_list[i];
      }
    }

    // Insert the current board at its new position.
    board_id_translation_list[old_position] = new_position;
    new_board_list[new_position] = board_list[old_position];

    refactor_board_list(mzx_world, new_board_list, num_boards,
     board_id_translation_list);

    free(board_id_translation_list);
  }
}

char get_default_id_char(int id)
{
  return def_id_chars[id];
}

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

#include "idarray.h"
#include "data.h"
#include "const.h"
#include "world_struct.h"

// Place an id/color/param combo in an array position, moving current to
// "under" status if possible, and clearing original "under". If placing an
// "under", then automatically clear lower no matter what. Also mark as
// updated.

void id_place(struct world *mzx_world, int array_x, int array_y,
 enum thing id, char color, char param)
{
  struct board *src_board = mzx_world->current_board;
  int offset;

  array_x = MZX_CLAMP(array_x, 0, src_board->board_width - 1);
  array_y = MZX_CLAMP(array_y, 0, src_board->board_height - 1);

  offset = (array_y * src_board->board_width) + array_x;
  offs_place_id(mzx_world, offset, id, color, param);
}

// Place ID using an offset instead of coordinate
void offs_place_id(struct world *mzx_world, unsigned int offset,
 enum thing id, char color, char param)
{
  struct board *src_board = mzx_world->current_board;
  enum thing p_id = (enum thing)src_board->level_id[offset];
  int p_flag = flags[p_id];
  int d_flag = flags[(int)id];

  // Mark as updated
  mzx_world->update_done[offset] = 1;

  // Is it a sensor and is the player being put on it?
  // Or, can it be moved under and can the new item not be moved under?
  if(((p_flag & A_SPEC_STOOD) && (id == PLAYER)) ||
   ((p_flag & A_UNDER) && !(d_flag & A_UNDER)))
  {
    // If moving the player down, move what's there underneath the player
    // into the under_player variables
    if(id == PLAYER)
    {
      mzx_world->under_player_id = src_board->level_under_id[offset];
      mzx_world->under_player_param = src_board->level_under_param[offset];
      mzx_world->under_player_color = src_board->level_under_color[offset];
    }

    // Then move what's currently on top under
    src_board->level_under_id[offset] = p_id;
    src_board->level_under_param[offset] = src_board->level_param[offset];
    src_board->level_under_color[offset] = src_board->level_color[offset];
  }
  else
  {
    // Clear anything that might be underneath
    src_board->level_under_id[offset] = 0;
    src_board->level_under_param[offset] = 0;
    src_board->level_under_color[offset] = 7;
  }

  // Put new combo
  src_board->level_id[offset] = (char)id;
  src_board->level_param[offset] = param;
  src_board->level_color[offset] = color;
}

// Remove the top thing at a position
void id_remove_top(struct world *mzx_world, int array_x, int array_y)
{
  struct board *current_board = mzx_world->current_board;
  int offset = (array_y * current_board->board_width) + array_x;

  offs_remove_id(mzx_world, offset);
}

// Remove the top thing at an offset
void offs_remove_id(struct world *mzx_world, unsigned int offset)
{
  struct board *src_board = mzx_world->current_board;
  enum thing id = (enum thing)src_board->level_id[offset];

  src_board->level_id[offset] = src_board->level_under_id[offset];
  src_board->level_param[offset] = src_board->level_under_param[offset];
  src_board->level_color[offset] = src_board->level_under_color[offset];

  if(id == PLAYER)
  {
    // Removing the player? Then put the under_player stuff under
    src_board->level_under_id[offset] = mzx_world->under_player_id;
    src_board->level_under_param[offset] = mzx_world->under_player_param;
    src_board->level_under_color[offset] = mzx_world->under_player_color;

    mzx_world->under_player_id = (char)SPACE;
    mzx_world->under_player_param = 0;
    mzx_world->under_player_color = 7;
  }
  else
  {
    src_board->level_under_id[offset] = (char)SPACE;
    src_board->level_under_param[offset] = 0;
    src_board->level_under_color[offset] = 7;
  }
}

void id_remove_under(struct world *mzx_world, int array_x, int array_y)
{
  struct board *src_board = mzx_world->current_board;
  int offset = (array_y * src_board->board_width) + array_x;

  // If removing something under the player place this stuff instead
  if((enum thing)src_board->level_id[offset] == PLAYER)
  {
    src_board->level_under_id[offset] = mzx_world->under_player_id;
    src_board->level_under_param[offset] = mzx_world->under_player_param;
    src_board->level_under_color[offset] = mzx_world->under_player_color;
    mzx_world->under_player_id = (char)SPACE;
    mzx_world->under_player_param = 0;
    mzx_world->under_player_color = 0;
  }
  else
  {
    src_board->level_under_id[offset] = (char)SPACE;
    src_board->level_under_param[offset] = 0;
    src_board->level_under_color[offset] = 7;
  }

  mzx_world->update_done[offset] = 1;
}

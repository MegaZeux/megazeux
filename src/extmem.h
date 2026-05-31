/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef MEGAZEUX_EXTMEM_H
#define MEGAZEUX_EXTMEM_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "board_struct.h"
#include "world_struct.h"

#define store_board_to_extram(b) \
 real_store_board_to_extram(b, __FILE__, __LINE__)
#define retrieve_board_from_extram(b) \
 real_retrieve_board_from_extram(b, false, __FILE__, __LINE__)
#define clear_board_from_extram(b) \
 real_retrieve_board_from_extram(b, true, __FILE__, __LINE__)

#define set_current_board(mzx_world, b) \
 real_set_current_board(mzx_world, b, __FILE__, __LINE__)
#define set_current_board_ext(mzx_world, b) \
 real_set_current_board_ext(mzx_world, b, __FILE__, __LINE__)

#ifdef CONFIG_EXTRAM

// Move the board's memory from normal RAM to extra RAM.
CORE_LIBSPEC void real_store_board_to_extram(struct board *board,
 const char *file, int line);

// Move the board's memory from extra RAM to normal RAM. If free_data is set,
// clear all buffers and NULL their respective pointers instead.
CORE_LIBSPEC void real_retrieve_board_from_extram(struct board *board,
 boolean free_data, const char *file, int line);

#ifdef CONFIG_EDITOR
CORE_LIBSPEC boolean board_extram_usage(struct board *board, size_t *compressed,
 size_t *uncompressed);
#endif /* CONFIG_EDITOR */

#else /* !CONFIG_EXTRAM */

#ifdef DEBUG
#include "util.h"
#endif

static inline void real_store_board_to_extram(struct board *board,
 const char *file, int line)
{
#ifdef DEBUG
  if(!board->is_extram)
    board->is_extram = true;
  else
    warn("board %p is already in extram! (%s:%d)\n", (void *)board, file, line);
#endif
}

static inline void real_retrieve_board_from_extram(struct board *board,
 boolean free_data, const char *file, int line)
{
#ifdef DEBUG
  if(board->is_extram)
    board->is_extram = false;
  else
    warn("board %p isn't in extram! (%s:%d)\n", (void *)board, file, line);
#endif
}

#endif /* !CONFIG_EXTRAM */

static inline void real_set_current_board(struct world *mzx_world,
 struct board *cur_board, const char *file, int line)
{
  if(mzx_world->current_board && mzx_world->current_board != cur_board)
    real_store_board_to_extram(mzx_world->current_board, file, line);
  mzx_world->current_board = cur_board;
}

static inline void real_set_current_board_ext(struct world *mzx_world,
 struct board *cur_board, const char *file, int line)
{
  if(mzx_world->current_board != cur_board)
  {
    real_set_current_board(mzx_world, cur_board, file, line);
    real_retrieve_board_from_extram(cur_board, false, file, line);
  }
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EXTMEM_H */

/* MegaZeux
 *
 * Copyright (C) 2002 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_SPRITE_STRUCT_H
#define MEGAZEUX_SPRITE_STRUCT_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#define MAX_SPRITES         256

struct sprite
{
  int x;
  int y;
  int ref_x;
  int ref_y;
  char color;
  unsigned int flags;
  unsigned int width;
  unsigned int height;
  signed int col_x;
  signed int col_y;
  unsigned int col_width;
  unsigned int col_height;
  int transparent_color;
  int offset;
  int qsort_order;
  int z;
};

struct collision_list
{
  int num;
  int collisions[MAX_SPRITES];
};

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_SPRITE_STRUCT_H */

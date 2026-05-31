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

#ifndef MEGAZEUX_SPRITE_H
#define MEGAZEUX_SPRITE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "sprite_struct.h"
#include "world_struct.h"

enum
{
  SPRITE_INITIALIZED  = (1 << 0), // Is active
  SPRITE_CHAR_CHECK   = (1 << 1), // CHAR_CHECK flag 1 (see below)
  SPRITE_OVER_OVERLAY = (1 << 2), // Is drawn over the overlay
  SPRITE_SRC_COLORS   = (1 << 3), // Uses "natural" colors
  SPRITE_STATIC       = (1 << 4), // Is static relative to viewport
  SPRITE_CHAR_CHECK2  = (1 << 5), // CHAR_CHECK flag 2 (see below)
  SPRITE_VLAYER       = (1 << 6), // References the vlayer
  SPRITE_UNBOUND      = (1 << 7), // Uses pixel positioning and collision
  SPRITE_OFF_ON_EXIT  = (1 << 8), // Turn off sprite when switching boards

  // Internal flag combination for pixel collision.
  SPRITE_PIXCHECK     = SPRITE_UNBOUND | SPRITE_CHAR_CHECK | SPRITE_CHAR_CHECK2,
};

/**
 * Classic sprite CHAR_CHECK:
 *
 * neither:  sprite collides with all of its chars
 * 1 set:    sprite doesn't collide when checking against char 32
 * 2 set:    sprite doesn't collide when checking against blank chars
 *
 * Additionally, if flag 2 is set, blank chars will not be drawn.
 */

/**
 * Unbound sprite CHAR_CHECK (if either colliding sprite is unbound):
 *
 * neither:  OTHER sprites collide with its full collision box
 * 1 set:    OTHER sprites won't collide when checking against char 32
 * 2 set:    OTHER sprites won't collide when checking against blank chars
 * 1 and 2:  Only visible pixels can be collided against in this sprite.
 */

void plot_sprite(struct world *mzx_world, struct sprite *cur_sprite, int color,
 int x, int y);
void draw_sprites(struct world *mzx_world);
boolean sprite_at_xy(struct world *mzx_world, struct sprite *cur_sprite, int x, int y);
int sprite_colliding_xy(struct world *mzx_world, struct sprite *check_sprite,
 int x, int y);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_SPRITE_H */

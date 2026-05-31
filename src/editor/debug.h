/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_DEBUG_H
#define MEGAZEUX_DEBUG_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../core.h"

int get_counter_safe(struct world *mzx_world, const char *name, int id);

void __debug_counters(context *ctx);
void __draw_debug_box(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_DEBUG_H */

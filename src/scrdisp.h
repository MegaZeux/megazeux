/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_SCRDISP_H
#define MEGAZEUX_SCRDISP_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

// Maximum line length of scrolls, including the newline or terminator.
#define SCROLL_MAX_LINE_LEN 256

// Type 0/1 to DISPLAY a scroll/sign
CORE_LIBSPEC void scroll_edit(struct world *mzx_world, struct scroll *scroll,
 int type);

// type == 0 scroll, 1 sign, 2 scroll edit
void scroll_edging_ext(struct world *mzx_world, int type, boolean mask);
void help_display(struct world *mzx_world, char *help, int offs,
 char *file, char *label);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_SCRDISP_H */

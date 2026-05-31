/* MegaZeux
 *
 * Copyright (C) 2020-2025 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef MEGAZEUX_EDITOR_ANSI_H
#define MEGAZEUX_EDITOR_ANSI_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "edit.h"

boolean validate_ansi(const char *filename, int wrap_width, int *width, int *height);

boolean import_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 enum thing convert_id);

boolean export_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 boolean text_only, boolean doorway_mode, const char *title, const char *author);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EDITOR_ANSI_H */

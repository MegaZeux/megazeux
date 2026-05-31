/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef __3DS_RENDER_H__
#define __3DS_RENDER_H__

#include <stdlib.h>
#include <3ds.h>
#include <citro3d.h>

#include "../../compat.h"

__M_BEGIN_DECLS

struct ctr_render_data;

C3D_Tex *ctr_load_png(const char *name);

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex *texture,
 int tx, int ty, int tw, int th,
 float x, float y, float w, float h, float z, u32 color, boolean flipy);

void ctr_request_set_wide(bool wide);
int ctr_get_subscreen_height(void);

__M_END_DECLS

#endif /* __3DS_RENDER_H__ */

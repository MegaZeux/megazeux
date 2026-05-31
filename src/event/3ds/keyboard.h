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

#ifndef MEGAZEUX_KEYBOARD_3DS_H
#define MEGAZEUX_KEYBOARD_3DS_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../event.h"

struct ctr_render_data; /* renderer_ctr.h */

typedef struct
{
  u16 x, y, w, h;
  enum keycode keycode;
  u8 flags;
} touch_area_t;

void ctr_keyboard_init(struct ctr_render_data *render_data);
void ctr_keyboard_draw(struct ctr_render_data *render_data);
boolean ctr_keyboard_update(struct buffered_status *status);
boolean ctr_keyboard_force_zoom_out(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_KEYBOARD_3DS_H */

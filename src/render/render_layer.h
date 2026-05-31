/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#ifndef MEGAZEUX_RENDER_LAYER_H
#define MEGAZEUX_RENDER_LAYER_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "../graphics.h"

void render_layer(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_RENDER_LAYER_H */

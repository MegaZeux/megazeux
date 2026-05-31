/* MegaZeux
 *
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

#ifndef __RENDER_H
#define __RENDER_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../graphics.h"

typedef void (*set_colors_function)(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg);

extern const set_colors_function set_colors8[4];
extern const set_colors_function set_colors16[4];
extern const set_colors_function set_colors32[4];

void yuy2_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg);
void uyvy_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg);
void yvyu_subsample_set_colors_mzx(const struct graphics_data *graphics,
 uint32_t * RESTRICT char_colors, uint8_t bg, uint8_t fg);

void render_graph8(uint8_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics, set_colors_function set_colors);
void render_graph16(uint16_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics, set_colors_function set_colors);
void render_graph32(uint32_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics);
void render_graph32s(uint32_t * RESTRICT pixels, size_t pitch,
 const struct graphics_data *graphics);

void render_cursor(uint32_t *pixels, size_t pitch, uint8_t bpp, unsigned int x,
 unsigned int y, uint32_t flatcolor, uint8_t lines, uint8_t offset);
void render_mouse(uint32_t *pixels, size_t pitch, uint8_t bpp, unsigned int x,
 unsigned int y, uint32_t mask, uint32_t amask, uint8_t w, uint8_t h);

void get_screen_coords_viewport(struct graphics_data *graphics,
 struct video_window *window, int screen_x, int screen_y,
 int *x, int *y, int *min_x, int *min_y, int *max_x, int *max_y);
void set_screen_coords_viewport(struct graphics_data *graphics,
 struct video_window *window, int x, int y,
 int *screen_x, int *screen_y);
void set_window_viewport_centered(struct graphics_data *graphics,
 struct video_window *window);

#if defined(CONFIG_RENDER_GL_FIXED) || defined(CONFIG_RENDER_GL_PROGRAM) \
 || defined(CONFIG_RENDER_SOFTSCALE) || defined(CONFIG_RENDER_SDLACCEL) \
 || defined(CONFIG_RENDER_YUV) || defined(CONFIG_RENDER_GX) \
 || defined(MZX_UNIT_TESTS)

#define HAVE_SET_WINDOW_VIEWPORT_SCALED

void set_window_viewport_scaled(struct graphics_data *graphics,
 struct video_window *window);

#endif /* CONFIG_RENDER_GL_FIXED || CONFIG_RENDER_GL_PROGRAM || ... */

__M_END_DECLS

#endif // __RENDER_H

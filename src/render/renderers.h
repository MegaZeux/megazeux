/* MegaZeux
 *
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef __RENDERERS_H
#define __RENDERERS_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "../graphics.h"

struct renderer
{
  const char *name;
  const char * const * aliases;

  boolean (*init_video)       (struct graphics_data *, struct config_info *);
  void    (*free_video)       (struct graphics_data *);
  boolean (*create_window)    (struct graphics_data *, struct video_window *);
  boolean (*resize_window)    (struct graphics_data *, struct video_window *);
  boolean (*resize_callback)  (struct graphics_data *, struct video_window *);
  void    (*set_viewport)     (struct graphics_data *, struct video_window *);
#if 0
  void    (*get_screen_coords)(struct graphics_data *, struct video_window *,
                                int screen_x, int screen_y, int *x, int *y,
                                int *min_x, int *min_y, int *max_x, int *max_y);
  void    (*set_screen_coords)(struct graphics_data *, struct video_window *,
                                int x, int y, int *screen_x, int *screen_y);
#endif
  boolean (*set_window_caption)(struct graphics_data *, struct video_window *,
                                const char *caption);
  boolean (*set_window_icon)  (struct graphics_data *, struct video_window *,
                                const char *icon_path);
  boolean (*set_screen_mode)  (struct graphics_data *, unsigned mode);
  void    (*update_colors)    (struct graphics_data *, struct rgb_color *palette,
                                unsigned int count);
  void    (*remap_char_range) (struct graphics_data *, uint16_t first, uint16_t count);
  void    (*remap_char)       (struct graphics_data *, uint16_t chr);
  void    (*remap_charbyte)   (struct graphics_data *, uint16_t chr, uint8_t byte);
  boolean (*switch_shader)    (struct graphics_data *, const char *name);
  void    (*render_graph)     (struct graphics_data *);
  void    (*render_layer)     (struct graphics_data *, struct video_layer *);
  void    (*render_cursor)    (struct graphics_data *, unsigned x, unsigned y,
                                uint16_t color, unsigned lines, unsigned offset);
  void    (*hardware_cursor)  (struct graphics_data *, unsigned x, unsigned y,
                                uint16_t color, unsigned lines, unsigned offset,
                                boolean enable);
  void    (*render_mouse)     (struct graphics_data *, unsigned x, unsigned y,
                                unsigned w, unsigned h);
  void    (*sync_screen)      (struct graphics_data *, struct video_window *);
  void    (*focus_pixel)      (struct graphics_data *, unsigned x, unsigned y);
};

extern const struct renderer * const renderers_available[];

extern const struct renderer renderer_software;
extern const struct renderer renderer_softscale;
extern const struct renderer renderer_sdlaccel;
extern const struct renderer renderer_yuv1;
extern const struct renderer renderer_yuv2;
extern const struct renderer renderer_gp2x;

extern const struct renderer renderer_opengl1;
extern const struct renderer renderer_opengl2;
extern const struct renderer renderer_glsl;
extern const struct renderer renderer_glslscale;
extern const struct renderer renderer_glsl_auto;

extern const struct renderer renderer_ega;
extern const struct renderer renderer_svga;
extern const struct renderer renderer_nds;
extern const struct renderer renderer_ctr;
extern const struct renderer renderer_xfb;
extern const struct renderer renderer_gx;
extern const struct renderer renderer_dreamcast;
extern const struct renderer renderer_dreamcast_fb;

boolean set_current_renderer(struct graphics_data *graphics, const char *name);

__M_END_DECLS

#endif // __RENDERERS_H

/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
 * Copyright (C) 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdlib.h>
#include <assert.h>

#include "render_layer.h"

#include "../graphics.h"

// Skip unused variants to reduce compile time on these platforms.
#if defined(CONFIG_WII) || defined(ANDROID) || defined(__EMSCRIPTEN__) || \
 defined(CONFIG_PSVITA)
#define SKIP_8BPP
#define SKIP_16BPP
#endif

#if defined(CONFIG_3DS)
#define SKIP_8BPP
#if !defined(CONFIG_SDL)
#define SKIP_16BPP
#endif
#endif

#if defined(CONFIG_DJGPP) && !defined(CONFIG_DOS_SVGA)
#define SKIP_8BPP
#define SKIP_16BPP
#endif

#if defined(CONFIG_DREAMCAST)
#define SKIP_8BPP
#endif

#include "render_layer_code.hpp"

#ifdef BUILD_REFERENCE_RENDERER
#include "render_layer_reference.h"
#endif

void render_layer(void * RESTRICT pixels,
 size_t width_px, size_t height_px, size_t pitch, int bpp,
 const struct graphics_data *graphics, const struct video_layer *layer)
{
#ifdef BUILD_REFERENCE_RENDERER
  reference_renderer(reinterpret_cast<uint32_t * RESTRICT>(pixels),
   width_px, height_px, pitch, graphics, layer);
  return;
#endif

  int smzx = 0;
  int trans = 0;
  int align = 0;
  int clip = 0;
  select_aligned_renderer(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);

  select_unaligned_renderer(align);

  render_layer_func(pixels, width_px, height_px, pitch, graphics, layer,
   bpp, align, smzx, trans, clip);
}

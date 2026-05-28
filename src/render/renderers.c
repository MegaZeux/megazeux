/* MegaZeux
 *
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "renderers.h"
#include "../util.h"

const struct renderer * const renderers_available[] =
{
#if defined(CONFIG_RENDER_SOFT)
  &renderer_software,
#endif
#if defined(CONFIG_RENDER_SOFTSCALE)
  &renderer_softscale,
#endif
#if defined(CONFIG_RENDER_SDLACCEL)
  &renderer_sdlaccel,
#endif
#if defined(CONFIG_RENDER_GL_FIXED)
  &renderer_opengl1,
  &renderer_opengl2,
#endif
#if defined(CONFIG_RENDER_GL_PROGRAM)
  &renderer_glsl,
  &renderer_glslscale,
  &renderer_glsl_auto,
#endif
#if defined(CONFIG_RENDER_YUV)
  &renderer_yuv1,
  &renderer_yuv2,
#endif
#if defined(CONFIG_RENDER_GP2X)
  &renderer_gp2x,
#endif
#if defined(CONFIG_NDS)
  &renderer_nds,
#endif
#if defined(CONFIG_3DS)
#if defined(CONFIG_RENDER_CTR)
  &renderer_ctr,
#endif
#endif
#if defined(CONFIG_WII)
#if defined(CONFIG_RENDER_GX)
  &renderer_gx,
#endif
#if !defined(CONFIG_SDL)
  &renderer_xfb,
#endif
#endif
#if defined(CONFIG_DJGPP)
  &renderer_ega,
#if defined(CONFIG_DOS_SVGA)
  &renderer_svga,
#endif
#endif
#if defined(CONFIG_DREAMCAST)
  &renderer_dreamcast,
  &renderer_dreamcast_fb,
#endif
  NULL
};

boolean set_current_renderer(struct graphics_data *graphics, const char *name)
{
  const struct renderer *renderer;
  int i;
  int j;

  // The first renderer was NULL, this shouldn't happen
  if(!renderers_available[0])
  {
    warn("No renderers built, please provide a valid config.h!\n");
    return false;
  }

  for(i = 0; renderers_available[i]; i++)
  {
    renderer = renderers_available[i];

    if(!strcasecmp(name, renderer->name))
      goto found;

    /* Some old renderer names need to be supported for compatibility reasons.
     * These optional aliases are listed in a separate renderer field.
     */
    if(renderer->aliases)
    {
      for(j = 0; renderer->aliases[j]; j++)
        if(!strcasecmp(name, renderer->aliases[j]))
          goto found;
    }
  }
  // If no match found, use first renderer in the renderer list
  renderer = renderers_available[0];
  i = 0;

found:
  graphics->renderer = renderer;
  graphics->renderer_num = i;
  graphics->window.is_init = false;

  debug("Video: using '%s' renderer.\n", renderer->name);
  return true;
}

/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2018 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "../render.h"
#include "../render_layer.h"
#include "../renderers.h"
#include "../yuv.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#define BOOL _BOOL
#include <ogc/system.h>
#include <ogc/conf.h>
#include <ogc/cache.h>
#include <ogc/video.h>
#include <ogc/gx.h>
#include <ogc/gu.h>
#undef BOOL

/**
 * This software renderer works by drawing directly to the Wii's xfb, which
 * is typically a convenient resolution of 640x480 (NTSC) or 640x572 (PAL).
 * This method generally works pretty fast. As the xfb uses YUY2 color packing,
 * this requires special color mixing in normal MZX mode (the same the overlay2
 * renderer uses).
 *
 * There are two problems with this strategy, though: 240p and 288p video modes
 * mean there needs to be a vertical downscaling fallback, and layer rendering
 * is not compatible with the YUY2 mixing. To facilitate both, an intermediate
 * buffer is drawn to and the final result is mixed and copied into the xfb.
 * Both of these situations make the renderer slower than usual.
 */

struct xfb_render_data
{
  GXRModeObj *rmode;
  u32 *xfb[2];
  u32 current_xfb;
  u32 pitch;
  u32 skip;
  u32 intermediate_fb[SCREEN_PIX_W * SCREEN_PIX_H];
  u8 intermediate_active;
  u8 intermediate_bpp;
  u8 require_240p_mode;
};

static boolean xfb_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct xfb_render_data *render_data;
  GXRModeObj *rmode;
  int skip_height;
  int render_height;
  u32 black = rgb_to_yuy2(0, 0, 0);

  graphics->resolution_width = SCREEN_PIX_W;
  graphics->resolution_height = SCREEN_PIX_H;
  graphics->window_width = SCREEN_PIX_W;
  graphics->window_height = SCREEN_PIX_H;

  render_data = cmalloc(sizeof(struct xfb_render_data));
  graphics->render_data = render_data;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 16;

  VIDEO_Init();

  rmode = VIDEO_GetPreferredMode(NULL);

  render_height = SCREEN_PIX_H;
  if(rmode->xfbHeight < render_height)
    render_height /= 2;

  skip_height = (rmode->xfbHeight - render_height) / 2;

  if(skip_height < 0)
    return false;

  render_data->rmode = rmode;
  render_data->pitch =
   VIDEO_PadFramebufferWidth(rmode->fbWidth) * VI_DISPLAY_PIX_SZ;
  render_data->skip = render_data->pitch * skip_height;

  render_data->require_240p_mode = (render_height < SCREEN_PIX_H);
  render_data->intermediate_active = false;
  render_data->intermediate_bpp = 16;

  render_data->xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->current_xfb = 0;

  VIDEO_Configure(rmode);
  VIDEO_ClearFrameBuffer(rmode, render_data->xfb[0], black);
  VIDEO_ClearFrameBuffer(rmode, render_data->xfb[1], black);
  VIDEO_SetNextFramebuffer(render_data->xfb[0]);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if(rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  return true;
}

static void xfb_free_video(struct graphics_data *graphics)
{
  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean xfb_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;
}

static void xfb_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  unsigned int i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
     rgb_to_yuy2(palette[i].r, palette[i].g, palette[i].b);
  }
}

static void xfb_render_graph(struct graphics_data *graphics)
{
  struct xfb_render_data *render_data = graphics->render_data;
  int mode = graphics->screen_mode;
  unsigned int pitch = render_data->pitch;
  unsigned int skip = render_data->skip;
  uint32_t *pixels;

  if(render_data->require_240p_mode)
  {
    render_data->intermediate_active = true;
    render_data->intermediate_bpp = 16;
    pixels = render_data->intermediate_fb;
  }
  else
  {
    render_data->intermediate_active = false;
    pixels = render_data->xfb[render_data->current_xfb];
    pixels += skip / sizeof(uint32_t);
  }

  if(!mode)
    render_graph16((uint16_t *)pixels, pitch, graphics,
     yuy2_subsample_set_colors_mzx);
  else
    render_graph16((uint16_t *)pixels, pitch, graphics, set_colors32[mode]);
}

static void xfb_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct xfb_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch * 2;
  uint32_t *pixels;

  render_data->intermediate_active = true;
  render_data->intermediate_bpp = 32;
  pixels = render_data->intermediate_fb;

  render_layer(pixels, SCREEN_PIX_W, SCREEN_PIX_H, pitch, 32, graphics, layer);
}

static void xfb_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct xfb_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  unsigned int skip = render_data->skip;
  unsigned int bpp = 16;

  uint32_t flatcolor = graphics->flat_intensity_palette[color];
  uint32_t *pixels;

  if(render_data->intermediate_active)
  {
    pixels = render_data->intermediate_fb;

    if(render_data->intermediate_bpp == 32)
    {
      pitch *= 2;
      bpp = 32;
    }
  }
  else
  {
    pixels = render_data->xfb[render_data->current_xfb];
    pixels += skip / sizeof(uint32_t);
  }

  render_cursor(pixels, pitch, bpp, x, y, flatcolor, lines, offset);
}

static void xfb_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct xfb_render_data *render_data = graphics->render_data;
  unsigned int pitch = render_data->pitch;
  unsigned int skip = render_data->skip;
  unsigned int bpp = 16;
  uint32_t *pixels;

  uint32_t mask = 0xFFFFFFFF;
  uint32_t alpha_mask = 0x0;

  if(render_data->intermediate_active)
  {
    pixels = render_data->intermediate_fb;

    if(render_data->intermediate_bpp == 32)
    {
      pitch *= 2;
      bpp = 32;
    }
  }
  else
  {
    pixels = render_data->xfb[render_data->current_xfb];
    pixels += skip / sizeof(uint32_t);
  }

  render_mouse(pixels, pitch, bpp, x, y, mask, alpha_mask, w, h);
}

#define Y1_MASK YUY2_Y1_MASK
#define Y2_MASK YUY2_Y2_MASK
#define UV_MASK YUY2_UV_MASK

static inline uint32_t yuy_mix_x(uint32_t a, uint32_t b)
{
  // Mixing two pixels horizontally. Keep Y values but average the UV values.
  uint32_t y1 = (a & Y1_MASK);
  uint32_t y2 = (b & Y2_MASK);
  uint32_t uv = (((a & UV_MASK) / 2) + ((b & UV_MASK) / 2)) & UV_MASK;
  return y1 | y2 | uv;
}

static inline uint32_t yuy_mix_y(uint32_t a, uint32_t b)
{
  // Mixing two pixels vertically. Average all components.
  uint32_t y1 = (((a & Y1_MASK) / 2) + ((b & Y1_MASK) / 2)) & Y1_MASK;
  uint32_t y2 = (((a & Y2_MASK) / 2) + ((b & Y2_MASK) / 2)) & Y2_MASK;
  uint32_t uv = (((a & UV_MASK) / 2) + ((b & UV_MASK) / 2)) & UV_MASK;
  return y1 | y2 | uv;
}

static void xfb_copy_buffer(struct graphics_data *graphics)
{
  struct xfb_render_data *render_data = graphics->render_data;
  u32 pitch = render_data->pitch;
  u32 skip = render_data->skip;
  u32 a, b, c, d;
  int x, y;

  u32 *src = render_data->intermediate_fb;
  u32 *src_b;

  u32 *dest = render_data->xfb[render_data->current_xfb];
  dest += skip / sizeof(u32);

  if(render_data->intermediate_bpp == 32)
  {
    if(render_data->require_240p_mode)
    {
      for(y = 0; y < SCREEN_PIX_H / 2; y++)
      {
        src_b = src + pitch * 2 / sizeof(u32);

        for(x = 0; x < SCREEN_PIX_W / 2; x++)
        {
          a = *(src++);
          b = *(src++);
          c = *(src_b++);
          d = *(src_b++);
          *(dest++) = yuy_mix_y(yuy_mix_x(a, b), yuy_mix_x(c, d));
        }
        src = src_b;
      }
    }
    else
    {
      for(x = 0; x < (SCREEN_PIX_W / 2) * SCREEN_PIX_H; x++)
      {
        a = *(src++);
        b = *(src++);
        *(dest++) = yuy_mix_x(a, b);
      }
    }
  }
  else
  {
    if(render_data->require_240p_mode)
    {
      for(y = 0; y < SCREEN_PIX_H / 2; y++)
      {
        src_b = src + pitch / sizeof(u32);

        for(x = 0; x < SCREEN_PIX_W / 2; x++)
        {
          a = *(src++);
          b = *(src_b++);
          *(dest++) = yuy_mix_y(a, b);
        }
        src = src_b;
      }
    }
    else
    {
      // NOTE: should never, ever happen. Draw directly to the xfb in this case
      memcpy(dest, src, (SCREEN_PIX_W / 2) * SCREEN_PIX_H * sizeof(u32));
    }
  }
}

static void xfb_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct xfb_render_data *render_data = graphics->render_data;

  if(render_data->intermediate_active)
  {
    xfb_copy_buffer(graphics);
    render_data->intermediate_active = false;
  }

  VIDEO_SetNextFramebuffer(render_data->xfb[render_data->current_xfb]);
  VIDEO_Flush();

  // FIXME: input will freeze under load without this, but this can cause
  // slow frames to half the framerate or worse at speed 2. This problem does
  // not seem to affect the GX renderer. Look into possible workarounds.
  VIDEO_WaitVSync();

  render_data->current_xfb ^= 1;
}

const struct renderer renderer_xfb =
{
  "xfb",
  NULL,
  xfb_init_video,
  xfb_free_video,
  xfb_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  xfb_update_colors,
  NULL,
  NULL,
  NULL,
  NULL,
  xfb_render_graph,
  xfb_render_layer,
  xfb_render_cursor,
  NULL,
  xfb_render_mouse,
  xfb_sync_screen,
  NULL,
};

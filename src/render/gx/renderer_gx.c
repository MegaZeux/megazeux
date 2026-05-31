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
#include "../renderers.h"

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

#define DEFAULT_FIFO_SIZE (1024 * 1024)

#define TEX_SCALE_W 1024
#define TEX_SCALE_H 512

#define SCALE_TEX_SIZE (sizeof(u32) * TEX_SCALE_W * TEX_SCALE_H)
#define SCALE_TEX_X0 (0.0 / TEX_SCALE_W)
#define SCALE_TEX_Y0 (1.0 / TEX_SCALE_H)
#define SCALE_TEX_X1 (640.0 / TEX_SCALE_W)
#define SCALE_TEX_Y1 (351.0 / TEX_SCALE_H)

/**
 * Char packing format (I4)
 *
 * 1) Two versions of each char are stored in the texture (MZX and SMZX).
 * 2) Chars are packed into 8x8 tiles, meaning they need to be split into
 *    two tiles when drawn to the charset texture. This also means that in
 *    this texture they occupy 8x16 pixels rather than the usual 8x14 pixels.
 */

#define CHAR_VARIANTS 2
#define CHAR_VAR_W (CHAR_VARIANTS * 8)
#define CHAR_VAR_H 16
#define SMZX_OFFSET CHAR_W

#define CHARSET_COLS 64
#define CHARSET_ROWS (FULL_CHARSET_SIZE / CHARSET_COLS)

#define TEX_DATA_W (CHARSET_COLS * CHAR_VAR_W)
#define TEX_DATA_H (CHARSET_ROWS * CHAR_VAR_H)

#define TEX_DATA_W_F (TEX_DATA_W * 1.0f)
#define TEX_DATA_H_F (TEX_DATA_H * 1.0f)

#define TEX_DATA_SIZE \
 (sizeof(u32) * CHAR_VAR_H * CHAR_VARIANTS * FULL_CHARSET_SIZE)

// (likely wasteful) tlut breakdown:
#define TLUT_MZX_OFFSET   (256 * 0) // Standard MZX
#define TLUT_SMZX_OFFSET  (256 * 1) // SMZX (separate because of SMZX_MESSAGE)
#define TLUT_T0_OFFSET    (256 * 2) // MZX/UI/SMZX tcol is idx 0
#define TLUT_T1_OFFSET    (256 * 3) // MZX/UI/SMZX tcol is idx 1
#define TLUT_T2_OFFSET    (256 * 4) // SMZX tcol is idx 2 / MZX fg + UI bg
#define TLUT_T3_OFFSET    (256 * 5) // SMZX tcol is idx 3 / MZX bg + UI fg
#define TLUT_UI_OFFSET    (256 * 6) // UI
#define NUM_TLUT          (256 * 7)

#define TLUT_MZX_BOTH_TRANSPARENT (-1)

// RGB5A3 transparent color for layer rendering.
#define NO_COLOR 0x0000

// Must be multiple of 32 bytes
struct ci4tlut
{
  u16 pal[16];
};

struct gx_render_data
{
  void *xfb[2];
  void *fifo;
  GXRModeObj *rmode;
  struct ci4tlut *mzxtlut;
  void *charimg;
  void *scaleimg;
  GXTlutObj mzxtlutobj[NUM_TLUT];
  GXTexObj chartex;
  GXTexObj scaletex;
  GXColor palette[FULL_PAL_SIZE];
  u16 tlutpal[FULL_PAL_SIZE];
  u8 chrdirty[FULL_CHARSET_SIZE];
  int chrdirty_all;
  int chrdirty_set;
  int paldirty;
  boolean invalidate;
  int current_xfb;
  s16 sx0, sy0, sx1, sy1;
};

// Lookup table for generating CI4 charset textures
static const u32 mzxtexline[256] =
{
  0x00000000, 0x0000000F, 0x000000F0, 0x000000FF,
  0x00000F00, 0x00000F0F, 0x00000FF0, 0x00000FFF,
  0x0000F000, 0x0000F00F, 0x0000F0F0, 0x0000F0FF,
  0x0000FF00, 0x0000FF0F, 0x0000FFF0, 0x0000FFFF,
  0x000F0000, 0x000F000F, 0x000F00F0, 0x000F00FF,
  0x000F0F00, 0x000F0F0F, 0x000F0FF0, 0x000F0FFF,
  0x000FF000, 0x000FF00F, 0x000FF0F0, 0x000FF0FF,
  0x000FFF00, 0x000FFF0F, 0x000FFFF0, 0x000FFFFF,
  0x00F00000, 0x00F0000F, 0x00F000F0, 0x00F000FF,
  0x00F00F00, 0x00F00F0F, 0x00F00FF0, 0x00F00FFF,
  0x00F0F000, 0x00F0F00F, 0x00F0F0F0, 0x00F0F0FF,
  0x00F0FF00, 0x00F0FF0F, 0x00F0FFF0, 0x00F0FFFF,
  0x00FF0000, 0x00FF000F, 0x00FF00F0, 0x00FF00FF,
  0x00FF0F00, 0x00FF0F0F, 0x00FF0FF0, 0x00FF0FFF,
  0x00FFF000, 0x00FFF00F, 0x00FFF0F0, 0x00FFF0FF,
  0x00FFFF00, 0x00FFFF0F, 0x00FFFFF0, 0x00FFFFFF,
  0x0F000000, 0x0F00000F, 0x0F0000F0, 0x0F0000FF,
  0x0F000F00, 0x0F000F0F, 0x0F000FF0, 0x0F000FFF,
  0x0F00F000, 0x0F00F00F, 0x0F00F0F0, 0x0F00F0FF,
  0x0F00FF00, 0x0F00FF0F, 0x0F00FFF0, 0x0F00FFFF,
  0x0F0F0000, 0x0F0F000F, 0x0F0F00F0, 0x0F0F00FF,
  0x0F0F0F00, 0x0F0F0F0F, 0x0F0F0FF0, 0x0F0F0FFF,
  0x0F0FF000, 0x0F0FF00F, 0x0F0FF0F0, 0x0F0FF0FF,
  0x0F0FFF00, 0x0F0FFF0F, 0x0F0FFFF0, 0x0F0FFFFF,
  0x0FF00000, 0x0FF0000F, 0x0FF000F0, 0x0FF000FF,
  0x0FF00F00, 0x0FF00F0F, 0x0FF00FF0, 0x0FF00FFF,
  0x0FF0F000, 0x0FF0F00F, 0x0FF0F0F0, 0x0FF0F0FF,
  0x0FF0FF00, 0x0FF0FF0F, 0x0FF0FFF0, 0x0FF0FFFF,
  0x0FFF0000, 0x0FFF000F, 0x0FFF00F0, 0x0FFF00FF,
  0x0FFF0F00, 0x0FFF0F0F, 0x0FFF0FF0, 0x0FFF0FFF,
  0x0FFFF000, 0x0FFFF00F, 0x0FFFF0F0, 0x0FFFF0FF,
  0x0FFFFF00, 0x0FFFFF0F, 0x0FFFFFF0, 0x0FFFFFFF,
  0xF0000000, 0xF000000F, 0xF00000F0, 0xF00000FF,
  0xF0000F00, 0xF0000F0F, 0xF0000FF0, 0xF0000FFF,
  0xF000F000, 0xF000F00F, 0xF000F0F0, 0xF000F0FF,
  0xF000FF00, 0xF000FF0F, 0xF000FFF0, 0xF000FFFF,
  0xF00F0000, 0xF00F000F, 0xF00F00F0, 0xF00F00FF,
  0xF00F0F00, 0xF00F0F0F, 0xF00F0FF0, 0xF00F0FFF,
  0xF00FF000, 0xF00FF00F, 0xF00FF0F0, 0xF00FF0FF,
  0xF00FFF00, 0xF00FFF0F, 0xF00FFFF0, 0xF00FFFFF,
  0xF0F00000, 0xF0F0000F, 0xF0F000F0, 0xF0F000FF,
  0xF0F00F00, 0xF0F00F0F, 0xF0F00FF0, 0xF0F00FFF,
  0xF0F0F000, 0xF0F0F00F, 0xF0F0F0F0, 0xF0F0F0FF,
  0xF0F0FF00, 0xF0F0FF0F, 0xF0F0FFF0, 0xF0F0FFFF,
  0xF0FF0000, 0xF0FF000F, 0xF0FF00F0, 0xF0FF00FF,
  0xF0FF0F00, 0xF0FF0F0F, 0xF0FF0FF0, 0xF0FF0FFF,
  0xF0FFF000, 0xF0FFF00F, 0xF0FFF0F0, 0xF0FFF0FF,
  0xF0FFFF00, 0xF0FFFF0F, 0xF0FFFFF0, 0xF0FFFFFF,
  0xFF000000, 0xFF00000F, 0xFF0000F0, 0xFF0000FF,
  0xFF000F00, 0xFF000F0F, 0xFF000FF0, 0xFF000FFF,
  0xFF00F000, 0xFF00F00F, 0xFF00F0F0, 0xFF00F0FF,
  0xFF00FF00, 0xFF00FF0F, 0xFF00FFF0, 0xFF00FFFF,
  0xFF0F0000, 0xFF0F000F, 0xFF0F00F0, 0xFF0F00FF,
  0xFF0F0F00, 0xFF0F0F0F, 0xFF0F0FF0, 0xFF0F0FFF,
  0xFF0FF000, 0xFF0FF00F, 0xFF0FF0F0, 0xFF0FF0FF,
  0xFF0FFF00, 0xFF0FFF0F, 0xFF0FFFF0, 0xFF0FFFFF,
  0xFFF00000, 0xFFF0000F, 0xFFF000F0, 0xFFF000FF,
  0xFFF00F00, 0xFFF00F0F, 0xFFF00FF0, 0xFFF00FFF,
  0xFFF0F000, 0xFFF0F00F, 0xFFF0F0F0, 0xFFF0F0FF,
  0xFFF0FF00, 0xFFF0FF0F, 0xFFF0FFF0, 0xFFF0FFFF,
  0xFFFF0000, 0xFFFF000F, 0xFFFF00F0, 0xFFFF00FF,
  0xFFFF0F00, 0xFFFF0F0F, 0xFFFF0FF0, 0xFFFF0FFF,
  0xFFFFF000, 0xFFFFF00F, 0xFFFFF0F0, 0xFFFFF0FF,
  0xFFFFFF00, 0xFFFFFF0F, 0xFFFFFFF0, 0xFFFFFFFF
};

// Lookup table for generating CI4 SMZX charset textures
static const u32 smzxtexline[256] =
{
  0x00000000, 0x00000055, 0x000000AA, 0x000000FF,
  0x00005500, 0x00005555, 0x000055AA, 0x000055FF,
  0x0000AA00, 0x0000AA55, 0x0000AAAA, 0x0000AAFF,
  0x0000FF00, 0x0000FF55, 0x0000FFAA, 0x0000FFFF,
  0x00550000, 0x00550055, 0x005500AA, 0x005500FF,
  0x00555500, 0x00555555, 0x005555AA, 0x005555FF,
  0x0055AA00, 0x0055AA55, 0x0055AAAA, 0x0055AAFF,
  0x0055FF00, 0x0055FF55, 0x0055FFAA, 0x0055FFFF,
  0x00AA0000, 0x00AA0055, 0x00AA00AA, 0x00AA00FF,
  0x00AA5500, 0x00AA5555, 0x00AA55AA, 0x00AA55FF,
  0x00AAAA00, 0x00AAAA55, 0x00AAAAAA, 0x00AAAAFF,
  0x00AAFF00, 0x00AAFF55, 0x00AAFFAA, 0x00AAFFFF,
  0x00FF0000, 0x00FF0055, 0x00FF00AA, 0x00FF00FF,
  0x00FF5500, 0x00FF5555, 0x00FF55AA, 0x00FF55FF,
  0x00FFAA00, 0x00FFAA55, 0x00FFAAAA, 0x00FFAAFF,
  0x00FFFF00, 0x00FFFF55, 0x00FFFFAA, 0x00FFFFFF,
  0x55000000, 0x55000055, 0x550000AA, 0x550000FF,
  0x55005500, 0x55005555, 0x550055AA, 0x550055FF,
  0x5500AA00, 0x5500AA55, 0x5500AAAA, 0x5500AAFF,
  0x5500FF00, 0x5500FF55, 0x5500FFAA, 0x5500FFFF,
  0x55550000, 0x55550055, 0x555500AA, 0x555500FF,
  0x55555500, 0x55555555, 0x555555AA, 0x555555FF,
  0x5555AA00, 0x5555AA55, 0x5555AAAA, 0x5555AAFF,
  0x5555FF00, 0x5555FF55, 0x5555FFAA, 0x5555FFFF,
  0x55AA0000, 0x55AA0055, 0x55AA00AA, 0x55AA00FF,
  0x55AA5500, 0x55AA5555, 0x55AA55AA, 0x55AA55FF,
  0x55AAAA00, 0x55AAAA55, 0x55AAAAAA, 0x55AAAAFF,
  0x55AAFF00, 0x55AAFF55, 0x55AAFFAA, 0x55AAFFFF,
  0x55FF0000, 0x55FF0055, 0x55FF00AA, 0x55FF00FF,
  0x55FF5500, 0x55FF5555, 0x55FF55AA, 0x55FF55FF,
  0x55FFAA00, 0x55FFAA55, 0x55FFAAAA, 0x55FFAAFF,
  0x55FFFF00, 0x55FFFF55, 0x55FFFFAA, 0x55FFFFFF,
  0xAA000000, 0xAA000055, 0xAA0000AA, 0xAA0000FF,
  0xAA005500, 0xAA005555, 0xAA0055AA, 0xAA0055FF,
  0xAA00AA00, 0xAA00AA55, 0xAA00AAAA, 0xAA00AAFF,
  0xAA00FF00, 0xAA00FF55, 0xAA00FFAA, 0xAA00FFFF,
  0xAA550000, 0xAA550055, 0xAA5500AA, 0xAA5500FF,
  0xAA555500, 0xAA555555, 0xAA5555AA, 0xAA5555FF,
  0xAA55AA00, 0xAA55AA55, 0xAA55AAAA, 0xAA55AAFF,
  0xAA55FF00, 0xAA55FF55, 0xAA55FFAA, 0xAA55FFFF,
  0xAAAA0000, 0xAAAA0055, 0xAAAA00AA, 0xAAAA00FF,
  0xAAAA5500, 0xAAAA5555, 0xAAAA55AA, 0xAAAA55FF,
  0xAAAAAA00, 0xAAAAAA55, 0xAAAAAAAA, 0xAAAAAAFF,
  0xAAAAFF00, 0xAAAAFF55, 0xAAAAFFAA, 0xAAAAFFFF,
  0xAAFF0000, 0xAAFF0055, 0xAAFF00AA, 0xAAFF00FF,
  0xAAFF5500, 0xAAFF5555, 0xAAFF55AA, 0xAAFF55FF,
  0xAAFFAA00, 0xAAFFAA55, 0xAAFFAAAA, 0xAAFFAAFF,
  0xAAFFFF00, 0xAAFFFF55, 0xAAFFFFAA, 0xAAFFFFFF,
  0xFF000000, 0xFF000055, 0xFF0000AA, 0xFF0000FF,
  0xFF005500, 0xFF005555, 0xFF0055AA, 0xFF0055FF,
  0xFF00AA00, 0xFF00AA55, 0xFF00AAAA, 0xFF00AAFF,
  0xFF00FF00, 0xFF00FF55, 0xFF00FFAA, 0xFF00FFFF,
  0xFF550000, 0xFF550055, 0xFF5500AA, 0xFF5500FF,
  0xFF555500, 0xFF555555, 0xFF5555AA, 0xFF5555FF,
  0xFF55AA00, 0xFF55AA55, 0xFF55AAAA, 0xFF55AAFF,
  0xFF55FF00, 0xFF55FF55, 0xFF55FFAA, 0xFF55FFFF,
  0xFFAA0000, 0xFFAA0055, 0xFFAA00AA, 0xFFAA00FF,
  0xFFAA5500, 0xFFAA5555, 0xFFAA55AA, 0xFFAA55FF,
  0xFFAAAA00, 0xFFAAAA55, 0xFFAAAAAA, 0xFFAAAAFF,
  0xFFAAFF00, 0xFFAAFF55, 0xFFAAFFAA, 0xFFAAFFFF,
  0xFFFF0000, 0xFFFF0055, 0xFFFF00AA, 0xFFFF00FF,
  0xFFFF5500, 0xFFFF5555, 0xFFFF55AA, 0xFFFF55FF,
  0xFFFFAA00, 0xFFFFAA55, 0xFFFFAAAA, 0xFFFFAAFF,
  0xFFFFFF00, 0xFFFFFF55, 0xFFFFFFAA, 0xFFFFFFFF
};

static boolean gx_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  const GXColor black = {0, 0, 0, 255};

  struct gx_render_data *render_data;
  f32 yscale;
  u32 xfbHeight;
  Mtx identmtx;
  GXRModeObj *rmode;
  Mtx44 projmtx;
  u16 sw, sh;
  int i;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  render_data = cmalloc(sizeof(struct gx_render_data));
  graphics->render_data = render_data;
  graphics->ratio = conf->video_ratio;
  graphics->gl_vsync = conf->gl_vsync;
  graphics->bits_per_pixel = 16;

  VIDEO_Init();

  rmode = render_data->rmode = VIDEO_GetPreferredMode(NULL);
  render_data->xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  render_data->current_xfb = 0;

  render_data->charimg = memalign(32, TEX_DATA_SIZE);
  memset(render_data->charimg, 0, TEX_DATA_SIZE);
  render_data->scaleimg = memalign(32, SCALE_TEX_SIZE);
  memset(render_data->scaleimg, 0, SCALE_TEX_SIZE);
  render_data->mzxtlut = memalign(32, sizeof(struct ci4tlut) * NUM_TLUT);
  memset(render_data->mzxtlut, 0, sizeof(struct ci4tlut) * NUM_TLUT);

  for(i = 0; i < NUM_TLUT; i++)
    GX_InitTlutObj(render_data->mzxtlutobj + i, render_data->mzxtlut + i,
     GX_TL_RGB5A3, 16);

  GX_InitTexObjCI(&render_data->chartex,
   render_data->charimg, TEX_DATA_W, TEX_DATA_H,
   GX_TF_CI4, GX_REPEAT, GX_REPEAT, GX_FALSE, GX_TLUT0);
  GX_InitTexObjLOD(&render_data->chartex, GX_NEAR, GX_NEAR, 0, 0, 0, GX_FALSE,
   GX_TRUE, GX_ANISO_1);

  GX_InitTexObj(&render_data->scaletex,
   render_data->scaleimg, TEX_SCALE_W, TEX_SCALE_H,
   GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);

  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(render_data->xfb[0]);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if(rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync();

  render_data->fifo = memalign(32, DEFAULT_FIFO_SIZE);
  memset(render_data->fifo, 0, DEFAULT_FIFO_SIZE);
  GX_Init(render_data->fifo, DEFAULT_FIFO_SIZE);

  yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
  xfbHeight = GX_SetDispCopyYScale(yscale);
  sw = ((rmode->fbWidth > 640) ? rmode->fbWidth : 640);
  sh = ((rmode->efbHeight > 352) ? rmode->efbHeight : 352);
  GX_SetScissor(0, 0, sw, sh);
  GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
  GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
  GX_SetFieldMode(rmode->field_rendering,
   ((rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE));
  if(rmode->aa)
    GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  else
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  GX_SetDispCopyGamma(GX_GM_1_0);
  GX_SetCopyClear(black, GX_MAX_Z24);
  GX_SetTexCopySrc(0, 0, 640, 352);
  GX_SetTexCopyDst(1024, 512, GX_TF_RGBA8, GX_FALSE);

  GX_ClearVtxDesc();
  GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
  GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  GX_SetNumChans(1);
  GX_SetNumTexGens(1);
  GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHTNULL,
   GX_DF_NONE, GX_AF_NONE);
  GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
  GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  guMtxIdentity(identmtx);
  GX_LoadPosMtxImm(identmtx, GX_PNMTX0);

  GX_SetViewport(0, 0, sw, sh, 0, 1);
  GX_SetAlphaUpdate(GX_TRUE);
  GX_SetCullMode(GX_CULL_NONE);

  GX_CopyDisp(render_data->xfb[1], GX_TRUE);
  GX_CopyDisp(render_data->xfb[0], GX_FALSE);
  GX_Flush();

  render_data->chrdirty_set = 1;
  render_data->chrdirty_all = 1;
  render_data->paldirty = 1;

  guOrtho(projmtx, -1, sh - 1, 0, sw, -1.0, 1.0);
  GX_LoadProjectionMtx(projmtx, GX_ORTHOGRAPHIC);
  return true;
}

static void gx_free_video(struct graphics_data *graphics)
{
  free(graphics->render_data);
  graphics->render_data = NULL;
}

static boolean gx_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct gx_render_data *render_data = graphics->render_data;
  float x, y, w, h, scale, xscale, yscale;
  int fh;

  if(window->is_fullscreen)
    scale = 8.0 / 9.0;
  else
    scale = 8.0 / 10.0;

  if(render_data->rmode->viTVMode == VI_TVMODE_PAL_INT)
    fh = 574;
  else
    fh = 480;

  if(CONF_GetAspectRatio() == CONF_ASPECT_16_9)
  {
    // 4:3 stretched to 16:9; correct ratio for non-square pixels.
    window->ratio_numerator *= 3;
    window->ratio_denominator *= 4;
  }

  window->width_px = 720;
  window->height_px = fh;
  video_window_update_viewport(window);

  if(render_data->rmode->viWidth > render_data->rmode->fbWidth)
    xscale = (float)render_data->rmode->fbWidth / render_data->rmode->viWidth;
  else
    xscale = 1.0;
  yscale = (float)render_data->rmode->efbHeight / render_data->rmode->viHeight;

  w = window->viewport_width * scale;
  h = window->viewport_height * scale;
  x = (window->width_px - w) / 2 - render_data->rmode->viXOrigin;
  y = (window->height_px - h) / 2 - render_data->rmode->viYOrigin;
  w *= xscale; h *= yscale; x *= xscale; y *= yscale;

  render_data->sx0 = x;
  render_data->sy0 = y + 1;
  render_data->sx1 = x + w;
  render_data->sy1 = y + h + 1;

  return true;
}

static void gx_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct gx_render_data *render_data = graphics->render_data;
  unsigned int i;

  render_data->paldirty = 1;
  for(i = 0; i < count; i++)
  {
    // 32-bit color palette (used for cursor)
    render_data->palette[i].r = palette[i].r;
    render_data->palette[i].g = palette[i].g;
    render_data->palette[i].b = palette[i].b;
    render_data->palette[i].a = 255;

    // RGB5A3 (1 RRRRR GGGGG BBBBB) or (0 RRRR GGGG BBBB AAA)
    render_data->tlutpal[i] = 0x8000 |
     (palette[i].b >> 3)             |
     ((palette[i].g & 0xF8) << 2)    |
     ((palette[i].r & 0xF8) << 7);
  }
}

static void gx_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  struct gx_render_data *render_data = graphics->render_data;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  if(count <= 256)
    memset(render_data->chrdirty + first, 1, count);

  else
    render_data->chrdirty_all = 1;

  render_data->chrdirty_set = 1;
}

static void gx_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct gx_render_data *render_data = graphics->render_data;
  render_data->chrdirty[chr] = 1;
  render_data->chrdirty_set = 1;
}

static void gx_remap_charbyte(struct graphics_data *graphics,
 uint16_t chr, uint8_t byte)
{
  struct gx_render_data *render_data = graphics->render_data;
  render_data->chrdirty[chr] = 1;
  render_data->chrdirty_set = 1;
}

/**
 * Draw a single char (MZX and SMZX) to the charset texture.
 * Because the texture is stored in 8x8 blocks, we need to draw the first
 * eight lines of the char, then skip ahead to the next row in the texture.
 */

static void gx_draw_char(struct graphics_data *graphics, uint16_t chr)
{
  struct gx_render_data *render_data = graphics->render_data;
  uint8_t *src = graphics->charset;
  u32 *dest = render_data->charimg;
  int byte;

  src += chr * CHAR_SIZE;

  dest += (chr / CHARSET_COLS) * CHAR_VAR_W * CHARSET_COLS * 2;
  dest += (chr % CHARSET_COLS) * CHAR_VAR_W;

  for(byte = 0; byte < 8; byte++, src++, dest++)
    *dest = mzxtexline[*src];

  src -= 8;
  for(byte = 0; byte < 8; byte++, src++, dest++)
    *dest = smzxtexline[*src];

  dest += (CHARSET_COLS - 1) * CHAR_VAR_W;

  for(byte = 0; byte < 6; byte++, src++, dest++)
    *dest = mzxtexline[*src];

  src -= 6;
  dest += 2;
  for(byte = 0; byte < 6; byte++, src++, dest++)
    *dest = smzxtexline[*src];
}

/**
 * Redraw the entire charset texture.
 * Because chars are stored in 8x8 blocks, the first 8 lines of each char in
 * a row need to be drawn and then the last 6 lines need to be drawn in the
 * next row of 8x8 blocks.
 */

static void gx_draw_charsets(struct graphics_data *graphics)
{
  struct gx_render_data *render_data = graphics->render_data;
  const uint8_t *src = graphics->charset;
  u32 *dest = render_data->charimg;
  int x;
  int y;
  int byte;

  for(y = 0; y < CHARSET_ROWS; y++)
  {
    for(x = 0; x < CHARSET_COLS; x++)
    {
      for(byte = 0; byte < 8; byte++, src++, dest++)
        *dest = mzxtexline[*src];

      src -= 8;
      for(byte = 0; byte < 8; byte++, src++, dest++)
        *dest = smzxtexline[*src];

      src += 6;
    }

    src -= CHARSET_COLS * CHAR_SIZE;

    for(x = 0; x < CHARSET_COLS; x++)
    {
      src += 8;
      for(byte = 0; byte < 6; byte++, src++, dest++)
        *dest = mzxtexline[*src];

      src -= 6;
      dest += 2;
      for(byte = 0; byte < 6; byte++, src++, dest++)
        *dest = smzxtexline[*src];

      dest += 2;
    }
  }
}

static void gx_check_remap_chars(struct graphics_data *graphics)
{
  struct gx_render_data *render_data = graphics->render_data;
  int i;

  if(!render_data->chrdirty_set)
    return;

  if(render_data->chrdirty_all)
  {
    gx_draw_charsets(graphics);
    render_data->chrdirty_all = false;
    memset(render_data->chrdirty, 0, FULL_CHARSET_SIZE);
    render_data->invalidate = true;
  }
  else
  {
    for(i = 0; i < FULL_CHARSET_SIZE; i++)
    {
      if(render_data->chrdirty[i])
      {
        gx_draw_char(graphics, i);
        render_data->chrdirty[i] = 0;
        render_data->invalidate = true;
      }
    }
  }

  render_data->chrdirty_set = 0;
}

static void gx_check_remap_palettes(struct graphics_data *graphics)
{
  struct gx_render_data *render_data = graphics->render_data;
  int i;

  if(render_data->paldirty)
  {
    for(i = 0; i < NUM_TLUT; i++)
      render_data->mzxtlut[i].pal[1] = 0;

    render_data->paldirty = 0;
    render_data->invalidate = true;
  }
}

static int gx_get_tlut_id_mzx(struct graphics_data *graphics,
 struct video_layer *layer, uint8_t bg_color, uint8_t fg_color)
{
  int tcol = layer->transparent_col;
  int tlut_id;

  if((tcol == (int)bg_color) && (tcol == (int)fg_color))
    return TLUT_MZX_BOTH_TRANSPARENT;

  if(tcol == (int)bg_color)
    return fg_color + TLUT_T0_OFFSET;

  if(tcol == (int)fg_color)
    return bg_color + TLUT_T1_OFFSET;

  tlut_id = ((bg_color & 0xF) << 4) | (fg_color & 0xF);

  if(bg_color >= 16 && fg_color >= 16)
    return tlut_id + TLUT_UI_OFFSET;

  // Mixed UI/game colors only occur during normal MZX mode, so it should
  // be safe to reuse the SMZX TCOL mappings for these.
  else
  if(bg_color >= 16)
    return tlut_id + TLUT_T2_OFFSET;

  else
  if(fg_color >= 16)
    return tlut_id + TLUT_T3_OFFSET;

  return tlut_id + TLUT_MZX_OFFSET;
}

static int gx_get_tlut_id_smzx(struct graphics_data *graphics,
 struct video_layer *layer, uint8_t bg_color, uint8_t fg_color)
{
  int palette_id = ((bg_color & 0xF) << 4) | (fg_color & 0xF);
  int idx0 = graphics->smzx_indices[palette_id * 4 + 0];
  int idx1 = graphics->smzx_indices[palette_id * 4 + 1];
  int idx2 = graphics->smzx_indices[palette_id * 4 + 2];
  int idx3 = graphics->smzx_indices[palette_id * 4 + 3];
  int tcol = layer->transparent_col;

  if(tcol == idx0)
    return palette_id + TLUT_T0_OFFSET;

  if(tcol == idx1)
    return palette_id + TLUT_T1_OFFSET;

  if(tcol == idx2)
    return palette_id + TLUT_T2_OFFSET;

  if(tcol == idx3)
    return palette_id + TLUT_T3_OFFSET;

  return palette_id + TLUT_SMZX_OFFSET;
}

static void gx_set_tlut_mzx(struct graphics_data *graphics,
 struct video_layer *layer, struct ci4tlut *tlut, int bg_color, int fg_color)
{
  struct gx_render_data *render_data = graphics->render_data;
  int tcol = layer->transparent_col;

  if(bg_color >= 16)
    bg_color = graphics->protected_pal_position + (bg_color & 0xF);
  else
    bg_color &= 0xF;

  if(fg_color >= 16)
    fg_color = graphics->protected_pal_position + (fg_color & 0xF);
  else
    fg_color &= 0xF;

  tlut->pal[0] =  bg_color != tcol ? render_data->tlutpal[bg_color] : NO_COLOR;
  tlut->pal[15] = fg_color != tcol ? render_data->tlutpal[fg_color] : NO_COLOR;
  tlut->pal[1] = 0xFFFF;
}

static void gx_set_tlut_smzx(struct graphics_data *graphics,
 struct video_layer *layer, struct ci4tlut *tlut, int bg_color, int fg_color)
{
  struct gx_render_data *render_data = graphics->render_data;
  int palette_id = ((bg_color & 0xF) << 4) | (fg_color & 0xF);
  int idx0 = graphics->smzx_indices[palette_id * 4 + 0];
  int idx1 = graphics->smzx_indices[palette_id * 4 + 1];
  int idx2 = graphics->smzx_indices[palette_id * 4 + 2];
  int idx3 = graphics->smzx_indices[palette_id * 4 + 3];
  int tcol = layer->transparent_col;

  tlut->pal[0] =  idx0 != tcol ? render_data->tlutpal[idx0] : NO_COLOR;
  tlut->pal[5] =  idx1 != tcol ? render_data->tlutpal[idx1] : NO_COLOR;
  tlut->pal[10] = idx2 != tcol ? render_data->tlutpal[idx2] : NO_COLOR;
  tlut->pal[15] = idx3 != tcol ? render_data->tlutpal[idx3] : NO_COLOR;
  tlut->pal[1] = 0xFFFF;
}


static uint16_t gx_get_char_value(struct video_layer *layer, uint16_t char_value)
{
  if(char_value == INVISIBLE_CHAR)
    return INVISIBLE_CHAR;

  if(char_value > 0xFF)
    return (char_value & 0xFF) + PROTECTED_CHARSET_POSITION;

  return (layer->offset + char_value) % PROTECTED_CHARSET_POSITION;
}

static void gx_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct gx_render_data *render_data = graphics->render_data;
  struct char_element *src = layer->data;
  struct ci4tlut *tlut;
  int last_tlut_id;
  int cur_tlut_id;
  uint16_t char_value;
  uint8_t bg_color;
  uint8_t fg_color;

  unsigned int x, y;
  int x1, x2, y1, y2;
  float u, v;
  float u2, v2;

  render_data->invalidate = false;

  gx_check_remap_chars(graphics);

  if(render_data->invalidate)
  {
    DCFlushRange(render_data->charimg, TEX_DATA_SIZE);
    GX_InvalidateTexAll();
    render_data->invalidate = false;
  }

  gx_check_remap_palettes(graphics);

  if(render_data->invalidate)
    GX_InvalidateTexAll();

  GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

  GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

  last_tlut_id = -1;

  if(!layer->mode)
  {
    for(y = 0; y < layer->h; y++)
    {
      for(x = 0; x < layer->w; x++, src++)
      {
        char_value = gx_get_char_value(layer, src->char_value);
        if(char_value == INVISIBLE_CHAR)
          continue;

        bg_color = src->bg_color;
        fg_color = src->fg_color;
        cur_tlut_id = gx_get_tlut_id_mzx(graphics, layer, bg_color, fg_color);

        if(cur_tlut_id == TLUT_MZX_BOTH_TRANSPARENT)
          continue;

        if(cur_tlut_id != last_tlut_id)
        {
          tlut = &(render_data->mzxtlut[cur_tlut_id]);

          if(!tlut->pal[1])
          {
            gx_set_tlut_mzx(graphics, layer, tlut, bg_color, fg_color);
            DCFlushRange(tlut, sizeof(struct ci4tlut));
          }
          GX_LoadTlut(&render_data->mzxtlutobj[cur_tlut_id], GX_TLUT0);
          GX_LoadTexObj(&render_data->chartex, GX_TEXMAP0);
          last_tlut_id = cur_tlut_id;
        }

        u = (char_value % CHARSET_COLS) * CHAR_VAR_W / TEX_DATA_W_F;
        v = (char_value / CHARSET_COLS) * CHAR_VAR_H / TEX_DATA_H_F;
        u2 = u + CHAR_W / TEX_DATA_W_F;
        v2 = v + CHAR_H / TEX_DATA_H_F;

        x1 = (int)x * CHAR_W + layer->x;
        y1 = (int)y * CHAR_H + layer->y;
        x2 = ((int)x + 1) * CHAR_W + layer->x;
        y2 = ((int)y + 1) * CHAR_H + layer->y;

        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
          GX_Position2s16(x1, y1);
          GX_TexCoord2f32(u,  v );
          GX_Position2s16(x2, y1);
          GX_TexCoord2f32(u2, v );
          GX_Position2s16(x2, y2);
          GX_TexCoord2f32(u2, v2);
          GX_Position2s16(x1, y2);
          GX_TexCoord2f32(u,  v2);
        GX_End();
      }
    }
  }
  else
  {
    for(y = 0; y < layer->h; y++)
    {
      for(x = 0; x < layer->w; x++, src++)
      {
        char_value = gx_get_char_value(layer, src->char_value);
        if(char_value == INVISIBLE_CHAR)
          continue;

        bg_color = src->bg_color;
        fg_color = src->fg_color;
        cur_tlut_id = gx_get_tlut_id_smzx(graphics, layer, bg_color, fg_color);

        if(cur_tlut_id != last_tlut_id)
        {
          tlut = &(render_data->mzxtlut[cur_tlut_id]);

          if(!tlut->pal[1])
          {
            gx_set_tlut_smzx(graphics, layer, tlut, bg_color, fg_color);
            DCFlushRange(tlut, sizeof(struct ci4tlut));
          }
          GX_LoadTlut(&render_data->mzxtlutobj[cur_tlut_id], GX_TLUT0);
          GX_LoadTexObj(&render_data->chartex, GX_TEXMAP0);
          last_tlut_id = cur_tlut_id;
        }

        u = (char_value % CHARSET_COLS) * CHAR_VAR_W / TEX_DATA_W_F;
        v = (char_value / CHARSET_COLS) * CHAR_VAR_H / TEX_DATA_H_F;
        u += SMZX_OFFSET / TEX_DATA_W_F;
        u2 = u + CHAR_W / TEX_DATA_W_F;
        v2 = v + CHAR_H / TEX_DATA_H_F;

        x1 = (int)x * CHAR_W + layer->x;
        y1 = (int)y * CHAR_H + layer->y;
        x2 = ((int)x + 1) * CHAR_W + layer->x;
        y2 = ((int)y + 1) * CHAR_H + layer->y;

        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
          GX_Position2s16(x1, y1);
          GX_TexCoord2f32(u,  v );
          GX_Position2s16(x2, y1);
          GX_TexCoord2f32(u2, v );
          GX_Position2s16(x2, y2);
          GX_TexCoord2f32(u2, v2);
          GX_Position2s16(x1, y2);
          GX_TexCoord2f32(u,  v2);
        GX_End();
      }
    }
  }

  GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

static void gx_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct gx_render_data *render_data = graphics->render_data;
  GXColor *pal = render_data->palette;

  GX_SetChanMatColor(GX_COLOR0A0, pal[color & 0xFF]);
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x * 8, y * 14 + offset);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x * 8 + 8, y * 14 + offset);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x * 8 + 8, y * 14 + offset + lines);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x * 8, y * 14 + offset + lines);
    GX_TexCoord2f32(0, 0);
  GX_End();
}

static void gx_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  const GXColor white = {255, 255, 255, 255};

  GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTCLR, GX_BL_ZERO, GX_LO_CLEAR);

  GX_SetChanMatColor(GX_COLOR0A0, white);
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(x, y);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x + w, y);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x + w, y + h);
    GX_TexCoord2f32(0, 0);
    GX_Position2s16(x, y + h);
    GX_TexCoord2f32(0, 0);
  GX_End();
}

static void gx_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct gx_render_data *render_data = graphics->render_data;
  static u8 tex_ptn[12][2] = {
    {6, 6}, {6, 6}, {6, 6},
    {6, 6}, {6, 6}, {6, 6},
    {6, 6}, {6, 6}, {6, 6},
    {6, 6}, {6, 6}, {6, 6},
  };
  static u8 tex_vf[7] = {0, 0, 21, 22, 21, 0, 0};
  GX_SetCopyFilter(GX_FALSE, tex_ptn, GX_FALSE, tex_vf);
  GX_CopyTex(render_data->scaleimg, GX_TRUE);
  GX_Flush();
  GX_PixModeSync();
  GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
  GX_LoadTexObj(&render_data->scaletex, GX_TEXMAP0);
  GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2s16(render_data->sx0, render_data->sy0);
    GX_TexCoord2f32(SCALE_TEX_X0, SCALE_TEX_Y0);
    GX_Position2s16(render_data->sx1, render_data->sy0);
    GX_TexCoord2f32(SCALE_TEX_X1, SCALE_TEX_Y0);
    GX_Position2s16(render_data->sx1, render_data->sy1);
    GX_TexCoord2f32(SCALE_TEX_X1, SCALE_TEX_Y1);
    GX_Position2s16(render_data->sx0, render_data->sy1);
    GX_TexCoord2f32(SCALE_TEX_X0, SCALE_TEX_Y1);
  GX_End();
  GX_DrawDone();
  GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GX_SetColorUpdate(GX_TRUE);
  GX_SetCopyFilter(render_data->rmode->aa, render_data->rmode->sample_pattern,
    GX_TRUE, render_data->rmode->vfilter);
  GX_CopyDisp(render_data->xfb[render_data->current_xfb], GX_TRUE);
  GX_Flush();
  VIDEO_SetNextFramebuffer(render_data->xfb[render_data->current_xfb]);
  VIDEO_Flush();

  if(graphics->gl_vsync)
    VIDEO_WaitVSync();

  render_data->current_xfb ^= 1;
}

const struct renderer renderer_gx =
{
  "gx",
  NULL,
  gx_init_video,
  gx_free_video,
  gx_create_window,
  gx_create_window,
  NULL,
  set_window_viewport_scaled,
  NULL,
  NULL,
  NULL,
  gx_update_colors,
  gx_remap_char_range,
  gx_remap_char,
  gx_remap_charbyte,
  NULL,
  NULL,
  gx_render_layer,
  gx_render_cursor,
  NULL,
  gx_render_mouse,
  gx_sync_screen,
  NULL,
};

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

// This file recursively includes itself to apply a sort of pseudo-templating
// in order to generate many versions of the render_func_... function. The
// right function to call is selected through a dispatch mechanism, also
// generated through recursive inclusion.

// Errors and warnings in this file are duplicated hundreds of times over, so
// when working on this function it can be good to redirect make's stderr
// stream to a file.

#ifndef RENDERER_BPP

#include "../algorithm/arch.h"

#define CONCAT(a,b,c,d,e,f,g,h,i,j,k,l,m) \
 a ## b ## c ## d ## e ## f ## g ## h ## i ## j ## k ## l ## m
#define RENDER_FUNCTION_NAME(b, t, a, s, c, p) \
 CONCAT(render_func_, b, bpp_, t, trans_, a, align_, s, smzx_, c, clip_, p, ppal)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/**
 * Renderer bits-per-pixel (8, 16, or 32).
 * Sets of renderers for lower bpps are larger since they try to support more
 * alignment options, so several platforms disable them altogether to reduce
 * executable size and/or compilation time.
 */

#ifndef SKIP_8BPP
#define RENDERER_BPP  8
#include "render_layer_code.h"
#undef RENDERER_BPP
#endif

#ifndef SKIP_16BPP
#define RENDERER_BPP  16
#include "render_layer_code.h"
#undef RENDERER_BPP
#endif

#ifndef SKIP_32BPP
#define RENDERER_BPP  32
#include "render_layer_code.h"
#undef RENDERER_BPP
#endif

static inline void RENDER_FUNCTION_NAME(X, X, X, X, X, X) (void *pixels,
 Uint32 pitch, struct graphics_data *graphics, struct video_layer *layer,
 int ppal, int clip, int smzx, int align, int trans, int bpp)
{
  switch(bpp)
  {
#ifndef SKIP_8BPP
    case 8:
      RENDER_FUNCTION_NAME(8, X, X, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx, align, trans);
      break;
#endif
#ifndef SKIP_16BPP
    case 16:
      RENDER_FUNCTION_NAME(16, X, X, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx, align, trans);
      break;
#endif
#ifndef SKIP_32BPP
    case 32:
      RENDER_FUNCTION_NAME(32, X, X, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx, align, trans);
      break;
#endif
    default:
      fprintf(stderr, "INVALID RENDERER ARG bpp=%d\n"
       "(is this bpp enabled for this platform?)\n", bpp);
      exit(1);
      break;
  }
}

#else /* RENDERER_BPP */

/**
 * Layer transparency enabled (1) or disabled (0).
 */

#ifndef RENDERER_TR

#define RENDERER_TR  0
#include "render_layer_code.h"
#undef RENDERER_TR

#define RENDERER_TR  1
#include "render_layer_code.h"
#undef RENDERER_TR

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, X, X, X, X, X)
 (void *pixels, Uint32 pitch, struct graphics_data *graphics,
 struct video_layer *layer, int ppal, int clip, int smzx, int align, int trans)
{
  switch(trans)
  {
    case 0:
      RENDER_FUNCTION_NAME(RENDERER_BPP, 0, X, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx, align);
      break;

    case 1:
      RENDER_FUNCTION_NAME(RENDERER_BPP, 1, X, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx, align);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG trans=%d\n", trans);
      exit(1);
      break;
  }
}

#else /* RENDERER_TR */

/**
 * Alignment of pixel buffer.
 * This always must be >= the current renderer's bits-per-pixel.
 */

#ifndef RENDERER_ALIGN

#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
#define RENDERER_ALIGN  64
#include "render_layer_code.h"
#undef RENDERER_ALIGN
#endif /* ARCHITECTURE_BITS >= 64 */

#if RENDERER_BPP <= 32
#define RENDERER_ALIGN  32
#include "render_layer_code.h"
#undef RENDERER_ALIGN
#if RENDERER_BPP <= 16
#define RENDERER_ALIGN  16
#include "render_layer_code.h"
#undef RENDERER_ALIGN
#if RENDERER_BPP <= 8
#define RENDERER_ALIGN  8
#include "render_layer_code.h"
#undef RENDERER_ALIGN
#endif /* RENDERER_BPP <= 8 */
#endif /* RENDERER_BPP <= 16 */
#endif /* RENDERER_BPP <= 32 */

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, X, X, X, X)
 (void *pixels, Uint32 pitch, struct graphics_data *graphics,
 struct video_layer *layer, int ppal, int clip, int smzx, int align)
{
  switch(align)
  {
#if ARCHITECTURE_BITS >= 64 && !defined(SKIP_64_ALIGN)
    case 64:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, 64, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx);
      break;
#endif /* ARCHITECTURE_BITS >= 64 */

#if RENDERER_BPP <= 32
    case 32:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, 32, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx);
      break;

#if RENDERER_BPP <= 16
    case 16:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, 16, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx);
      break;

#if RENDERER_BPP <= 8
    case 8:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, 8, X, X, X)
       (pixels, pitch, graphics, layer, ppal, clip, smzx);
      break;
#endif /* RENDERER_BPP <= 8 */
#endif /* RENDERER_BPP <= 16 */
#endif /* RENDERER_BPP <= 32 */

    default:
      fprintf(stderr, "INVALID RENDERER ARG align=%d bpp=%d\n",
       align, RENDERER_BPP);
      exit(1);
      break;
  }
}

#else /* RENDERER_ALIGN */

/**
 * Renderer is SMZX (1) or normal MZX (0).
 */

#ifndef RENDERER_SMZX

#define RENDERER_SMZX  0
#include "render_layer_code.h"
#undef RENDERER_SMZX

#define RENDERER_SMZX  1
#include "render_layer_code.h"
#undef RENDERER_SMZX

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, X, X, X)
 (void *pixels, Uint32 pitch, struct graphics_data *graphics,
 struct video_layer *layer, int ppal, int clip, int smzx)
{
  switch(smzx)
  {
    case 0:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, 0, X, X)
       (pixels, pitch, graphics, layer, ppal, clip);
      break;

    case 1:
    case 2:
    case 3:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, 1, X, X)
       (pixels, pitch, graphics, layer, ppal, clip);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG smzx=%d\n", smzx);
      exit(1);
      break;
  }
}

#else /* RENDERER_SMZX */

/**
 * Renderer should clip the layer at the screen boundaries (1) or not (0).
 */

#ifndef RENDERER_CLIP

#define RENDERER_CLIP  0
#include "render_layer_code.h"
#undef RENDERER_CLIP

#define RENDERER_CLIP  1
#include "render_layer_code.h"
#undef RENDERER_CLIP

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, X, X)
 (void *pixels, Uint32 pitch, struct graphics_data *graphics,
 struct video_layer *layer, int ppal, int clip)
{
  switch(clip)
  {
    case 0:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, 0, X)
       (pixels, pitch, graphics, layer, ppal);
      break;

    case 1:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, 1, X)
       (pixels, pitch, graphics, layer, ppal);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG clip=%d\n", clip);
      exit(1);
      break;
  }
}

#else /* RENDERER_CLIP */

/**
 * Protected palette offset is 16 or 256.
 * This doesn't necessarily correspond to the current SMZX mode, since layers
 * can be drawn in normal mode while SMZX is technically enabled.
 */

#ifndef RENDERER_PPAL

#define RENDERER_PPAL  256
#include "render_layer_code.h"
#undef RENDERER_PPAL

#define RENDERER_PPAL  16
#include "render_layer_code.h"
#undef RENDERER_PPAL

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, RENDERER_CLIP, X)
 (void *pixels, Uint32 pitch, struct graphics_data *graphics,
 struct video_layer *layer, int ppal)
{
  switch(ppal)
  {
    case 256:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, RENDERER_CLIP, 256)
       (pixels, pitch, graphics, layer);
      break;

    case 16:
      RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, RENDERER_CLIP, 16)
       (pixels, pitch, graphics, layer);
      break;

    default:
      fprintf(stderr, "INVALID RENDERER ARG ppal=%d\n", ppal);
      exit(1);
      break;
  }
}

#else /* RENDERER_PPAL */

#if RENDERER_BPP == 32
#define PIXTYPE Uint32
#elif RENDERER_BPP == 16
#define PIXTYPE Uint16
#else
#define PIXTYPE Uint8
#endif

#if RENDERER_ALIGN == 64
#define ALIGNTYPE Uint64
#elif RENDERER_ALIGN == 32
#define ALIGNTYPE Uint32
#elif RENDERER_ALIGN == 16
#define ALIGNTYPE Uint16
#else
#define ALIGNTYPE Uint8
#endif

#define PPW (RENDERER_ALIGN / RENDERER_BPP)

//#pragma message (STR(RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, RENDERER_CLIP, RENDERER_PPAL)))

static inline void RENDER_FUNCTION_NAME(RENDERER_BPP, RENDERER_TR, RENDERER_ALIGN, RENDERER_SMZX, RENDERER_CLIP, RENDERER_PPAL)
 (PIXTYPE *pixels, Uint32 pitch, struct graphics_data *graphics, struct video_layer *layer)
{
  Uint32 mode = RENDERER_SMZX;

  Uint32 ch_x, ch_y;
  Uint16 c;

  struct char_element *src = layer->data;
  int row;
  #if RENDERER_TR
  int tcol = layer->transparent_col;
  #if ((PPW > 1) || (RENDERER_SMZX == 0))
  ALIGNTYPE bgdata;
  ALIGNTYPE mask = (PIXTYPE)(~0);
  #endif /* ((PPW > 1) || (RENDERER_SMZX == 0)) */
  #endif /* RENDERER_TR */

  Uint8 *char_ptr;
  Uint8 current_char_byte;
  PIXTYPE pcol;

  ALIGNTYPE *drawPtr;
  ALIGNTYPE pix;

  ALIGNTYPE *outPtr;
  Uint32 align_pitch = pitch / sizeof(ALIGNTYPE);
  Uint32 advance_char = CHAR_W * sizeof(PIXTYPE) / sizeof(ALIGNTYPE);
  Uint32 advance_char_row = align_pitch * CHAR_H - advance_char * layer->w;

  int i;

  PIXTYPE char_colors[4];
  int char_idx[4];
  int write_pos;

  #if RENDERER_CLIP
  int pixel_x, pixel_y;
  #endif

  // Position the output ptr at the location of the first char
  outPtr = (ALIGNTYPE *)pixels;
  outPtr += layer->y * (int)align_pitch;
  outPtr += layer->x * (int)sizeof(PIXTYPE) / (int)sizeof(ALIGNTYPE);

  for (ch_y = 0; ch_y < layer->h; ch_y++) {
    for (ch_x = 0; ch_x < layer->w; ch_x++) {
      c = src->char_value;
      #if RENDERER_CLIP
      pixel_x = layer->x + ch_x * CHAR_W;
      pixel_y = layer->y + ch_y * CHAR_H;
      if (pixel_x <= -CHAR_W || pixel_y <= -CHAR_H ||
          pixel_x >= CHAR_W * SCREEN_W || pixel_y >= CHAR_H * SCREEN_H)
        c = INVISIBLE_CHAR;
      #endif
      if (c != INVISIBLE_CHAR) {
        // Char values of 256+, prior to offsetting, are from the protected set
        if (c > 0xFF) {
          c = (c & 0xFF) + PROTECTED_CHARSET_POSITION;
        } else {
          c += layer->offset;
          c %= PROTECTED_CHARSET_POSITION;
        }

        if (mode) {
          for (i = 0; i < 4; i++) {
            char_idx[i] = graphics->smzx_indices[((src->bg_color & 0xF) << 4 | (src->fg_color & 0xF)) * 4 + i];
            if (RENDERER_BPP > 8)
              char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
            else
              char_colors[i] = char_idx[i];
          }
        } else {
          if ((RENDERER_BPP > 8 || RENDERER_PPAL < 240) && src->bg_color >= 16) char_idx[0] = (src->bg_color - 16) % 16 + RENDERER_PPAL;
          else char_idx[0] = src->bg_color & 0x0F;
          if ((RENDERER_BPP > 8 || RENDERER_PPAL < 240) && src->fg_color >= 16) char_idx[1] = (src->fg_color - 16) % 16 + RENDERER_PPAL;
          else char_idx[1] = src->fg_color & 0x0F;
          for (i = 0; i < 2; i++) {
            if (RENDERER_BPP > 8)
              char_colors[i] = graphics->flat_intensity_palette[char_idx[i]];
            else
              char_colors[i] = char_idx[i];
          }
        }
        char_ptr = graphics->charset + (c * CHAR_H);
        drawPtr = outPtr;

        for (row = 0; row < CHAR_H; row++) {
          current_char_byte = char_ptr[row];

          #if RENDERER_CLIP
          if (pixel_y + row >= 0 && pixel_y + row < CHAR_H * SCREEN_H)
          #endif
          {
            for (write_pos = 0; write_pos < CHAR_W / PPW; write_pos++) {
              #if RENDERER_CLIP
              if (
                #if RENDERER_SMZX == 0 && PPW == 1
                pixel_x + write_pos >= 0
                #else /* RENDERER_SMZX != 0 || PPW != 1*/
                pixel_x + write_pos >= -1
                #endif /* RENDERER_SMZX == 0 && PPW == 1 */
                &&
                pixel_x + write_pos < CHAR_W * SCREEN_W
                )
              #endif /* RENDERER_CLIP */
              {
                #if RENDERER_SMZX == 0
                  #if RENDERER_TR
                  bgdata = drawPtr[write_pos];
                  #endif /* RENDERER_TR */
                  pix = 0;
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-1)))) << (write_pos*PPW+(PPW-1)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-1)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-1);
                  #if PPW > 1
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-2)))) << (write_pos*PPW+(PPW-2)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-2)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-2);
                  #if PPW > 2
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-3)))) << (write_pos*PPW+(PPW-3)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-3)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-3);
                  #if PPW > 3
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-4)))) << (write_pos*PPW+(PPW-4)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-4)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-4);
                  #if PPW > 4
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-5)))) << (write_pos*PPW+(PPW-5)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-5)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-5);
                  #if PPW > 5
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-6)))) << (write_pos*PPW+(PPW-6)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-6)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-6);
                  #if PPW > 6
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-7)))) << (write_pos*PPW+(PPW-7)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-7)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-7);
                  #if PPW > 7
                  pcol = (current_char_byte & (0x80 >> (write_pos*PPW+(PPW-8)))) << (write_pos*PPW+(PPW-8)) >> 7;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & (mask << RENDERER_BPP*(PPW-8)); else
                  #endif /* RENDERER_TR */
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-8);
                  #if PPW > 8
                  #error UNSUPPORTED PPW
                  #endif /* PPW > 8 */
                  #endif /* PPW > 7 */
                  #endif /* PPW > 6 */
                  #endif /* PPW > 5 */
                  #endif /* PPW > 4 */
                  #endif /* PPW > 3 */
                  #endif /* PPW > 2 */
                  #endif /* PPW > 1 */
                #else /* RENDERER_SMZX != 0 */
                  #if PPW > 1
                  pix = 0;
                  #if RENDERER_TR
                  bgdata = drawPtr[write_pos];
                  #endif /* RENDERER_TR */
                  pcol = (current_char_byte & (0xC0 >> (write_pos*PPW+(PPW-2)))) << (write_pos*PPW+(PPW-2)) >> 6;

                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & ((mask << RENDERER_BPP*(PPW-1) | (mask << RENDERER_BPP*(PPW-2)))); else
                  #endif /* RENDERER_TR */
                  {
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-2);
                  #if RENDERER_TR
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-1);
                  #endif /* RENDERER_TR */
                  }
                  #if PPW > 2
                  pcol = (current_char_byte & (0xC0 >> (write_pos*PPW+(PPW-4)))) << (write_pos*PPW+(PPW-4)) >> 6;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & ((mask << RENDERER_BPP*(PPW-3) | (mask << RENDERER_BPP*(PPW-4)))); else
                  #endif /* RENDERER_TR */
                  {
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-4);
                  #if RENDERER_TR
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-3);
                  #endif /* RENDERER_TR */
                  }
                  #if PPW > 4
                  pcol = (current_char_byte & (0xC0 >> (write_pos*PPW+(PPW-6)))) << (write_pos*PPW+(PPW-6)) >> 6;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & ((mask << RENDERER_BPP*(PPW-5) | (mask << RENDERER_BPP*(PPW-6)))); else
                  #endif /* RENDERER_TR */
                  {
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-6);
                  #if RENDERER_TR
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-5);
                  #endif /* RENDERER_TR */
                  }
                  #if PPW > 6
                  pcol = (current_char_byte & (0xC0 >> (write_pos*PPW+(PPW-8)))) << (write_pos*PPW+(PPW-8)) >> 6;
                  #if RENDERER_TR
                  if (char_idx[pcol] == tcol) pix |= bgdata & ((mask << RENDERER_BPP*(PPW-7) | (mask << RENDERER_BPP*(PPW-8)))); else
                  #endif /* RENDERER_TR */
                  {
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-8);
                  #if RENDERER_TR
                  pix |= (ALIGNTYPE)char_colors[pcol] << RENDERER_BPP*(PPW-7);
                  #endif /* RENDERER_TR */
                  }
                  #if PPW > 8
                  #error UNSUPPORTED PPW
                  #endif /* PPW > 8 */
                  #endif /* PPW > 6 */
                  #endif /* PPW > 4 */
                  #endif /* PPW > 2 */

                  #if !RENDERER_TR
                  pix |= (pix << RENDERER_BPP);
                  #endif /* !RENDERER_TR */
                  #else /* PPW == 1 */
                  pcol = (current_char_byte & (0xC0 >> write_pos)) << write_pos >> 6;


                  pix = char_colors[pcol];
                  #if RENDERER_CLIP
                  if (pixel_x + write_pos >= 0)
                  #endif /* RENDERER_CLIP */
                  {
                  #if RENDERER_TR
                  if (tcol == char_idx[pcol])
                    pix = drawPtr[write_pos];
                  #endif /* RENDERER_TR */
                  drawPtr[write_pos] = pix;
                  }

                  write_pos++;
                  #if RENDERER_TR
                  if (tcol == char_idx[pcol])
                    pix = drawPtr[write_pos];
                  #endif /* RENDERER_TR */

                  #endif /* PPW > 1 */

                #endif /* RENDERER_SMZX == 0 */

                #if RENDERER_CLIP
                if (pixel_x + write_pos < CHAR_W * SCREEN_W)
                #endif /* RENDERER_CLIP */
                drawPtr[write_pos] = pix;
              }
            }

          }

          drawPtr += align_pitch;
        }
      }
      src++;
      outPtr += advance_char;
    }
    outPtr += advance_char_row;
  }
}

#undef PPW
#undef ALIGNTYPE
#undef PIXTYPE

#endif /* RENDERER_PPAL */

#endif /* RENDERER_CLIP */

#endif /* RENDERER_SMZX */

#endif /* RENDERER_ALIGN */

#endif /* RENDERER_TR */

#endif /* RENDERER_BPP */

/* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

#ifndef MEGAZEUX_SMZXCONV_H
#define MEGAZEUX_SMZXCONV_H

struct rgba_color;

typedef struct {
  unsigned char chr;
  unsigned char clr;
} mzx_tile;

typedef unsigned char mzx_glyph[14];

typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} mzx_color;

typedef struct _smzx_converter smzx_converter;

smzx_converter *smzx_convert_init (int w, int h, int chroff, int chrskip,
 int chrlen, int clroff, int clrlen);
int smzx_convert (smzx_converter *c, const struct rgba_color *img, mzx_tile *tile,
 mzx_glyph *chr, mzx_color *pal);
void smzx_convert_free (smzx_converter *c);

#endif /* MEGAZEUX_SMZXCONV_H */

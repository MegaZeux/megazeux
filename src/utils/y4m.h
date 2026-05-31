/* MegaZeux
 *
 * Copyright (C) 2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_UTILS_Y4M_H
#define MEGAZEUX_UTILS_Y4M_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>
#include <stdio.h>

enum y4m_subsampling
{
  Y4M_SUB_420JPEG,
  Y4M_SUB_420MPEG2,
  Y4M_SUB_420PALDV,
  Y4M_SUB_411,
  Y4M_SUB_422,
  Y4M_SUB_444,
  Y4M_SUB_444ALPHA,
  Y4M_SUB_MONO,
};

enum y4m_interlacing
{
  Y4M_INTER_UNKNOWN,
  Y4M_INTER_PROGRESSIVE,
  Y4M_INTER_TOP_FIRST,
  Y4M_INTER_BOTTOM_FIRST,
  Y4M_INTER_MIXED,
};

enum y4m_color_range
{
  Y4M_RANGE_FULL,
  Y4M_RANGE_STUDIO,
};

enum y4m_frame_interlacing
{
  Y4M_FRAME_PRESENT,
  Y4M_FRAME_TOP_FIRST,
  Y4M_FRAME_TOP_FIRST_REPEAT,
  Y4M_FRAME_BOTTOM_FIRST,
  Y4M_FRAME_BOTTOM_FIRST_REPEAT,
  Y4M_FRAME_SINGLE_PROGRESSIVE,
  Y4M_FRAME_DOUBLE_PROGRESSIVE,
  Y4M_FRAME_TRIPLE_PROGRESSIVE,
};

enum y4m_frame_temporal_sampling
{
  Y4M_FRAME_TEMP_PROGRESSIVE,
  Y4M_FRAME_TEMP_INTERLACED,
};

enum y4m_frame_subsampling
{
  Y4M_FRAME_SUB_PROGRESSIVE,
  Y4M_FRAME_SUB_INTERLACED,
  Y4M_FRAME_SUB_UNKNOWN,
};

struct y4m_data
{
  size_t rgba_buffer_size;
  size_t ram_per_frame;

  uint32_t width;
  uint32_t height;
  enum y4m_subsampling subsampling;
  enum y4m_interlacing interlacing;
  enum y4m_color_range color_range;
  uint32_t framerate_n;
  uint32_t framerate_d;
  uint32_t pixel_n;
  uint32_t pixel_d;

  size_t y_size;
  size_t c_size;
  uint32_t c_width;
  uint32_t c_height;
  unsigned c_x_shift;
  unsigned c_y_shift;
};

struct y4m_frame_data
{
  uint8_t *y;
  uint8_t *pb;
  uint8_t *pr;
  uint8_t *a;

  enum y4m_frame_interlacing frame_interlacing;
  enum y4m_frame_temporal_sampling frame_temporal_sampling;
  enum y4m_frame_subsampling frame_subsampling;
};

struct y4m_rgba_color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

boolean y4m_init(struct y4m_data *y4m, FILE *fp);
boolean y4m_init_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf);
boolean y4m_begin_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf, FILE *fp);
boolean y4m_read_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf, FILE *fp);
void y4m_convert_frame_rgba(const struct y4m_data *y4m,
 const struct y4m_frame_data *yf, struct y4m_rgba_color *dest);
void y4m_free_frame(struct y4m_frame_data *yf);
void y4m_free(struct y4m_data *y4m);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_UTILS_Y4M_H */

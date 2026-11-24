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

/**
 * Simple Y4M support library, as mjpegtools is missing from most repositories.
 */

#include <stdlib.h>
#include <ctype.h>

#include "y4m.h"

static boolean unsigned_value(uint32_t *v, const char *buf)
{
  char *end;
  *v = strtoul(buf, &end, 10);
  if(end == buf || end[0] != '\0')
    return false;

  return true;
}

static boolean ratio_value(uint32_t *n, uint32_t *d, const char *buf)
{
  char *end;
  char *end2;
  *n = strtoul(buf, &end, 10);
  if(end == buf || end[0] != ':')
    return false;

  end++;
  *d = strtoul(end, &end2, 10);
  if(end == end2 || end2[0] != '\0')
    return false;

  return true;
}

static boolean char_value(char *c, const char *buf)
{
  *c = *buf;
  if(!isprint((unsigned char)*c) || buf[1] != '\0')
    return false;

  return true;
}

static boolean subsampling_value(enum y4m_subsampling *sub, const char *buf)
{
  if(!strcmp(buf, "420jpeg"))
  {
    *sub = Y4M_SUB_420JPEG;
    return true;
  }
  if(!strcmp(buf, "420mpeg2"))
  {
    *sub = Y4M_SUB_420MPEG2;
    return true;
  }
  if(!strcmp(buf, "420paldv"))
  {
    *sub = Y4M_SUB_420PALDV;
    return true;
  }
  if(!strcmp(buf, "411"))
  {
    *sub = Y4M_SUB_411;
    return true;
  }
  if(!strcmp(buf, "422"))
  {
    *sub = Y4M_SUB_422;
    return true;
  }
  if(!strcmp(buf, "444"))
  {
    *sub = Y4M_SUB_444;
    return true;
  }
  if(!strcmp(buf, "444alpha"))
  {
    *sub = Y4M_SUB_444ALPHA;
    return true;
  }
  return false;
}

static boolean interlacing_value(enum y4m_interlacing *inter, const char *buf)
{
  char type;
  if(!char_value(&type, buf))
    return false;

  switch(type)
  {
    case '?':
      *inter = Y4M_INTER_UNKNOWN;
      return true;
    case 'p':
      *inter = Y4M_INTER_PROGRESSIVE;
      return true;
    case 't':
      *inter = Y4M_INTER_TOP_FIRST;
      return true;
    case 'b':
      *inter = Y4M_INTER_BOTTOM_FIRST;
      return true;
    case 'm':
      *inter = Y4M_INTER_MIXED;
      return true;
  }
  return false;
}

static boolean color_range_value(enum y4m_color_range *range, const char *buf)
{
  if(!strcmp(buf, "FULL"))
  {
    *range = Y4M_RANGE_FULL;
    return true;
  }
  if(!strcmp(buf, "LIMITED"))
  {
    *range = Y4M_RANGE_STUDIO;
    return true;
  }
  return false;
}

static boolean read_field(char *buf, size_t sz, FILE *fp)
{
  size_t i;
  int c;

  c = fgetc(fp);
  if(c != ' ')
  {
    ungetc(c, fp);
    return false;
  }

  for(i = 0; i < sz; i++)
  {
    c = fgetc(fp);
    if(c < 0)
      return false;

    if(c == ' ' || c == '\n')
    {
      ungetc(c, fp);
      break;
    }
    buf[i] = c;
  }
  buf[i] = '\0';
  return true;
}

boolean y4m_init(struct y4m_data *y4m, FILE *fp)
{
  char buf[256];
  memset(y4m, 0, sizeof(*y4m));
  y4m->subsampling = Y4M_SUB_420JPEG;
  y4m->interlacing = Y4M_INTER_UNKNOWN;

  if(!fread(buf, 9, 1, fp) || memcmp(buf, "YUV4MPEG2", 9))
    return false;

  while(read_field(buf, sizeof(buf), fp))
  {
    switch(buf[0])
    {
      case 'W': /* Width */
        if(!unsigned_value(&y4m->width, buf + 1))
          return false;
        break;
      case 'H': /* Height */
        if(!unsigned_value(&y4m->height, buf + 1))
          return false;
        break;
      case 'C': /* Chroma subsampling */
        if(!subsampling_value(&y4m->subsampling, buf + 1))
          return false;
        break;
      case 'I': /* Interlacing */
        if(!interlacing_value(&y4m->interlacing, buf + 1))
          return false;
        break;
      case 'F': /* Framerate */
        if(!ratio_value(&y4m->framerate_n, &y4m->framerate_d, buf + 1))
          return false;
        break;
      case 'A': /* Pixel aspect ratio */
        if(!ratio_value(&y4m->pixel_n, &y4m->pixel_d, buf + 1))
          return false;
        break;
      case 'X': /* metadata */
        if(!strncmp(buf + 1, "COLORRANGE=", 11))
        {
          if(!color_range_value(&y4m->color_range, buf + 12))
            return false;
        }
        break;
    }
  }
  if(fgetc(fp) != '\n')
    return false;

  if(!y4m->width || !y4m->height)
    return false;

  y4m->y_size = y4m->width * y4m->height;
  y4m->c_size = 0;
  switch(y4m->subsampling)
  {
    case Y4M_SUB_420JPEG:
    case Y4M_SUB_420MPEG2:
    case Y4M_SUB_420PALDV:
      y4m->c_size = y4m->y_size >> 2;
      y4m->c_width = y4m->width >> 1;
      y4m->c_height = y4m->height >> 1;
      y4m->c_x_shift = 1;
      y4m->c_y_shift = 1;
      break;

    case Y4M_SUB_411:
      y4m->c_size = y4m->y_size >> 2;
      y4m->c_width = y4m->width >> 2;
      y4m->c_height = y4m->height;
      y4m->c_x_shift = 2;
      break;

    case Y4M_SUB_422:
      y4m->c_size = y4m->y_size >> 1;
      y4m->c_width = y4m->width >> 1;
      y4m->c_height = y4m->height;
      y4m->c_x_shift = 1;
      break;

    case Y4M_SUB_444:
    case Y4M_SUB_444ALPHA:
      y4m->c_size = y4m->y_size;
      y4m->c_width = y4m->width;
      y4m->c_height = y4m->height;
      break;

    case Y4M_SUB_MONO:
      /* No chroma planes */
      break;
  }

  y4m->ram_per_frame = y4m->y_size;
  if(y4m->c_size)
    y4m->ram_per_frame += 2 * y4m->c_size;
  if(y4m->subsampling == Y4M_SUB_444ALPHA)
    y4m->ram_per_frame += y4m->y_size;

  y4m->rgba_buffer_size = y4m->y_size * sizeof(struct y4m_rgba_color);
  return true;
}

boolean y4m_init_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf)
{
  memset(yf, 0, sizeof(*yf));

  yf->y = (uint8_t *)malloc(y4m->y_size);
  if(!yf->y)
    return false;

  if(y4m->c_size)
  {
    yf->pb = (uint8_t *)malloc(y4m->c_size);
    yf->pr = (uint8_t *)malloc(y4m->c_size);
    if(!yf->pb || !yf->pr)
    {
      y4m_free_frame(yf);
      return false;
    }
  }

  if(y4m->subsampling == Y4M_SUB_444ALPHA)
  {
    yf->a = (uint8_t *)malloc(y4m->y_size);
    if(!yf->a)
    {
      y4m_free_frame(yf);
      return false;
    }
  }

  return true;
}

boolean y4m_begin_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf, FILE *fp)
{
  char buf[256];

  if(!fread(buf, 5, 1, fp) || memcmp(buf, "FRAME", 5))
    return false;

  while(read_field(buf, sizeof(buf), fp))
  {
    switch(buf[0])
    {
      case 'I': /* Interlacing */
        // FIXME: implement
        break;

      case 'X': /* metadata */
        break;
    }
  }
  if(fgetc(fp) != '\n')
    return false;

  return true;
}

boolean y4m_read_frame(const struct y4m_data *y4m, struct y4m_frame_data *yf, FILE *fp)
{
  if(fread(yf->y, 1, y4m->y_size, fp) < y4m->y_size)
    return false;

  if(y4m->c_size)
  {
    if(fread(yf->pb, 1, y4m->c_size, fp) < y4m->c_size ||
     fread(yf->pr, 1, y4m->c_size, fp) < y4m->c_size)
      return false;
  }
  if(yf->a)
  {
    if(fread(yf->a, 1, y4m->y_size, fp) < y4m->y_size)
      return false;
  }
  return true;
}

void y4m_convert_frame_rgba(const struct y4m_data *y4m,
 const struct y4m_frame_data *yf, struct y4m_rgba_color *dest)
{
  // TODO: siting?
  // TODO: interlacing?
  const uint8_t *y = yf->y;
  const uint8_t *pb = yf->pb;
  const uint8_t *pr = yf->pr;
  const uint8_t *a = yf->a;
  size_t shift = y4m->c_x_shift;
  size_t max_sub = 1 << y4m->c_y_shift;
  size_t row_sub = 0;
  size_t i;
  size_t j;
  boolean chroma = (pb && pr);

  for(i = 0; i < y4m->height; i++)
  {
    for(j = 0; j < y4m->width; j++)
    {
      int yval = y[j];
      int pbval = chroma ? pb[j >> shift] - 128 : 0;
      int prval = chroma ? pr[j >> shift] - 128 : 0;
      int aval = a ? a[j] : 255;

      int r = yval + 359 * prval / 256;
      int g = yval - (88 * pbval + 183 * prval) / 256;
      int b = yval + 453 * pbval / 256;

      if(y4m->color_range == Y4M_RANGE_STUDIO)
      {
        r = (r - 16) * 255 / 224;
        g = (g - 16) * 255 / 224;
        b = (b - 16) * 255 / 224;
      }

      dest->r = MZX_CLAMP(r, 0, 255);
      dest->g = MZX_CLAMP(g, 0, 255);
      dest->b = MZX_CLAMP(b, 0, 255);
      dest->a = aval;
      dest++;
    }

    y += y4m->width;
    if(chroma)
    {
      row_sub++;
      if(row_sub >= max_sub)
      {
        row_sub = 0;
        pb += y4m->c_width;
        pr += y4m->c_width;
      }
    }
    if(a)
      a += y4m->width;
  }
}

void y4m_free_frame(struct y4m_frame_data *yf)
{
  free(yf->y);
  free(yf->pb);
  free(yf->pr);
  free(yf->a);
  yf->y = yf->pb = yf->pr = yf->a = NULL;
}

void y4m_free(struct y4m_data *y4m)
{
  // nop
}

/* MegaZeux
 *
 * Copyright (C) 2020-2023 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_ZIP_DEFLATE64_H
#define MEGAZEUX_ZIP_DEFLATE64_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <stdlib.h>
#include <zlib.h>

// Not really how you're supposed to do this, oh well... :(
#include "../../contrib/infback9/infback9.h"
#include "../../contrib/infback9/infback9.c"
#include "../../contrib/infback9/inffix9.h"
#include "../../contrib/infback9/inflate9.h"
#include "../../contrib/infback9/inftree9.h"
#include "../../contrib/infback9/inftree9.c"

#include "zip.h"

struct deflate64_stream_data
{
  struct zip_stream_data zs;
  z_stream z;
  uint8_t window[1 << 16];
};

/**
 * Allocate an inflate64 stream.
 */
static inline struct zip_stream_data *inflate64_create(void)
{
  return (struct zip_stream_data *)malloc(sizeof(struct deflate64_stream_data));
}

static inline void inflate64_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  struct deflate64_stream_data *d64s = ((struct deflate64_stream_data *)zs);
  memset(zs, 0, sizeof(struct zip_stream_data));
  memset(&(d64s->z), 0, sizeof(z_stream));
}

static inline void inflate64_close(struct zip_stream_data *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

static inline unsigned inflate64_in_callback(void *data,
 z_const unsigned char **in)
{
  struct zip_stream_data *zs = (struct zip_stream_data *)data;
  size_t length = zs->input_length;
  *in = (void *)zs->input_buffer;
  zs->input_buffer = NULL;
  zs->input_length = 0;
  zs->final_input_length += length;
  return length;
}

static inline int inflate64_out_callback(void *data, unsigned char *buffer,
 unsigned buffer_len)
{
  struct zip_stream_data *zs = (struct zip_stream_data *)data;
  int err = 0;

  if(buffer_len > zs->output_length)
  {
    buffer_len = zs->output_length;
    err = -1;
  }

  if(buffer_len)
  {
    memcpy(zs->output_buffer, buffer, buffer_len);
    zs->output_buffer += buffer_len;
    zs->output_length -= buffer_len;
    zs->final_output_length += buffer_len;
  }
  return err;
}

static inline enum zip_error inflate64_file(struct zip_stream_data *zs)
{
  struct deflate64_stream_data *d64s = ((struct deflate64_stream_data *)zs);
  int err;

  if(zs->finished)
    return ZIP_STREAM_FINISHED;

  if(!zs->output_buffer)
    return ZIP_OUTPUT_FULL;

  if(!zs->input_buffer)
    return ZIP_INPUT_EMPTY;

  err = inflateBack9Init(&(d64s->z), d64s->window);
  if(err != Z_OK)
    return ZIP_DECOMPRESS_FAILED;

  err = inflateBack9(
    &(d64s->z),
    inflate64_in_callback, zs,
    inflate64_out_callback, zs
  );

  zs->finished = true;

  inflateBack9End(&(d64s->z));

  if(err == Z_STREAM_END)
    return ZIP_STREAM_FINISHED;

  return ZIP_DECOMPRESS_FAILED;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_DEFLATE64_H */

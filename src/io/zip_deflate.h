/* MegaZeux
 *
 * Copyright (C) 2020-2024 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Deflate (zip compression method 8) compressor and decompressor via zlib.
 */

#ifndef MEGAZEUX_ZIP_DEFLATE_H
#define MEGAZEUX_ZIP_DEFLATE_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "zip.h"

struct deflate_stream_data
{
  struct zip_stream_data zs;
  z_stream z;
  boolean is_inflate;
  boolean is_deflate;
  boolean should_finish;
  uint16_t flags;
};

static inline struct zip_stream_data *deflate_create(void)
{
  return (struct zip_stream_data *)calloc(1, sizeof(struct deflate_stream_data));
}

static inline void deflate_destroy(struct zip_stream_data *zs)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);

  if(ds->is_inflate)
    inflateEnd(&(ds->z));

  if(ds->is_deflate)
    deflateEnd(&(ds->z));

  free(zs);
}

static inline void inflate_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  // Only clear the common portion of the stream data.
  memset(zs, 0, sizeof(struct zip_stream_data));
}

static inline void deflate_open(struct zip_stream_data *zs, uint16_t method,
 uint16_t flags)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);
  int prev_flags = ds->flags;
  // Only clear the common portion of the stream data.
  memset(zs, 0, sizeof(struct zip_stream_data));
  zs->is_compression_stream = true;
  ds->should_finish = false;
  ds->flags = flags & (ZIP_F_DEFLATE_MAXIMUM | ZIP_F_DEFLATE_FAST);

  if(ds->is_deflate && prev_flags != ds->flags)
  {
    deflateEnd(&(ds->z));
    ds->is_deflate = false;
  }
}

static inline void deflate_close(struct zip_stream_data *zs,
 size_t *final_input_length, size_t *final_output_length)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);
  if(!zs->finished)
  {
    // Reset the stream to be used again.
    if(ds->is_inflate)
      inflateReset(&(ds->z));

    if(ds->is_deflate)
      deflateReset(&(ds->z));

    zs->finished = true;
  }

  if(final_input_length)
    *final_input_length = zs->final_input_length;

  if(final_output_length)
    *final_output_length = zs->final_output_length;
}

static inline boolean deflate_input(struct zip_stream_data *zs,
 const void *src, size_t src_len)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);
  ds->z.next_in = (Bytef *)src;
  ds->z.avail_in = (uInt)src_len;
  return true;
}

static inline boolean deflate_output(struct zip_stream_data *zs, void *dest,
 size_t dest_len)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);
  ds->z.next_out = (Bytef *)dest;
  ds->z.avail_out = (uInt)dest_len;
  return true;
}

static inline enum zip_error inflate_init(struct zip_stream_data *zs)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);

  if(zs->is_initialized)
    return ZIP_SUCCESS;

  if(ds->is_deflate)
  {
    deflateEnd(&(ds->z));
    ds->is_deflate = false;
  }

  if(ds->z.avail_in)
  {
    // This is a raw deflate stream, so use -MAX_WBITS.
    if(!ds->is_inflate)
    {
      inflateInit2(&(ds->z), -MAX_WBITS);
      ds->is_inflate = true;
    }
    zs->is_initialized = true;
    return ZIP_SUCCESS;
  }
  return ZIP_INPUT_EMPTY;
}

static inline enum zip_error inflate_block(struct zip_stream_data *zs)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);

  enum zip_error result = inflate_init(zs);
  int err;

  if(result)
    return result;

  if(zs->finished)
    return ZIP_STREAM_FINISHED;

  if(!ds->z.avail_out)
    return ZIP_OUTPUT_FULL;

  if(!ds->z.avail_in)
    return ZIP_INPUT_EMPTY;

  err = inflate(&(ds->z), Z_FINISH);
  if(err == Z_STREAM_END)
  {
    zs->final_input_length = ds->z.total_in;
    zs->final_output_length = ds->z.total_out;
    zs->finished = true;

    inflateReset(&(ds->z));
    return ZIP_STREAM_FINISHED;
  }

  if(err == Z_OK || err == Z_BUF_ERROR)
  {
    if(!ds->z.avail_out)
      return ZIP_OUTPUT_FULL;

    if(!ds->z.avail_in)
      return ZIP_INPUT_EMPTY;
  }
  return ZIP_DECOMPRESS_FAILED;
}

static inline enum zip_error deflate_init(struct zip_stream_data *zs)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);

  if(ds->is_inflate)
  {
    inflateEnd(&(ds->z));
    ds->is_inflate = false;
  }

  if(!ds->is_deflate)
  {
    int level = Z_DEFAULT_COMPRESSION;
    if(ds->flags & ZIP_F_DEFLATE_MAXIMUM)
      level = Z_BEST_COMPRESSION;
    if(ds->flags & ZIP_F_DEFLATE_FAST)
      level = Z_BEST_SPEED;

    // This is a raw deflate stream, so use -MAX_WBITS.
    // Note: aside from the compression and windowbits, these are all defaults.
    deflateInit2(&(ds->z), level, Z_DEFLATED, -MAX_WBITS,
     8, Z_DEFAULT_STRATEGY);
    ds->is_deflate = true;
  }
  zs->is_initialized = true;
  return ZIP_SUCCESS;
}

static inline enum zip_error deflate_block(struct zip_stream_data *zs,
 boolean should_finish)
{
  struct deflate_stream_data *ds = ((struct deflate_stream_data *)zs);

  enum zip_error result = deflate_init(zs);
  int flush = Z_NO_FLUSH;
  int err;

  if(result)
    return result;

  if(zs->finished)
    return ZIP_STREAM_FINISHED;

  if(!ds->z.avail_out)
    return ZIP_OUTPUT_FULL;

  should_finish |= ds->should_finish;
  if(!should_finish && !ds->z.avail_in)
    return ZIP_INPUT_EMPTY;

  if(should_finish)
  {
    ds->should_finish = true;
    flush = Z_FINISH;
  }

  err = deflate(&(ds->z), flush);
  if(err == Z_STREAM_END)
  {
    zs->final_input_length = ds->z.total_in;
    zs->final_output_length = ds->z.total_out;
    zs->finished = true;

    deflateReset(&(ds->z));
    return ZIP_STREAM_FINISHED;
  }

  if(err == Z_OK || err == Z_BUF_ERROR)
  {
    if(!ds->z.avail_out)
      return ZIP_OUTPUT_FULL;

    if(flush != Z_FINISH && !ds->z.avail_in)
      return ZIP_INPUT_EMPTY;
  }
  return ZIP_COMPRESS_FAILED;
}

static inline enum zip_error deflate_bound(struct zip_stream_data *zs,
 size_t src_len, size_t *bound_len)
{
  struct deflate_stream_data *ds = (struct deflate_stream_data *)zs;
  enum zip_error result = deflate_init(zs);

  if(!bound_len)
    return ZIP_NULL;
  if(result)
    return result;

  *bound_len = deflateBound(&(ds->z), src_len);
  return ZIP_SUCCESS;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_DEFLATE_H */

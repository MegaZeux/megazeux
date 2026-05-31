/* MegaZeux
 *
 * Copyright (C) 2020-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_ZIP_STREAM_H
#define MEGAZEUX_ZIP_STREAM_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <inttypes.h>
#include "zip.h"

/**
 * Describes ZIP stream functions for a particular (de)compresson method.
 */
struct zip_method_handler
{
  // Create a stream data structure for this method handler.
  struct zip_stream_data *(*create)(void);

  // Destroy a stream data structure for this method handler.
  // This function will free the provided zip_stream_data pointer.
  void (*destroy)(struct zip_stream_data *);

  // Open a decompression stream.
  void (*decompress_open)(struct zip_stream_data *, uint16_t method,
   uint16_t flags);

  // Open a compression stream.
  void (*compress_open)(struct zip_stream_data *, uint16_t method,
   uint16_t flags);

  // Frees any memory allocated by the stream and does any work necessary to
  // reset the zip_stream_data struct for the next file, if applicable.
  void (*close)(struct zip_stream_data *, size_t *final_input_length,
   size_t *final_output_length);

  // Provide the next input buffer for a stream.
  boolean (*input)(struct zip_stream_data *, const void *src, size_t src_len);

  // Provide the next output buffer for a stream.
  boolean (*output)(struct zip_stream_data *, void *dest, size_t dest_len);

  // Decompress, treating the input and output as an entire file.
  enum zip_error (*decompress_file)(struct zip_stream_data *);

  // Decompress until the input is exhausted or the output is full.
  // Can be called multiple times.
  enum zip_error (*decompress_block)(struct zip_stream_data *);

  // Compress until the input is exhausted or the output is full.
  // Can be called multiple times. Provide "true" to the second parameter
  // to finish the stream.
  enum zip_error (*compress_block)(struct zip_stream_data *, boolean);

  // Bound final compressed stream length based on input stream length.
  enum zip_error (*compress_bound)(struct zip_stream_data *, size_t src_len,
   size_t *bound_len);
};

/**
 * ZIP stream function registry for supported (de)compression methods.
 */
UTILS_LIBSPEC
extern const struct zip_method_handler * const zip_method_handlers[ZIP_M_MAX_SUPPORTED + 1];

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_STREAM_H */

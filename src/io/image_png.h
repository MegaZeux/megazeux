/* MegaZeux
 *
 * Copyright (C) 2021-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_IMAGE_PNG_H
#define MEGAZEUX_IO_IMAGE_PNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "image_common.h"

#define PNG_SIGNATURE_STRING "\x89PNG\r\n\x1A\n"

enum png_error
{
  PNG_OK = 0,
  PNG_ERROR_IO,
  PNG_ERROR_MEM,
  PNG_ERROR_INIT,
  PNG_ERROR_INVALID,
  PNG_ERROR_NOT_A_PNG
};

enum png_pixel_fmt
{
  PNG_PIXEL_RGB24,
  PNG_PIXEL_RGBA32
};

struct png_rgba
{
  uint8_t r; /* red component (0 to 255) */
  uint8_t g; /* green component (0 to 255) */
  uint8_t b; /* blue component (0 to 255) */
  uint8_t a; /* alpha component (0 to 255) */
};

typedef size_t (*png_read_function)(void * RESTRICT dest, size_t num, void * RESTRICT handle);
typedef size_t (*png_write_function)(const void *src, size_t num, void * RESTRICT handle);
typedef unsigned char png_bool;

typedef png_bool (*png_read_alloc_function)(uint32_t width, uint32_t height,
 struct png_rgba **pixels, size_t *pixels_row_pitch, void *priv);
typedef const struct png_rgba *(*png_write_row_function)(uint32_t width, void *priv);

/**
 * Read a PNG image into RGBA32 in memory, converting its format as necessary.
 *
 * @param handle      file handle to read input data from.
 * @param readfn      function to read data from the provided file handle.
 * @param priv        caller private data for allocfn.
 * @param allocfn     function to allocate the png_rgba array in memory.
 *                    Upon calling this function, an array of (width x height)
 *                    png_rgba structs should be allocated, and this function
 *                    should return IMAGE_TRUE, writing a pointer to the start
 *                    of this array to *pixels and the row pitch in bytes
 *                    (usually width * 4) to *pixels_row_pitch. If the
 *                    dimensions fail constraints, or if allocation fails,
 *                    return IMAGE_FALSE instead.
 *                    This function should store a copy of the pixel array and
 *                    dimensions for the caller via priv, as png_read has no
 *                    separate mechanism to return these values.
 *                    The caller is responsible for freeing any data allocated
 *                    by this function whether or not png_read fails.
 * @param skip_sig    if true, assume the first 8 bytes have already been read.
 *                    If false, this function will check the signature.
 * @return            PNG_OK on success, otherwise a relevant png_error value.
 */
enum png_error png_read(void *handle, png_read_function readfn,
 void *priv, png_read_alloc_function allocfn, png_bool skip_signature);

/**
 * Write a PNG image to file.
 *
 * @param width       total width of the PNG to write. The memory pointed to
 *                    by the write row callback return value should contain
 *                    at least this many pixels.
 * @param height      total height of the PNG to write. The write row callback
 *                    will be called this many times to receive row data.
 * @param fmt         format to be written.
 * @param handle      file handle to write output data to.
 * @param writefn     function to write data to the provided file handle.
 * @param priv        caller private data for writerowfn.
 * @param writerowfn  function to return a pointer to the next row of pixels
 *                    to write. The memory pointed to by the return value of
 *                    this function must contain at least width pixels.
 *                    This function should return NULL on error.
 * @return            PNG_OK on success, otherwise a relevant png_error value.
 */
enum png_error png_write(
 uint32_t width, uint32_t height, enum png_pixel_fmt fmt,
 void *handle, png_write_function writefn,
 void *priv, png_write_row_function writerowfn);

#ifdef __cplusplus
}
#endif

#endif /* MEGAZEUX_IO_IMAGE_PNG_H */

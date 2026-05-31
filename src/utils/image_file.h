/* MegaZeux
 *
 * Copyright (C) 2021-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_UTILS_IMAGE_FILE_H
#define MEGAZEUX_UTILS_IMAGE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/* Read up to `num` bytes from `handle` into the buffer pointed to by `dest`.
 * Returns the number of bytes actually read from `handle`. */
typedef size_t (*image_read_function)(void *dest, size_t num, void *handle);
typedef unsigned char image_bool;

enum image_bool_values
{
  IMAGE_FALSE,
  IMAGE_TRUE
};

enum image_error
{
  IMAGE_OK = 0,
  IMAGE_ERROR_UNKNOWN,
  IMAGE_ERROR_IO,
  IMAGE_ERROR_MEM,
  IMAGE_ERROR_SIGNATURE,
  IMAGE_ERROR_CONSTRAINT,
  /* RAW errors. */
  IMAGE_ERROR_RAW_UNSUPPORTED_BPP,
  /* PNG errors. */
  IMAGE_ERROR_PNG_INIT,
  IMAGE_ERROR_PNG_FAILED,
  /* GIF errors. */
  IMAGE_ERROR_GIF_INVALID,
  IMAGE_ERROR_GIF_SIGNATURE,
  /* BMP errors. */
  IMAGE_ERROR_BMP_UNSUPPORTED_DIB,
  IMAGE_ERROR_BMP_UNSUPPORTED_PLANES,
  IMAGE_ERROR_BMP_UNSUPPORTED_COMPRESSION,
  IMAGE_ERROR_BMP_UNSUPPORTED_BPP,
  IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE8,
  IMAGE_ERROR_BMP_UNSUPPORTED_BPP_RLE4,
  IMAGE_ERROR_BMP_UNSUPPORTED_BPP_BITFIELDS,
  IMAGE_ERROR_BMP_BAD_1BPP,
  IMAGE_ERROR_BMP_BAD_2BPP,
  IMAGE_ERROR_BMP_BAD_4BPP,
  IMAGE_ERROR_BMP_BAD_8BPP,
  IMAGE_ERROR_BMP_BAD_16BPP,
  IMAGE_ERROR_BMP_BAD_24BPP,
  IMAGE_ERROR_BMP_BAD_32BPP,
  IMAGE_ERROR_BMP_BAD_RLE,
  IMAGE_ERROR_BMP_BAD_SIZE,
  IMAGE_ERROR_BMP_BAD_COLOR_TABLE,
  IMAGE_ERROR_BMP_BAD_BITFIELDS_DIB,
  IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_CONTINUITY,
  IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_OVERLAP,
  IMAGE_ERROR_BMP_BAD_BITFIELDS_MASK_RANGE,
  /* NetPBM errors. */
  IMAGE_ERROR_PBM_BAD_HEADER,
  IMAGE_ERROR_PBM_BAD_MAXVAL,
  IMAGE_ERROR_PAM_BAD_WIDTH,
  IMAGE_ERROR_PAM_BAD_HEIGHT,
  IMAGE_ERROR_PAM_BAD_DEPTH,
  IMAGE_ERROR_PAM_BAD_MAXVAL,
  IMAGE_ERROR_PAM_BAD_TUPLTYPE,
  IMAGE_ERROR_PAM_MISSING_ENDHDR,
  IMAGE_ERROR_PAM_DEPTH_TUPLTYPE_MISMATCH,
  /* TGA errors. */
  IMAGE_ERROR_TGA_NOT_A_TGA,
  IMAGE_ERROR_TGA_BAD_RLE,
};

struct rgba_color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct image_file
{
  uint32_t width;
  uint32_t height;
  struct rgba_color *data;
};

/**
 * If raw images are allowed, let the caller specify how the raw data should be
 * interpreted.
 */
struct image_raw_format
{
  uint32_t width;
  uint32_t height;
  uint32_t bytes_per_pixel;
  image_bool force_raw;
};

enum image_error load_image_from_file(const char *filename,
 struct image_file *dest, const struct image_raw_format *raw_format);
enum image_error load_image_from_stream(void *handle, image_read_function readfn,
 struct image_file *dest, const struct image_raw_format *raw_format);
void image_free(struct image_file *dest);

/**
 * Get the error string for a given error.
 *
 * @param   err   a `image_error` value.
 * @returns       a statically allocated string corresponding to the error.
 */
const char *image_error_string(enum image_error err);

#ifdef __cplusplus
}
#endif

#endif /* MEGAZEUX_UTILS_IMAGE_FILE_H */

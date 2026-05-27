/* MegaZeux
 *
 * Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Emscripten-specific zip utility.
 * This probably uses a bit more memory than UZIP as the archive needs to be
 * copied into module memory, but supports more decompression methods than UZIP.
 */

#include "../io/zip.h"
#include "utils_alloc.h"
#include <stdlib.h>
#include <emscripten.h>

#pragma clang diagnostic ignored "-Wmissing-prototypes"

int error(const char *message, unsigned int a, unsigned int b, unsigned int c)
{
  fprintf(stderr, "%s\n", message);
  exit(-1);
}

EMSCRIPTEN_KEEPALIVE
struct zip_archive *emzip_open(const void *src, size_t src_len)
{
  return zip_open_mem_read(src, src_len);
}

EMSCRIPTEN_KEEPALIVE
size_t emzip_length(struct zip_archive *zp)
{
  size_t u_size;
  if(!zip_get_next_uncompressed_size(zp, &u_size))
    return u_size;
  return 0;
}

EMSCRIPTEN_KEEPALIVE
const char *emzip_filename(struct zip_archive *zp)
{
  char *filename = cmalloc(MAX_PATH);
  if(zip_get_next_name(zp, filename, MAX_PATH-1))
  {
    free(filename);
    return NULL;
  }
  return filename;
}

EMSCRIPTEN_KEEPALIVE
const char *emzip_extract(struct zip_archive *zp)
{
  size_t u_size;

  if(!zip_get_next_uncompressed_size(zp, &u_size))
  {
    char *file = cmalloc(u_size);
    if(!zip_read_file(zp, file, u_size, &u_size))
      return file;

    free(file);
  }
  return NULL;
}

EMSCRIPTEN_KEEPALIVE
void emzip_skip(struct zip_archive *zp)
{
  zip_skip_file(zp);
}

EMSCRIPTEN_KEEPALIVE
void emzip_close(struct zip_archive *zp)
{
  zip_close(zp, NULL);
}

// Emscripten optimization sometimes clobbers free() despite indicating it
// should be kept alive, so just expose malloc and free functions here to be
// safe.
EMSCRIPTEN_KEEPALIVE
void *emzip_malloc(size_t length)
{
  return cmalloc(length);
}

EMSCRIPTEN_KEEPALIVE
void emzip_free(void *ptr)
{
  free(ptr);
}

int main(void)
{
  return 0;
}

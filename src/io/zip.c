/* MegaZeux
 *
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <zlib.h>

// This needs to stay self-sufficient - don't use core functions.
// Including util.h for the macros only...

#include "../util.h"

#include "memfile.h"
#include "vio.h"
#include "zip.h"
#include "zip_stream.h"

// Data descriptors are short areas after the compressed file containing the
// uncompressed size, compressed size, and CRC-32 checksum of the file, and
// are useful for platforms where seeking backward is impossible/infeasible.

// The NDS libfat library may have a bug with seeking backwards from the end of
// the file. Enable data descriptors so the save can be handled in one pass.
// The 3DS has incredibly slow file access that seems to be negatively impacted
// by backwards seeks in particular, so enable data descriptors for it too.
// The Switch may similarly benefit with this.
// DJGPP, Amiga may be running on exceptionally slow hardware.

#if defined(CONFIG_NDS) || defined(CONFIG_3DS) || defined(CONFIG_SWITCH) || \
 defined(CONFIG_PSVITA) || defined(CONFIG_DJGPP) || defined(CONFIG_AMIGA)
#define ZIP_WRITE_DATA_DESCRIPTOR
#endif

// Similarly, some platforms are slow and need faster compression.
// TODO: these should probably be runtime configurable instead.
#if defined(CONFIG_NDS) || defined(CONFIG_PSP) || defined(CONFIG_DJGPP)
#define ZIP_WRITE_DEFLATE_FAST
#endif

#define ZIP_VERSION_MINIMUM 20
#define ZIP64_VERSION_MINIMUM 45
#define ZIP_VERSION(x) ((x) & 0x00ff)

#define DATA_DESCRIPTOR_LEN 12
#define ZIP64_DATA_DESCRIPTOR_LEN 20

#define ZIP_DEFAULT_NUM_FILES 4
#define ZIP_MAX_NUM_FILES MIN(1u << 24u, SIZE_MAX)

#define ZIP_STREAM_BUFFER_SIZE 65536
#define ZIP_STREAM_BUFFER_U_SIZE (ZIP_STREAM_BUFFER_SIZE * 3 / 4)
#define ZIP_STREAM_BUFFER_C_SIZE (ZIP_STREAM_BUFFER_SIZE / 4)

#define LOCAL_FILE_HEADER_LEN 30
#define CENTRAL_FILE_HEADER_LEN 46
#define EOCD_RECORD_LEN 22

#define ZIP64_EOCD_RECORD_LEN 56
#define ZIP64_LOCATOR_LEN 20
#define ZIP64_LOCAL_EXTRA_LEN 20 /* Fixed size for LOCAL only. */
#define ZIP64_MAX_EXTRA_LEN 32 /* Variable size in CENTRAL only. */

#define ZIP_DEFAULT_HEADER_BUFFER \
 (CENTRAL_FILE_HEADER_LEN + ZIP64_MAX_EXTRA_LEN + 8)

/**
 * This zip reader/writer was designed:
 *
 * 1) Around the needs of our world/board/MZM files. These files need
 * binary headers (or at least it's nice to have them). The way they're
 * split up also requires fairly fast zip reading and rewriting that
 * minimizes file IO function usage.
 *
 * 2) With the ability to use a memory buffer instead of a file. This is
 * mainly for MZMs, which need to be writable to memory.
 *
 * While not copied from or even based on ajs's zipio, a few pointers were
 * taken from it. While zips can't currently be read and modified at the
 * same time, this would be a potentially useful future addition with
 * implications for downver or accessing files/worlds from inside of a zip.
 */

static uint32_t zip_get_dos_date_time(void)
{
  time_t current_time = time(NULL);
  struct tm *tm = localtime(&current_time);

  uint16_t time;
  uint16_t date;

  time =  (tm->tm_hour << 11);
  time |= (tm->tm_min << 5);
  time |= (tm->tm_sec >> 1);

  date =  ((tm->tm_year - 80) << 9);
  date |= ((tm->tm_mon + 1) << 5);
  date |= (tm->tm_mday);

  return (date << 16) | time;
}

static const char *zip_error_string(enum zip_error code)
{
  switch(code)
  {
    case ZIP_SUCCESS:
      return "no error";
    case ZIP_IGNORE_FILE:
      return "no error; file in archive was ignored";
    case ZIP_EOF:
      return "reached end of file";
    case ZIP_NULL:
      return "function received null archive";
    case ZIP_NULL_BUF:
      return "function received null buffer";
    case ZIP_ALLOC_ERROR:
      return "out of memory";
    case ZIP_STAT_ERROR:
      return "fstat failed for input file";
    case ZIP_SEEK_ERROR:
      return "could not seek to position";
    case ZIP_READ_ERROR:
      return "could not read from position";
    case ZIP_WRITE_ERROR:
      return "could not write to position";
    case ZIP_BOUND_ERROR:
      return "value exceeds bound of provided field";
    case ZIP_INVALID_READ_IN_WRITE_MODE:
      return "can't read in write mode";
    case ZIP_INVALID_WRITE_IN_READ_MODE:
      return "can't write in read mode";
    case ZIP_INVALID_FILE_READ_UNINITIALIZED:
      return "directory has not been read";
    case ZIP_INVALID_FILE_READ_IN_STREAM_MODE:
      return "can't read file in stream mode";
    case ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE:
      return "can't write file in stream mode";
    case ZIP_INVALID_STREAM_READ:
      return "can't read/close; not streaming";
    case ZIP_INVALID_STREAM_WRITE:
      return "can't write/close; not streaming";
    case ZIP_NOT_MEMORY_ARCHIVE:
      return "archive isn't a memory archive";
    case ZIP_NO_EOCD:
      return "file is not a zip archive";
    case ZIP_NO_EOCD_ZIP64:
      return "Zip64 EOCD locator or Zip64 EOCD not found or invalid";
    case ZIP_NO_CENTRAL_DIRECTORY:
      return "could not find or read central directory";
    case ZIP_INCOMPLETE_CENTRAL_DIRECTORY:
      return "central directory is missing records";
    case ZIP_UNSUPPORTED_VERSION:
      return "unsupported minimum version to extract";
    case ZIP_UNSUPPORTED_NUMBER_OF_ENTRIES:
      return "unsupported number of files in archive";
    case ZIP_UNSUPPORTED_MULTIPLE_DISKS:
      return "unsupported multiple volume archive";
    case ZIP_UNSUPPORTED_FLAGS:
      return "unsupported flags";
    case ZIP_UNSUPPORTED_DECOMPRESSION:
      return "unsupported method for decompression";
    case ZIP_UNSUPPORTED_COMPRESSION:
      return "unsupported method; use DEFLATE or none";
    case ZIP_UNSUPPORTED_DECOMPRESSION_STREAM:
      return "method does not support partial decompression";
    case ZIP_UNSUPPORTED_COMPRESSION_STREAM:
      return "method does not support partial compression";
    case ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM:
      return "can not open compressed files for direct memory read/write";
    case ZIP_NO_ZIP64_EXTRA_DATA:
      return "missing  extra data field";
    case ZIP_INVALID_ZIP64:
      return "invalid Zip64 extra data field";
    case ZIP_MISSING_LOCAL_HEADER:
      return "could not find file header";
    case ZIP_HEADER_MISMATCH:
      return "local header mismatch";
    case ZIP_CRC32_MISMATCH:
      return "CRC-32 mismatch; validation failed";
    case ZIP_DECOMPRESS_FAILED:
      return "decompression failed";
    case ZIP_COMPRESS_FAILED:
      return "compression failed";
    case ZIP_INPUT_EMPTY:
      return "stream input buffer exhausted";
    case ZIP_OUTPUT_FULL:
      return "stream output buffer full";
    case ZIP_STREAM_FINISHED:
      return "end of stream reached";
  }
  warn("zip_error_string: received unknown error code %d!\n", code);
  return "UNKNOWN ERROR";
}

static void zip_error(const char *func, enum zip_error code)
{
  warn("%s: %s\n", func, zip_error_string(code));
}

/**
 * Mac OS X likes to pollute ZIPs with metadata files. Try to detect these...
 * Also check for Thumbs.db because Windows :(
 *
 * If these are ever needed for some reason, it should be trivial to add an
 * option to disable this.
 */
static inline boolean zip_is_ignore_file(const char *filename, size_t len)
{
  if(len >= 9)
  {
    if(!strncasecmp(filename, "__MACOSX/", 9))
      return true;

    if(!strcasecmp(filename + len - 9, ".DS_Store") &&
     (len == 9 || filename[len - 10] == '/'))
      return true;

    if(!strcasecmp(filename + len - 9, "Thumbs.db") &&
     (len == 9 || filename[len - 10] == '/'))
      return true;
  }
  return false;
}

/**
 * Return true if a method is supported for decompression.
 */
static boolean zip_method_is_supported(uint8_t method)
{
  if(method > ZIP_M_NONE && method <= ZIP_M_MAX_SUPPORTED)
    return !!zip_method_handlers[method];

  return (method == ZIP_M_NONE);
}

/**
 * Set a zip archive's zip stream functions for a given method, or to NULL
 * if the specified method isn't supported.
 */
static enum zip_error zip_get_stream(struct zip_archive *zp, uint8_t method,
 enum zip_internal_state new_mode)
{
  zp->stream = NULL;
  zp->stream_data = NULL;

  if(method == ZIP_M_NONE)
  {
    // Special handling for store.
    return ZIP_SUCCESS;
  }

  if(method <= ZIP_M_MAX_SUPPORTED)
  {
    const struct zip_method_handler *result = zip_method_handlers[method];

    switch(new_mode)
    {
      // These should never reach here!
      case ZIP_S_READ_UNINITIALIZED:
      case ZIP_S_READ_FILES:
      case ZIP_S_READ_MEMSTREAM:
      case ZIP_S_WRITE_UNINITIALIZED:
      case ZIP_S_WRITE_FILES:
      case ZIP_S_WRITE_MEMSTREAM:
      case ZIP_S_ERROR:
        return -1;

      case ZIP_S_READ_STREAM:
      {
        if(!result || !result->decompress_open)
          return ZIP_UNSUPPORTED_DECOMPRESSION;

        break;
      }

      case ZIP_S_WRITE_STREAM:
      {
        if(!result || !result->compress_open)
          return ZIP_UNSUPPORTED_COMPRESSION;

        break;
      }
    }

    if(!zp->stream_data_ptrs[method])
    {
      struct zip_stream_data *tmp = result->create();
      if(!tmp)
        return ZIP_ALLOC_ERROR;

      zp->stream_data_ptrs[method] = tmp;
    }

    zp->stream = result;
    zp->stream_data = zp->stream_data_ptrs[method];
    return ZIP_SUCCESS;
  }
  return -1;
}

/**
 * Calculate an upper bound for the total compressed size of a memory block
 * of a given length with deflate.
 */
int zip_bound_deflate_usage(size_t length)
{
  z_stream stream;
  int bound;

  memset(&stream, 0, sizeof(z_stream));

  // Note: aside from the windowbits, these are all defaults
  deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
   8, Z_DEFAULT_STRATEGY);

  bound = deflateBound(&stream, length);

  deflateEnd(&stream);

  return bound;
}

/**
 * Calculate an upper bound for the total size of headers for an archive.
 */
int zip_bound_total_header_usage(int num_files, int max_name_size)
{
  // Expected:
  // max_name_size = 8 for MZX world data
  //               = 5 for MZM data
  int extra = 0;

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  extra = num_files * ZIP64_DATA_DESCRIPTOR_LEN;    // data descriptor size
#endif

  return num_files *
   // base + file name
   (LOCAL_FILE_HEADER_LEN + max_name_size +         // Local
    CENTRAL_FILE_HEADER_LEN + max_name_size +       // Central directory
    ZIP64_LOCAL_EXTRA_LEN + ZIP64_MAX_EXTRA_LEN) +  // Zip64 extra field
    ZIP64_EOCD_RECORD_LEN + ZIP64_LOCATOR_LEN +     // Zip64 EOCD record
    EOCD_RECORD_LEN + extra;                        // EOCD record
}


/* Basic checks to make sure various functions can actually be used. */

static inline enum zip_error zip_read_file_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_READ_FILES:          return ZIP_SUCCESS;
    case ZIP_S_READ_STREAM:         return ZIP_INVALID_FILE_READ_IN_STREAM_MODE;
    case ZIP_S_READ_MEMSTREAM:      return ZIP_INVALID_FILE_READ_IN_STREAM_MODE;
    case ZIP_S_READ_UNINITIALIZED:  return ZIP_INVALID_FILE_READ_UNINITIALIZED;
    default:                        return ZIP_INVALID_READ_IN_WRITE_MODE;
  }
}

static inline enum zip_error zip_read_stream_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_READ_STREAM:         return ZIP_SUCCESS;
    case ZIP_S_READ_FILES:          return ZIP_INVALID_STREAM_READ;
    case ZIP_S_READ_MEMSTREAM:      return ZIP_INVALID_STREAM_READ;
    case ZIP_S_READ_UNINITIALIZED:  return ZIP_INVALID_FILE_READ_UNINITIALIZED;
    default:                        return ZIP_INVALID_READ_IN_WRITE_MODE;
  }
}

static inline enum zip_error zip_write_file_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_WRITE_FILES:         return ZIP_SUCCESS;
    case ZIP_S_WRITE_STREAM:        return ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE;
    case ZIP_S_WRITE_MEMSTREAM:     return ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE;
    case ZIP_S_WRITE_UNINITIALIZED: return ZIP_SUCCESS;
    default:                        return ZIP_INVALID_WRITE_IN_READ_MODE;
  }
}

static inline enum zip_error zip_write_stream_mode_check(struct zip_archive *zp)
{
  switch(zp->mode)
  {
    case ZIP_S_WRITE_STREAM:        return ZIP_SUCCESS;
    case ZIP_S_WRITE_FILES:         return ZIP_INVALID_STREAM_WRITE;
    case ZIP_S_WRITE_MEMSTREAM:     return ZIP_INVALID_STREAM_WRITE;
    case ZIP_S_WRITE_UNINITIALIZED: return ZIP_INVALID_STREAM_WRITE;
    default:                        return ZIP_INVALID_WRITE_IN_READ_MODE;
  }
}

static inline void precalculate_read_errors(struct zip_archive *zp)
{
  zp->read_file_error = zip_read_file_mode_check(zp);
  zp->read_stream_error = zip_read_stream_mode_check(zp);
}

static inline void precalculate_write_errors(struct zip_archive *zp)
{
  zp->write_file_error = zip_write_file_mode_check(zp);
  zp->write_stream_error = zip_write_stream_mode_check(zp);
}

/**
 * Allocate a zip file header struct, including any necessary extra data.
 */
static struct zip_file_header *zip_allocate_file_header(uint16_t filename_len)
{
  // Attempt to reclaim any alignment padding to fit the filename...
  size_t size = MAX(sizeof(struct zip_file_header),
   offsetof(struct zip_file_header, file_name) + filename_len + 1);

  return (struct zip_file_header *)malloc(size);
}

/**
 * Free a zip file header struct, including any necessary extra data.
 * If the provided file header pointer is NULL, this function does nothing.
 */
static void zip_free_file_header(struct zip_file_header *fh)
{
  // Filename is now allocated as part of the base struct, so no need to
  // explicitly free it...
  free(fh);
}

/**
 * @return `true` if the current file header is Zip64, otherwise `false`.
 *         When writing, this must be called AFTER a file is written.
 */
static boolean zip_file_is_zip64(struct zip_file_header *fh)
{
  if(fh->compressed_size >= 0xfffffffful ||
   fh->uncompressed_size >= 0xfffffffful ||
   fh->offset >= 0xfffffffful)
    return true;
  return false;
}

/**
 * Ensure the size of the zip archive header buffer is at least
 * the provided value.
 */
static boolean zip_ensure_header_buffer_size(struct zip_archive *zp,
 size_t new_size)
{
  if(zp->header_buffer_alloc >= new_size)
    return true;

  if(zp->header_buffer)
  {
    uint8_t *tmp = (uint8_t *)realloc(zp->header_buffer, new_size);
    if(!tmp)
      return false;

    zp->header_buffer = tmp;
  }
  else
  {
    zp->header_buffer = (uint8_t *)malloc(new_size);
    if(!zp->header_buffer)
      return false;
  }
  zp->header_buffer_alloc = new_size;
  return true;
}

static char file_sig_local[] =
{
  0x50,
  0x4b,
  0x03,
  0x04
};

static char file_sig_central[] =
{
  0x50,
  0x4b,
  0x01,
  0x02
};

static uint32_t data_descriptor_sig = 0x08074b50;

static enum zip_error zip_read_file_header_signature(struct zip_archive *zp,
 boolean is_central)
{
  vfile *vf = zp->vf;
  int n, i;

  char *magic = is_central ? file_sig_central : file_sig_local;

  // Find the next file header
  i = 0;
  while(1)
  {
    n = vfgetc(vf);
    if(n < 0)
      return ZIP_MISSING_LOCAL_HEADER;

    // Match the signature
    if(n == magic[i])
    {
      i++;
      if(i == 4)
        // We've found the file header signature
        break;
    }
    else

    // Even if it's not a match, it could be the start of the signature.
    if(n == 'P')
      i = 1;

    else
      i = 0;
  }

  return ZIP_SUCCESS;
}

// Search extra fields for Zip64 info.
static enum zip_error zip_read_file_header_extra_fields(struct zip_archive *zp,
 struct zip_file_header *fh, int extra_length, boolean is_central)
{
  struct memfile mf;
  int tag;
  int sz;

  if(!zip_ensure_header_buffer_size(zp, extra_length))
    return ZIP_ALLOC_ERROR;

  if(!vfread(zp->header_buffer, extra_length, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(zp->header_buffer, extra_length, &mf);

  while(extra_length >= 4)
  {
    extra_length -= 4;
    tag = mfgetw(&mf);
    sz = mfgetw(&mf);
    if(sz > extra_length)
      return ZIP_NO_ZIP64_EXTRA_DATA;

    if(tag == 0x0001) /* Zip64 extra data field. */
    {
      if(is_central)
      {
        // Central headers only contain the used fields (fixed order).
        if(fh->uncompressed_size >= 0xfffffffful)
        {
          if(sz < 8)
            return ZIP_INVALID_ZIP64;

          fh->uncompressed_size = mfgetuq(&mf);
        }
        if(fh->compressed_size >= 0xfffffffful)
        {
          if(sz < 8)
            return ZIP_INVALID_ZIP64;

          fh->compressed_size = mfgetuq(&mf);
        }
        if(fh->offset >= 0xfffffffful)
        {
          if(sz < 8)
            return ZIP_INVALID_ZIP64;

          fh->offset = mfgetuq(&mf);
        }
        // Ignore start disk.
      }
      else
      {
        // Local headers always contain both length fields and nothing else.
        if(sz < ZIP64_LOCAL_EXTRA_LEN - 4)
          return ZIP_INVALID_ZIP64;

        if(fh->uncompressed_size >= 0xfffffffful)
          fh->uncompressed_size = mfgetuq(&mf);
        else
          mf.current += 8;

        if(fh->compressed_size >= 0xfffffffful)
          fh->compressed_size = mfgetuq(&mf);
        else
          mf.current += 8;
      }
      return ZIP_SUCCESS;
    }

    extra_length -= sz;
    mfseek(&mf, sz, SEEK_CUR);
  }
  return ZIP_NO_ZIP64_EXTRA_DATA;
}

/**
 * Read a zip file header from the central directory. This function will
 * allocate a new zip_file_header struct at the provided destination.
 */
static enum zip_error zip_read_central_file_header(struct zip_archive *zp,
 struct zip_file_header **_central_fh)
{
  struct zip_file_header *central_fh;
  enum zip_error result;
  char buffer[CENTRAL_FILE_HEADER_LEN];
  struct memfile mf;

  int extract_version;
  int file_name_length;
  int extra_length;
  int skip_length;
  int method;
  int flags;

  result = zip_read_file_header_signature(zp, true);
  if(result)
    return result;

  // We already read four
  if(!vfread(buffer, CENTRAL_FILE_HEADER_LEN - 4, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(buffer, CENTRAL_FILE_HEADER_LEN - 4, &mf);

  // Jump ahead to get the file name length since it's needed to allocate the
  // file header struct now.
  mfseek(&mf, 24, SEEK_SET);
  file_name_length = mfgetw(&mf);
  mfseek(&mf, 0, SEEK_SET);

  central_fh = zip_allocate_file_header(file_name_length);
  if(!central_fh)
    return ZIP_ALLOC_ERROR;

  *_central_fh = central_fh;

  // Version made by              2
  // Version needed to extract    2
  mf.current += 2;
  extract_version = mfgetw(&mf);
  if(ZIP_VERSION(extract_version) > ZIP64_VERSION_MINIMUM)
    return ZIP_UNSUPPORTED_VERSION;

  // General purpose bit flag     2

  flags = mfgetw(&mf);

  if((flags & ~(ZIP_F_ALLOWED | ZIP_F_UNUSED)) != 0)
  {
    warn(
      "Zip using unsupported options "
      "(allowing %d, found %d -- unsupported: %d).\n",
      ZIP_F_ALLOWED,
      flags,
      flags & ~ZIP_F_ALLOWED
    );
    return ZIP_UNSUPPORTED_FLAGS;
  }
  central_fh->flags = flags;

  // Compression method           2

  method = mfgetw(&mf);

  if(method < 0)
    return ZIP_READ_ERROR;

  if(!zip_method_is_supported(method))
    return ZIP_UNSUPPORTED_DECOMPRESSION;

  central_fh->method = method;

  // File last modification time  2
  // File last modification date  2
  mf.current += 4;

  // CRC-32, sizes                12

  central_fh->crc32 = mfgetud(&mf);
  central_fh->compressed_size = mfgetud(&mf);
  central_fh->uncompressed_size = mfgetud(&mf);

  // File name length             2
  // Extra field length           2
  // File comment length          2

  file_name_length = mfgetw(&mf);
  central_fh->file_name_length = file_name_length;
  extra_length = mfgetw(&mf);
  skip_length =  mfgetw(&mf);

  // Disk number of file start    2
  // Internal file attributes     2
  // External file attributes     4
  mf.current += 8;

  // Offset to local header       4 (from start of file)
  central_fh->offset = mfgetud(&mf);

  // File name (n)
  vfread(central_fh->file_name, file_name_length, 1, zp->vf);
  central_fh->file_name[file_name_length] = 0;

  // Version 4.5 and up: if one of several fields is maximum value, it's Zip64.
  if(ZIP_VERSION(extract_version) >= ZIP64_VERSION_MINIMUM &&
   zip_file_is_zip64(central_fh))
  {
    enum zip_error r = zip_read_file_header_extra_fields(zp, central_fh,
     extra_length, true);
    if(r != ZIP_SUCCESS)
      return r;
  }
  else
    skip_length += extra_length;

  // Done. Skip to the position where the next header should be.
  if(skip_length && vfseek(zp->vf, skip_length, SEEK_CUR))
    return ZIP_SEEK_ERROR;

  if(zip_is_ignore_file(central_fh->file_name, file_name_length))
    return ZIP_IGNORE_FILE;

  return ZIP_SUCCESS;
}

/**
 * Verify a local zip file header against the header read from the central
 * directory. Any serious discrepancies mean this file may be invalid.
 */
static enum zip_error zip_verify_local_file_header(struct zip_archive *zp,
 struct zip_file_header *central_fh)
{
  enum zip_error result;
  char buffer[LOCAL_FILE_HEADER_LEN];
  struct memfile mf;
  boolean local_is_zip64 = false;

  uint32_t crc32;
  uint64_t compressed_size;
  uint64_t uncompressed_size;
  int file_name_length;
  int extra_length;
  int data_position;
  int version;
  int flags;

  result = zip_read_file_header_signature(zp, false);
  if(result)
    return result;

  // We already read four
  if(!vfread(buffer, LOCAL_FILE_HEADER_LEN - 4, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(buffer, LOCAL_FILE_HEADER_LEN - 4, &mf);

  // Version required to extract  2
  version = mfgetw(&mf);
  if(ZIP_VERSION(version) > ZIP64_VERSION_MINIMUM)
    return ZIP_UNSUPPORTED_VERSION;

  // General purpose bit flag     2
  flags = mfgetw(&mf);

  // Compression method           2
  // File last modification time  2
  // File last modification date  2
  mf.current += 6;

  // CRC-32, sizes              12
  crc32 = mfgetud(&mf);
  compressed_size = mfgetud(&mf);
  uncompressed_size = mfgetud(&mf);

  // File name length           2
  // Extra field length         2
  file_name_length = mfgetw(&mf);
  extra_length = mfgetw(&mf);

  data_position = vftell(zp->vf) + file_name_length + extra_length;

  // If there is a data descriptor, or if one of the size fields suggests
  // there is a Zip64 extra field, then search for the Zip64 extra field.
  if(ZIP_VERSION(version) >= ZIP64_VERSION_MINIMUM &&
   (compressed_size >= 0xfffffffful || uncompressed_size >= 0xfffffffful ||
   (flags & ZIP_F_DATA_DESCRIPTOR)))
  {
    struct zip_file_header tmp;
    enum zip_error res;

    tmp.compressed_size = compressed_size;
    tmp.uncompressed_size = uncompressed_size;

    // Skip filename
    if(vfseek(zp->vf, file_name_length, SEEK_CUR))
      return ZIP_SEEK_ERROR;

    res = zip_read_file_header_extra_fields(zp, &tmp, extra_length, false);
    if(res == ZIP_SUCCESS)
    {
      local_is_zip64 = true;
      compressed_size = tmp.compressed_size;
      uncompressed_size = tmp.uncompressed_size;
    }
    else

    if(res != ZIP_NO_ZIP64_EXTRA_DATA)
      return res;
  }

  if(flags & ZIP_F_DATA_DESCRIPTOR)
  {
    // With data descriptor.
    char dd_buffer[24];
    struct memfile dd_mf;
    uint32_t peek;
    int sz = (local_is_zip64 ? ZIP64_DATA_DESCRIPTOR_LEN : DATA_DESCRIPTOR_LEN) + 4;

    // Signature (optional)       4
    // CRC-32, sizes              12 (regular) or 20 (Zip64)

    vfseek(zp->vf, data_position + central_fh->compressed_size, SEEK_SET);
    vfread(dd_buffer, sz, 1, zp->vf);
    mfopen(dd_buffer, sz, &dd_mf);

    // The data descriptor may or may not have an optional signature field,
    // meaning it may be either 12 or 16 bytes long (20 or 24 for Zip64).
    // If the CRC-32 is equal to the magic and the next four bytes are equal
    // to the CRC-32, this is probably why.
    crc32 = mfgetud(&dd_mf);
    peek = mfgetud(&dd_mf);
    if(crc32 == data_descriptor_sig && peek == central_fh->crc32)
      crc32 = peek;
    else
      dd_mf.current -= 4;

    if(local_is_zip64)
    {
      compressed_size = mfgetuq(&dd_mf);
      uncompressed_size = mfgetuq(&dd_mf);
    }
    else
    {
      compressed_size = mfgetud(&dd_mf);
      uncompressed_size = mfgetud(&dd_mf);
    }
  }

  // Verify values are correct.
  if((flags & ~ZIP_F_UNUSED) != (central_fh->flags & ~ZIP_F_UNUSED) ||
   crc32 != central_fh->crc32 ||
   compressed_size != central_fh->compressed_size ||
   uncompressed_size != central_fh->uncompressed_size)
    return ZIP_HEADER_MISMATCH;

  if(vfseek(zp->vf, data_position, SEEK_SET))
    return ZIP_SEEK_ERROR;

  return ZIP_SUCCESS;
}

/**
 * Write a local or central zip file header to the output file.
 * The provided zip file header struct's data should be fully initialized.
 */
static enum zip_error zip_write_file_header(struct zip_archive *zp,
 struct zip_file_header *fh, boolean is_central)
{
  enum zip_error result = ZIP_SUCCESS;
  boolean zip64_uncompressed = (fh->uncompressed_size >= 0xfffffffful);
  boolean zip64_compressed = (fh->compressed_size >= 0xfffffffful);
  boolean zip64_offset = (fh->offset >= 0xfffffffful);
  boolean zip64_extra = false;

  char *magic;
  struct memfile mf;
  size_t header_size;
  size_t extra_size = 0;
  uint16_t ver;
  int i;

  if(is_central)
  {
    zip64_extra = zip_file_is_zip64(fh);
    if(zip64_extra)
    {
      extra_size = 4;
      if(zip64_uncompressed)
        extra_size += 8;
      if(zip64_compressed)
        extra_size += 8;
      if(zip64_offset)
        extra_size += 8;
      // Multi-disk not supported; extra disk field won't be present.
      assert(extra_size < ZIP64_MAX_EXTRA_LEN);
    }
    header_size = fh->file_name_length + CENTRAL_FILE_HEADER_LEN + extra_size;
    magic = file_sig_central;
  }
  else
  {
#ifndef ZIP_WRITE_DATA_DESCRIPTOR
    // Position to write CRC, sizes after file write
    int64_t header_start = vftell(zp->vf);
    zp->stream_crc_position = header_start + 14;
    zp->stream_zip64_position = header_start + LOCAL_FILE_HEADER_LEN +
     fh->file_name_length + 4;
#endif
    zip64_extra = zp->zip64_current;
    if(zip64_extra)
      extra_size = ZIP64_LOCAL_EXTRA_LEN;

    header_size = fh->file_name_length + LOCAL_FILE_HEADER_LEN + extra_size;
    magic = file_sig_local;
  }

  if(header_size > zp->header_buffer_alloc)
  {
    uint8_t *tmp = (uint8_t *)realloc(zp->header_buffer, header_size);
    if(!tmp)
      return ZIP_ALLOC_ERROR;

    zp->header_buffer = tmp;
    zp->header_buffer_alloc = header_size;
  }

  mfopen_wr(zp->header_buffer, header_size, &mf);

  // Signature
  for(i = 0; i<4; i++)
    mfputc(magic[i], &mf);

  // Version made by (central directory only)
  // Version needed to extract
  ver = zip64_extra ? ZIP64_VERSION_MINIMUM : ZIP_VERSION_MINIMUM;
  mfputw(ver, &mf);
  if(is_central)
    mfputw(ver, &mf);

  // General purpose bit flag
  mfputw(fh->flags, &mf);

  // Compression method
  mfputw(fh->method, &mf);

  // File last modification time
  // File last modification date
  mfputud(zp->header_timestamp, &mf);

  // note: zp->stream_crc_position should be here.

  // CRC-32
  mfputud(fh->crc32, &mf);

  // Compressed size
  mfputud(zip64_compressed ? 0xfffffffful : fh->compressed_size, &mf);

  // Uncompressed size
  mfputud(zip64_uncompressed ? 0xfffffffful : fh->uncompressed_size, &mf);

  // File name length
  mfputw(fh->file_name_length, &mf);

  // Extra field length
  mfputw(extra_size, &mf);

  // (central directory only fields)
  if(is_central)
  {
    // File comment length
    mfputw(0, &mf);

    // Disk number where file starts
    mfputw(0, &mf);

    // Internal file attributes
    mfputw(0, &mf);

    // External file attributes
    mfputd(0, &mf);

    // Relative offset of local file header
    mfputd(zip64_offset ? 0xfffffffful : fh->offset, &mf);
  }

  // File name
  mfwrite(fh->file_name, fh->file_name_length, 1, &mf);

  // Extra field (zero bytes or 32 bytes if Zip64)
  if(zip64_extra)
  {
    // Tag: Zip64 Extra field
    mfputw(0x0001, &mf);
    // Length of extra field
    mfputw(extra_size - 4, &mf);
    // Uncompressed size
    if(!is_central || zip64_uncompressed)
      mfputuq(fh->uncompressed_size, &mf);
    // Compressed size
    if(!is_central || zip64_compressed)
      mfputuq(fh->compressed_size, &mf);
    // Local header offset
    if(is_central && zip64_offset)
      mfputuq(fh->offset, &mf);
  }

  // File comment (zero bytes)

  if(!vfwrite(zp->header_buffer, header_size, 1, zp->vf))
    result = ZIP_WRITE_ERROR;

  return result;
}

/***********/
/* Reading */
/***********/

/**
 * Allocate or resize the stream buffer. A single buffer is used across the
 * entire zip read/write session to reduce the number of allocations/frees.
 */
static enum zip_error zip_set_stream_buffer_size(struct zip_archive *zp, size_t size)
{
  size = MAX(size, ZIP_STREAM_BUFFER_SIZE);

  if(!zp->stream_buffer || zp->stream_buffer_alloc < size)
  {
    uint8_t *tmp = (uint8_t *)realloc(zp->stream_buffer, size);
    if(!tmp)
      return ZIP_ALLOC_ERROR;

    zp->stream_buffer = tmp;
    zp->stream_buffer_alloc = size;
  }
  return ZIP_SUCCESS;
}

/**
 * Decompress data from a stream into the destination or into the stream
 * buffer.
 */
static enum zip_error zread_stream(uint8_t *destBuf, size_t readLen,
 size_t *consumed, struct zip_archive *zp)
{
  struct zip_stream_data *stream_data = zp->stream_data;
  boolean direct_write = (readLen == zp->stream_u_left);
  uint8_t *in;
  size_t in_size;
  size_t out_size;
  enum zip_error (*decompress_fn)(struct zip_stream_data *);
  enum zip_error result;

  while(readLen)
  {
    if(zp->stream_buffer_pos < zp->stream_buffer_end)
    {
      size_t amount = zp->stream_buffer_end - zp->stream_buffer_pos;
      amount = MIN(readLen, amount);

      memcpy(destBuf, zp->stream_buffer + zp->stream_buffer_pos, amount);
      zp->stream_buffer_pos += amount;
      destBuf += amount;
      readLen -= amount;
      if(!readLen)
        break;
    }

    // Fetch more decompressed data. If direct_write is true, this can be
    // fetched directly into the output. Otherwise, it needs to stick around
    // so it can be copied for future reads.

    if(zp->stream->decompress_block)
    {
      result = zip_set_stream_buffer_size(zp, ZIP_STREAM_BUFFER_SIZE);
      if(result != ZIP_SUCCESS)
        return result;

      decompress_fn = zp->stream->decompress_block;

      in = zp->stream_buffer;
      in_size = ZIP_STREAM_BUFFER_SIZE;
      out_size = 0;

      if(!direct_write)
      {
        in = zp->stream_buffer + ZIP_STREAM_BUFFER_U_SIZE;
        in_size = ZIP_STREAM_BUFFER_C_SIZE;
        out_size = ZIP_STREAM_BUFFER_U_SIZE;
      }
    }
    else

    if(zp->stream_left)
    {
      size_t size = zp->streaming_file->compressed_size;
      out_size = zp->streaming_file->uncompressed_size;
      in_size = size;

      if(!direct_write)
        size += out_size;

      result = zip_set_stream_buffer_size(zp, size);
      if(result != ZIP_SUCCESS)
        return result;

      decompress_fn = zp->stream->decompress_file;

      in = zp->stream_buffer;
      if(!direct_write)
        in += out_size;
    }
    // If block decompression isn't available, the entire file should have
    // already been decompressed. Requesting any more data is an instant EOF.
    else
      return ZIP_EOF;

    if(direct_write)
    {
      zp->stream->output(stream_data, destBuf, readLen);
      zp->stream_buffer_end = 0;
      readLen = 0;
    }
    else
    {
      zp->stream->output(stream_data, zp->stream_buffer, out_size);
      zp->stream_buffer_end = out_size;
    }

    while((result = decompress_fn(stream_data)) == ZIP_INPUT_EMPTY)
    {
      in_size = MIN(in_size, zp->stream_left - *consumed);
      if(!in_size)
        return ZIP_EOF;

      zp->stream->input(stream_data, in, in_size);
      if(!vfread(in, in_size, 1, zp->vf))
        return ZIP_READ_ERROR;

      *consumed += in_size;
    }
    if(result != ZIP_OUTPUT_FULL && result != ZIP_STREAM_FINISHED)
      return result;

    zp->stream_buffer_pos = 0;
  }
  return ZIP_SUCCESS;
}

/**
 * Read data from a zip archive. Only works while streaming a file.
 */
enum zip_error zread(void *destBuf, size_t readLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  size_t consumed = 0;

  enum zip_error result;

  result = zp->read_stream_error;
  if(result)
    goto err_out;

  if(!readLen)
    return ZIP_SUCCESS;

  if(!zp->stream_u_left)
    return ZIP_EOF;

  // Can't read past the length of the file...
  readLen = MIN(readLen, zp->stream_u_left);

  fh = zp->streaming_file;

  // No compression
  if(fh->method == ZIP_M_NONE)
  {
    consumed = readLen;
    if(!vfread(destBuf, readLen, 1, zp->vf))
    {
      result = ZIP_EOF;
      goto err_out;
    }
  }
  else

  // Decompression via stream
  if(zp->stream)
  {
    result = zread_stream(destBuf, readLen, &consumed, zp);
    if(result)
      goto err_out;
  }
  else
  {
    result = ZIP_UNSUPPORTED_DECOMPRESSION;
    goto err_out;
  }

  // Update the crc32 and streamed amount
  zp->stream_crc32 = crc32(zp->stream_crc32, destBuf, readLen);
  zp->stream_u_left -= readLen;
  zp->stream_left -= consumed;
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zread", result);

  return result;
}

/**
 * Get the name of the next file in the archive.
 */
enum zip_error zip_get_next_name(struct zip_archive *zp,
 char *name, int name_buffer_size)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  fh = zp->files[zp->pos];

  // Copy the file name, if requested
  if(name && name_buffer_size)
  {
    name_buffer_size = MIN(name_buffer_size, fh->file_name_length);
    memcpy(name, fh->file_name, name_buffer_size);
    name[name_buffer_size] = 0;
  }

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_name", result);
  return result;
}

/**
 * Get the MZX properties of the next file in the archive.
 * These fields need to be initialized externally; see world_format.h.
 *
 * @param zp        zip archive structure.
 * @param file_id   pointer to variable to store the file type ID to, or null.
 * @param board_id  pointer to variable to store the board number to, or null.
 * @param robot_id  pointer to variable to store the object number to, or null.
 * @return          `ZIP_SUCCESS` on success;
 *                  `ZIP_EOF` if the end of archive has been reached;
 *                  `ZIP_NULL` if `zp` is a null pointer;
 *                  `ZIP_INVALID_READ_IN_WRITE_MODE` if `zp` is a write archive.
 *                  `ZIP_INVALID_FILE_READ_UNINITIALIZED` if the central
 *                  directory has not been read yet;
 *                  `ZIP_INVALID_FILE_READ_IN_STREAM_MODE` if `zp` has opened
 *                  the current file for reading.
 */
enum zip_error zip_get_next_mzx_file_id(struct zip_archive *zp,
 unsigned int *file_id, unsigned int *board_id, unsigned int *robot_id)
{
  struct zip_file_header *fh;
  enum zip_error result = ZIP_NULL;

  if(!zp)
    goto err_out;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
    return ZIP_EOF;

  fh = zp->files[zp->pos];

  if(file_id)
    *file_id = fh->mzx_file_id;

  if(board_id)
    *board_id = fh->mzx_board_id;

  if(robot_id)
    *robot_id = fh->mzx_robot_id;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_file_id", result);
  return result;
}

/**
 * Get the uncompressed length of the next file in the archive within the
 * range of `size_t`. This should be used when reading whole files to memory.
 *
 * @param zp      zip archive structure.
 * @param u_size  pointer to `size_t` variable to store uncompressed size to.
 *                If null, no size will be stored.
 * @return        `ZIP_SUCCESS` on success;
 *                `ZIP_EOF` if the end of the archive has been reached;
 *                `ZIP_BOUND_ERROR` if the uncompressed size is greater than
 *                or equal to `SIZE_MAX`;
 *                `ZIP_NULL` if `zp` is a null pointer;
 *                `ZIP_INVALID_READ_IN_WRITE_MODE` if `zp` is a write archive.
 *                `ZIP_INVALID_FILE_READ_UNINITIALIZED` if the central
 *                directory has not been read yet;
 *                `ZIP_INVALID_FILE_READ_IN_STREAM_MODE` if `zp` has opened
 *                the current file for reading.
 */
enum zip_error zip_get_next_uncompressed_size(struct zip_archive *zp,
 size_t *u_size)
{
  uint64_t tmp = 0;
  enum zip_error result = zip_get_next_uncompressed_size64(zp, &tmp);
  if(result)
    return result;

  if(tmp >= SIZE_MAX)
    return ZIP_BOUND_ERROR;

  if(u_size)
    *u_size = tmp;

  return ZIP_SUCCESS;
}

/**
 * Get the uncompressed length of the next file in the archive. This should
 * be used when the entire file does not need to fit into memory.
 *
 * @param zp      zip archive structure.
 * @param u_size  pointer to `uint64_t` variable to store uncompressed size to.
 *                If null, no size will be stored.
 * @return        `ZIP_SUCCESS` on success;
 *                `ZIP_EOF` if the end of the archive has been reached;
 *                `ZIP_NULL` if `zp` is a null pointer;
 *                `ZIP_INVALID_READ_IN_WRITE_MODE` if `zp` is a write archive.
 *                `ZIP_INVALID_FILE_READ_UNINITIALIZED` if the central
 *                directory has not been read yet;
 *                `ZIP_INVALID_FILE_READ_IN_STREAM_MODE` if `zp` has opened
 *                the current file for reading.
 */
enum zip_error zip_get_next_uncompressed_size64(struct zip_archive *zp,
 uint64_t *u_size)
{
  enum zip_error result = ZIP_NULL;

  if(!zp)
    goto err_out;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
    return ZIP_EOF;

  if(u_size)
    *u_size = zp->files[zp->pos]->uncompressed_size;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_u_size", result);
  return result;
}

/**
 * Get the compression method of the next file in the archive.
 */
enum zip_error zip_get_next_method(struct zip_archive *zp, unsigned int *method)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  if(method)
    *method = zp->files[zp->pos]->method;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_get_next_method", result);
  return result;
}

/**
 * Common function for zip stream opening.
 * mode should be either ZIP_S_READ_STREAM or ZIP_S_READ_MEMSTREAM.
 */
static enum zip_error zip_read_stream_open(struct zip_archive *zp, uint8_t mode)
{
  struct zip_file_header *central_fh;

  uint64_t c_size;
  uint64_t u_size;
  uint16_t method;

  uint64_t read_pos;
  enum zip_error result;

  result = (zp ? zp->read_file_error : ZIP_NULL);
  if(result)
    return result;

  if(zp->pos >= zp->num_files)
    return ZIP_EOF;

  central_fh = zp->files[zp->pos];

  c_size = central_fh->compressed_size;
  u_size = central_fh->uncompressed_size;
  method = central_fh->method;

  // Special mem stream checks.
  if(mode == ZIP_S_READ_MEMSTREAM)
  {
    if(!zp->is_memory)
      return ZIP_NOT_MEMORY_ARCHIVE;

    if(method != ZIP_M_NONE)
      return ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM;
  }

  // Attempt to get stream functions for this compression method. This will
  // return an error if the method is not supported for decompression.
  result = zip_get_stream(zp, method, mode);
  if(result)
    return result;

  // Seek to the start of the record
  read_pos = vftell(zp->vf);
  if(read_pos != central_fh->offset)
  {
    if(vfseek(zp->vf, central_fh->offset, SEEK_SET))
      return ZIP_SEEK_ERROR;
  }

  // Verify the local header matches the central directory header.
  result = zip_verify_local_file_header(zp, central_fh);
  if(result)
    return result;

  // Everything looks good. Set up stream mode.
  zp->mode = mode;
  zp->stream_buffer_pos = 0;
  zp->stream_buffer_end = 0;
  zp->streaming_file = central_fh;
  zp->stream_u_left = u_size;
  zp->stream_left = c_size;
  zp->stream_crc32 = 0;

  if(zp->stream)
    zp->stream->decompress_open(zp->stream_data, method, central_fh->flags);

  precalculate_read_errors(zp);
  return ZIP_SUCCESS;
}

/**
 * Open a stream to read the next file from a zip archive. If provided, destLen
 * will be the uncompressed size of the file on return (or 0 upon error).
 */
enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 uint64_t *destLen)
{
  enum zip_error result = zip_read_stream_open(zp, ZIP_S_READ_STREAM);
  if(result)
    goto err_out;

  if(destLen)
    *destLen = zp->streaming_file->uncompressed_size;

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_open_file_stream", result);
  if(destLen)
    *destLen = 0;
  return result;
}

/**
 * Like zip_read_open_file_stream, but allows the direct reading of
 * uncompressed files. This is abusable, but quicker than the alternatives.
 *
 * ZIP_NOT_MEMORY_ARCHIVE or ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM will be
 * silently returned if the current archive or file doesn't support this; use
 * regular functions instead in this case.
 */
enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  enum zip_error result = zip_read_stream_open(zp, ZIP_S_READ_MEMSTREAM);
  if(result)
    goto err_out;

  if(!vfile_get_memfile_block(zp->vf, zp->streaming_file->compressed_size, mf))
  {
    result = ZIP_EOF;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_EOF &&
   result != ZIP_NOT_MEMORY_ARCHIVE &&
   result != ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM)
    zip_error("zip_read_open_mem_stream", result);
  memset(mf, 0, sizeof(struct memfile));
  return result;
}

/**
 * Shared function for closing both types of zip read streams.
 */
enum zip_error zip_read_close_stream(struct zip_archive *zp)
{
  uint32_t expected_crc32;
  uint32_t stream_crc32;
  uint8_t buffer[512];
  int size;

  enum zip_error result;

  // mem streams need special cleanup but are mostly the same.
  if(zp && zp->mode == ZIP_S_READ_MEMSTREAM)
  {
    struct memfile mf;
    size_t c_size = zp->streaming_file->compressed_size;

    // If this doesn't work, something modified the zip archive structures.
    if(!vfile_get_memfile_block(zp->vf, c_size, &mf))
      assert(0);

    zp->read_stream_error = ZIP_SUCCESS;
    zp->stream_u_left = 0;
    zp->stream_crc32 = crc32(0, mf.current, c_size);
  }

  result = (zp ? zp->read_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  // If the stream was incomplete, finish the crc32
  while(zp->stream_u_left)
  {
    size = MIN(sizeof(buffer), zp->stream_u_left);
    result = zread(buffer, size, zp);
    if(result)
      break;
  }

  // TODO maybe check final in/out...
  if(zp->stream)
    zp->stream->close(zp->stream_data, NULL, NULL);

  expected_crc32 = zp->streaming_file->crc32;
  stream_crc32 = zp->stream_crc32;

  // Increment the position and clear the streaming vars
  zp->mode = ZIP_S_READ_FILES;
  zp->streaming_file = NULL;
  zp->stream = NULL;
  zp->stream_left = 0;
  zp->stream_u_left = 0;
  zp->stream_crc32 = 0;
  zp->pos++;

  precalculate_read_errors(zp);

  // Check for an error from zread...
  if(result)
    goto err_out;

  // Check the CRC-32 of the stream
  if(expected_crc32 != stream_crc32)
  {
    warn("crc check: expected %"PRIx32", got %"PRIx32"\n",
     expected_crc32, stream_crc32);
    result = ZIP_CRC32_MISMATCH;
    goto err_out;
  }

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_read_close_stream", result);
  return result;
}

/**
 * Rewind to the start of the zip archive.
 */
enum zip_error zip_rewind(struct zip_archive *zp)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->num_files == 0)
  {
    return ZIP_EOF;
  }

  zp->pos = 0;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_rewind", result);
  return result;
}

/**
 * Skip the current file in the zip archive.
 */
enum zip_error zip_skip_file(struct zip_archive *zp)
{
  enum zip_error result;

  result = zp->read_file_error;
  if(result)
    goto err_out;

  if(zp->pos >= zp->num_files)
  {
    return ZIP_EOF;
  }

  zp->pos++;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_skip_file", result);
  return result;
}

/**
 * Read a file from the a zip archive. If provided, the value of readLen will
 * be set to the number of bytes read into the buffer.
 */
enum zip_error zip_read_file(struct zip_archive *zp,
 void *destBuf, size_t destLen, size_t *readLen)
{
  uint64_t u_size;
  enum zip_error result;

  // No need to check mode; the functions used here will

  // We can't accept a NULL pointer here, though
  if(!destBuf)
  {
    result = ZIP_NULL_BUF;
    goto err_out;
  }

  result = zip_read_open_file_stream(zp, &u_size);
  if(result)
    goto err_out;

  u_size = MIN(destLen, u_size);

  result = zread(destBuf, u_size, zp);
  if(result && result != ZIP_EOF)
    goto err_close;

  result = zip_read_close_stream(zp);
  if(result)
    goto err_out;

  if(readLen)
    *readLen = u_size;

  return ZIP_SUCCESS;

err_close:
  zip_read_close_stream(zp);

err_out:
  if(result != ZIP_EOF)
    zip_error("zip_read_file", result);

  if(readLen)
    *readLen = 0;

  return result;
}

/***********/
/* Writing */
/***********/

/**
 * Check if there's enough room in a memory archive to write a number of bytes
 * at the current position in the file. If there isn't, and if the buffer is
 * expandable, vfile_get_memfile_block will ensure that the buffer is expanded
 * to allow the requested size. Otherwise, this function will return ZIP_EOF.
 */
static enum zip_error zip_ensure_capacity(size_t len, struct zip_archive *zp)
{
  if(!vfile_get_memfile_block(zp->vf, len, NULL))
    return ZIP_EOF;

  return ZIP_SUCCESS;
}

/**
 * Flush data out to the file.
 */
static enum zip_error zwrite_out(const void *buffer, size_t len,
 struct zip_archive *zp)
{
  if(zp->is_memory && zip_ensure_capacity(len, zp))
    return ZIP_EOF;

  if(!vfwrite(buffer, len, 1, zp->vf))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
}

/**
 * Compress an input buffer and output it as-needed until the entire input has
 * been consumed. A null buffer can be provided to end the stream.
 */
static enum zip_error zwrite_stream_compress(const void *buffer, size_t len,
 size_t *write_len, struct zip_archive *zp)
{
  struct zip_stream_data *stream_data = zp->stream_data;
  uint8_t *out = zp->stream_buffer + ZIP_STREAM_BUFFER_U_SIZE;
  size_t out_len = ZIP_STREAM_BUFFER_C_SIZE;
  enum zip_error result;
  boolean finish = !buffer;

  if(!finish)
    zp->stream->input(stream_data, buffer, len);

  assert(zp->stream->compress_block);
  while((result = zp->stream->compress_block(stream_data, finish)) == ZIP_OUTPUT_FULL)
  {
    result = zwrite_out(out, out_len, zp);
    if(result)
      return result;

    *write_len += out_len;
    zp->stream->output(stream_data, out, out_len);
  }
  if(result != ZIP_INPUT_EMPTY && result != ZIP_STREAM_FINISHED)
    return result;

  if(finish)
  {
    uint64_t total_written = zp->stream_left + *write_len;
    if(result != ZIP_STREAM_FINISHED)
      return ZIP_COMPRESS_FAILED;

    // Flush any leftover data.
    if(stream_data->final_output_length > total_written)
    {
      out_len = stream_data->final_output_length - total_written;
      result = zwrite_out(out, out_len, zp);
      if(result)
        return result;

      *write_len += out_len;
    }
  }
  return ZIP_SUCCESS;
}

/**
 * Buffer stream input data, compressing and flushing it to file as-needed.
 */
static enum zip_error zwrite_stream(const void *src, size_t srcLen,
 size_t *write_len, struct zip_archive *zp)
{
  enum zip_error result;

  assert(src);
  if(zp->stream_buffer_pos + srcLen <= zp->stream_buffer_end)
  {
    memcpy(zp->stream_buffer + zp->stream_buffer_pos, src, srcLen);
    zp->stream_buffer_pos += srcLen;
    return ZIP_SUCCESS;
  }

  if(zp->stream_buffer_pos)
  {
    result = zwrite_stream_compress(zp->stream_buffer, zp->stream_buffer_pos,
     write_len, zp);
    zp->stream_buffer_pos = 0;
    if(result)
      return result;

    if(srcLen <= zp->stream_buffer_end / 2)
    {
      memcpy(zp->stream_buffer, src, srcLen);
      zp->stream_buffer_pos = srcLen;
      return ZIP_SUCCESS;
    }
  }
  return zwrite_stream_compress(src, srcLen, write_len, zp);
}

/**
 * Finish the stream; write anything left on the buffer, then write a null
 * buffer to signal the stream end.
 */
static enum zip_error zwrite_finish(size_t *write_len, struct zip_archive *zp)
{
  enum zip_error result;

  if(zp->stream_buffer_pos)
  {
    result = zwrite_stream_compress(zp->stream_buffer, zp->stream_buffer_pos,
     write_len, zp);
    if(result)
      return result;

    zp->stream_buffer_pos = 0;
  }
  return zwrite_stream_compress(NULL, 0, write_len, zp);
}

/**
 * Write data to a zip archive. Only works in stream mode.
 */
enum zip_error zwrite(const void *src, size_t srcLen, struct zip_archive *zp)
{
  struct zip_file_header *fh;
  size_t writeLen = 0;

  enum zip_error result;

  result = (zp ? zp->write_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(!srcLen)
    return ZIP_SUCCESS;

  fh = zp->streaming_file;

  // No compression
  if(fh->method == ZIP_M_NONE)
  {
    writeLen = srcLen;
    result = zwrite_out(src, srcLen, zp);
    if(result)
      goto err_out;
  }
  else

  // Compression via stream
  if(zp->stream)
  {
    result = zwrite_stream(src, srcLen, &writeLen, zp);
    if(result)
      goto err_out;
  }
  else
  {
    result = ZIP_UNSUPPORTED_COMPRESSION;
    goto err_out;
  }

  // Update the stream
  fh->uncompressed_size += srcLen;
  zp->stream_crc32 = crc32(zp->stream_crc32, src, srcLen);
  zp->stream_left += writeLen;
  return ZIP_SUCCESS;

err_out:
  zip_error("zwrite", result);
  return result;
}

/**
 * Write data to a zip archive. Only works in stream mode. This bypasses the
 * input buffer for streams known to only need to write a single block and
 * should only be used internally.
 */
static enum zip_error zwrite_direct(const void *src, size_t srcLen,
 struct zip_archive *zp)
{
  size_t writeLen = 0;
  enum zip_error result;

  if(!zp->stream || zp->stream_buffer_pos)
    return zwrite(src, srcLen, zp);

  result = zwrite_stream_compress(src, srcLen, &writeLen, zp);
  if(result)
    return result;

  // Update the stream
  zp->streaming_file->uncompressed_size += srcLen;
  zp->stream_crc32 = crc32(zp->stream_crc32, src, srcLen);
  zp->stream_left += writeLen;
  return ZIP_SUCCESS;
}

/**
 * Enable/disable Zip64 for the next file to be written.
 */
enum zip_error zip_set_zip64_enabled(struct zip_archive *zp,
 boolean enable_zip64)
{
  if(!zp)
    return ZIP_NULL;

  zp->zip64_enabled = enable_zip64;
  return ZIP_SUCCESS;
}

/**
 * Writes the data descriptor for a file. If data descriptors are turned
 * off, this function will seek back and add the data into the local header.
 */
static inline enum zip_error zip_write_data_descriptor(struct zip_archive *zp,
 struct zip_file_header *fh)
{
  char buffer[ZIP64_DATA_DESCRIPTOR_LEN];
  struct memfile mf;
  uint32_t c_size = MIN(fh->compressed_size, 0xfffffffful);
  uint32_t u_size = MIN(fh->uncompressed_size, 0xfffffffful);

  mfopen_wr(buffer, DATA_DESCRIPTOR_LEN, &mf);
  mfputud(fh->crc32, &mf);
  mfputud(c_size, &mf);
  mfputud(u_size, &mf);

#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  {
    if(zp->zip64_current)
    {
      if(zp->is_memory && zip_ensure_capacity(ZIP64_DATA_DESCRIPTOR_LEN, zp))
        return ZIP_EOF;

      mfopen_wr(buffer, ZIP64_DATA_DESCRIPTOR_LEN, &mf);
      mfseek(&mf, 4, SEEK_SET);
      mfputuq(fh->compressed_size, &mf);
      mfputuq(fh->uncompressed_size, &mf);
      vfwrite(buffer, ZIP64_DATA_DESCRIPTOR_LEN, 1, zp->vf);
    }
    else
    {
      // Write data descriptor
      if(zp->is_memory && zip_ensure_capacity(DATA_DESCRIPTOR_LEN, zp))
        return ZIP_EOF;

      vfwrite(buffer, 12, 1, zp->vf);
    }
  }
#else
  {
    // Go back and write sizes and CRC32
    int64_t return_position = vftell(zp->vf);

    if(vfseek(zp->vf, zp->stream_crc_position, SEEK_SET))
      return ZIP_SEEK_ERROR;

    if(!vfwrite(buffer, DATA_DESCRIPTOR_LEN, 1, zp->vf))
      return ZIP_WRITE_ERROR;

    if(zp->zip64_current)
    {
      // Write the Zip64 copies of the sizes.
      // Both are always present in the local header.
      if(vfseek(zp->vf, zp->stream_zip64_position, SEEK_SET))
        return ZIP_SEEK_ERROR;

      vfputq(fh->uncompressed_size, zp->vf);
      vfputq(fh->compressed_size, zp->vf);
    }

    if(vfseek(zp->vf, return_position, SEEK_SET))
      return ZIP_SEEK_ERROR;
  }
#endif // !ZIP_WRITE_DATA_DESCRIPTOR

  return ZIP_SUCCESS;
}

/**
 * Common function for zip write stream opening.
 * mode should be either ZIP_S_WRITE_STREAM or ZIP_S_WRITE_MEMSTREAM.
 */
static enum zip_error zip_write_open_stream(struct zip_archive *zp,
 const char *name, int method, uint8_t mode)
{
  struct zip_file_header *fh;
  uint16_t file_name_len;

  enum zip_error result;

  result = (zp ? zp->write_file_error : ZIP_NULL);
  if(result)
    return result;

  // Special mem stream checks.
  if(mode == ZIP_S_WRITE_MEMSTREAM)
  {
    if(!zp->is_memory)
      return ZIP_NOT_MEMORY_ARCHIVE;

    // Sanity check (this shouldn't actually happen ever).
    if(method == ZIP_M_DEFLATE)
      return ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM;
  }

  // memfiles: make sure there's enough space for the header
  if(zp->is_memory && zip_ensure_capacity(strlen(name) + 30, zp))
    return ZIP_EOF;

  // Attempt to get stream functions for this compression method. This will
  // return an error if the method is not supported for compression.
  result = zip_get_stream(zp, method, mode);
  if(result)
    return result;

  // If needed, expand the files list so the file can be added to it.
  if(zp->pos == zp->files_alloc)
  {
    size_t count = zp->files_alloc * 2;
    struct zip_file_header **tmp = (struct zip_file_header **)realloc(zp->files,
     count * sizeof(struct zip_file_header *));
    if(!tmp)
      return ZIP_ALLOC_ERROR;

    zp->files = tmp;
    zp->files_alloc = count;
  }

  file_name_len = strlen(name);
  fh = zip_allocate_file_header(file_name_len);
  if(!fh)
    return ZIP_ALLOC_ERROR;

  // Set up the header
  fh->flags = 0;
#ifdef ZIP_WRITE_DATA_DESCRIPTOR
  fh->flags |= ZIP_F_DATA_DESCRIPTOR;
#endif
#ifdef ZIP_WRITE_DEFLATE_FAST
  if(method == ZIP_M_DEFLATE)
    fh->flags |= ZIP_F_DEFLATE_FAST;
#endif
  fh->method = method;
  fh->crc32 = 0;
  fh->compressed_size = 0;
  fh->uncompressed_size = 0;
  fh->offset = vftell(zp->vf);
  fh->file_name_length = file_name_len;
  memcpy(fh->file_name, name, file_name_len + 1);

  // Write the header
  result = zip_write_file_header(zp, fh, 0);
  if(result)
  {
    zip_free_file_header(fh);
    zp->streaming_file = NULL;
    return result;
  }

  // Now that the header is written, it can be added to the files list.
  zp->running_file_name_length += file_name_len;
  zp->files[zp->pos] = fh;
  zp->num_files++;

  // Set up the stream
  zp->mode = mode;
  zp->streaming_file = fh;
  zp->stream_buffer_pos = 0;
  zp->stream_buffer_end = ZIP_STREAM_BUFFER_U_SIZE;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  if(zp->stream)
  {
    struct zip_stream_data *stream_data = zp->stream_data;
    zp->stream->compress_open(stream_data, method, fh->flags);

    result = zip_set_stream_buffer_size(zp, ZIP_STREAM_BUFFER_SIZE);
    if(result != ZIP_SUCCESS)
      return result;

    zp->stream->output(stream_data, zp->stream_buffer + ZIP_STREAM_BUFFER_U_SIZE,
     ZIP_STREAM_BUFFER_C_SIZE);
  }

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;
}

/**
 * Enable or disable Zip64 based on a fixed input file size and known
 * compression mode.
 */
static enum zip_error zip_write_autodetect_zip64(struct zip_archive *zp,
 int method, uint8_t mode, size_t src_len)
{
  enum zip_error result;

  zp->zip64_current = false;
  if(!zp->zip64_enabled)
    return ZIP_SUCCESS;

  if(src_len >= 0xfffffffful)
  {
    zp->zip64_current = true;
    return ZIP_SUCCESS;
  }

  // Hack--set up the stream early so its compress_bound function can be used.
  result = zip_get_stream(zp, method, mode);
  if(result)
    return result;

  if(zp->stream != NULL && zp->stream->compress_bound != NULL)
  {
    size_t bound_len = 0;
    int flags = 0;

    // Bounding requires initializing the zstream, so initialize first
    // using the correct compression level.
#ifdef ZIP_WRITE_DEFLATE_FAST
    if(method == ZIP_M_DEFLATE)
      flags |= ZIP_F_DEFLATE_FAST;
#endif

    zp->stream->compress_open(zp->stream_data, method, flags);
    result = zp->stream->compress_bound(zp->stream_data, src_len, &bound_len);
    if(result)
      return result;

    if(bound_len >= 0xfffffffful)
      zp->zip64_current = true;
  }
  return ZIP_SUCCESS;
}

/**
 * Open a file writing stream to write the next file in the zip archive.
 * If Zip64 is enabled, this file will always be written with Zip64 data.
 */
enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method)
{
  enum zip_error result;

  zp->zip64_current = zp->zip64_enabled;
  result = zip_write_open_stream(zp, name, method, ZIP_S_WRITE_STREAM);
  if(result)
    goto err_out;

  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_open_file_stream", result);
  return result;
}

/**
 * Like zip_write_open_file_stream, but allows the direct writing of
 * uncompressed files to memory (deflate is not supported by this method).
 * This is abusable, but quicker than the alternatives. The exact filesize
 * of the file to be written must be provided to this function.
 *
 * ZIP_NOT_MEMORY_ARCHIVE will be silently returned if the current archive
 * doesn't support this; use regular functions instead in this case.
 */
enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name, size_t length)
{
  enum zip_error result = zip_write_autodetect_zip64(zp, ZIP_M_NONE,
   ZIP_S_WRITE_MEMSTREAM, length);
  if(result)
    goto err_out;

  result = zip_write_open_stream(zp, name, ZIP_M_NONE, ZIP_S_WRITE_MEMSTREAM);
  if(result)
    goto err_out;

  if(!vfile_get_memfile_block(zp->vf, length, mf))
  {
    result = ZIP_EOF;
    goto err_out;
  }
  return ZIP_SUCCESS;

err_out:
  if(result != ZIP_NOT_MEMORY_ARCHIVE &&
   result != ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM)
    zip_error("zip_write_open_file_stream", result);
  mfopen(NULL, 0, mf);
  return result;
}

/**
 * Close a file writing stream.
 */
enum zip_error zip_write_close_stream(struct zip_archive *zp)
{
  struct zip_file_header *fh;
  enum zip_error result;

  result = (zp ? zp->write_stream_error : ZIP_NULL);
  if(result)
    goto err_out;

  if(zp->stream)
  {
    struct zip_stream_data *stream_data = zp->stream_data;
    size_t final_in = 0;
    size_t final_out = 0;
    size_t write_len = 0;

    result = zwrite_finish(&write_len, zp);
    if(result)
      goto err_out;

    zp->stream_left += write_len;
    zp->stream->close(stream_data, &final_in, &final_out);

    if(final_in != zp->streaming_file->uncompressed_size)
      warn("uncompressed size mismatch: %zu != %zu (THIS IS AN MZX BUG)\n",
       final_in, (size_t)zp->streaming_file->uncompressed_size);
    if(final_out != zp->stream_left)
      warn("compressed size mismatch: %zu != %zu (THIS IS AN MZX BUG)\n",
       final_out, (size_t)zp->stream_left);
  }

  // Add missing fields to the header.
  fh = zp->streaming_file;
  fh->crc32 = zp->stream_crc32;
  fh->compressed_size = zp->stream_left;

  // Write the missing fields to the local header.
  result = zip_write_data_descriptor(zp, fh);
  if(result)
    goto err_out;

  zp->pos++;

  // Clean up the stream
  zp->mode = ZIP_S_WRITE_FILES;
  zp->streaming_file = NULL;
  zp->stream_buffer_end = 0;
  zp->stream_left = 0;
  zp->stream_crc32 = 0;

  precalculate_write_errors(zp);
  return ZIP_SUCCESS;

err_out:
  zip_error("zip_write_close_stream", result);
  return result;
}

/**
 * Close a memory writing stream. Unlike with reading, this currently needs its
 * own function still (to receive the memfile pointer indicating the write).
 */
enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf)
{
  unsigned char *start = mf->start;
  unsigned char *end = mf->current;
  size_t length = end - start;

  enum zip_error result =
   !zp ? ZIP_NULL :
   zp->mode != ZIP_S_WRITE_MEMSTREAM ? ZIP_INVALID_STREAM_WRITE :
   ZIP_SUCCESS;

  if(result)
    goto err_out;

  // Compute some things that a regular stream would have here but are missing
  // from mem streams.
  zp->stream_crc32 = crc32(0, start, length);
  zp->stream_left = length;
  zp->streaming_file->uncompressed_size = length;

  // Increment the program position (since direct writing was used)
  vfseek(zp->vf, mf->current - mf->start, SEEK_CUR);

  // Now the regular function can be used...
  zp->write_stream_error = ZIP_SUCCESS;
  return zip_write_close_stream(zp);

err_out:
  zip_error("zip_write_close_mem_stream", result);
  return result;
}

/**
 * Write a file to a zip archive.
 * Currently handled methods: ZIP_M_NONE, ZIP_M_DEFLATE
 *
 * If Zip64 is enabled, then Zip64 will be automatically selected for this
 * file based on its total length and its compressed size bound (if applicable).
 */
enum zip_error zip_write_file(struct zip_archive *zp, const char *name,
 const void *src, size_t srcLen, int method)
{
  enum zip_error result;

  // No need to check mode; the functions used here will

  // Attempting to DEFLATE a small file? Store instead...
  if(srcLen < 256 && method == ZIP_M_DEFLATE)
    method = ZIP_M_NONE;

  result = zip_write_autodetect_zip64(zp, method, ZIP_S_WRITE_STREAM, srcLen);
  if(result)
    goto err_out;

  result = zip_write_open_stream(zp, name, method, ZIP_S_WRITE_STREAM);
  if(result)
    goto err_out;

  result = zwrite_direct(src, srcLen, zp);
  if(result && result != ZIP_EOF)
    goto err_close;

  result = zip_write_close_stream(zp);
  if(result)
    goto err_out;

  return ZIP_SUCCESS;

err_close:
  zip_write_close_stream(zp);

err_out:
  zip_error("zip_write_file", result);
  return result;
}

/**
 * Reads the central directory of a zip archive. This places the archive into
 * file read mode; read files using zip_read_file(). If this fails, the input
 * is probably not actually a zip archive or uses features we don't support.
 */

struct zip_eocd
{
  uint32_t disk_start_of_central_directory;
  uint32_t disk_eocd;
  uint64_t records_on_disk;
  uint64_t records_total;
  uint64_t size_central_directory;
  uint64_t offset_central_directory;
  uint64_t zip64_eocd_offset;
};

static char eocd_sig[] =
{
  0x50,
  0x4b,
  0x05,
  0x06
};

static char zip64_eocd_sig[] =
{
  0x50,
  0x4b,
  0x06,
  0x06,
};

static char zip64_locator_sig[] =
{
  0x50,
  0x4b,
  0x06,
  0x07
};

static enum zip_error zip_find_eocd(struct zip_archive *zp)
{
  vfile *vf = zp->vf;
  int i;
  int j;
  int n;

  // Go to the latest position the EOCD might be.
  if(vfseek(vf, (int64_t)(zp->end_in_file - EOCD_RECORD_LEN), SEEK_SET))
  {
    return ZIP_NO_EOCD;
  }

  // Find the end of central directory signature.
  i = 0;
  j = -EOCD_RECORD_LEN;
  do
  {
    n = vfgetc(vf);
    j++;

    if(n == eocd_sig[i])
    {
      i++;
      if(i == 4)
        // We've found the EOCD signature
        break;
    }

    else
    {
      if(n < 0)
        break;

      i = i ? i + 5 :
       (n == 0x06 ? 4 :
        n == 0x05 ? 3 :
        n == 0x4b ? 2 : 5);
      j -= i;

      if(vfseek(vf, -i, SEEK_CUR))
      {
        i = 0;
        break;
      }

      i = 0;
    }
  }
  // Max length of signature + comment.
  while(j >= -65557);

  if(i < 4)
  {
    return ZIP_NO_EOCD;
  }

  // Found the EOCD record signature                    4
  return ZIP_SUCCESS;
}

/**
 * Read the standard 22 byte EOCD record, minus the signature bytes.
 */
static enum zip_error zip_read_eocd(struct zip_archive *zp,
 struct zip_eocd *eocd)
{
  char buffer[EOCD_RECORD_LEN - 4];
  struct memfile mf;

  // Already read the first 4 signature bytes.
  if(!vfread(buffer, EOCD_RECORD_LEN - 4, 1, zp->vf))
    return ZIP_READ_ERROR;

  mfopen(buffer, EOCD_RECORD_LEN - 4, &mf);

  // Number of this disk                                2
  // Disk where central directory starts                2
  // Number of central directory records on this disk   2
  // Total number of central directory records          2
  eocd->disk_eocd                       = mfgetw(&mf);
  eocd->disk_start_of_central_directory = mfgetw(&mf);
  eocd->records_on_disk                 = mfgetw(&mf);
  eocd->records_total                   = mfgetw(&mf);

  // Size of central directory (bytes)                  4
  eocd->size_central_directory = mfgetud(&mf);

  // Offset to central directory from start of file     4
  eocd->offset_central_directory = mfgetud(&mf);

  // Comment length (ignore)                            2
  return ZIP_SUCCESS;
}

/**
 * Read the Zip64 EOCD record locator from the current position in the
 * stream, if it exists.
 */
static enum zip_error zip_read_zip64_eocd_locator(struct zip_archive *zp,
 struct zip_eocd *eocd)
{
  uint8_t buffer[ZIP64_LOCATOR_LEN];
  uint32_t disk_start_of_zip64_eocd;
  uint32_t disk_total;
  struct memfile mf;

  if(!vfread(buffer, ZIP64_LOCATOR_LEN, 1, zp->vf))
    return ZIP_READ_ERROR;

  if(memcmp(buffer, zip64_locator_sig, 4))
    return ZIP_NO_EOCD_ZIP64;

  mfopen(buffer + 4, ZIP64_LOCATOR_LEN - 4, &mf);

  // Disk where central directory starts                4
  disk_start_of_zip64_eocd = mfgetud(&mf);
  // Relative offset of the zip64 EOCD                  8
  eocd->zip64_eocd_offset = mfgetuq(&mf);
  // Total number of disks                              4
  disk_total = mfgetud(&mf);

  if(disk_start_of_zip64_eocd != 0 || disk_total != 1)
    return ZIP_UNSUPPORTED_MULTIPLE_DISKS;

  // Impossible to support with 64-bit stdio.
  if(eocd->zip64_eocd_offset > INT64_MAX)
    return ZIP_INVALID_ZIP64;

  return ZIP_SUCCESS;
}

/**
 * Read the Zip64 EOCD record. Despite APPNOTE.TXT implying that only the
 * fields dummied in the EOCD should be used, PKZIP 6.0 unconditionally
 * prefers ALL fields from this structure over the old fields when present.
 */
static enum zip_error zip_read_zip64_eocd(struct zip_archive *zp,
 struct zip_eocd *eocd)
{
  uint8_t buffer[ZIP64_EOCD_RECORD_LEN];
  struct memfile mf;
  uint64_t sz;
  uint16_t ver;

  if(!vfread(buffer, ZIP64_EOCD_RECORD_LEN, 1, zp->vf))
    return ZIP_READ_ERROR;

  if(memcmp(buffer, zip64_eocd_sig, 4))
    return ZIP_NO_EOCD_ZIP64;

  mfopen(buffer + 4, ZIP64_EOCD_RECORD_LEN - 4, &mf);
  // Size of Zip64 EOCD record                          8
  sz = mfgetuq(&mf);
  if(sz < ZIP64_EOCD_RECORD_LEN - 12)
    return ZIP_INVALID_ZIP64;

  // Version made by                                    2
  // Version needed to extract                          2
  mf.current += 2;
  ver = mfgetw(&mf);
  if(ZIP_VERSION(ver) != ZIP64_VERSION_MINIMUM)
    return ZIP_UNSUPPORTED_VERSION;

  // Number of this disk                                4
  eocd->disk_eocd = mfgetud(&mf);

  // Disk where central directory starts                4
  eocd->disk_start_of_central_directory = mfgetud(&mf);

  // Central directory entries on this disk             8
  // Total number of central directory entries          8
  eocd->records_on_disk = mfgetuq(&mf);
  eocd->records_total = mfgetuq(&mf);

  // Size of the central directory                      8
  eocd->size_central_directory = mfgetuq(&mf);

  // Offset of the central directory                    8
  eocd->offset_central_directory = mfgetuq(&mf);

  // Zip64 extensible data sector (ignore)
  return ZIP_SUCCESS;
}

/**
 * Read the EOCD record and central directory of the zip to memory.
 * This is required before the zip can be properly read.
 */
static enum zip_error zip_read_directory(struct zip_archive *zp)
{
  struct zip_eocd eocd;
  int64_t eocd_pos;
  size_t i, j;
  int result;

  result = zip_find_eocd(zp);
  if(result)
    goto err_out;

  eocd_pos = vftell(zp->vf) - 4;
  if(eocd_pos < 0)
  {
    result = ZIP_SEEK_ERROR;
    goto err_out;
  }

  // Fix bogus old GCC warnings
  memset(&eocd, 0, sizeof(eocd));

  result = zip_read_eocd(zp, &eocd);
  if(result != ZIP_SUCCESS)
    goto err_out;

  // Check for zip64 locator.
  if(eocd.records_total && eocd_pos >= ZIP64_LOCATOR_LEN)
  {
    if(vfseek(zp->vf, eocd_pos - ZIP64_LOCATOR_LEN, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_out;
    }

    result = zip_read_zip64_eocd_locator(zp, &eocd);
    if(result == ZIP_SUCCESS && (uint64_t)eocd_pos >= eocd.zip64_eocd_offset)
    {
      if(vfseek(zp->vf, eocd.zip64_eocd_offset, SEEK_SET))
      {
        result = ZIP_SEEK_ERROR;
        goto err_out;
      }

      result = zip_read_zip64_eocd(zp, &eocd);
      if(result != ZIP_SUCCESS)
        goto err_out;
    }
    else

    if(result != ZIP_NO_EOCD_ZIP64)
      goto err_out;
  }

  // Test for unsupported features like multiple disks.
  if(eocd.disk_eocd != 0 || eocd.disk_start_of_central_directory != 0 ||
   eocd.records_on_disk != eocd.records_total)
  {
    result = ZIP_UNSUPPORTED_MULTIPLE_DISKS;
    goto err_out;
  }

  if(eocd.records_total >= ZIP_MAX_NUM_FILES)
  {
    result = ZIP_UNSUPPORTED_NUMBER_OF_ENTRIES;
    goto err_out;
  }

  if((uint64_t)eocd_pos < eocd.offset_central_directory)
  {
    result = ZIP_NO_CENTRAL_DIRECTORY;
    goto err_out;
  }

  zp->num_files = eocd.records_total;
  zp->size_central_directory = eocd.size_central_directory;
  zp->offset_central_directory = eocd.offset_central_directory;

  // Load central directory records
  if(zp->num_files)
  {
    size_t n = zp->num_files;
    struct zip_file_header **f =
     (struct zip_file_header **)calloc(n, sizeof(struct zip_file_header *));
    zp->files = f;
    if(!f)
    {
      result = ZIP_ALLOC_ERROR;
      goto err_out;
    }

    // Go to the start of the central directory.
    if(vfseek(zp->vf, zp->offset_central_directory, SEEK_SET))
    {
      result = ZIP_SEEK_ERROR;
      goto err_realloc;
    }

    for(i = 0, j = 0; i < n; i++)
    {
      result = zip_read_central_file_header(zp, &(f[j]));
      if(result)
      {
        zip_free_file_header(f[j]);
        f[j] = NULL;
        if(result == ZIP_IGNORE_FILE || result == ZIP_UNSUPPORTED_VERSION)
        {
          zp->num_files--;
          continue;
        }

        zip_error("error reading central directory record", result);
        break;
      }
      zp->files_alloc++;
      j++;
    }

    if(zp->files_alloc == 0)
    {
      result = ZIP_NO_CENTRAL_DIRECTORY;
      goto err_realloc;
    }

    if(zp->files_alloc < zp->num_files)
    {
      warn("expected %zu central directory records but only found %zu\n",
       n, zp->files_alloc);
      result = ZIP_INCOMPLETE_CENTRAL_DIRECTORY;
      goto err_realloc;
    }

    // Shrink file header array.
    if(zp->files_alloc < n)
    {
      f = (struct zip_file_header **)realloc(zp->files,
       zp->files_alloc * sizeof(struct zip_file_header *));
      if(f)
        zp->files = f;
    }
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;
  precalculate_read_errors(zp);

  // At this point, we're probably at the EOCD. Reading files will seek
  // to the start of their respective entries, so just leave it alone.
  return ZIP_SUCCESS;

err_realloc:
  zp->num_files = zp->files_alloc;

  // Shrink file header array.
  if(zp->files_alloc)
  {
    struct zip_file_header **f = (struct zip_file_header **)realloc(zp->files,
     zp->files_alloc * sizeof(struct zip_file_header *));
    if(f)
      zp->files = f;
  }
  else
  {
    free(zp->files);
    zp->files = NULL;
  }

  // We're in file read mode now.
  zp->mode = ZIP_S_READ_FILES;
  precalculate_read_errors(zp);

err_out:
  zip_error("zip_read_directory", result);
  return result;
}

/**
 * Write the EOCD during the archive close.
 */
static enum zip_error zip_write_eocd_record(struct zip_archive *zp)
{
  char buffer[EOCD_RECORD_LEN];
  struct memfile mf;
  uint32_t size;
  uint32_t offset;
  uint16_t num_files;
  int i;

  // Memfiles: make sure there's enough space
  if(zp->is_memory && zip_ensure_capacity(EOCD_RECORD_LEN, zp))
    return ZIP_EOF;

  // Clamp these values for Zip64.
  num_files = MIN(zp->num_files, 0xffff);
  size = MIN(zp->size_central_directory, 0xfffffffful);
  offset = MIN(zp->offset_central_directory, 0xfffffffful);

  mfopen_wr(buffer, EOCD_RECORD_LEN, &mf);

  // Signature                                          4
  for(i = 0; i<4; i++)
    mfputc(eocd_sig[i], &mf);

  // Number of this disk                                2
  mfputw(0, &mf);

  // Disk where central directory starts                2
  mfputw(0, &mf);

  // Number of central directory records on this disk   2
  mfputw(num_files, &mf);

  // Total number of central directory records          2
  mfputw(num_files, &mf);

  // Size of central directory                          4
  mfputud(size, &mf);

  // Offset of central directory                        4
  mfputud(offset, &mf);

  // Comment length                                     2
  mfputw(0, &mf);

  // Comment (length is zero)

  if(!vfwrite(buffer, EOCD_RECORD_LEN, 1, zp->vf))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
}

/**
 * Test if a Zip64 EOCD needs to be emitted.
 * An archive can use Zip64 extra data features without needing a Zip64 EOCD.
 */
static boolean zip_write_zip64_is_required(struct zip_archive *zp)
{
  if(zp->num_files >= 0xffff)
    return true;
  if(zp->size_central_directory >= 0xfffffffful)
    return true;
  if(zp->offset_central_directory >= 0xfffffffful)
    return true;

  return false;
}

/**
 * Write the Zip64 EOCD and locator during the archive close.
 */
static enum zip_error zip_write_zip64_eocd_record(struct zip_archive *zp)
{
  char buffer[ZIP64_EOCD_RECORD_LEN + ZIP64_LOCATOR_LEN];
  struct memfile mf;
  int64_t zip64_eocd_offset = vftell(zp->vf);

  if(zp->is_memory && zip_ensure_capacity(sizeof(buffer), zp))
    return ZIP_EOF;

  if(zip64_eocd_offset < 0)
    return ZIP_SEEK_ERROR;

  mfopen_wr(buffer, sizeof(buffer), &mf);

  // Signature                                          4
  mfwrite(zip64_eocd_sig, 4, 1, &mf);

  // Size of Zip64 end of central directory record      8
  mfputuq(ZIP64_EOCD_RECORD_LEN - 12, &mf);

  // Version made by                                    2
  // Version needed to extract                          2
  mfputw(ZIP64_VERSION_MINIMUM, &mf);
  mfputw(ZIP64_VERSION_MINIMUM, &mf);

  // Number of this disk                                4
  // Disk where central directory starts                4
  mfputud(0, &mf);
  mfputud(0, &mf);

  // Number of central directory records on this disk   8
  // Total number of central directory records          8
  mfputuq(zp->num_files, &mf);
  mfputuq(zp->num_files, &mf);

  // Size of the central directory                      8
  // Offset of start of central directory               8
  mfputuq(zp->size_central_directory, &mf);
  mfputuq(zp->offset_central_directory, &mf);

  // Extensible data sector                             0

  // Signature                                          4
  mfwrite(zip64_locator_sig, 4, 1, &mf);

  // Disk where the Zip64 EOCD is located               4
  mfputud(0, &mf);

  // Offset of the Zip64 EOCD record                    8
  mfputq(zip64_eocd_offset, &mf);

  // Total number of disks                              4
  mfputud(1, &mf);

  assert(mftell(&mf) == sizeof(buffer));
  if(!vfwrite(buffer, sizeof(buffer), 1, zp->vf))
    return ZIP_WRITE_ERROR;

  return ZIP_SUCCESS;
}

/**
 * Attempts to close the zip archive, and when writing, constructs the central
 * directory and EOCD record. Upon returning ZIP_SUCCESS, *final_length will be
 * set to the final length of the file.
 */
enum zip_error zip_close(struct zip_archive *zp, uint64_t *final_length)
{
  int result = ZIP_SUCCESS;
  int mode;
  size_t i;

  if(!zp)
    return ZIP_NULL;

  // On the off chance someone actually tries this...
  if(zp->is_memory && final_length &&
   (void *)final_length == (void *)zp->external_buffer_size)
  {
    warn(
      "zip_close: Detected use of external buffer size pointer as final_length "
      "(should provide NULL instead). Report this!\n"
    );
    final_length = NULL;
  }

  mode = zp->mode;

  // Before initiating the close, make sure there wasn't an open write stream!
  if(zp->streaming_file && mode == ZIP_S_WRITE_STREAM)
  {
    warn("zip_close called while writing file stream!\n");
    zip_write_close_stream(zp);
    mode = ZIP_S_WRITE_FILES;
  }

  if(mode == ZIP_S_WRITE_FILES)
  {
    uint64_t size_cd = zp->running_file_name_length;
    size_t size_eocd = EOCD_RECORD_LEN;

    for(i = 0; i < zp->num_files; i++)
    {
      struct zip_file_header *fh = zp->files[i];
      boolean is_zip64 = false;
      if(fh->uncompressed_size >= 0xfffffffful)
      {
        is_zip64 = true;
        size_cd += 8;
      }
      if(fh->compressed_size >= 0xfffffffful)
      {
        is_zip64 = true;
        size_cd += 8;
      }
      if(fh->offset >= 0xfffffffful)
      {
        is_zip64 = true;
        size_cd += 8;
      }
      size_cd += CENTRAL_FILE_HEADER_LEN + (is_zip64 ? 4 : 0);
    }

    zp->offset_central_directory = vftell(zp->vf);
    zp->size_central_directory = size_cd;

    if(zip_write_zip64_is_required(zp))
      size_eocd += ZIP64_EOCD_RECORD_LEN + ZIP64_LOCATOR_LEN;

    // Calculate projected file size in case more space is needed
    if(final_length)
      *final_length = zp->offset_central_directory + size_cd + size_eocd;

    // Ensure there's enough space to close the file
    if(zp->is_memory && zip_ensure_capacity(size_cd + size_eocd, zp))
    {
      result = ZIP_EOF;
      mode = ZIP_S_ERROR;
    }
  }

  zp->pos = 0;
  for(i = zp->pos; i < zp->num_files; i++)
  {
    struct zip_file_header *fh = zp->files[i];

    if(fh)
    {
      // Write the central directory
      if(mode == ZIP_S_WRITE_FILES)
      {
        result = zip_write_file_header(zp, fh, true);
        if(result)
        {
          // Not much that can be done at this point.
          mode = ZIP_S_ERROR;
        }
      }
      zip_free_file_header(fh);
    }
  }

  // Write the end of central directory record
  if(mode == ZIP_S_WRITE_FILES || mode == ZIP_S_WRITE_UNINITIALIZED)
  {
    size_t end_pos;
    uint64_t actual_size_cd = vftell(zp->vf) - zp->offset_central_directory;
    if(mode == ZIP_S_WRITE_FILES)
      assert(actual_size_cd == zp->size_central_directory);
    zp->size_central_directory = actual_size_cd;

    if(zip_write_zip64_is_required(zp))
      zip_write_zip64_eocd_record(zp);

    result = zip_write_eocd_record(zp);
    end_pos = vftell(zp->vf);

    if(final_length)
      *final_length = end_pos;

    // Just to be sure, close the vfile before attempting any changes to the
    // external buffer if this is an expandable memory ZIP.
    vfclose(zp->vf);

    // Reduce the size of the buffer if this is an expandable memory zip.
    if(zp->is_memory && zp->external_buffer && zp->external_buffer_size &&
     *(zp->external_buffer_size) > end_pos)
    {
      void *tmp = realloc(*(zp->external_buffer), end_pos);
      if(tmp)
      {
        *(zp->external_buffer) = tmp;
        *(zp->external_buffer_size) = end_pos;
      }
    }
  }
  else
  {
    vfclose(zp->vf);
    if(final_length)
      *final_length = zp->end_in_file;
  }

  for(i = 0; i < ARRAY_SIZE(zp->stream_data_ptrs); i++)
    if(zip_method_handlers[i] && zp->stream_data_ptrs[i])
      zip_method_handlers[i]->destroy(zp->stream_data_ptrs[i]);

  free(zp->header_buffer);
  free(zp->stream_buffer);
  free(zp->files);
  free(zp);

  if(result != ZIP_SUCCESS)
    zip_error("zip_close", result);

  return result;
}

/**
 * Perform additional setup for write mode.
 */
static boolean zip_init_for_write(struct zip_archive *zp, int num_files)
{
  zp->files_alloc = num_files;
  zp->files = (struct zip_file_header **)malloc(num_files *
   sizeof(struct zip_file_header *));

  if(!zp->files)
    return false;

  zp->header_buffer = (uint8_t *)malloc(ZIP_DEFAULT_HEADER_BUFFER);
  zp->header_buffer_alloc = ZIP_DEFAULT_HEADER_BUFFER;
  zp->header_timestamp = zip_get_dos_date_time();
  zp->running_file_name_length = 0;
  zp->zip64_enabled = true;

  if(!zp->header_buffer)
  {
    free(zp->files);
    return false;
  }

  zp->mode = ZIP_S_WRITE_UNINITIALIZED;
  return true;
}

/**
 * Set up a new zip archive struct.
 */
static struct zip_archive *zip_new_archive(void)
{
  struct zip_archive *zp =
   (struct zip_archive *)calloc(1, sizeof(struct zip_archive));

  zp->mode = ZIP_S_READ_UNINITIALIZED;
  return zp;
}

/**
 * Open a zip archive located in a file for reading. Returns a zip_archive
 * pointer if this archive is ready for file reading; otherwise, returns
 * NULL. On failure, this will also close the file pointer.
 */
struct zip_archive *zip_open_vf_read(vfile *vf)
{
  if(vf)
  {
    struct zip_archive *zp = zip_new_archive();
    int64_t file_len;

    if(!zp)
      return NULL;

    zp->vf = vf;
    file_len = vfilelength(zp->vf, false);

    if(file_len < 0)
    {
      zip_error("zip_open_vf_read", ZIP_STAT_ERROR);
      vfclose(vf);
      free(zp);
      return NULL;
    }

    zp->end_in_file = file_len;

    if(ZIP_SUCCESS != zip_read_directory(zp))
    {
      zip_close(zp, NULL);
      return NULL;
    }

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

struct zip_archive *zip_open_file_read(const char *file_name)
{
  vfile *vf = vfopen_unsafe(file_name, "rb");

  return zip_open_vf_read(vf);
}

/**
 * Open a zip archive located in a file for writing. The archive will be in
 * raw write mode, for use with zip_write(), until zip_write_file() is called.
 * Afterward, the archive will be in file write mode.
 */
struct zip_archive *zip_open_vf_write(vfile *vf)
{
  if(vf)
  {
    struct zip_archive *zp = zip_new_archive();
    if(!zp)
      return NULL;

    zp->vf = vf;

    if(!zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES))
    {
      free(zp);
      return NULL;
    }

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

struct zip_archive *zip_open_file_write(const char *file_name)
{
  vfile *vf = vfopen_unsafe(file_name, "wb");

  return zip_open_vf_write(vf);
}

/**
 * Open a zip archive located in memory for reading. Returns a zip_archive
 * pointer if this archive is ready for file reading; otherwise, returns
 * NULL.
 */
struct zip_archive *zip_open_mem_read(const void *src, size_t len)
{
  if(src && len > 0)
  {
    struct zip_archive *zp = zip_new_archive();
    if(!zp)
      return NULL;

    zp->vf = vfile_init_mem((void *)src, len, "rb");
    zp->is_memory = true;
    zp->end_in_file = len;

    if(ZIP_SUCCESS != zip_read_directory(zp))
    {
      zip_close(zp, NULL);
      return NULL;
    }

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

/**
 * Open a zip archive for writing to a block of memory. Returns a zip_archive
 * upon success; otherwise, returns NULL. An optional offset can be specified
 * indicated the position in the file to begin writing ZIP data.
 */
struct zip_archive *zip_open_mem_write(void *src, size_t len, size_t start_pos)
{
  if(src && len > 0 && start_pos < len)
  {
    struct zip_archive *zp = zip_new_archive();
    if(!zp)
      return NULL;

    zp->vf = vfile_init_mem(src, len, "wb");
    zp->is_memory = true;
    if(!zp->vf)
    {
      free(zp);
      return NULL;
    }

    if(!zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES))
    {
      vfclose(zp->vf);
      free(zp);
      return NULL;
    }
    vfseek(zp->vf, start_pos, SEEK_SET);

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

/**
 * Open a zip archive for writing to a block of memory. See the above function.
 *
 * The locations described by external_buffer and external_buffer_size must be
 * initialized before this function is called. If this archive exceeds the size
 * of its buffer, it will automatically attempt to reallocate the buffer.
 */
struct zip_archive *zip_open_mem_write_ext(void **external_buffer,
 size_t *external_buffer_size, size_t start_pos)
{
  if(external_buffer && external_buffer_size)
  {
    struct zip_archive *zp = zip_new_archive();
    if(!zp)
      return NULL;

    // Allow expansion to be managed by vio.c. The external buffer pointers are
    // stored here as well, so zip_close can shrink the buffer.
    zp->vf = vfile_init_mem_ext(external_buffer, external_buffer_size, "wb");
    zp->external_buffer = external_buffer;
    zp->external_buffer_size = external_buffer_size;
    zp->is_memory = true;

    if(!zp->vf)
    {
      free(zp);
      return NULL;
    }

    if(!zip_init_for_write(zp, ZIP_DEFAULT_NUM_FILES))
    {
      vfclose(zp->vf);
      free(zp);
      return NULL;
    }
    vfseek(zp->vf, start_pos, SEEK_SET);

    precalculate_read_errors(zp);
    precalculate_write_errors(zp);
    return zp;
  }
  return NULL;
}

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

#ifndef MEGAZEUX_ZIP_H
#define MEGAZEUX_ZIP_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stdint.h>

// This needs to stay self-sufficient.
// Don't use core features, don't extern anything.

#include "vfile.h"

/**
 * Methods universally supported by MZX are 0 (store) and 8 (Deflate).
 * Emscripten and utils builds also enable decompressors for 1 (Shrink),
 * 2-5 (Reduce), 6 (Implode), and 9 (Deflate64).
 */
enum zip_compression_method
{
  ZIP_M_NONE                    = 0, // Store
  ZIP_M_SHRUNK                  = 1,
  ZIP_M_REDUCED_1               = 2, // Reduced with compression factor 1
  ZIP_M_REDUCED_2               = 3, // Reduced with compression factor 2
  ZIP_M_REDUCED_3               = 4, // Reduced with compression factor 3
  ZIP_M_REDUCED_4               = 5, // Reduced with compression factor 4
  ZIP_M_IMPLODED                = 6,
  ZIP_M_DEFLATE                 = 8,
  ZIP_M_DEFLATE64               = 9,
  ZIP_M_DCL_IMPLODING           = 10, // Old IBM TERSE
  ZIP_M_BZIP2                   = 12,
  ZIP_M_LZMA                    = 14,
  ZIP_M_IBM_TERSE               = 18,
  ZIP_M_LZ77                    = 19,
  ZIP_M_WAVPACK                 = 97,
  ZIP_M_PPMD                    = 98,
};

#define ZIP_M_MAX_SUPPORTED ZIP_M_DEFLATE64

enum zip_general_purpose_flag
{
  ZIP_F_ENCRYPTED           = (1<<0), // Indicates that a file is encrypted.
  ZIP_F_COMPRESSION_1       = (1<<1), // Varies.
  ZIP_F_COMPRESSION_2       = (1<<2), // Varies.
  ZIP_F_DATA_DESCRIPTOR     = (1<<3), // Sizes and CRC32 stored after file data.
  ZIP_F_ENHANCED_DEFLATE    = (1<<4), // Reserved for Deflate64.
  ZIP_F_COMPRESSED_PATCHED  = (1<<5), // ?????
  ZIP_F_STRONG_ENCRYPTION   = (1<<6), // Indicates strong encryption is used.
  ZIP_F_UNUSED_7            = (1<<7),
  ZIP_F_UNUSED_8            = (1<<8),
  ZIP_F_UNUSED_9            = (1<<9),
  ZIP_F_UNUSED_10           = (1<<10),
  ZIP_F_LANGUAGE_ENCODING   = (1<<11), // Indicates UTF-8 filename/comments.
  ZIP_F_UNUSED_12           = (1<<12),
  ZIP_F_MASKED_HEADER_DATA  = (1<<13), // See: strong encryption.
  ZIP_F_UNUSED_14           = (1<<14),
  ZIP_F_UNUSED_15           = (1<<15)
};

// These flags are allowed for all DEFLATE and stored files we care about.
#define ZIP_F_ALLOWED     (\
  ZIP_F_DATA_DESCRIPTOR   |\
  ZIP_F_COMPRESSION_1     |\
  ZIP_F_COMPRESSION_2     |\
  ZIP_F_LANGUAGE_ENCODING )

// Some ancient archives from unknown sources seem to have set the unused
// flags in places. These should be safe to ignore.
#define ZIP_F_UNUSED (ZIP_F_UNUSED_7 | ZIP_F_UNUSED_8 | ZIP_F_UNUSED_9 |\
 ZIP_F_UNUSED_10 | ZIP_F_UNUSED_12 | ZIP_F_UNUSED_14 | ZIP_F_UNUSED_15)

// DEFLATE-specific compression flags.
#define ZIP_F_DEFLATE_MAXIMUM   ZIP_F_COMPRESSION_1
#define ZIP_F_DEFLATE_FAST      ZIP_F_COMPRESSION_2

enum zip_internal_state
{
  ZIP_S_READ_UNINITIALIZED,
  ZIP_S_READ_FILES,
  ZIP_S_READ_STREAM,
  ZIP_S_READ_MEMSTREAM,
  ZIP_S_WRITE_UNINITIALIZED,
  ZIP_S_WRITE_FILES,
  ZIP_S_WRITE_STREAM,
  ZIP_S_WRITE_MEMSTREAM,
  ZIP_S_ERROR,
};

enum zip_error
{
  ZIP_SUCCESS = 0,
  ZIP_IGNORE_FILE,
  ZIP_EOF,
  ZIP_NULL,
  ZIP_NULL_BUF,
  ZIP_ALLOC_ERROR,
  ZIP_STAT_ERROR,
  ZIP_SEEK_ERROR,
  ZIP_READ_ERROR,
  ZIP_WRITE_ERROR,
  ZIP_BOUND_ERROR,
  ZIP_INVALID_READ_IN_WRITE_MODE,
  ZIP_INVALID_WRITE_IN_READ_MODE,
  ZIP_INVALID_FILE_READ_UNINITIALIZED,
  ZIP_INVALID_FILE_READ_IN_STREAM_MODE,
  ZIP_INVALID_FILE_WRITE_IN_STREAM_MODE,
  ZIP_INVALID_STREAM_READ,
  ZIP_INVALID_STREAM_WRITE,
  ZIP_NOT_MEMORY_ARCHIVE,
  ZIP_NO_EOCD,
  ZIP_NO_EOCD_ZIP64,
  ZIP_NO_CENTRAL_DIRECTORY,
  ZIP_INCOMPLETE_CENTRAL_DIRECTORY,
  ZIP_UNSUPPORTED_VERSION,
  ZIP_UNSUPPORTED_NUMBER_OF_ENTRIES,
  ZIP_UNSUPPORTED_MULTIPLE_DISKS,
  ZIP_UNSUPPORTED_FLAGS,
  ZIP_UNSUPPORTED_DECOMPRESSION,
  ZIP_UNSUPPORTED_COMPRESSION,
  ZIP_UNSUPPORTED_DECOMPRESSION_STREAM,
  ZIP_UNSUPPORTED_COMPRESSION_STREAM,
  ZIP_UNSUPPORTED_METHOD_MEMORY_STREAM,
  ZIP_NO_ZIP64_EXTRA_DATA,
  ZIP_INVALID_ZIP64,
  ZIP_MISSING_LOCAL_HEADER,
  ZIP_HEADER_MISMATCH,
  ZIP_CRC32_MISMATCH,
  ZIP_DECOMPRESS_FAILED,
  ZIP_COMPRESS_FAILED,
  ZIP_INPUT_EMPTY,
  ZIP_OUTPUT_FULL,
  ZIP_STREAM_FINISHED,
};

struct zip_file_header
{
  uint16_t flags;
  uint16_t method;
  uint32_t crc32;
  uint64_t compressed_size;
  uint64_t uncompressed_size;
  uint64_t offset;
  uint32_t mzx_file_id;
  uint8_t mzx_board_id;
  uint8_t mzx_robot_id;
  uint16_t file_name_length;
  // This struct is allocated with an extended area for the filename.
  // This field MUST be at least 4-aligned and it must be the last field in the
  // struct (any extra padding after will be used).
  char file_name[1];
};

/**
 * Data for an active ZIP (de)compression stream.
 */
struct zip_stream_data
{
  // Current position and remaining size of the input buffer.
  const uint8_t *input_buffer;
  size_t input_length;

  // Current position and remaining size of the output buffer.
  uint8_t *output_buffer;
  size_t output_length;

  // Final input and output size. zlib only supports up to 32-bit
  // sizes on 32-bit systems and the legacy methods shouldn't be big
  // enough to matter, so size_t is fine here for now.
  size_t final_input_length;
  size_t final_output_length;

  // Has this stream been initialized?
  boolean is_initialized;

  // Is this stream finished?
  boolean finished;

  // Is this a compression stream? (false for a decompression stream)
  boolean is_compression_stream;
};

struct zip_method_handler;

struct zip_archive
{
  uint8_t mode;

  size_t pos;
  size_t num_files; // ZIP supports 2^64-1 files, but this needs to fit in RAM.
  size_t files_alloc;
  uint64_t size_central_directory;
  uint64_t offset_central_directory;

  uint8_t *header_buffer;
  uint32_t header_buffer_alloc;
  uint32_t header_timestamp;
  uint32_t running_file_name_length;

  struct zip_file_header **files;
  struct zip_file_header *streaming_file;
  uint8_t *stream_buffer;
  uint32_t stream_buffer_pos;
  uint32_t stream_buffer_end;
  uint32_t stream_buffer_alloc;
  uint64_t stream_crc_position;
  uint64_t stream_zip64_position;
  uint64_t stream_u_left;
  uint64_t stream_left;
  uint32_t stream_crc32;

  uint32_t start_in_file;
  uint64_t end_in_file;

  enum zip_error read_file_error;
  enum zip_error read_stream_error;
  enum zip_error write_file_error;
  enum zip_error write_stream_error;

  vfile *vf;

  boolean is_memory;
  boolean zip64_enabled; // Zip64 should be used as-needed.
  boolean zip64_current; // Zip64 is active for current file.
  void **external_buffer;
  size_t *external_buffer_size;

  const struct zip_method_handler *stream;
  struct zip_stream_data *stream_data;
  struct zip_stream_data *stream_data_ptrs[ZIP_M_MAX_SUPPORTED + 1];
};

UTILS_LIBSPEC int zip_bound_deflate_usage(size_t length);
UTILS_LIBSPEC int zip_bound_total_header_usage(int num_files, int max_name_size);

UTILS_LIBSPEC enum zip_error zread(void *destBuf, size_t readLen,
 struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_get_next_name(struct zip_archive *zp,
 char *name, int name_buffer_size);

UTILS_LIBSPEC enum zip_error zip_get_next_mzx_file_id(struct zip_archive *zp,
 unsigned int *prop_id, unsigned int *board_id, unsigned int *robot_id);

UTILS_LIBSPEC enum zip_error zip_get_next_method(struct zip_archive *zp,
 unsigned int *method);

UTILS_LIBSPEC enum zip_error zip_get_next_uncompressed_size(
 struct zip_archive *zp, size_t *u_size);
UTILS_LIBSPEC enum zip_error zip_get_next_uncompressed_size64(
 struct zip_archive *zp, uint64_t *u_size);

UTILS_LIBSPEC enum zip_error zip_read_open_file_stream(struct zip_archive *zp,
 uint64_t *destLen);

UTILS_LIBSPEC enum zip_error zip_read_close_stream(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_read_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

UTILS_LIBSPEC enum zip_error zip_rewind(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_skip_file(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_read_file(struct zip_archive *zp,
 void *destBuf, size_t destLen, size_t *readLen);

UTILS_LIBSPEC enum zip_error zwrite(const void *src, size_t srcLen,
 struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_set_zip64_enabled(struct zip_archive *zp,
 boolean enable_zip64);

UTILS_LIBSPEC enum zip_error zip_write_open_file_stream(struct zip_archive *zp,
 const char *name, int method);

UTILS_LIBSPEC enum zip_error zip_write_close_stream(struct zip_archive *zp);

UTILS_LIBSPEC enum zip_error zip_write_open_mem_stream(struct zip_archive *zp,
 struct memfile *mf, const char *name, size_t length);

UTILS_LIBSPEC enum zip_error zip_write_close_mem_stream(struct zip_archive *zp,
 struct memfile *mf);

UTILS_LIBSPEC enum zip_error zip_write_file(struct zip_archive *zp,
 const char *name, const void *src, size_t srcLen, int method);

UTILS_LIBSPEC enum zip_error zip_close(struct zip_archive *zp,
 uint64_t *final_length);

UTILS_LIBSPEC struct zip_archive *zip_open_vf_read(vfile *vf);
UTILS_LIBSPEC struct zip_archive *zip_open_vf_write(vfile *vf);
UTILS_LIBSPEC struct zip_archive *zip_open_file_read(const char *file_name);
UTILS_LIBSPEC struct zip_archive *zip_open_file_write(const char *file_name);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_read(const void *src, size_t len);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_write(void *src, size_t len,
 size_t start_pos);
UTILS_LIBSPEC struct zip_archive *zip_open_mem_write_ext(void **external_buffer,
 size_t *external_buffer_size, size_t start_pos);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ZIP_H */

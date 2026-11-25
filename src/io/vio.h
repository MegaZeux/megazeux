/* MegaZeux
 *
 * Copyright (C) 2019-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_VIO_H
#define MEGAZEUX_IO_VIO_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "vfile.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

enum vfileflags
{
  /* Internal flags. */
  VF_FILE               = (1<<0),
  VF_MEMORY             = (1<<1),
  VF_MEMORY_EXPANDABLE  = (1<<2),
  VF_MEMORY_FREE        = (1<<3), // Free memory buffer on vfclose.
  VF_READ               = (1<<4),
  VF_WRITE              = (1<<5),
  VF_APPEND             = (1<<6),
  VF_BINARY             = (1<<7),
  VF_TRUNCATE           = (1<<8),
  VF_VIRTUAL            = (1<<9), // Virtual or cached file.

  /* Public flags. */
  V_DONT_CACHE   = (1<<27), // do not add this file to the cache.
  V_FORCE_CACHE  = (1<<28), // ignore the auto cache settings, always cache.
  V_SMALL_BUFFER = (1<<29), // setvbuf <= 256 for real files in binary mode.
  V_LARGE_BUFFER = (1<<30), // setvbuf >= 8192 for real files in binary mode.

  VF_STORAGE_MASK       = (VF_FILE | VF_MEMORY),
  VF_PUBLIC_MASK        = (V_DONT_CACHE | V_FORCE_CACHE |
                           V_SMALL_BUFFER | V_LARGE_BUFFER)
};

enum vdir_type
{
  DIR_TYPE_UNKNOWN,
  DIR_TYPE_FILE,
  DIR_TYPE_DIR,
  NUM_DIR_TYPES
};

enum vdirflags
{
  VDIR_NO_SCAN          = (1<<0), // Don't scan the directory to get its length.
                                  // This will break seek, tell, and length.
  VDIR_FAST             = VDIR_NO_SCAN, // Enable all speed hacks.
  VDIR_PUBLIC_MASK      = VDIR_NO_SCAN
};

MZX_LIBIO boolean vio_filesystem_init(size_t max_size, size_t max_file_size,
 boolean enable_auto_cache);
MZX_LIBIO boolean vio_filesystem_exit(void);
MZX_LIBIO size_t vio_filesystem_total_cached_usage(void);
MZX_LIBIO size_t vio_filesystem_total_memory_usage(void);
MZX_LIBIO boolean vio_virtual_file(const char *path);
MZX_LIBIO boolean vio_virtual_directory(const char *path);
MZX_LIBIO boolean vio_invalidate_at_least(size_t *amount_to_free);
MZX_LIBIO boolean vio_invalidate_all(void);

MZX_LIBIO vfile *vfopen_unsafe_ext(const char *filename, const char *mode,
 int user_flags);
MZX_LIBIO vfile *vfopen_unsafe(const char *filename, const char *mode);
MZX_LIBIO vfile *vfile_init_fp(FILE *fp, const char *mode);
MZX_LIBIO vfile *vfile_init_mem(void *buffer, size_t size, const char *mode);
MZX_LIBIO vfile *vfile_init_mem_ext(void **external_buffer,
 size_t *external_buffer_size, const char *mode);
MZX_LIBIO vfile *vtempfile(size_t initial_size);
MZX_LIBIO boolean vfile_force_to_memory(vfile *vf);
MZX_LIBIO int vfclose(vfile *vf);

MZX_LIBIO int vfile_get_mode_flags(const char *mode);
MZX_LIBIO int vfile_get_flags(vfile *vf);
boolean vfile_get_memfile_block(vfile *vf, size_t length, struct memfile *dest);

MZX_LIBIO int vchdir(const char *path);
MZX_LIBIO char *vgetcwd(char *buf, size_t size);
MZX_LIBIO int vmkdir(const char *path, int mode);
MZX_LIBIO int vrename(const char *oldpath, const char *newpath);
MZX_LIBIO int vunlink(const char *path);
MZX_LIBIO int vrmdir(const char *path);
MZX_LIBIO int vaccess(const char *path, int mode);
MZX_LIBIO int vstat(const char *path, struct stat *buf);

MZX_LIBIO int vfgetc(vfile *vf);
MZX_LIBIO int vfgetw(vfile *vf);
MZX_LIBIO int vfgetd(vfile *vf);
MZX_LIBIO int64_t vfgetq(vfile *vf);
MZX_LIBIO int vfputc(int character, vfile *vf);
MZX_LIBIO int vfputw(int character, vfile *vf);
MZX_LIBIO int vfputd(int character, vfile *vf);
MZX_LIBIO int64_t vfputq(int64_t character, vfile *vf);
MZX_LIBIO size_t vfread(void *dest, size_t size, size_t count, vfile *vf);
MZX_LIBIO size_t vfwrite(const void *src, size_t size, size_t count, vfile *vf);
MZX_LIBIO char *vfsafegets(char *dest, int size, vfile *vf);
MZX_LIBIO int vfputs(const char *src, vfile *vf);
MZX_LIBIO int vf_printf(vfile *vf, const char *fmt, ...)
 MZX_ATTRIBUTE_PRINTF(2, 3);
MZX_LIBIO int vf_vprintf(vfile *vf, const char *fmt, va_list args)
 MZX_ATTRIBUTE_PRINTF(2, 0);
MZX_LIBIO int vungetc(int ch, vfile *vf);
MZX_LIBIO int vfseek(vfile *vf, int64_t offset, int whence);
MZX_LIBIO int64_t vftell(vfile *vf);
MZX_LIBIO void vrewind(vfile *vf);
MZX_LIBIO int64_t vfilelength(vfile *vf, boolean rewind);

MZX_LIBIO vdir *vdir_open(const char *path);
MZX_LIBIO vdir *vdir_open_ext(const char *path, int flags);
MZX_LIBIO int vdir_close(vdir *dir);
MZX_LIBIO boolean vdir_read(vdir *dir, char *buffer, size_t len, enum vdir_type *type);
MZX_LIBIO boolean vdir_seek(vdir *dir, long position);
MZX_LIBIO boolean vdir_rewind(vdir *dir);
MZX_LIBIO long vdir_tell(vdir *dir);
MZX_LIBIO long vdir_length(vdir *dir);

MZX_LIBIO vvolumelist *vvolumelist_open(void);
MZX_LIBIO int vvolumelist_close(vvolumelist *volumes);
MZX_LIBIO const char *vvolumelist_read(vvolumelist *volumes);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_VIO_H */

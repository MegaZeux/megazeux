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

#ifndef __IO_VIO_H
#define __IO_VIO_H

#include "../compat.h"

__M_BEGIN_DECLS

#include "vfile.h"
#include "../platform/platform_attribute.h" /* ATTRIBUTE_PRINTF */

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

UTILS_LIBSPEC boolean vio_filesystem_init(size_t max_size, size_t max_file_size,
 boolean enable_auto_cache);
UTILS_LIBSPEC boolean vio_filesystem_exit(void);
UTILS_LIBSPEC size_t vio_filesystem_total_cached_usage(void);
UTILS_LIBSPEC size_t vio_filesystem_total_memory_usage(void);
UTILS_LIBSPEC boolean vio_virtual_file(const char *path);
UTILS_LIBSPEC boolean vio_virtual_directory(const char *path);
UTILS_LIBSPEC boolean vio_invalidate_at_least(size_t *amount_to_free);
UTILS_LIBSPEC boolean vio_invalidate_all(void);

UTILS_LIBSPEC vfile *vfopen_unsafe_ext(const char *filename, const char *mode,
 int user_flags);
UTILS_LIBSPEC vfile *vfopen_unsafe(const char *filename, const char *mode);
UTILS_LIBSPEC vfile *vfile_init_fp(FILE *fp, const char *mode);
UTILS_LIBSPEC vfile *vfile_init_mem(void *buffer, size_t size, const char *mode);
UTILS_LIBSPEC vfile *vfile_init_mem_ext(void **external_buffer,
 size_t *external_buffer_size, const char *mode);
UTILS_LIBSPEC vfile *vtempfile(size_t initial_size);
UTILS_LIBSPEC boolean vfile_force_to_memory(vfile *vf);
UTILS_LIBSPEC int vfclose(vfile *vf);

UTILS_LIBSPEC int vfile_get_mode_flags(const char *mode);
UTILS_LIBSPEC int vfile_get_flags(vfile *vf);
boolean vfile_get_memfile_block(vfile *vf, size_t length, struct memfile *dest);

UTILS_LIBSPEC int vchdir(const char *path);
UTILS_LIBSPEC char *vgetcwd(char *buf, size_t size);
UTILS_LIBSPEC int vmkdir(const char *path, int mode);
UTILS_LIBSPEC int vrename(const char *oldpath, const char *newpath);
UTILS_LIBSPEC int vunlink(const char *path);
UTILS_LIBSPEC int vrmdir(const char *path);
UTILS_LIBSPEC int vaccess(const char *path, int mode);
UTILS_LIBSPEC int vstat(const char *path, struct stat *buf);

UTILS_LIBSPEC int vfgetc(vfile *vf);
UTILS_LIBSPEC int vfgetw(vfile *vf);
UTILS_LIBSPEC int vfgetd(vfile *vf);
UTILS_LIBSPEC int64_t vfgetq(vfile *vf);
UTILS_LIBSPEC int vfputc(int character, vfile *vf);
UTILS_LIBSPEC int vfputw(int character, vfile *vf);
UTILS_LIBSPEC int vfputd(int character, vfile *vf);
UTILS_LIBSPEC int64_t vfputq(int64_t character, vfile *vf);
UTILS_LIBSPEC size_t vfread(void *dest, size_t size, size_t count, vfile *vf);
UTILS_LIBSPEC size_t vfwrite(const void *src, size_t size, size_t count, vfile *vf);
UTILS_LIBSPEC char *vfsafegets(char *dest, int size, vfile *vf);
UTILS_LIBSPEC int vfputs(const char *src, vfile *vf);
UTILS_LIBSPEC int vf_printf(vfile *vf, const char *fmt, ...)
 ATTRIBUTE_PRINTF(2, 3);
UTILS_LIBSPEC int vf_vprintf(vfile *vf, const char *fmt, va_list args)
 ATTRIBUTE_PRINTF(2, 0);
UTILS_LIBSPEC int vungetc(int ch, vfile *vf);
UTILS_LIBSPEC int vfseek(vfile *vf, int64_t offset, int whence);
UTILS_LIBSPEC int64_t vftell(vfile *vf);
UTILS_LIBSPEC void vrewind(vfile *vf);
UTILS_LIBSPEC int64_t vfilelength(vfile *vf, boolean rewind);

UTILS_LIBSPEC vdir *vdir_open(const char *path);
UTILS_LIBSPEC vdir *vdir_open_ext(const char *path, int flags);
UTILS_LIBSPEC int vdir_close(vdir *dir);
UTILS_LIBSPEC boolean vdir_read(vdir *dir, char *buffer, size_t len, enum vdir_type *type);
UTILS_LIBSPEC boolean vdir_seek(vdir *dir, long position);
UTILS_LIBSPEC boolean vdir_rewind(vdir *dir);
UTILS_LIBSPEC long vdir_tell(vdir *dir);
UTILS_LIBSPEC long vdir_length(vdir *dir);

UTILS_LIBSPEC vvolumelist *vvolumelist_open(void);
UTILS_LIBSPEC int vvolumelist_close(vvolumelist *volumes);
UTILS_LIBSPEC const char *vvolumelist_read(vvolumelist *volumes);

__M_END_DECLS

#endif /* __IO_VIO_H */

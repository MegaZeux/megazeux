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

#ifndef MEGAZEUX_IO_VIO_POSIX_H
#define MEGAZEUX_IO_VIO_POSIX_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

/**
 * Standard path function and dirent wrappers.
 * On most relevant platforms these already support UTF-8.
 */

#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vio_volume.h"

// pspdev/devkitPSP historically does not have a rewinddir implementation.
// libctru (3DS) and libnx (Switch) have rewinddir but it doesn't work.
#if defined(CONFIG_3DS) || defined(CONFIG_PSP) || defined(CONFIG_SWITCH)
#define PLATFORM_NO_REWINDDIR
#endif

/* clang has broken MemorySanitizer instrumentation for *stat, leading to false
 * positives. Use this define to enable memset() in the functions that use them.
 */
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define STAT_NEEDS_MEMSET 1
#endif
#endif

/**
 * Regular readdir/dirent can cause issues for 32-bit executables running
 * on large filesystems. This is caused by values for d_ino or d_off
 * overflowing their respective fields (even though this code doesn't
 * care about them). An example of this is running 32-bit ARM executables
 * using qemu-arm-static on an AMD64 host. For POSIX systems, please define
 * _FILE_OFFSET_BITS=64 if it is available (see the *nix Makefile fragment).
 */

static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
  return fopen_unsafe(path, mode);
}

static inline FILE *platform_tmpfile(void)
{
#ifdef CONFIG_NDS
  // tmpfile() pulls in the non-integer-only variant of sprintf from newlib,
  // while the only functions currently using tmpfile() have a fallback.
  return NULL;
#else
  return tmpfile();
#endif /* CONFIG_NDS */
}

static inline char *platform_getcwd(char *buf, size_t size)
{
  return getcwd(buf, size);
}

static inline int platform_chdir(const char *path)
{
  return chdir(path);
}

static inline int platform_mkdir(const char *path, int mode)
{
  return mkdir(path, mode);
}

static inline int platform_rename(const char *oldpath, const char *newpath)
{
  return rename(oldpath, newpath);
}

static inline int platform_unlink(const char *path)
{
  return unlink(path);
}

static inline int platform_rmdir(const char *path)
{
  return rmdir(path);
}

static inline int platform_access(const char *path, int mode)
{
#if defined(CONFIG_AMIGA)
  /* X_OK seems to have non-POSIX semantics here, replace with R_OK. */
  if(mode & X_OK)
    mode = (mode & ~X_OK) | R_OK;
#endif
#if defined(CONFIG_DREAMCAST)
  // KallistiOS doesn't have access() :(
  return 0;
#else
  return access(path, mode);
#endif
}

static inline int platform_stat(const char *path, struct stat *buf)
{
#ifdef STAT_NEEDS_MEMSET
  memset(buf, 0, sizeof(struct stat));
#endif
  return stat(path, buf);
}

struct dir_handle
{
  DIR *dir;
};

static inline boolean platform_opendir(struct dir_handle *dh, const char *path)
{
  dh->dir = opendir(path);
  return dh->dir != NULL;
}

static inline int platform_closedir(struct dir_handle dh)
{
  return closedir(dh.dir);
}

static inline boolean platform_readdir(struct dir_handle dh, char *buffer,
 size_t buffer_len, int *d_type)
{
  // Note: ino_t may cause issues unless _FILE_OFFSET_BITS=64 is defined.
  struct dirent *d = readdir(dh.dir);
  if(!d)
    return false;

#ifdef CONFIG_PSP
  // SCE IO does this instead for some reason. This issue is only present in
  // post-devkitPSP builds for some reason. FIXME: bandaid, upstream fix needed?
  if(d->d_name[0] == '\0')
    return false;
#endif

  if(buffer && buffer_len)
    snprintf(buffer, buffer_len, "%s", d->d_name);

#ifdef DT_UNKNOWN
  if(d_type)
    *d_type = d->d_type;
#endif

  return true;
}

static inline boolean platform_rewinddir(struct dir_handle dh)
{
#ifndef PLATFORM_NO_REWINDDIR
  rewinddir(dh.dir);
  return true;
#endif

  return false;
}

static inline int platform_fseek(FILE *fp, int64_t offset, int whence)
{
#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
  return fseeko(fp, offset, whence);
#else
  if(offset >= LONG_MAX)
    return -1;
  return fseek(fp, offset, whence);
#endif
}

static inline int64_t platform_ftell(FILE *fp)
{
#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
  return ftello(fp);
#else
  return ftell(fp);
#endif
}

#if defined(__EMSCRIPTEN__)

/**
 * For unknown reasons, Emscripten fstat is much slower than the fallback.
 */
static inline int64_t platform_filelength(FILE *fp)
{
  return -1;
}

#else /* fstat */

static inline int64_t platform_filelength(FILE *fp)
{
  // Note: off_t, ino_t may cause issues unless _FILE_OFFSET_BITS=64 is defined.
  struct stat st;
  int fd = fileno(fp);

#ifdef STAT_NEEDS_MEMSET
  memset(&st, 0, sizeof(struct stat));
#endif

  if(fd < 0)
    return -1;
  if(fstat(fd, &st) < 0)
    return -1;

  return st.st_size;
}

#endif /* fstat */

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_VIO_POSIX_H */

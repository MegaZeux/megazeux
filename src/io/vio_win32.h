/* MegaZeux
 *
 * Copyright (C) 2018-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_VIO_WIN32_H
#define MEGAZEUX_IO_VIO_WIN32_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

/**
 * Win32 UTF-8 stdio, unistd, and dirent path function wrappers.
 */

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#ifndef _WIN32_WINNT_WIN2K
#define _WIN32_WINNT_WIN2K 0x500
#endif
#ifndef _WIN32_WINNT_WINXP
#define _WIN32_WINNT_WINXP 0x501
#endif

/**
 * The wide char conversion functions were added in Windows 2000 and generally
 * the wide versions of the stdio functions just don't appear to work in older
 * OSes (tested Win98 with KernelEx using tdm-gcc 5.1.0).
 */
#if WINVER >= _WIN32_WINNT_WIN2K
#define WIDE_PATHS 1
#endif

#ifdef WIDE_PATHS
/**
 * Convert a UTF-8 char string into a UTF-16 wide char string for use with
 * Win32 wide char functions. Returns the length of the output (including the
 * null terminator) or 0 on failure.
 */
static inline int utf8_to_utf16(const char *src, wchar_t *dest, int dest_size)
{
  return MultiByteToWideChar(
    CP_UTF8,
    0,
    (LPCCH)src,
    -1, // Null terminated.
    (LPWSTR)dest,
    dest_size
  );
}

/**
 * Convert a UTF-16 wide char string into a UTF-8 char string for general
 * usage. Returns the length of the output (including the null terminator)
 * or 0 on failure.
 */
static inline int utf16_to_utf8(const wchar_t *src, char *dest, int dest_size)
{
  return WideCharToMultiByte(
    CP_UTF8,
    0,
    (LPCWCH)src,
    -1, // Null terminated.
    (LPSTR)dest,
    dest_size,
    NULL,
    NULL
  );
}
#endif

static inline FILE *platform_fopen_unsafe(const char *path, const char *mode)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];
  wchar_t wmode[64];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    if(utf8_to_utf16(mode, wmode, 64))
      return _wfopen(wpath, wmode);
#endif
  return fopen_unsafe(path, mode);
}

static inline FILE *platform_tmpfile(void)
{
#ifdef WIDE_PATHS
  /**
   * The non-terrible solution to doing this was introduced around roughly the
   * same time wide path support was so just tie them together. This also
   * hopefully means that user dirs with unicode characters won't break.
   */
  wchar_t wpath[MAX_PATH];
  wchar_t wfile[MAX_PATH];

  if(GetTempPathW(MAX_PATH, wpath))
  {
    if(GetTempFileNameW(wpath, L"MZX", 0, wfile))
      return _wfopen(wfile, L"wb+");
  }
#endif
  /* This attempts to put a file in C:\ and generally is terrible! */
  return tmpfile();
}

static inline char *platform_getcwd(char *buf, size_t size)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  if(!_wgetcwd(wpath, MAX_PATH))
    return NULL;

  if(utf16_to_utf8(wpath, buf, size))
    return buf;
#endif
  return getcwd(buf, size);
}

static inline int platform_chdir(const char *path)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wchdir(wpath);
#endif
  return chdir(path);
}

static inline int platform_mkdir(const char *path, int mode)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wmkdir(wpath);
#endif
  return mkdir(path);
}

static inline int platform_rename(const char *oldpath, const char *newpath)
{
#ifdef WIDE_PATHS
  wchar_t woldpath[MAX_PATH];
  wchar_t wnewpath[MAX_PATH];

  if(utf8_to_utf16(oldpath, woldpath, MAX_PATH))
    if(utf8_to_utf16(newpath, wnewpath, MAX_PATH))
      return _wrename(woldpath, wnewpath);
#endif
  return rename(oldpath, newpath);
}

static inline int platform_unlink(const char *path)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wunlink(wpath);
#endif
  return _unlink(path);
}

static inline int platform_rmdir(const char *path)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  if(utf8_to_utf16(path, wpath, MAX_PATH))
    return _wrmdir(wpath);
#endif
  return rmdir(path);
}

static inline int platform_access(const char *path, int mode)
{
  // X_OK isn't supported on Windows. Use R_OK instead...
  if(mode & X_OK)
    mode = (mode & ~X_OK) | R_OK;

#ifdef WIDE_PATHS
  {
    wchar_t wpath[MAX_PATH];

    if(utf8_to_utf16(path, wpath, MAX_PATH))
      return _waccess(wpath, mode);
  }
#endif
  return access(path, mode);
}

static inline int platform_stat(const char *path, struct stat *buf)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];
  struct _stat stat_info;
  int ret;

  if(utf8_to_utf16(path, wpath, MAX_PATH))
  {
    ret = _wstat(wpath, &stat_info);

    buf->st_gid = stat_info.st_gid;
    buf->st_atime = (time_t)stat_info.st_atime;
    buf->st_ctime = (time_t)stat_info.st_ctime;
    buf->st_dev = stat_info.st_dev;
    buf->st_ino = stat_info.st_ino;
    buf->st_mode = stat_info.st_mode;
    buf->st_mtime = (time_t)stat_info.st_mtime;
    buf->st_nlink = stat_info.st_nlink;
    buf->st_rdev = stat_info.st_rdev;
    buf->st_size = (_off_t)stat_info.st_size;
    buf->st_uid = stat_info.st_uid;
    return ret;
  }
#endif
  return stat(path, buf);
}

struct dir_handle
{
  DIR *dir;
#ifdef WIDE_PATHS
  _WDIR *wdir;
#endif
};

static inline boolean platform_opendir(struct dir_handle *dh, const char *path)
{
#ifdef WIDE_PATHS
  wchar_t wpath[MAX_PATH];

  dh->dir = NULL;
  dh->wdir = NULL;

  if(utf8_to_utf16(path, wpath, MAX_PATH))
  {
    dh->wdir = _wopendir(wpath);
    if(dh->wdir)
      return true;
  }
#endif

  dh->dir = opendir(path);
  return dh->dir != NULL;
}

static inline int platform_closedir(struct dir_handle dh)
{
#ifdef WIDE_PATHS
  if(dh.wdir)
    return _wclosedir(dh.wdir);
#endif
  return closedir(dh.dir);
}

static inline boolean platform_readdir(struct dir_handle dh, char *buffer,
 size_t buffer_len, int *d_type)
{
  struct dirent *d;

#ifdef WIDE_PATHS
  if(dh.wdir)
  {
    struct _wdirent *wd = _wreaddir(dh.wdir);
    if(!wd)
      return false;

    if(buffer && buffer_len)
      if(!utf16_to_utf8(wd->d_name, buffer, buffer_len))
        buffer[0] = '\0';

#ifdef DT_UNKNOWN
    if(d_type)
      *d_type = wd->d_type;
#endif

    return true;
  }
#endif

  d = readdir(dh.dir);
  if(!d)
    return false;

  if(buffer && buffer_len)
  {
    snprintf(buffer, buffer_len, "%s", d->d_name);
    buffer[buffer_len - 1] = '\0';
  }

#ifdef DT_UNKNOWN
  if(d_type)
    *d_type = d->d_type;
#endif

  return true;
}

static inline boolean platform_rewinddir(struct dir_handle dh)
{
#ifdef WIDE_PATHS
  if(dh.wdir)
  {
    _wrewinddir(dh.wdir);
    return true;
  }
#endif

  rewinddir(dh.dir);
  return true;
}

struct vvolumelist_handle
{
  DWORD drives;
  DWORD pos;
};

static inline boolean platform_vvolumelist_open(struct vvolumelist_handle *vh)
{
  vh->drives = GetLogicalDrives();
  vh->pos = 0;
  return true;
}

static inline void platform_vvolumelist_close(struct vvolumelist_handle *vh)
{
  /* nop */
  (void)vh;
}

static inline int platform_vvolumelist_read(struct vvolumelist_handle *vh,
 char *buf, size_t buf_sz)
{
  while(vh->pos < 32)
  {
    DWORD num = (vh->pos++);

    if(vh->drives & (1u << num))
      return snprintf(buf, buf_sz, "%c:", 'A' + (char)num);
  }
  return 0;
}

static inline int platform_fseek(FILE *fp, int64_t offset, int whence)
{
#if WINVER >= _WIN32_WINNT_WINXP
  return _fseeki64(fp, offset, whence);
#else
  fpos_t pos;
  int fd = _fileno(fp);
  if(fd < 0)
    return -1;

  // _lseeki64 works at a lower level than stdio functions.
  // This forces MSVCRT to synchronize any buffered data.
  if(fgetpos(fp, &pos) < 0 || fsetpos(fp, &pos) < 0)
    return -1;

  return _lseeki64(fd, offset, whence) >= 0 ? 0 : -1;
#endif
}

static inline int64_t platform_ftell(FILE *fp)
{
#if WINVER >= _WIN32_WINNT_WINXP
  return _ftelli64(fp);
#else
  fpos_t pos;
  int fd = _fileno(fp);
  if(fd < 0)
    return -1;

  // _telli64 works at a lower level than stdio functions.
  // This forces MSVCRT to synchronize any buffered data.
  if(fgetpos(fp, &pos) < 0 || fsetpos(fp, &pos) < 0)
    return -1;

  return _telli64(fd);
#endif
}

static inline int64_t platform_filelength(FILE *fp)
{
  int fd = _fileno(fp);
  return fd >= 0 ? _filelengthi64(fd) : -1;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_VIO_WIN32_H */

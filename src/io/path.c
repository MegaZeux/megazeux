/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012, 2020-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "../util.h"
#include "../platform/log.h"
#include "path.h"
#include "vio.h"

/**
 * Tokenize a null terminated path string. This is a special implementation
 * of `strsep` for paths.
 *
 * Note this function currently isn't aware of absolute paths, so "C:/a" will
 * tokenize to "C:" followed by "a". If this behavior isn't desired, use the
 * return value of `path_is_absolute` before tokenizing to skip the root. This
 * function also does not skip duplicate slashes or trailing slashes
 * (see `path_clean`).
 *
 * @param next  pointer to the current position in the path string.
 *              The value will be updated to the location of the next token or
 *              to `NULL` if there are no further tokens.
 * @return      the initial value of `next` if it represents a token (i.e. is
 *              not `NULL`), otherwise `NULL`.
 */
char *path_tokenize(char **next)
{
  char *src = *next;
  if(src)
  {
    char *current = strpbrk(src, "\\/");
    if(current)
    {
      *current = '\0';
      *next = current + 1;
    }
    else
      *next = NULL;
  }
  return src;
}

/**
 * Similar to `path_tokenize`, but strips one path token off of the end of the
 * provided path.
 *
 * This function is not aware of absolute paths; if the token returned is a
 * root, the directory separator portion of the root will be replaced with nul.
 *
 * @param  base       Pointer to the path to reverse tokenize. If there is only
 *                    one token in this path, NULL will be stored to *`base`.
 *                    Otherwise, this value will not be modified.
 * @param  base_len   Pointer to the current length of the path pointed to by
 *                    `base`. If there is only one token in the path, 0 will be
 *                    stored to *`base_len`. This field MUST be initialized
 *                    properly if provided. If not provided, each call will
 *                    perform one `strlen` call.
 * @return            A pointer to the token removed from *`base`, or NULL if
 *                    *`base` is NULL or `base` is NULL.
 */
char *path_reverse_tokenize(char **_base, size_t *_base_len)
{
  size_t base_len;
  char *base;
  char *pos;
  if(!_base || !*_base)
    return NULL;

  base = *_base;
  base_len = _base_len ? *_base_len : strlen(base);
  pos = base + base_len;

  while(pos >= base)
  {
    if(isslash(*pos))
    {
      if(_base_len)
        *_base_len = (pos > base) ? pos - base - 1 : 0;

      *pos = '\0';
      return pos + 1;
    }
    pos--;
  }

  *_base = NULL;
  if(_base_len)
    *_base_len = 0;
  return base;
}

/**
 * Force a given filename path to use the provided file extension. If the
 * filename already has the given extension, the string will not be modified.
 *
 * @param  path         Path to force the extension of.
 * @param  buffer_len   Size of the buffer of the path.
 * @param  ext          Extension to add to the path.
 * @return              true on success, otherwise false.
 */
boolean path_force_ext(char *path, size_t buffer_len, const char *ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);

  if((path_len < ext_len) || (path[path_len - ext_len] != '.') ||
   strcasecmp(path + path_len - ext_len, ext))
  {
    if(path_len + ext_len >= buffer_len)
      return false;

    snprintf(path + path_len, MAX_PATH - path_len, "%s", ext);
    path[buffer_len - 1] = '\0';
  }
  return true;
}

/**
 * Get the position of the extension in a filename path.
 *
 * @param  path   Path to get the extension position of.
 * @return        The index of the start of the extension or -1 if not found.
 */
ssize_t path_get_ext_offset(const char *path)
{
  ssize_t path_len = strlen(path);
  ssize_t root_len = path_is_absolute(path);
  ssize_t ext_pos;

  for(ext_pos = path_len - 1; ext_pos >= root_len; ext_pos--)
  {
    // Don't let this detect an "extension" of a directory!
    if(isslash(path[ext_pos]))
      break;

    if(path[ext_pos] == '.')
      return ext_pos;
  }
  return -1;
}

/**
 * Get the location of the first char of the filename in a filename path.
 * If there is no filename, the returned index will be the location of the
 * null terminator. If the string is empty, -1 will be returned.
 */
static ssize_t path_get_filename_offset(const char *path)
{
  struct stat stat_info;
  ssize_t root_len;
  ssize_t pos;
  if(!path || !path[0])
    return -1;

  // If this path stats and happens to be a directory without a terminating
  // slash, the filename offset is the end of the string...
  if(vstat(path, &stat_info) >= 0 && S_ISDIR(stat_info.st_mode))
    return strlen(path);

  // Otherwise, find the last directory separator (if any).
  pos = (ssize_t)strlen(path) - 1;
  root_len = path_is_absolute(path);
  while(pos >= root_len)
  {
    if(isslash(path[pos]))
      return pos + 1;

    pos--;
  }
  return root_len;
}

/**
 * Determine if the given path is an absolute path.
 *
 * @param  path   Path to test.
 * @return        length of root token if this is an absolute path, otherwise 0.
 */
ssize_t path_is_absolute(const char *path)
{
  size_t len;
  size_t i;

#ifdef PATH_UNC_ROOTS
  // Windows unified DOS and UNC roots.
  len = strlen(path);
  if(len > 2 && isslash(path[0]) && isslash(path[1]) && !isslash(path[2]))
  {
    i = 3;
    // UNC roots may be prefixed with \\.\UNC\, try to catch that.
    if((path[2] == '.'|| path[2] == '?') && isslash(path[3])
     && !strncasecmp(path + 4, "unc", 3) && isslash(path[7]))
    {
      i = 8;
    }

    // Host name or . or ?
    for(; i < len; i++)
      if(isslash(path[i]))
        break;

    if(i + 1 < len && !isslash(path[i + 1]))
    {
      // Share or root name.
      for(i += 1; i < len; i++)
        if(isslash(path[i]))
          break;

      if(isslash(path[i]))
        return i + 1;

      return i;
    }
  }
#endif /* UNC roots */

#ifdef PATH_UNIX_STYLE_ROOTS
  // Unix-style root.
  if(isslash(path[0]))
    return 1;
#endif

  // DOS-style or Amiga-style root.
#ifndef PATH_UNC_ROOTS
  len = strlen(path);
#endif
  for(i = 0; i < len; i++)
  {
#ifndef PATH_AMIGA_STYLE_ROOTS

    if(isslash(path[i]))
      break;

    if(path[i] == ':')
    {
      if(i == 0)
        break;

      i++;
#ifdef PATH_DOS_STYLE_ROOTS
      // True DOS-style roots do not require a trailing slash.
      if(path[i] == '\0')
        return i;
#endif

      if(isslash(path[i]))
      {
        while(isslash(path[i]))
          i++;
#ifndef PATH_DOS_STYLE_ROOTS
        // Allow DOS-style roots in Linux only with >=2 slashes.
        if(i >= 2 && isslash(path[i - 2]))
#endif
        return i;
      }
      break;
    }

#else /* PATH_AMIGA_STYLE_ROOTS */

    /* Amiga: root ends at the first ':'.
     * If ':' is the first character, it's an absolute path on the current
     * disk. The root can include slashes! :(
     */
    if(path[i] == ':')
    {
      i++;

      /* Include any following slashes (they will get cleaned). */
      while(isslash(path[i]))
        i++;

      return i;
    }

#endif
  }
  return 0;
}

/**
 * Determine if the given path is an absolute path, using DOS-style roots rules.
 *
 * @param  path   Path to test.
 * @return        length of root token if this is an absolute path, otherwise 0.
 */
ssize_t path_is_absolute_dos(const char *path)
{
  size_t len;
  size_t i;

  // Unix-style root.
  if(isslash(path[0]))
    return 1;

  // DOS-style root.
  len = strlen(path);
  for(i = 0; i < len; i++)
  {
    if(isslash(path[i]))
      break;

    if(path[i] == ':')
    {
      if(i == 0)
        break;

      i++;
      // True DOS-style roots do not require a trailing slash.
      if(path[i] == '\0')
        return i;

      if(isslash(path[i]))
      {
        while(isslash(path[i]))
          i++;
        return i;
      }
      break;
    }
  }
  return 0;
}

/**
 * Determine if the given path is a root path.
 *
 * @param  path   Path to test.
 * @return        `true` if the path is a root path, otherwise `false`.
 */
boolean path_is_root(const char *path)
{
  ssize_t root_len = path_is_absolute(path);
  return root_len && !path[root_len];
}

/**
 * Determine if the given path contains a directory.
 *
 * @param  path   Path to test.
 * @return        True if the path contains a directory, otherwise false.
 */
boolean path_has_directory(const char *path)
{
  struct stat stat_info;
  size_t len;
  size_t i;

  if(!path || !path[0])
    return false;

  // Check for slashes first to avoid a stat call...
  len = strlen(path);
  for(i = 0; i < len; i++)
    if(isslash(path[i]))
      return true;

  return vstat(path, &stat_info) >= 0 && S_ISDIR(stat_info.st_mode);
}

/**
 * Truncate a path to its directory portion, if any.
 *
 * @param  path         Path to truncate.
 * @param  buffer_len   Size of the path buffer.
 * @return              The length of the directory string, or -1 on error.
 */
ssize_t path_to_directory(char *path, size_t buffer_len)
{
  ssize_t filename_pos = path_get_filename_offset(path);

  // Invalid path.
  if(filename_pos < 0 || (size_t)filename_pos >= buffer_len)
  {
    if(buffer_len > 0)
      path[0] = '\0';

    return -1;
  }

  path[filename_pos] = '\0';
  if(filename_pos > 0)
    filename_pos = path_clean(path, buffer_len);

  return filename_pos;
}

/**
 * Remove the directory portion of a filename path, if any.
 *
 * @param  path         Filename path to remove the directory portion of.
 * @param  buffer_len   Size of the path buffer.
 * @return              The length of the filename string, or -1 on error.
 */
ssize_t path_to_filename(char *path, size_t buffer_len)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path.
  if(filename_pos < 0 || path_len - (size_t)filename_pos >= buffer_len)
  {
    if(buffer_len > 0)
      path[0] = '\0';

    return -1;
  }

  // If there isn't a directory prefix, don't modify the buffer...
  if(filename_pos == 0)
    return path_len;

  filename_len = path_len - filename_pos;
  if(filename_len > 0)
    memmove(path, path + filename_pos, filename_len);
  path[filename_len] = '\0';

  return filename_len;
}

/**
 * Copy the directory portion of a filename or directory path to the
 * destination buffer.
 *
 * @param  dest       Destination buffer for the directory.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to get the directory of.
 * @return            The length of the directory string, or -1 on error.
 */
ssize_t path_get_directory(char *dest, size_t dest_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);

  // Invalid path, or it's too long to store.
  if(filename_pos < 0 || (size_t)filename_pos >= dest_len)
  {
    if(dest_len > 0)
      dest[0] = '\0';

    return -1;
  }

  dest[filename_pos] = '\0';
  if(filename_pos > 0)
  {
    memcpy(dest, path, filename_pos);
    filename_pos = path_clean(dest, dest_len);
  }
  return filename_pos;
}

/**
 * Copy the filename portion of a filename path to the destination buffer.
 *
 * @param  dest       Destination buffer for the filename.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to get the filename of.
 * @return            The length of the filename string, or -1 on error.
 */
ssize_t path_get_filename(char *dest, size_t dest_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path, or it's too long to store.
  if(filename_pos < 0 || path_len - (size_t)filename_pos >= dest_len)
  {
    if(dest_len > 0)
      dest[0] = '\0';

    return -1;
  }

  filename_len = path_len - filename_pos;
  dest[filename_len] = '\0';
  if(filename_len > 0)
    memcpy(dest, path + filename_pos, filename_len);

  return filename_len;
}

/**
 * Copy both the directory and the filename portions of a filename path to
 * individual destination buffers.
 *
 * @param  d_dest   Destination buffer for the directory.
 * @param  d_len    Size of the directory destination buffer.
 * @param  f_dest   Destination buffer for the filename.
 * @param  f_len    Size of the filename destination buffer.
 * @param  path     Path to get the directory and filename of.
 * @return          true on success, otherwise false.
 */
boolean path_get_directory_and_filename(char *d_dest, size_t d_len,
 char *f_dest, size_t f_len, const char *path)
{
  ssize_t filename_pos = path_get_filename_offset(path);
  size_t path_len = strlen(path);
  size_t filename_len;

  // Invalid path, or one of the components is too long to store.
  if(filename_pos < 0 || (size_t)filename_pos >= d_len ||
   path_len - (size_t)filename_pos >= f_len)
  {
    if(d_len > 0)
      d_dest[0] = '\0';
    if(f_len > 0)
      f_dest[0] = '\0';

    return false;
  }

  d_dest[filename_pos] = '\0';
  if(filename_pos > 0)
  {
    memcpy(d_dest, path, filename_pos);
    path_clean(d_dest, d_len);
  }

  filename_len = path_len - filename_pos;
  f_dest[filename_len] = '\0';
  if(filename_len > 0)
    memcpy(f_dest, path + filename_pos, filename_len);

  return true;
}

/**
 * Get the parent directory of a provided path string if the path string
 * contains a parent component.
 *
 * @param  dest       Destination buffer for the parent.
 * @param  dest_len   Size of the destination buffer.
 * @return            Length of the parent string, -1 if the path string is
 *                    invalid or doesn't fit in the provided buffer, or 0
 *                    if the path contains no parent directory.
 */
ssize_t path_get_parent(char *dest, size_t dest_len, const char *path)
{
  ssize_t path_len;
  ssize_t root_len;
  ssize_t i;
  if(!path || !path[0])
    return -1;

  // Find last slash without respect to vstat.
  root_len = path_is_absolute(path);
  path_len = strlen(path);
  for(i = path_len - 1; i >= root_len; i--)
    if(isslash(path[i]))
      break;
  // Include the separator.
  i++;

  if((size_t)i >= dest_len)
    return -1;

  dest[i] = '\0';
  if(i > 0)
  {
    memcpy(dest, path, i);
    i = path_clean(dest, dest_len);
  }
  return i;
}

/**
 * Clean a path according to the current platform, namely:
 * - All: normalize slashes to this platform's directory separator.
 * - POSIX, DOS/Win32: remove extra directory separators and ending separators.
 * - Amiga: remove end-of-root directory separators, single ending separator.
 *
 * @param  path       Path to clean slashes of.
 * @param  path_len   Size of the path buffer.
 * @return            The length of the destination path.
 */
size_t path_clean(char *path, size_t path_len)
{
  boolean need_copy = false;
  size_t root_len;
  size_t i = 0;
  size_t j = 0;

  root_len = path_is_absolute(path);

#ifdef PATH_UNC_ROOTS
  // UNC roots should retain two slashes at the start.
  if(root_len >= 2 && isslash(path[0]) && isslash(path[1]))
  {
    path[0] = path[1] = DIR_SEPARATOR_CHAR;
    i = j = 1; // Merge >2 slashes into the second one.
  }
#endif

#ifdef PATH_AMIGA_STYLE_ROOTS
  /* Strip root trailing slashes, do not normalize internal root slashes. */
  i = j = root_len;
  if(root_len && isslash(path[j - 1]))
  {
    while(j > 0 && isslash(path[j - 1]))
      j--;

    need_copy = true;
  }
#elif !defined(PATH_DOS_STYLE_ROOTS)
  // Non-native DOS-style double slash roots should retain two slashes.
  if(root_len >= 4 && !isslash(path[0]))
  {
    while(root_len >= 3 && path[root_len - 3] != ':')
      root_len--;
    path[root_len - 2] = path[root_len - 1] = DIR_SEPARATOR_CHAR;
    i = j = root_len - 1; // Merge >2 slashes into the second slash.
  }
#endif

  while((i < path_len) && path[i])
  {
    if(isslash(path[i]))
    {
#ifndef PATH_AMIGA_STYLE_ROOTS
      while(isslash(path[i]))
        i++;

      if(i > j + 1)
        need_copy = true;
#else
      /* Can't merge slashes on Amiga. */
      i++;
#endif

      path[j++] = DIR_SEPARATOR_CHAR;
    }
    else
    {
      if(need_copy)
        path[j] = path[i];

      i++;
      j++;
    }
  }
  path[j] = '\0';

  /* Trim trailing slashes unless they are a component of the root.
   * Amiga can only do this if there is exactly one trailing slash. */
  if(j >= 2 && j > root_len && path[j - 1] == DIR_SEPARATOR_CHAR)
#ifdef PATH_AMIGA_STYLE_ROOTS
    if(path[j - 2] != DIR_SEPARATOR_CHAR)
#endif
    path[--j] = '\0';

  return j;
}

/**
 * Create a duplicate of a path according to the current platform, namely:
 * - All: normalize slashes to this platform's directory separator.
 * - POSIX, DOS/Win32: remove extra directory separators and ending separators.
 * - Amiga: remove end-of-root directory separators, single ending separator.
 *
 * @param  dest       Destination buffer for the cleaned path.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to clean slashes of.
 * @return            The length of the destination path.
 */
size_t path_clean_copy(char *dest, size_t dest_len, const char *path)
{
  size_t path_len = strlen(path);
  size_t root_len;
  size_t i = 0;
  size_t j = 0;

  root_len = path_is_absolute(path);

#ifdef PATH_UNC_ROOTS
  // UNC roots should retain two slashes at the start.
  if(root_len >= 2 && isslash(path[0]) && isslash(path[1]))
  {
    if(dest_len >= 2)
      dest[j++] = DIR_SEPARATOR_CHAR;
    i = 1; // Merge >2 slashes into the second one.
  }
#endif

#ifdef PATH_AMIGA_STYLE_ROOTS
  /* Strip root trailing slashes, do not normalize internal root slashes. */
  if(root_len)
  {
    while(j < dest_len - 1 && i < root_len && path[i] != ':')
      dest[j++] = path[i++];

    if(j < dest_len - 1)
      dest[j++] = ':';

    i = root_len;
  }
#elif !defined(PATH_DOS_STYLE_ROOTS)
  // Non-native DOS-style double slash roots should retain two slashes.
  if(root_len >= 4 && !isslash(path[0]))
  {
    while(j < dest_len - 1 && i < root_len && !isslash(path[i]))
      dest[j++] = path[i++];

    i++;
    if(j < dest_len - 1)
      dest[j++] = DIR_SEPARATOR_CHAR;
    // Merge >2 slashes into the second slash.
    root_len = j + 1;
  }
#endif

  while((i < path_len) && (j < dest_len - 1))
  {
    if(isslash(path[i]))
    {
#ifndef PATH_AMIGA_STYLE_ROOTS
      /* Can't merge slashes on Amiga. */
      while(isslash(path[i]))
#endif
        i++;

      dest[j++] = DIR_SEPARATOR_CHAR;
    }
    else
      dest[j++] = path[i++];
  }
  dest[j] = '\0';

  /* Trim trailing slashes unless they are a component of the root.
   * Amiga can only do this if there is exactly one trailing slash. */
  if(j >= 2 && j > root_len && dest[j - 1] == DIR_SEPARATOR_CHAR)
#ifdef PATH_AMIGA_STYLE_ROOTS
    if(dest[j - 2] != DIR_SEPARATOR_CHAR)
#endif
    dest[--j] = '\0';

  return j;
}

/**
 * Create a duplicate of a path, cleaning specifically using POSIX with DOS
 * single slash root rules:
 * - Normalize all slashes to the POSIX directory separator /.
 * - Merge duplicate slashes and trim any final trailing slashes.
 * - Do not preserve double root slashes.
 *
 * @param  dest       Destination buffer for the cleaned path.
 * @param  dest_len   Size of the destination buffer.
 * @param  path       Path to clean slashes of.
 * @return            The length of the destination path.
 */
size_t path_clean_copy_posixdos(char *dest, size_t dest_len, const char *path)
{
  size_t path_len = strlen(path);
  size_t root_len;
  size_t i = 0;
  size_t j = 0;

  root_len = path_is_absolute_dos(path);

  while((i < path_len) && (j < dest_len - 1))
  {
    if(isslash(path[i]))
    {
      while(isslash(path[i]))
        i++;

      dest[j++] = '/';
    }
    else
      dest[j++] = path[i++];
  }
  dest[j] = '\0';

  /* Trim trailing slashes unless they are a component of the root. */
  if(j >= 2 && j > root_len && dest[j - 1] == '/')
    dest[--j] = '\0';

  return j;
}

/**
 * Strip redundant POSIX/DOS/Win32 current directory tokens (".") out of
 * a path. TODO: this function should be merged into the path clean functions
 * in 2.94, but for now it strictly uses DOS roots rules, because it's needed
 * by Amiga to fix DOS paths provided by games.
 */
ssize_t path_clean_current_tokens(char *path, size_t path_len)
{
  size_t i = path_is_absolute(path);
  size_t j = i;

  while(i < path_len && path[i])
  {
    if(path[i] == '.' && /* TODO: i >= root_len && */
     (j == 0 || isslash(path[j - 1])) &&
     (path[i + 1] == '\0' || isslash(path[i + 1])))
    {
      i++;
      while(isslash(path[i]))
        i++;
      if(j == 0 && path[i] == '\0')
        path[j++] = '.';
    }
    else
    {
      if(j < i)
        path[j] = path[i];
      j++;
      i++;
    }
  }
  path[j] = '\0';

  return j;
}

/**
 * Trim slashes and colon off the end of DOS/Amiga-style roots. This should be
 * used on root tokens only (the return value of path_is_absolute may be
 * provided as path_len). This function will preserve ":" as a root for Amiga.
 *
 * @param  path       Root token to clean trailing slashes/colon off of.
 * @param  path_len   Length of root token to clean.
 * @return            New length of root token.
 */
ssize_t path_clean_root(char *path, size_t path_len)
{
  size_t len = strlen(path);
  if(len > path_len)
    len = path_len;

  while(len >= 2 && isslash(path[len - 1]))
    path[--len] = '\0';

  if(len >= 2 && path[len - 1] == ':')
    path[--len] = '\0';

  if(len == 1 && isslash(path[0]))
    path[0] = DIR_SEPARATOR_CHAR;

  return len;
}

/**
 * Append a relative directory or filename path to an existing base path.
 * This function does not handle ./, ../, etc; to resolve relative paths
 * containing those, use path_navigate() instead. On success, the destination
 * is guaranteed to have cleaned  by `path_clean`/`path_clean_copy`.
 *
 * @param  path         Existing base path.
 * @param  buffer_len   Size of the base path buffer.
 * @param  rel          Relative path to be joined to the end of the base path.
 * @return              The new length of the base path, or -1 on error.
 */
ssize_t path_append(char *path, size_t buffer_len, const char *rel)
{
  size_t path_len = strlen(path);
  size_t rel_len = strlen(rel);

  // Needs to be able to fit the worst case size: path + separator + rel + \0.
  if(path_len && rel_len && path_len + rel_len + 2 <= buffer_len)
  {
    path_len = path_clean(path, buffer_len);
    if(path[path_len - 1] != DIR_SEPARATOR_CHAR)
      path[path_len++] = DIR_SEPARATOR_CHAR;

    rel_len = path_clean_copy(path + path_len, buffer_len - path_len, rel);
    return path_len + rel_len;
  }
  return -1;
}

/**
 * Join a base directory path to a relative directory or filename path.
 * This function does not handle ./, ../, etc; to resolve relative paths
 * containing those, use path_navigate() instead. On success, the destination
 * is guaranteed to have cleaned  by `path_clean`/`path_clean_copy`.
 *
 * @param  dest       Destination buffer for the joined path.
 * @param  dest_len   Size of the destination buffer.
 * @param  base       Base directory path.
 * @param  rel        Relative path to be joined to the end of the base path.
 * @return            The length of the resulting path, or -1 on error.
 */
ssize_t path_join(char *dest, size_t dest_len, const char *base, const char *rel)
{
  size_t base_len = strlen(base);
  size_t rel_len = strlen(rel);

  // Needs to be able to fit the worst case size: base + separator + rel + \0.
  if(base_len && rel_len && base_len + rel_len + 2 <= dest_len)
  {
    base_len = path_clean_copy(dest, dest_len, base);
    if(dest[base_len - 1] != DIR_SEPARATOR_CHAR)
      dest[base_len++] = DIR_SEPARATOR_CHAR;

    rel_len = path_clean_copy(dest + base_len, dest_len - base_len, rel);
    return base_len + rel_len;
  }
  return -1;
}

/**
 * Determine if `path` is prefixed by `prefix`. Returns the index of the first
 * non-prefix and non-slash char of `path` if `path` is prefixed by `prefix`,
 * otherwise -1.
 */
static ssize_t path_has_prefix(const char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len)
{
  // Normal string compare, but allow different kinds of slashes.
  size_t i = 0;
  size_t j = 0;
  while(i < prefix_len && prefix[i])
  {
    if(j >= buffer_len || !path[j])
      return -1;

    if(isslash(prefix[i]))
    {
      if(!isslash(path[j]))
        return -1;

      // Skip duplicate slashes.
      while(isslash(prefix[i]))
        i++;
      while(isslash(path[j]))
        j++;
    }
    else
    {
      if(prefix[i++] != path[j++])
        return -1;
    }
  }

  // Make sure this was actually a valid prefix--the prefix should either have
  // a trailing slash or the next character of the path should be a slash.
  if(!isslash(prefix[i - 1]) && !isslash(path[j]))
    return -1;

  // The prefix likely does not have trailing slashes, so skip them.
  while(isslash(path[j]))
    j++;

  return j;
}

/**
 * Remove a directory prefix from a path if it exists. The prefix does not
 * need trailing slashes, but it must represent a complete directory name.
 *
 * @param  path         Path to remove a prefix from.
 * @param  buffer_len   Size of the path buffer.
 * @param  prefix       Prefixed directory to remove from path if found.
 * @param  prefix_len   Length of prefix to test or 0 to test the entire prefix.
 * @return              New length of path, or -1 if the prefix was not found.
 */
ssize_t path_remove_prefix(char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len)
{
  prefix_len = prefix_len ? prefix_len : strlen(prefix);

  if(prefix_len)
  {
    ssize_t offset = path_has_prefix(path, buffer_len, prefix, prefix_len);
    if(offset < 0)
      return -1;

    return path_clean_copy(path, buffer_len, path + offset);
  }
  return -1;
}

/* Check a single path token for a reserved DOS device. */
static boolean path_safety_check_dos_device(const char *token, size_t token_len)
{
  /* These names can not exist alone or followed by any extension,
   * including multiple extensions like ".tar.gz". */
  static const char * const devices_num[] =
  {
    "com",
    "lpt",
  };
  static const char * const devices[] =
  {
    "aux",
    "con",
    "nul",
    "prn",
  };
  /* Numbered devices are reserved 1-9 and for Latin-1 superscripts 1-3. */
  static const char reserved_numbers[] = "123456789\xb9\xb2\xb3";

  size_t i;

  /* path_get_ext_offset gets the last extension, this needs to exclude all */
  for(i = 0; i < token_len; i++)
    if(token[i] == '.')
      break;

  token_len = i;

  if(token_len == 3)
  {
    for(i = 0; i < ARRAY_SIZE(devices); i++)
      if(!strncasecmp(token, devices[i], 3))
        return true;
  }
  else

  if(token_len == 4)
  {
    for(i = 0; i < ARRAY_SIZE(devices_num); i++)
    {
      if(strncasecmp(token, devices_num[i], 3))
        continue;

      if(strchr(reserved_numbers, token[3]))
        return true;

      /* Matched one device prefix, won't match another... */
      break;
    }
  }
  return false;
}

/**
 * Perform safety checks on a provided path, optionally including any of the
 * following checks. Use following a path_clean function for best results.
 * Many of these checks are deliberately overly aggressive, since the specifics
 * of what counts as a root may vary between systems.
 *
 * PATH_SAFE_UNIX_ROOT:     detect paths beginning with any slash, including
 *                          Unix and UNC roots.
 * PATH_SAFE_DOS_ROOT:      detect any path that may be mistaken for containing
 *                          a DOS or Amiga root (any path containing a colon).
 * PATH_SAFE_UNIX_PARENT:   detect paths containing Unix parent directory
 *                          tokens (..), which may break sandboxing.
 * PATH_SAFE_AMIGA_PARENT:  detect path containing adjacent slashes, which
 *                          may break sandboxing.
 * PATH_SAFE_DOS_CHARACTER: detect characters forbidden by NTFS/VFAT/exFAT,
 *                          including 1-31 and any of these: "*:<>?|. Doesn't
 *                          exclude \0, /, or \ (meaningful in C-string paths),
 *                          or several characters only forbidden pre-VFAT
 *                          (existing Windows-era games rely on these).
 * PATH_SAFE_DOS_DEVICE:    detect patch containing deserved DOS/Windows
 *                          device names, which can cause hangs or worse in
 *                          DOS and Windows.
 *
 * @param path        the path to safety check.
 * @param check_mask  a mask of the above enum values for checks to perform.
 * @return            the first check in the provided check_mask that fails,
 *                    otherwise PATH_SAFE_OK.
 */
enum path_safe_mask path_safety_check(const char *path, int check_mask)
{
  size_t len = strlen(path);
  const char *pos;

  if((check_mask & PATH_SAFE_UNIX_ROOT) && isslash(path[0]))
    return PATH_SAFE_UNIX_ROOT;

  if((check_mask & PATH_SAFE_DOS_ROOT) && strchr(path, ':'))
    return PATH_SAFE_DOS_ROOT;

  if(check_mask & PATH_SAFE_UNIX_PARENT)
  {
    pos = path;
    while((pos = strstr(pos, "..")))
    {
      if((pos == path || isslash(pos[-1])) &&
       (pos[2] == '\0' || isslash(pos[2])))
        return PATH_SAFE_UNIX_PARENT;

      pos += 2;
    }
  }

  if(check_mask & PATH_SAFE_AMIGA_PARENT)
  {
    pos = path;
    while((pos = strpbrk(pos, "/\\")))
    {
      if(isslash(pos[1]))
        return PATH_SAFE_AMIGA_PARENT;

      pos++;
    }
  }

  if(check_mask & PATH_SAFE_DOS_CHARACTER)
  {
    if(strpbrk(path,
          "\x01\x02\x03\x04\x05\x06\x07"
      "\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
      "\x10\x11\x12\x13\x14\x15\x16\x17"
      "\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
      "\"*:<>?|"))
      return PATH_SAFE_DOS_CHARACTER;
  }

  if(check_mask & PATH_SAFE_DOS_DEVICE)
  {
    size_t root_len = path_is_absolute(path);
    const char *end = path + len;

    /* path tokenize functions are currently destructive, so do this instead. */
    pos = path + root_len;
    for(;;)
    {
      size_t token_len;
      const char *next = strpbrk(pos, "/\\");
      token_len = (next ? next : end) - pos;

      if(path_safety_check_dos_device(pos, token_len))
        return PATH_SAFE_DOS_DEVICE;
      if(!next)
        break;

      pos = next + 1;
    }
  }
  return PATH_SAFE_OK;
}

/**
 * Get a descriptive error string for a path_safety_check result.
 *
 * @param   value   the return value from path_safety_check. If a mask of
 *                  multiple values is somehow passed here, the lowest value
 *                  in the set will be used.
 * @return          a statically allocated constant string describing the error.
 */
const char *path_safety_strerror(enum path_safe_mask value)
{
  int x = (int)value;

  switch(x & -x)
  {
    case PATH_SAFE_OK:            return "no error";
    case PATH_SAFE_UNIX_ROOT:     return "POSIX/UNC absolute path";
    case PATH_SAFE_DOS_ROOT:      return "DOS/Amiga absolute path";
    case PATH_SAFE_UNIX_PARENT:   return "'..' parent directory token";
    case PATH_SAFE_AMIGA_PARENT:  return "Amiga parent directory token";
    case PATH_SAFE_DOS_CHARACTER: return "DOS/Windows reserved character";
    case PATH_SAFE_DOS_DEVICE:    return "DOS/Windows reserved device";
    default:                      return "unknown";
  }
}

/**
 * Internal common implementation for `path_navigate` and `path_navigate_no_check`.
 */
static ssize_t path_navigate_internal(char *path, size_t path_len, const char *target,
 boolean allow_checks)
{
  struct stat stat_info;
  char buffer[MAX_PATH];
  const char *current;
  const char *next;
  const char *end;
  char current_char;
  size_t root_len;
  size_t len;

  if(!path || !target || !target[0])
    return -1;

  current = target;
  end = target + strlen(target);

  len = path_is_absolute(target);
  if(len)
  {
    /**
     * Destination starts with any type of root, including:
     *
     * 1) a Unix-style root directory e.g. / or \.
     * Aside from Unix-likes, these are also supported by console platforms.
     * Even Windows (back through XP at least) doesn't seem to mind them.
     *
     * 2) a DOS-style root directory e.g. C: or sdcard:/.
     * Aside from Windows, these are often used by console SDKs (albeit with /
     * instead of \) to distinguish SD cards and the like.
     * Linux and macOS also allow them for VFS roots (with double slashes).
     *
     * 3) a Windows-style UNC root (Windows only) e.g. \\localhost\share\
     * These are used for network folders and long paths.
     */
    next = target + len;
    snprintf(buffer, MAX_PATH, "%.*s" DIR_SEPARATOR, (int)len, target);
    path_clean(buffer, MAX_PATH);

    if(allow_checks && vstat(buffer, &stat_info) < 0)
    {
      trace("--PATH-- path_navigate_internal: stat of root '%s' failed (%d)\n",
       buffer, errno);
      return -1;
    }

    current = next;
    while(isslash(current[0]))
      current++;
  }
  else
  {
    /**
     * Destination is relative--start from the current path. Make sure there's
     * a trailing separator.
     */
    len = path_clean_copy(buffer, MAX_PATH, path);
    if(!len)
      return -1;

    if(!isslash(buffer[len - 1]) && len + 1 < MAX_PATH)
    {
      buffer[len++] = DIR_SEPARATOR_CHAR;
      buffer[len] = '\0';
    }
  }

#ifdef PATH_DOS_STYLE_ROOTS
  // Any attempted DOS-style root after the start is a broken path.
  if(strchr(current, ':'))
    return -1;
#endif

  current_char = current[0];
  len = strlen(buffer);
  root_len = path_is_absolute(buffer);

  // Apply directory fragments to the path.
  while(current_char != '\0')
  {
    // Increment next to skip the separator so it will be copied over.
    next = strpbrk(current, "/\\");
    if(!next) next = end;
    else      next++;

#ifndef PATH_AMIGA_STYLE_ROOTS
    // . does nothing, .. goes back one level
    if(current_char == '.' && (current[1] == '\0' || isslash(current[1])))
    {
      // current directory, don't do anything.
    }
    else

    if(current_char == '.' && current[1] == '.' &&
     (current[2] == '\0' || isslash(current[2])))
#else /* PATH_AMIGA_STYLE_ROOTS */
    /* Zero-length path token -> parent directory (as Unix ".."). */
    if((next - current) == 1 && isslash(current[0]))
#endif
    {
      // Skip the rightmost separator (current level) and look for the
      // previous separator. If found, truncate the path to it.
      // Do not attempt to consume portions of the root.
      if(len > root_len)
      {
        char *pos = buffer + len - 1;
        char *end = buffer + root_len;
        do
        {
          pos--;
        }
        while(pos >= end && !isslash(*pos));

        if(pos >= buffer)
        {
          pos[1] = '\0';
          len = strlen(buffer);
        }
      }
    }
    else
    {
      snprintf(buffer + len, MAX_PATH - len, "%.*s", (int)(next - current),
       current);
      buffer[MAX_PATH - 1] = '\0';
      len = strlen(buffer);
    }

    current = next;
    current_char = current[0];
  }

  // This needs to be done before the stat for some platforms (e.g. 3DS)
  len = path_clean(buffer, MAX_PATH);
  if(len < path_len)
  {
    trace("--PATH-- path_navigate_internal: '%s'\n", buffer);
    if(allow_checks)
    {
      if(vstat(buffer, &stat_info) < 0)
      {
        trace("--PATH-- path_navigate_internal: failed stat (%d)\n", errno);
        return -1;
      }
      if(!S_ISDIR(stat_info.st_mode))
      {
        trace("--PATH-- path_navigate_internal: not dir (%d)\n", (int)stat_info.st_mode);
        return -1;
      }
      if(vaccess(buffer, R_OK|X_OK) < 0)
      {
        trace("--PATH-- path_navigate_internal: failed access (%d)\n", errno);
        return -1;
      }
    }
    memcpy(path, buffer, len + 1);
    path[path_len - 1] = '\0';
    return len;
  }

  return -1;
}

/**
 * Navigate a directory path to a target like chdir. The provided directory
 * path must be a valid directory. The target may be a relative path or an
 * absolute path in either Unix or Windows style. If "." or ".." is found in
 * the target, it will be handled appropriately. If the final resulting path
 * successfully stats, the provided path will be overwritten with the
 * destination. If the final path is not valid or if an error occurs, the
 * provided path will not be modified.
 *
 * @param  path       Directory path to navigate from.
 * @param  path_len   Size of path buffer.
 * @param  target     Target to navigate to.
 * @return            The new length of the path, or -1 on error.
 */
ssize_t path_navigate(char *path, size_t path_len, const char *target)
{
  return path_navigate_internal(path, path_len, target, true);
}

/**
 * Like `path_navigate`, but no `vstat` or `vaccess` call will be performed to
 * check the resulting path. See `path_navigate` for more info.
 *
 * @param  path       Directory path to navigate from.
 * @param  path_len   Size of path buffer.
 * @param  target     Target to navigate to.
 * @return            The new length of the path, or -1 on error.
 */
ssize_t path_navigate_no_check(char *path, size_t path_len, const char *target)
{
  return path_navigate_internal(path, path_len, target, false);
}

/**
 * Create the parent directory of a given filename if it doesn't exist
 * (similar to mkdir -p). This function will call vstat multiple times and
 * will call vmkdir to create directories as needed. This function will not
 * make recursive calls.
 *
 * @param filename    Filename to create the parent directory of.
 * @return            `0` on success or a non-zero value on error.
 *                    See `enum path_create_error`.
 */
enum path_create_error path_create_parent_recursively(const char *filename)
{
  struct stat stat_info;
  char parent_directory[MAX_PATH];
  ssize_t pos;

  ssize_t parent_len = path_get_directory(parent_directory, MAX_PATH, filename);
  if(parent_len < 0)
    return PATH_CREATE_ERR_BUFFER;

  // No parent directory? Don't need to do anything...
  if(parent_len == 0)
    return PATH_CREATE_SUCCESS;

  /**
   * Step 1: walk the parent directory backwards and stat it successively until
   * it finds something that exists. Replace slashes with nuls; they can be
   * added back when needed again.
   */
  pos = parent_len;
  do
  {
    parent_directory[pos] = '\0';
    if(vstat(parent_directory, &stat_info))
    {
      // Make sure the error is that the dir is missing and not something else.
      if(errno != ENOENT)
        return PATH_CREATE_ERR_STAT_ERROR;
    }
    else

    /**
     * If a file exists where a directory needs to be placed there isn't
     * really anything else that can be done.
     */
    if(!S_ISDIR(stat_info.st_mode))
    {
      return PATH_CREATE_ERR_FILE_EXISTS;
    }
    else
      break;

    // Find the next slash.
    while(pos > 0 && !isslash(parent_directory[pos]))
      pos--;
  }
  while(pos > 0);

  // The entire path already exists? Nothing else needs to be done...
  if(pos == parent_len)
    return PATH_CREATE_SUCCESS;

  /**
   * Step 2: restore slashes and mkdir until the original end of the parent
   * directory is found.
   */
  while(pos < parent_len)
  {
    /**
     * If pos==0, the base of the path didn't exist and needs to be created.
     * Otherwise, look for slashes that were removed, fix them, and create
     * the next directory.
     */
    if(!pos || !parent_directory[pos])
    {
      if(!parent_directory[pos])
        parent_directory[pos] = DIR_SEPARATOR_CHAR;

      if(vmkdir(parent_directory, 0755))
        return PATH_CREATE_ERR_MKDIR_FAILED;
    }

    while(pos < parent_len && parent_directory[pos])
      pos++;
  }
  return PATH_CREATE_SUCCESS;
}

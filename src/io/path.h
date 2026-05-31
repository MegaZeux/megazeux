/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#ifndef MEGAZEUX_IO_PATH_H
#define MEGAZEUX_IO_PATH_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <stddef.h>

// Most platforms allow / as a root.
#define PATH_UNIX_STYLE_ROOTS

#if defined(_WIN32) || defined(CONFIG_DJGPP) || defined(CONFIG_DOS_STYLE_ROOTS)
// DOS-style roots with one slash (all platforms allow two).
#define PATH_DOS_STYLE_ROOTS
#endif

#if defined(CONFIG_AMIGA)
/* Amiga exists in an alternate universe where roots are delimited only
 * by the colon, and any 0-length dir component is equivalent to ".."
 * on everything else. This requires some pretty different handling. */
#define PATH_AMIGA_STYLE_ROOTS
#undef PATH_UNIX_STYLE_ROOTS
#endif

#if defined(_WIN32)
// Windows-style UNC roots.
#define PATH_UNC_ROOTS
#endif

#ifndef DIR_SEPARATOR
#if defined(_WIN32) || defined(CONFIG_DJGPP)
#define DIR_SEPARATOR "\\"
#define DIR_SEPARATOR_CHAR '\\'
#else /* !_WIN32 && !CONFIG_DJGPP */
#define DIR_SEPARATOR "/"
#define DIR_SEPARATOR_CHAR '/'
#endif
#endif /* DIR_SEPARATOR */

/* Standalone current/parent directory string for file manager usage.
 * Amiga has a parent directory token, but not a current directory token.
 * These defines are only guaranteed to work as standalone paths. */
#ifndef PATH_PARENT_DIR
#if defined(CONFIG_AMIGA)
#define PATH_CURRENT_ROOT ":"
#define PATH_CURRENT_DIR ""
#define PATH_PARENT_DIR "/"
#else
#define PATH_CURRENT_ROOT DIR_SEPARATOR
#define PATH_CURRENT_DIR "."
#define PATH_PARENT_DIR ".."
#endif
#endif /* PATH_PARENT_DIR */

/* Normalized root separator string for constructing paths. */
#ifndef PATH_ROOT_SEPARATOR
#if defined(PATH_AMIGA_STYLE_ROOTS)
#define PATH_ROOT_SEPARATOR ":"
#elif defined(PATH_DOS_STYLE_ROOTS)
#define PATH_ROOT_SEPARATOR ":" DIR_SEPARATOR
#else
#define PATH_ROOT_SEPARATOR ":" DIR_SEPARATOR DIR_SEPARATOR
#endif
#endif

enum path_safe_mask
{
  PATH_SAFE_OK            = 0,
  PATH_SAFE_UNIX_ROOT     = (1 << 0),       /* Path begins with a slash */
  PATH_SAFE_DOS_ROOT      = (1 << 1),       /* Path contains : anywhere */
  PATH_SAFE_UNIX_PARENT   = (1 << 2),       /* Path contains a . or .. token */
  PATH_SAFE_AMIGA_PARENT  = (1 << 3),       /* Path contains adjacent slashes */
  PATH_SAFE_DOS_CHARACTER = (1 << 4),       /* Path contains reserved char(s):
                                             * 1-31, or any of "*:<>?| */
  PATH_SAFE_DOS_DEVICE    = (1 << 5),       /* Path is a reserved DOS/Windows
                                             * device that can cause hangs. */

  PATH_SAFE_ANY_ROOT      = (PATH_SAFE_UNIX_ROOT | PATH_SAFE_DOS_ROOT),
  PATH_SAFE_ANY           = (PATH_SAFE_ANY_ROOT | PATH_SAFE_UNIX_PARENT |
                             PATH_SAFE_AMIGA_PARENT | PATH_SAFE_DOS_CHARACTER |
                             PATH_SAFE_DOS_DEVICE),
};

enum path_create_error
{
  PATH_CREATE_SUCCESS,
  PATH_CREATE_ERR_BUFFER,
  PATH_CREATE_ERR_STAT_ERROR,
  PATH_CREATE_ERR_MKDIR_FAILED,
  PATH_CREATE_ERR_FILE_EXISTS
};

/**
 * Determine if a character is a path separating slash.
 * @param  chr  Character to check.
 * @return      True if the character is a slash, otherwise false.
 */
static inline boolean isslash(const char chr)
{
  return (chr == '\\') || (chr == '/');
}

UTILS_LIBSPEC char *path_tokenize(char **next);
UTILS_LIBSPEC char *path_reverse_tokenize(char **base, size_t *base_len);

UTILS_LIBSPEC boolean path_force_ext(char *path, size_t buffer_len, const char *ext);
UTILS_LIBSPEC ssize_t path_get_ext_offset(const char *path);

UTILS_LIBSPEC ssize_t path_is_absolute(const char *path);
UTILS_LIBSPEC ssize_t path_is_absolute_dos(const char *path);
UTILS_LIBSPEC boolean path_is_root(const char *path);
UTILS_LIBSPEC boolean path_has_directory(const char *path);
UTILS_LIBSPEC ssize_t path_to_directory(char *path, size_t buffer_len);
UTILS_LIBSPEC ssize_t path_to_filename(char *path, size_t buffer_len);
UTILS_LIBSPEC ssize_t path_get_directory(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC ssize_t path_get_filename(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC boolean path_get_directory_and_filename(char *d_dest, size_t d_len,
 char *f_dest, size_t f_len, const char *path);
UTILS_LIBSPEC ssize_t path_get_parent(char *dest, size_t dest_len,
 const char *path);

UTILS_LIBSPEC size_t path_clean(char *path, size_t path_len);
UTILS_LIBSPEC size_t path_clean_copy(char *dest, size_t dest_len, const char *path);
UTILS_LIBSPEC size_t path_clean_copy_posixdos(char *dest, size_t dest_len,
 const char *path);
UTILS_LIBSPEC ssize_t path_clean_current_tokens(char *path, size_t path_len);
UTILS_LIBSPEC ssize_t path_clean_root(char *path, size_t path_len);
UTILS_LIBSPEC ssize_t path_append(char *path, size_t buffer_len, const char *rel);
UTILS_LIBSPEC ssize_t path_join(char *dest, size_t dest_len, const char *base,
 const char *rel);
UTILS_LIBSPEC ssize_t path_remove_prefix(char *path, size_t buffer_len,
 const char *prefix, size_t prefix_len);

UTILS_LIBSPEC enum path_safe_mask path_safety_check(const char *path, int flags);
UTILS_LIBSPEC const char *path_safety_strerror(enum path_safe_mask value);

UTILS_LIBSPEC ssize_t path_navigate(char *path, size_t path_len,
 const char *target);
UTILS_LIBSPEC ssize_t path_navigate_no_check(char *path, size_t path_len,
 const char *target);

UTILS_LIBSPEC enum path_create_error path_create_parent_recursively(
 const char *filename);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_PATH_H */

/* MegaZeux
 *
 * Copyright (C) 2026 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "string_sep.h"

#include <string.h>

/**
 * Thread-safe replacement for strtok. This implementation should be
 * equivalent to the GNU extension function.
 *
 * @param cursor  pointer to the current position in the string. The next
 *                position in the string will be written here if applicable.
 * @param delim   a string containing a list of one or more delimiter
 *                characters to search for.
 * @return        the beginning of the next token (i.e. the old value
 *                pointed to by the cursor pointer), or NULL if there are no
 *                further tokens.
 */
char *mzx_strsep(char ** RESTRICT cursor, const char * RESTRICT delim)
{
  char *ret;
  char *next;

  if(cursor == NULL || *cursor == NULL || delim == NULL)
    return NULL;

  ret = *cursor;
  next = strpbrk(ret, delim);
  if(next != NULL)
    *(next++) = '\0';

  *cursor = next;
  return ret;
}

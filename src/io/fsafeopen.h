/* MegaZeux
 *
 * Copyright (C) 2004 Alistair Strachan <alistair@devzero.co.uk>
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

#ifndef MEGAZEUX_IO_FSAFEOPEN_H
#define MEGAZEUX_IO_FSAFEOPEN_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include "vfile.h"

enum
{
  FSAFE_SUCCESS = 0,
  FSAFE_MATCHED_DIRECTORY,
  FSAFE_MATCH_FAILED,
  FSAFE_BRUTE_FORCE_FAILED,
  FSAFE_BRUTE_FORCE_SFN_AMBIGUOUS,
  FSAFE_BRUTE_FORCE_SFN_OVERFLOW,
  FSAFE_INVALID_ARGUMENT,
  FSAFE_ABSOLUTE_PATH_ERROR,
  FSAFE_WINDOWS_DRIVE_LETTER_ERROR,
  FSAFE_PARENT_DIRECTORY_ERROR,
  FSAFE_DOS_RESERVED_CHARACTER_ERROR,
  FSAFE_DOS_DEVICE_NAME_ERROR
};

int fsafetranslate(const char *path, char *newpath, size_t buffer_len);
vfile *fsafeopen(const char *path, const char *mode);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_FSAFEOPEN_H */

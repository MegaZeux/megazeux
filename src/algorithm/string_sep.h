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

#ifndef MEGAZEUX_ALGORITHM_STRING_SEP_H
#define MEGAZEUX_ALGORITHM_STRING_SEP_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

/* This function is a GNU and BSD extension and isn't guaranteed to be
 * present in the MinGW/MSVC and Amiga C libraries. */
#if defined(_WIN32) || defined(__amigaos__)
#define strsep  mzx_strsep
#endif

UTILS_LIBSPEC char *mzx_strsep(char ** RESTRICT cursor, const char * RESTRICT delim);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ALGORITHM_STRING_SEP_H */

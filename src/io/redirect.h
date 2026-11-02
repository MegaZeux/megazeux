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

#ifndef MEGAZEUX_IO_REDIRECT_H
#define MEGAZEUX_IO_REDIRECT_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_STDIO_REDIRECT
UTILS_LIBSPEC extern FILE *mzxout_h;
UTILS_LIBSPEC extern FILE *mzxerr_h;
#define mzxout (mzxout_h ? mzxout_h : stdout)
#define mzxerr (mzxerr_h ? mzxerr_h : stderr)
#else
#define mzxout stdout
#define mzxerr stderr
#endif

UTILS_LIBSPEC boolean redirect_stdio_init(const char *base_path, boolean require_conf);
UTILS_LIBSPEC void redirect_stdio_exit(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_REDIRECT_H */

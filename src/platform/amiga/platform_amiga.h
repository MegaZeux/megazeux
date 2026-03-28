/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
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

#ifndef MEGAZEUX_PLATFORM_AMIGA_H
#define MEGAZEUX_PLATFORM_AMIGA_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

CORE_LIBSPEC extern long __stack_chk_guard[8];
CORE_LIBSPEC void __stack_chk_fail(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_AMIGA_H */

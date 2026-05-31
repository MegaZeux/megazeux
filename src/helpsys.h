/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

#ifndef MEGAZEUX_HELPSYS_H
#define MEGAZEUX_HELPSYS_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_HELPSYS

#include "core.h"
#include "world_struct.h"

CORE_LIBSPEC void help_open(struct world *mzx_world, const char *file_name);
CORE_LIBSPEC void help_close(struct world *mzx_world);
CORE_LIBSPEC void help_system(context *ctx, struct world *mzx_world);

#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_HELPSYS_H */

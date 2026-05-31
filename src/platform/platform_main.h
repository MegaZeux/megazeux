/* MegaZeux
 *
 * Copyright (C) 2008 Alan Williams <mralert@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_MAIN_H
#define MEGAZEUX_PLATFORM_MAIN_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_REPLACE_MAIN

int real_main(int argc, char *argv[]);
#define main real_main

#endif /* CONFIG_REPLACE_MAIN */

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_MAIN_H */

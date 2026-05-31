/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

#ifndef MEGAZEUX_EXTMEM_NDS_H
#define MEGAZEUX_EXTMEM_NDS_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

boolean nds_ram_init(void);

uint32_t *platform_extram_alloc(size_t len);
uint32_t *platform_extram_resize(void *buffer, size_t len);
void platform_extram_free(void *buffer);
void platform_extram_lock(void);
void platform_extram_unlock(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EXTMEM_NDS_H */

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

#ifndef MEGAZEUX_RUN_STUBS_H
#define MEGAZEUX_RUN_STUBS_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#ifdef CONFIG_EDITOR
boolean is_editor(void);
void editor_init(void);
void init_macros(void);

void default_editor_config(void);
void store_editor_config_backup(void);
void free_editor_config(void);
#else
static inline boolean is_editor(void) { return false; }
static inline void editor_init(void) {}
static inline void init_macros(void) {}

static inline void default_editor_config(void) {}
static inline void store_editor_config_backup(void) {}
static inline void free_editor_config(void) {}
#endif


#ifdef CONFIG_UPDATER
boolean updater_init(void);
boolean is_updater(void);
#else
static inline boolean updater_init(void)
{ return true; }
static inline boolean is_updater(void)
{ return false; }
#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_RUN_STUBS_H */

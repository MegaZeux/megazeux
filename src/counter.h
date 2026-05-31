/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1999 Charles Goetzman
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

#ifndef MEGAZEUX_COUNTER_H
#define MEGAZEUX_COUNTER_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "counter_struct.h"
#include "world_struct.h"

// Settings SAVE_ROBOT should use for disassembling Robotic.
#define SAVE_ROBOT_DISASM_EXTRAS  true
#define SAVE_ROBOT_DISASM_BASE    10

CORE_LIBSPEC void counter_fsg(void);
CORE_LIBSPEC int match_function_counter(const char *dest, const char *src);
CORE_LIBSPEC int get_counter(struct world *mzx_world, const char *name, int id);
CORE_LIBSPEC const struct counter *get_counter_pointer(struct world *mzx_world,
 const char *name, int id);
CORE_LIBSPEC void set_counter(struct world *mzx_world, const char *name,
 int value, int id);
CORE_LIBSPEC void new_counter(struct world *mzx_world, const char *name,
 int value, int id);
CORE_LIBSPEC void sort_counter_list(struct counter_list *counter_list);
CORE_LIBSPEC void counter_list_size(struct counter_list *counter_list,
 size_t *list_size, size_t *table_size, size_t *counters_size);

void initialize_gateway_functions(struct world *mzx_world);
void inc_counter(struct world *mzx_world, const char *name, int value, int id);
void dec_counter(struct world *mzx_world, const char *name, int value, int id);
void mul_counter(struct world *mzx_world, const char *name, int value, int id);
void div_counter(struct world *mzx_world, const char *name, int value, int id);
void mod_counter(struct world *mzx_world, const char *name, int value, int id);

int set_counter_special(struct world *mzx_world, char *char_value,
 int value, int id);

void load_new_counter(struct counter_list *counter_list, int index,
 const char *name, int name_length, int value);

void clear_counter_list(struct counter_list *counter_list);

// Even old games tended to use at least this many.
#define MIN_COUNTER_ALLOCATE 32

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_COUNTER_H */

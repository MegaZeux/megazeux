/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_PLATFORM_DSO_H
#define MEGAZEUX_PLATFORM_DSO_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

/* Use as (dso_fn_ptr *) to store a loaded void(*)(void) to a function pointer. */
typedef void (*dso_fn_ptr)(void);
struct dso_library;

/* Initialize a (dso_fn_ptr *) via (void *) to avoid strict aliasing warnings. */
union dso_fn_ptr_ptr
{
  void *in;
  dso_fn_ptr *value;
};

/* SDL1/2, dlopen return void * instead of a function pointer. */
union dso_suppress_warning
{
  void *in;
  dso_fn_ptr out;
};

struct dso_syms_map
{
  const char *name;
  union dso_fn_ptr_ptr sym_ptr;
};

#define DSO_MAP_END { NULL, { NULL }}

CORE_LIBSPEC struct dso_library *platform_load_library(const char *name);
CORE_LIBSPEC void platform_unload_library(struct dso_library *library);
CORE_LIBSPEC boolean platform_load_function(struct dso_library *library,
 const struct dso_syms_map *syms_map);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_PLATFORM_DSO_H */

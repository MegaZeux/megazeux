/* MegaZeux
 *
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Standalone copy of the check_alloc functions so the utils can link fewer
 * core objects from MZX.
 */

#ifndef MEGAZEUX_UTILS_ALLOC_H
#define MEGAZEUX_UTILS_ALLOC_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

/* Workaround for linking zip.o */
FILE *mzxout_h = NULL;
FILE *mzxerr_h = NULL;

#ifdef CONFIG_CHECK_ALLOC

#include <stdlib.h>

static void out_of_memory_check(void *p, const char *file, int line)
{
  if(!p)
  {
    fprintf(stderr, "Out of memory in %s:%d\n", file, line);
    fflush(stderr);
    exit(-1);
  }
}

CORE_LIBSPEC void *check_calloc(size_t nmemb, size_t size, const char *file, int line)
{
  void *result = calloc(nmemb, size);
  out_of_memory_check(result, file, line);
  return result;
}

CORE_LIBSPEC void *check_malloc(size_t size, const char *file, int line)
{
  void *result = malloc(size);
  out_of_memory_check(result, file, line);
  return result;
}

CORE_LIBSPEC void *check_realloc(void *ptr, size_t size, const char *file, int line)
{
  void *result = realloc(ptr, size);
  out_of_memory_check(result, file, line);
  return result;
}

#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_UTILS_ALLOC_H */

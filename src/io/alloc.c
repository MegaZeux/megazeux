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

/* Virtual filesystem malloc/calloc/realloc wrappers.
 * These functions attempt to free memory from the virtual filesystem cache
 * when allocation fails. This requires an allocator with a functioning
 * return value (i.e. not Linux). Note that these functions can NOT be safely
 * called from vfs.c or during any vio.c operation performed while the VFS is
 * under write lock. Because vfs.c relies on path.c internally, it also can't
 * be used in path.c functions.
 *
 * The pointers returned by these functions should be freed with stdlib free().
 */

#include "alloc.h"
#include "vio.h"

#include <stdlib.h>

static boolean try_invalidate_cache(size_t amount, boolean *retry)
{
  if(!*retry || !amount)
    return false;

  if(!vio_invalidate_at_least(&amount))
  {
    vio_invalidate_all();
    *retry = false;
  }
  return true;
}

void *mzx_io_malloc(size_t sz)
{
  void *ret = malloc(sz);
  if(ret == NULL && sz != 0)
  {
    boolean retry = true;
    while(ret == NULL && try_invalidate_cache(sz, &retry))
      ret = malloc(sz);
  }
  return ret;
}

void *mzx_io_calloc(size_t nmemb, size_t sz)
{
  void *ret = calloc(nmemb, sz);
  if(ret == NULL && sz != 0 && nmemb != 0 && sz <= SIZE_MAX / nmemb)
  {
    boolean retry = true;
    size_t tmp = nmemb * sz;
    while(ret == NULL && try_invalidate_cache(tmp, &retry))
      ret = calloc(nmemb, sz);
  }
  return ret;
}

void *mzx_io_realloc(void *ptr, size_t sz)
{
  void *ret = realloc(ptr, sz);
  if(ret == NULL && sz != 0)
  {
    boolean retry = true;
    while(ret == NULL && try_invalidate_cache(sz, &retry))
      ret = realloc(ptr, sz);
  }
  return ret;
}

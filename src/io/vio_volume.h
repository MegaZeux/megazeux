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

#ifndef MEGAZEUX_IO_VIO_VOLUME_H
#define MEGAZEUX_IO_VIO_VOLUME_H

#include "../compat.h"
#include "../util.h"
#include "path.h"

MEGAZEUX_BEGIN_DECLS

/* Volume list reading implementations for vio_posix.h
 * These are more involved than the rest of vio_posix.h
 * but generally not worth their own compilation units. */

#if defined(CONFIG_DJGPP)

#include <dir.h>

struct vvolumelist_handle
{
  int current_disk;
  int max_disk;
  int pos;
};

static inline boolean platform_vvolumelist_open(struct vvolumelist_handle *vh)
{
  vh->current_disk = getdisk();
  vh->max_disk = setdisk(vh->current_disk);
  vh->pos = 0;
  return true;
}

static inline void platform_vvolumelist_close(struct vvolumelist_handle *vh)
{
  setdisk(vh->current_disk);
}

static inline int platform_vvolumelist_read(struct vvolumelist_handle *vh,
 char *buf, size_t buf_sz)
{
  while(vh->pos < vh->max_disk)
  {
    int num = (vh->pos++);

    setdisk(num);
    if(getdisk() != num)
      continue;

    return snprintf(buf, buf_sz, "%c:", 'A' + (char)num);
  }
  return 0;
}


#elif defined(CONFIG_AMIGA)

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>

struct vvolumelist_handle
{
  struct DosList *dl;
  size_t pos;
};

static inline boolean platform_vvolumelist_open(struct vvolumelist_handle *vh)
{
  vh->dl = LockDosList(LDF_VOLUMES | LDF_READ);
  vh->pos = 0;
  return true;
}

static inline void platform_vvolumelist_close(struct vvolumelist_handle *vh)
{
  UnLockDosList(LDF_VOLUMES | LDF_READ);
}

static inline int platform_vvolumelist_read(struct vvolumelist_handle *vh,
 char *buf, size_t buf_sz)
{
  /* Patch in SYS: after the reported list. */
  static const char * const extras[] =
  {
    "SYS"
  };

  while((vh->dl = NextDosEntry(vh->dl, LDF_VOLUMES)))
  {
    uint8_t *bname = (uint8_t *)BADDR(vh->dl->dol_Name);
    int len = bname[0];

    return snprintf(buf, buf_sz, "%*.*s:", len, len, (char *)bname + 1);
  }

  while(vh->pos < ARRAY_SIZE(extras))
  {
    size_t num = (vh->pos++);
    return snprintf(buf, buf_sz, "%s:", extras[num]);
  }
  return 0;
}


#elif defined(CONFIG_WII)

#include <sys/iosupport.h>

struct vvolumelist_handle
{
  int pos;
};

static inline boolean platform_vvolumelist_open(struct vvolumelist_handle *vh)
{
  vh->pos = 0;
  return true;
}

static inline void platform_vvolumelist_close(struct vvolumelist_handle *vh)
{
  /* nop */
  (void)vh;
}

static inline int platform_vvolumelist_read(struct vvolumelist_handle *vh,
 char *buf, size_t buf_sz)
{
  while(vh->pos < STD_MAX)
  {
    int num = (vh->pos++);

    if(devoptab_list[num] && devoptab_list[num]->chdir_r)
      return snprintf(buf, buf_sz, "%s:", devoptab_list[num]->name);
  }
  return 0;
}


#else /* Use a list of hardcoded roots. */

static const char * const volume_list[] =
{
#if defined(CONFIG_PSVITA)
  "app0:",
  "imc0:",
  "uma0:",
  "ux0:"
#else /* Normal POSIX--patch in / and nothing else. */
  DIR_SEPARATOR
#endif
};

struct vvolumelist_handle
{
  unsigned pos;
};

static inline boolean platform_vvolumelist_open(struct vvolumelist_handle *vh)
{
  vh->pos = 0;
  return true;
}

static inline void platform_vvolumelist_close(struct vvolumelist_handle *vh)
{
  /* nop */
  (void)vh;
}

static inline int platform_vvolumelist_read(struct vvolumelist_handle *vh,
 char *buf, size_t buf_sz)
{
  while(vh->pos < ARRAY_SIZE(volume_list))
  {
    unsigned num = (vh->pos++);

    return snprintf(buf, buf_sz, "%s", volume_list[num]);
  }
  return 0;
}

#endif

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_VIO_VOLUME_H */

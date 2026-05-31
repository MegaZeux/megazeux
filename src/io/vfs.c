/* MegaZeux
 *
 * Copyright (C) 2021-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

#include "vfs.h"

#ifdef VIRTUAL_FILESYSTEM

#ifdef VIRTUAL_FILESYSTEM_PARALLEL
#include "../platform/platform.h"
#endif

#include "../util.h"
#include "path.h"
#include "vio.h"

// Maximum reference count for a given VFS inode.
#define VFS_MAX_REFCOUNT 255
// Maximum name length.
#define VFS_MAX_NAME UINT16_MAX

#define VFS_DEFAULT_FILE_SIZE 32
#define VFS_DEFAULT_DIR_SIZE 4

#define VFS_NO_INODE 0
#define VFS_ROOT_INODE 1

#define VFS_IDX_SELF   0
#define VFS_IDX_PARENT 1

#define VFS_INODE_TYPE(n) ((n)->flags & VFS_INODE_TYPEMASK)
#define VFS_IS_CACHED(n) ((n)->timestamp != 0)
#define VFS_IS_INVALIDATED(n) ((n)->parent == VFS_NO_INODE)

enum vfs_inode_flags
{
  VFS_INODE_FILE       = (1<<0),
  VFS_INODE_DIR        = (1<<1),
  VFS_INODE_TYPEMASK   = VFS_INODE_FILE | VFS_INODE_DIR,
  VFS_INODE_IS_REAL    = (1<<2), // Cache of a real location in the filesystem.
  VFS_INODE_NAME_ALLOC = (1<<7),
};

union vfs_inode_contents
{
  void *has_contents;
  uint32_t *inodes;
  unsigned char *data;
};

/* `name` buffer is long enough to confortably fit null terminated 8.3 names. */
struct vfs_inode
{
  union vfs_inode_contents contents;
  size_t length;
  size_t length_alloc;
  uint32_t timestamp;
  int64_t create_time;
  int64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  uint16_t name_length;
  uint32_t parent;
  char name[16];
};

/* If `flags` & VFS_INODE_NAME_ALLOC, `name` is a pointer instead of a buffer.
 * `name` is still null terminated in this case. Alloc size is strlen(name)+1. */
struct vfs_inode_name_alloc
{
  union vfs_inode_contents contents;
  size_t length;
  size_t length_alloc;
  uint32_t timestamp;
  int64_t create_time;
  int64_t modify_time;
  uint8_t flags; // 0=deleted
  uint8_t refcount; // If 255, refuse creation of new refs.
  uint16_t name_length;
  uint32_t parent;
  char *name;
};

struct vfilesystem
{
  struct vfs_inode **table;
  uint32_t table_length;
  uint32_t table_alloc;
  uint32_t table_next;
  uint32_t current;
  uint32_t current_root;
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_thread_id origin;
  platform_mutex lock;
  platform_cond cond;
  int num_writers;
  int num_readers;
  int num_promotions;
#endif
  size_t cache_total;
  boolean is_writer;
  boolean disable_timestamp;
  enum vfs_error error;
  char current_path[MAX_PATH];
  size_t current_path_len;
};

static time_t vfs_get_date(void)
{
  return time(NULL);
}

static uint32_t vfs_get_timestamp(void)
{
  uint32_t t = (uint32_t)vfs_get_date();
  return t ? t : 1;
}

static int vfs_name_cmp(const char *a, const char *b)
{
#ifdef VIRTUAL_FILESYSTEM_CASE_INSENSITIVE
  return strcasecmp(a, b);
#else
  return strcmp(a, b);
#endif
}


/**
 * vfs_inode functions.
 */

static boolean vfs_inode_init_name(struct vfs_inode *n, const char *name)
{
  size_t name_len = strlen(name);
  if(name_len > VFS_MAX_NAME)
    return false;

  if(name_len >= sizeof(n->name))
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    char *inode_name = (char *)malloc(name_len + 1);
    if(!inode_name)
      return false;

    _n->flags |= VFS_INODE_NAME_ALLOC;
    _n->name = inode_name;
    memcpy(_n->name, name, name_len + 1);
  }
  else
  {
    n->flags &= ~VFS_INODE_NAME_ALLOC;
    memcpy(n->name, name, name_len + 1);
  }

  n->name_length = name_len;
  return true;
}

static boolean vfs_inode_rename(struct vfs_inode *n, const char *name)
{
  if(n->flags & VFS_INODE_NAME_ALLOC)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    char *prev_name = _n->name;

    if(!vfs_inode_init_name(n, name))
    {
      _n->name = prev_name;
      return false;
    }
    free(prev_name);
    return true;
  }
  return vfs_inode_init_name(n, name);
}

static inline const char *vfs_inode_name(struct vfs_inode *n)
{
  if(n->flags & VFS_INODE_NAME_ALLOC)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;
    return _n->name;
  }
  return n->name;
}

static boolean vfs_inode_init_file(struct vfs_inode *n, const char *name,
 size_t init_alloc, boolean is_real)
{
  if(init_alloc < VFS_DEFAULT_FILE_SIZE)
    init_alloc = VFS_DEFAULT_FILE_SIZE;

  n->contents.data = (unsigned char *)malloc(init_alloc);
  if(!n->contents.data)
    return false;

  n->length = 0;
  n->length_alloc = init_alloc;
  n->timestamp = is_real ? 1 : 0;
  n->create_time = 0;
  n->modify_time = 0;
  n->refcount = 0;
  n->flags = VFS_INODE_FILE;
  n->parent = VFS_NO_INODE;
  if(is_real)
    n->flags |= VFS_INODE_IS_REAL;

  if(!vfs_inode_init_name(n, name))
  {
    free(n->contents.data);
    n->flags = 0;
    return false;
  }
  return true;
}

static boolean vfs_inode_init_directory(struct vfs_inode *n, const char *name,
 size_t init_alloc, boolean is_real)
{
  if(init_alloc < VFS_DEFAULT_DIR_SIZE)
    init_alloc = VFS_DEFAULT_DIR_SIZE;

  n->contents.inodes = (uint32_t *)malloc(init_alloc * sizeof(uint32_t));
  if(!n->contents.inodes)
    return false;

  n->length = 2;
  n->length_alloc = init_alloc;
  n->timestamp = is_real ? 1 : 0;
  n->create_time = 0;
  n->modify_time = 0;
  n->refcount = 0;
  n->flags = VFS_INODE_DIR;
  n->parent = VFS_NO_INODE;
  if(is_real)
    n->flags |= VFS_INODE_IS_REAL;

  if(!vfs_inode_init_name(n, name))
  {
    free(n->contents.inodes);
    n->flags = 0;
    return false;
  }
  return true;
}

static void vfs_inode_clear(struct vfs_inode *n)
{
  if(n->flags)
  {
    struct vfs_inode_name_alloc *_n = (struct vfs_inode_name_alloc *)n;

    if(n->contents.has_contents)
    {
      free(n->contents.has_contents);
      n->contents.has_contents = NULL;
    }

    if(n->flags & VFS_INODE_NAME_ALLOC)
    {
      free(_n->name);
      _n->name = NULL;
    }

    n->flags = 0;
  }
}

static boolean vfs_inode_expand_directory(struct vfs_inode *n, size_t count)
{
  uint32_t *inodes;
  uint64_t new_length = n->length + count;
  uint64_t new_alloc = n->length_alloc;

  if(new_length > UINT32_MAX)
    return false;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);

  if(n->length_alloc >= new_length)
    return true;

  while(new_length > new_alloc)
    new_alloc <<= 1;

  if(new_alloc > UINT32_MAX)
    return false;

  inodes = (uint32_t *)realloc(n->contents.inodes, new_alloc * 2 * sizeof(uint32_t));
  if(!inodes)
    return false;

  n->contents.inodes = inodes;
  n->length_alloc = new_alloc;
  return true;
}

static boolean vfs_inode_insert_directory(struct vfs_inode *n, uint32_t inode, uint32_t pos)
{
  uint32_t *inodes;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(pos >= 2 && pos <= n->length);

  if(n->length == UINT32_MAX)
    return false;

  inodes = n->contents.inodes;

  if(pos < n->length)
    memmove(inodes + pos + 1, inodes + pos, sizeof(uint32_t) * (n->length - pos));

  inodes[pos] = inode;
  n->length++;
  return true;
}

static boolean vfs_inode_delete_directory(struct vfs_inode *n, uint32_t pos)
{
  uint32_t *inodes;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(pos >= 2 && pos < n->length);

  inodes = n->contents.inodes;

  if(pos < n->length - 1)
    memmove(inodes + pos, inodes + pos + 1, sizeof(uint32_t) * (n->length - pos - 1));
  n->length--;
  return true;
}

static boolean vfs_inode_move_directory(struct vfs_inode *n, uint32_t old_pos,
 uint32_t new_pos)
{
  uint32_t *inodes;
  uint32_t inode;

  assert(VFS_INODE_TYPE(n) == VFS_INODE_DIR);
  assert(old_pos >= 2 && old_pos < n->length);
  assert(new_pos >= 2 && new_pos < n->length);

  inodes = n->contents.inodes;
  inode = inodes[old_pos];

  if(old_pos < new_pos)
  {
    memmove(inodes + old_pos, inodes + old_pos + 1, sizeof(uint32_t) * (new_pos - old_pos));
    inodes[new_pos] = inode;
  }
  else

  if(old_pos > new_pos)
  {
    memmove(inodes + new_pos + 1, inodes + new_pos, sizeof(uint32_t) * (old_pos - new_pos));
    inodes[new_pos] = inode;
  }
  return true;
}


/**
 * vfilesystem functions.
 */

static uint32_t vfs_seterror(vfilesystem *vfs, enum vfs_error e)
{
  vfs->error = e;
  // Most functions setting the error return VFS_NO_INODE,
  // so doing so here makes things slightly cleaner.
  return VFS_NO_INODE;
}

static enum vfs_error vfs_geterror(vfilesystem *vfs)
{
  int error = vfs->error;
  vfs->error = VFS_ERR_UNKNOWN;
  return error;
}

static boolean vfs_init_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_mutex_init(&(vfs->lock));
  platform_cond_init(&(vfs->cond));
  vfs->origin = platform_get_thread_id();
  vfs->num_readers = 0;
  vfs->num_writers = 0;
  vfs->num_promotions = 0;
  vfs->is_writer = false;
  return true;
#endif
  vfs->is_writer = true;
  return true;
}

static boolean vfs_clear_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  platform_mutex_destroy(&(vfs->lock));
  platform_cond_destroy(&(vfs->cond));
#endif
  return true;
}

static boolean vfs_read_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
  {
    vfs_seterror(vfs, VFS_ERR_UNKNOWN);
    return false;
  }

  while(vfs->num_writers || vfs->is_writer)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_readers++;

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_read_unlock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
  {
    vfs_seterror(vfs, VFS_ERR_UNKNOWN);
    return false;
  }

  assert(vfs->num_readers > 0);
  vfs->num_readers--;
  if(!vfs->num_readers)
    platform_cond_broadcast(&(vfs->cond));

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_write_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
  {
    vfs_seterror(vfs, VFS_ERR_UNKNOWN);
    return false;
  }

  vfs->num_writers++;

  while(vfs->num_readers || vfs->is_writer)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_writers--;
  vfs->is_writer = true;

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

static boolean vfs_write_unlock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
  {
    vfs_seterror(vfs, VFS_ERR_UNKNOWN);
    return false;
  }

  assert(vfs->is_writer);
  vfs->is_writer = false;
  platform_cond_broadcast(&(vfs->cond));

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

/**
 * Promote the current thread's reader lock to a writer lock. On failure,
 * returns `false`, and the current thread will still have a reader lock.
 */
static boolean vfs_elevate_lock(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  if(!platform_mutex_lock(&(vfs->lock)))
  {
    vfs_seterror(vfs, VFS_ERR_UNKNOWN);
    return false;
  }

  vfs->num_writers++;
  vfs->num_promotions++;

  while(vfs->num_readers > vfs->num_promotions || vfs->is_writer)
    platform_cond_wait(&(vfs->cond), &(vfs->lock));

  vfs->num_readers--;
  vfs->num_writers--;
  vfs->num_promotions--;
  vfs->is_writer = true;

  platform_mutex_unlock(&(vfs->lock));
#endif
  return true;
}

/**
 * Initialize the given VFS. The initialized VFS will contain one root inode
 * corresponding to C: (Windows) or / (everything else).
 */
static boolean vfs_setup(vfilesystem *vfs)
{
  struct vfs_inode *n;

  memset(vfs, 0, sizeof(vfilesystem));

  vfs->table = (struct vfs_inode **)calloc(4, sizeof(struct vfs_inode *));
  if(!vfs->table)
    return false;

  vfs->table[VFS_NO_INODE] = (struct vfs_inode *)calloc(1, sizeof(struct vfs_inode));
  vfs->table[VFS_ROOT_INODE] = (struct vfs_inode *)calloc(1, sizeof(struct vfs_inode));
  if(!vfs->table[VFS_NO_INODE] || !vfs->table[VFS_ROOT_INODE])
    return false;

  if(!vfs_init_lock(vfs))
    return false;

  vfs->table_length = 2;
  vfs->table_alloc = 4;
  vfs->table_next = 2;
  vfs->current = VFS_ROOT_INODE;
  vfs->current_root = VFS_ROOT_INODE;
  vfs->disable_timestamp = false;
  vfs_seterror(vfs, VFS_ERR_UNKNOWN);

  /* 0: list of roots. */
  n = vfs->table[VFS_NO_INODE];
  n->contents.inodes = (uint32_t *)malloc(3 * sizeof(uint32_t));
  if(!n->contents.inodes)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_NO_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_NO_INODE;
  n->contents.inodes[2] = VFS_ROOT_INODE;
  n->length = 3;
  n->length_alloc = 3;
  n->timestamp = 0;
  n->flags = VFS_INODE_DIR;
  n->refcount = 255;
  n->parent = VFS_NO_INODE;
  vfs_inode_init_name(n, "");

  /* 1: / or C:\ */
  n = vfs->table[VFS_ROOT_INODE];
  n->contents.inodes = (uint32_t *)malloc(4 * sizeof(uint32_t));
  if(!n->contents.inodes)
    return false;

  n->contents.inodes[VFS_IDX_SELF]   = VFS_ROOT_INODE;
  n->contents.inodes[VFS_IDX_PARENT] = VFS_ROOT_INODE;
  n->length = 2;
  n->length_alloc = 4;
  n->timestamp = 0;
  n->create_time = vfs_get_date();
  n->modify_time = n->create_time;
  n->flags = VFS_INODE_DIR;
  n->refcount = 0;
  n->parent = VFS_ROOT_INODE;
#if defined(VIRTUAL_FILESYSTEM_DOS_DRIVE)
  vfs_inode_init_name(n, "C");
  strcpy(vfs->current_path, "C:\\");
#elif defined(VIRTUAL_FILESYSTEM_AMIGA_DRIVE)
  vfs_inode_init_name(n, "sys");
  strcpy(vfs->current_path, "sys:");
#else
  vfs_inode_init_name(n, "/");
  strcpy(vfs->current_path, n->name);
#endif

  vfs->current_path_len = strlen(vfs->current_path);
  return true;
}

/**
 * Release all allocated memory for a given VFS.
 */
static void vfs_clear(vfilesystem *vfs)
{
  size_t i;
  for(i = 0; i < vfs->table_length; i++)
  {
    if(vfs->table[i])
    {
      vfs_inode_clear(vfs->table[i]);
      free(vfs->table[i]);
    }
  }

  vfs_clear_lock(vfs);

  free(vfs->table);
  vfs->table = NULL;
}

/**
 * Allocate a given inode.
 */
static struct vfs_inode *vfs_allocate_inode(vfilesystem *vfs, uint32_t inode)
{
  vfs->table[inode] = (struct vfs_inode *)malloc(sizeof(struct vfs_inode));
  if(vfs->table[inode])
    vfs->table[inode]->flags = 0;

  return vfs->table[inode];
}

/**
 * Get a pointer to an inode data structure from its index.
 * This will return a pointer even for cleared inodes.
 */
static struct vfs_inode *vfs_get_inode_ptr(vfilesystem *vfs, uint32_t inode)
{
  assert(inode < vfs->table_length);
  return vfs->table[inode];
}

/**
 * Get the next unused inode in the VFS. table_next will be advanced to the
 * position of the returned inode. This will allocate more inode space in the
 * VFS if needed.
 */
static uint32_t vfs_get_next_free_inode(vfilesystem *vfs)
{
  while(vfs->table_next < vfs->table_length)
  {
    if(!vfs->table[vfs->table_next])
    {
      if(!vfs_allocate_inode(vfs, vfs->table_next))
        return vfs_seterror(vfs, VFS_ENOSPC);

      return vfs->table_next;
    }

    if(!vfs->table[vfs->table_next]->flags)
      return vfs->table_next;

    vfs->table_next++;
  }

  if(vfs->table_length >= vfs->table_alloc)
  {
    struct vfs_inode **ptr;
    if(!vfs->table_alloc)
      vfs->table_alloc = 4;

    vfs->table_alloc <<= 1;
    ptr = (struct vfs_inode **)realloc(vfs->table,
     vfs->table_alloc * sizeof(struct vfs_inode *));
    if(!ptr)
      return vfs_seterror(vfs, VFS_ENOSPC);

    vfs->table = ptr;
  }

  if(!vfs_allocate_inode(vfs, vfs->table_length))
    return vfs_seterror(vfs, VFS_ENOSPC);

  return (vfs->table_length++);
}

/* Special pointers for platform-agnostic resolution of parent/current dir
 * tokens. The content of these doesn't actually matter, as long as they
 * are different. */
static const char vfs_special_current_dir[] = "<c>";
static const char vfs_special_parent_dir[] = "<p>";

/**
 * Resolve parent/current dir tokens on a per-platform basis into generic
 * pointers that can be handled in a platform-agnostic manner.
 */
static const char *vfs_get_special_inode_name(const char *name,
 const char *has_next_token)
{
#ifndef VIRTUAL_FILESYSTEM_AMIGA_DRIVE
  (void)has_next_token;

  if(name[0] == '.')
  {
    if(name[1] == '\0')
      return vfs_special_current_dir;

    if(name[1] == '.' && name[2] == '\0')
      return vfs_special_parent_dir;
  }
#else
  if(name[0] == '\0')
    return has_next_token ? vfs_special_parent_dir : vfs_special_current_dir;
#endif
  return name;
}

/**
 * Returns the inode of a given name within parent. This name should not include
 * path separators. The value of `index` will be set to the index of the inode
 * if it exists, otherwise the index where that inode should be placed.
 */
static uint32_t vfs_get_inode_in_parent_by_name(vfilesystem *vfs,
 struct vfs_inode *parent, const char *name, uint32_t *index)
{
  uint32_t a = 2;
  uint32_t b = parent->length - 1;
  uint32_t current;
  uint32_t inode;
  int cmp;

  assert(parent->length >= 2);

  if(!name[0])
    return VFS_NO_INODE;

  if(name == vfs_special_current_dir)
  {
    if(index)
      *index = VFS_IDX_SELF;
    return parent->contents.inodes[VFS_IDX_SELF];
  }

  if(name == vfs_special_parent_dir)
  {
    if(index)
      *index = VFS_IDX_PARENT;
    return parent->contents.inodes[VFS_IDX_PARENT];
  }

  while(a <= b)
  {
    current = (b - a) / 2 + a;
    inode = parent->contents.inodes[current];

    cmp = vfs_name_cmp(name, vfs_inode_name(vfs_get_inode_ptr(vfs, inode)));
    if(cmp > 0)
    {
      a = current + 1;
    }
    else

    if(cmp < 0)
    {
      b = current - 1;
    }
    else
    {
      if(index)
        *index = current;
      return inode;
    }
  }

  if(index)
    *index = a;
  return VFS_NO_INODE;
}

/**
 * Get the first inode for a given path string.
 * This is either the "working directory" inode or a root inode.
 * The string pointed to by `path` will be advanced past the base token if it
 * exists.
 */
static uint32_t vfs_get_path_base_inode(vfilesystem *vfs, const char **path)
{
  ssize_t len = path_is_absolute(*path);
  uint32_t inode;
  char buffer[32];

  if(!*path[0])
#ifdef VIRTUAL_FILESYSTEM_AMIGA_DRIVE
    return vfs->current;
#else
    return vfs_seterror(vfs, VFS_ENOENT);
#endif

  if(len >= (ssize_t)sizeof(buffer))
    return vfs_seterror(vfs, VFS_ENOENT);

  if(len > 0)
  {
    struct vfs_inode *roots = vfs_get_inode_ptr(vfs, 0);
#ifdef PATH_UNC_ROOTS
    // These can be passed from pretty much anywhere and need to be
    // stripped out here.
    if(len >= 4 && (!memcmp(*path, "\\\\.\\", 4) || !memcmp(*path, "\\\\?\\", 4)))
    {
      *path += 4;
      len -= 4;
    }
#endif
    memcpy(buffer, *path, len);
    buffer[len] = '\0';
    *path += len;

    path_clean_root(buffer, sizeof(buffer));

    inode = vfs_get_inode_in_parent_by_name(vfs, roots, buffer, NULL);
    if(inode == VFS_NO_INODE)
    {
#ifdef VIRTUAL_FILESYSTEM_DOS_DRIVE
      // Windows and DJGPP: / behaves as the root of the current active drive.
      if(!strcmp(buffer, DIR_SEPARATOR))
        return vfs->current_root;
#endif
#ifdef VIRTUAL_FILESYSTEM_AMIGA_DRIVE
      // Amiga: ':' behaves as the root of the current active drive.
      if(!strcmp(buffer, ":"))
        return vfs->current_root;
#endif

      return vfs_seterror(vfs, VFS_ENOENT);
    }

    return inode;
  }
  return vfs->current;
}

/**
 * Find an inode in the VFS located at the given relative path within a known
 * base inode. `relative_path` should be cleaned for slashes prior to calling
 * this function and should be writeable.
 */
static uint32_t vfs_get_inode_by_relative_path(vfilesystem *vfs, uint32_t inode,
 char *relative_path)
{
  struct vfs_inode *parent;
  const char *current;
  char *next;

  next = relative_path;
  while((current = path_tokenize(&next)))
  {
    current = vfs_get_special_inode_name(current, next);
    if(!current[0])
      continue;

    parent = vfs_get_inode_ptr(vfs, inode);
    if(VFS_INODE_TYPE(parent) != VFS_INODE_DIR)
      return vfs_seterror(vfs, VFS_ENOTDIR);

    inode = vfs_get_inode_in_parent_by_name(vfs, parent, current, NULL);
    if(!inode)
      return vfs_seterror(vfs, VFS_ENOENT);
  }

  return inode;
}

/**
 * Find an inode in the VFS located at the given path.
 */
static uint32_t vfs_get_inode_by_path(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  uint32_t inode;

  inode = vfs_get_path_base_inode(vfs, &path);
  if(!inode)
    return VFS_NO_INODE;

  path_clean_copy(buffer, sizeof(buffer), path);

  return vfs_get_inode_by_relative_path(vfs, inode, buffer);
}

/**
 * Find an inode and its parent inode in the VFS from a given path, if either
 * exists. The name of the child inode will be copied to the provided buffer.
 */
static boolean vfs_get_inode_and_parent_by_path(vfilesystem *vfs, const char *path,
 uint32_t *_parent, uint32_t *_inode, char *name, size_t name_length)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  char *child;
  uint32_t inode = VFS_NO_INODE;
  uint32_t parent = VFS_NO_INODE;
  uint32_t base;

  base = vfs_get_path_base_inode(vfs, &path);
  if(!base)
    return false;

  path_clean_copy(buffer, sizeof(buffer), path);

  // If the buffer is empty, the entire path is a root.
  // The target inode is the root and its parent is itself in this situation.
  if(!buffer[0])
  {
    child = buffer;
    parent = base;
    inode = base;
  }
  else
  {
    child = strrchr(buffer, DIR_SEPARATOR_CHAR);
    if(child)
    {
      /* Preserve the slash in case it is important for an Amiga path. */
      char tmp = *(++child);
      *child = '\0';
      parent = vfs_get_inode_by_relative_path(vfs, base, buffer);
      *child = tmp;
    }
    else
    {
      child = buffer;
      parent = base;
    }

    if(parent)
    {
      const char *tmp = vfs_get_special_inode_name(child, NULL);

      p = vfs_get_inode_ptr(vfs, parent);
      if(VFS_INODE_TYPE(p) != VFS_INODE_DIR)
      {
        vfs_seterror(vfs, VFS_ENOTDIR);
        return false;
      }
      inode = vfs_get_inode_in_parent_by_name(vfs, p, tmp, NULL);
    }
  }

  if(_parent)
    *_parent = parent;
  if(_inode)
    *_inode = inode;
  if(name)
    snprintf(name, name_length, "%s", child);

  return true;
}

/**
 * Get the path for a given directory inode.
 */
static boolean vfs_get_inode_path(vfilesystem *vfs, uint32_t inode,
 char *buffer, size_t buffer_length)
{
  struct vfs_inode *n;
  uint32_t current;
  uint32_t next;
  size_t needed = 0;
  size_t segments = 0;
  boolean root_separator = false;

  // 1. Get the required size for this string.
  current = inode;
  while(true)
  {
    n = vfs_get_inode_ptr(vfs, current);
    if(!n || !n->contents.inodes || VFS_INODE_TYPE(n) != VFS_INODE_DIR)
      return false;

    next = n->contents.inodes[VFS_IDX_PARENT];

    needed += n->name_length;

    if(next == current)
    {
      if(strcmp(vfs_inode_name(n), DIR_SEPARATOR))
      {
        needed += strlen(PATH_ROOT_SEPARATOR);
        root_separator = true;
      }
      break;
    }
    else
      segments++;

    current = next;
  }

  /* Add separating slashes, not including the root separator. */
  if(segments > 1)
    needed += segments - 1;

  if(needed + 1 >= buffer_length)
    return false;

  // 2. Copy name into the buffer.
  buffer[needed] = '\0';
  current = inode;
  while(true)
  {
    n = vfs_get_inode_ptr(vfs, current);

    next = n->contents.inodes[VFS_IDX_PARENT];
    if(next == current)
      break;

    needed -= n->name_length;
    memcpy(buffer + needed, vfs_inode_name(n), n->name_length);
    buffer[--needed] = DIR_SEPARATOR_CHAR;
    current = next;
  }
  // Add root name into buffer separately.
  memcpy(buffer, vfs_inode_name(n), n->name_length);
  if(root_separator)
    memcpy(buffer + n->name_length, PATH_ROOT_SEPARATOR, strlen(PATH_ROOT_SEPARATOR));
  return true;
}

/**
 * Generate an inode in the VFS with a given name. This name should not include
 * path separators.
 */
static uint32_t vfs_make_inode(vfilesystem *vfs, uint32_t parent,
 const char *name, size_t init_alloc, int flags)
{
  struct vfs_inode *p;
  struct vfs_inode *n;
  uint32_t pos;
  uint32_t pos_in_parent;
  boolean is_real;

  p = vfs_get_inode_ptr(vfs, parent);

  assert(VFS_INODE_TYPE(p) == VFS_INODE_DIR);
  assert(flags & VFS_INODE_TYPEMASK);

  if(!name[0])
    return vfs_seterror(vfs, VFS_ENOENT);

  pos = vfs_get_inode_in_parent_by_name(vfs, p, name, &pos_in_parent);
  if(pos != VFS_NO_INODE)
    return vfs_seterror(vfs, VFS_EEXIST);

  pos = vfs_get_next_free_inode(vfs);
  if(!pos)
    return VFS_NO_INODE;

  if(!vfs_inode_expand_directory(p, 1))
    return vfs_seterror(vfs, VFS_ENOSPC);

  n = vfs_get_inode_ptr(vfs, pos);
  is_real = !!(flags & VFS_INODE_IS_REAL);

  if((flags & VFS_INODE_TYPEMASK) == VFS_INODE_DIR)
  {
    if(!vfs_inode_init_directory(n, name, init_alloc, is_real))
      return vfs_seterror(vfs, VFS_ENOSPC);

    // Init self and parent inodes.
    n->contents.inodes[VFS_IDX_SELF] = pos;
    n->contents.inodes[VFS_IDX_PARENT] = p->contents.inodes[VFS_IDX_SELF];
  }
  else
  {
    if(!vfs_inode_init_file(n, name, init_alloc, is_real))
      return vfs_seterror(vfs, VFS_ENOSPC);

    // Update tracking for the total size of cached files.
    if(is_real)
      vfs->cache_total += n->length_alloc;
  }
  // Replace the placeholder timestamp with a real one...
  if(is_real && !vfs->disable_timestamp)
    n->timestamp = vfs_get_timestamp();

  vfs_inode_insert_directory(p, pos, pos_in_parent);
  n->parent = parent;
  return pos;
}

/**
 * Delete an inode in the VFS. It will be removed from its parent directory.
 * The inode will be cleared and marked for reuse immediately if there are no
 * open file references. Otherwise, it will be cleared and marked for reuse
 * when its reference count reaches 0.
 */
static boolean vfs_delete_inode(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *p;
  struct vfs_inode *n;
  uint32_t pos_in_parent;
  uint32_t pos;

  if(inode == VFS_NO_INODE)
    return vfs_seterror(vfs, VFS_ENOENT);

  n = vfs_get_inode_ptr(vfs, inode);

  if(n->parent != VFS_NO_INODE)
  {
    if(n->parent == inode)
      return vfs_seterror(vfs, VFS_EBUSY);

    p = vfs_get_inode_ptr(vfs, n->parent);
    assert(VFS_INODE_TYPE(p) == VFS_INODE_DIR);

    // Find in parent.
    pos = vfs_get_inode_in_parent_by_name(vfs, p, vfs_inode_name(n),
     &pos_in_parent);
    if(pos != inode)
      return vfs_seterror(vfs, VFS_ENOENT);

    // Remove from parent.
    vfs_inode_delete_directory(p, pos_in_parent);
    n->parent = VFS_NO_INODE;
  }

  // Clear the inode and mark it for reuse if it's not currently in-use.
  if(n->refcount == 0)
  {
    // Update tracking for the total size of cached files.
    if(VFS_INODE_TYPE(n) == VFS_INODE_FILE && VFS_IS_CACHED(n))
    {
      assert(vfs->cache_total >= n->length_alloc);
      vfs->cache_total -= n->length_alloc;
    }

    vfs_inode_clear(n);
    if(inode < vfs->table_next)
      vfs->table_next = inode;
  }
  return true;
}

/**
 * Move an inode in the VFS from one directory to another. The old and new
 * parent directories may be the same directory.
 */
static boolean vfs_move_inode(vfilesystem *vfs, uint32_t old_parent,
 uint32_t new_parent, uint32_t inode, const char *old_name, const char *new_name)
{
  struct vfs_inode *old_p;
  struct vfs_inode *new_p;
  struct vfs_inode *n;
  uint32_t old_pos_in_parent;
  uint32_t new_pos_in_parent = 0;
  uint32_t pos;

  if(inode == VFS_NO_INODE)
    return vfs_seterror(vfs, VFS_ENOENT);

  n = vfs_get_inode_ptr(vfs, inode);
  old_p = vfs_get_inode_ptr(vfs, old_parent);
  new_p = vfs_get_inode_ptr(vfs, new_parent);
  assert(VFS_INODE_TYPE(old_p) == VFS_INODE_DIR);
  assert(VFS_INODE_TYPE(new_p) == VFS_INODE_DIR);

  if(old_p != new_p && !vfs_inode_expand_directory(new_p, 1))
    return vfs_seterror(vfs, VFS_ENOSPC);

  // Find in parents.
  pos = vfs_get_inode_in_parent_by_name(vfs, old_p, old_name, &old_pos_in_parent);
  if(pos != inode)
    return vfs_seterror(vfs, VFS_ENOENT);

  pos = vfs_get_inode_in_parent_by_name(vfs, new_p, new_name, &new_pos_in_parent);
  if(pos != VFS_NO_INODE)
    return vfs_seterror(vfs, VFS_EEXIST);

  // Rename.
  if(!vfs_inode_rename(n, new_name))
    return vfs_seterror(vfs, VFS_ENOSPC);

  // Move from old position to new position.
  if(old_p != new_p)
  {
    vfs_inode_delete_directory(old_p, old_pos_in_parent);
    vfs_inode_insert_directory(new_p, inode, new_pos_in_parent);
  }
  else
  {
    // The position determined above assumes this will keep its old position,
    // so adjust it to account for its own removal.
    if(new_pos_in_parent > old_pos_in_parent)
      new_pos_in_parent--;

    vfs_inode_move_directory(old_p, old_pos_in_parent, new_pos_in_parent);
  }

  n->parent = new_parent;
  return true;
}

/**
 * Determine if inode `A` is the same as or is an ancestor of inode `B`.
 * Both inodes `A` and `B` MUST be directories currently.
 */
static boolean vfs_is_ancestor_inode(vfilesystem *vfs, uint32_t A, uint32_t B)
{
  struct vfs_inode *a = vfs_get_inode_ptr(vfs, A);
  struct vfs_inode *b = vfs_get_inode_ptr(vfs, B);
  if(!a || !b)
    return false;

  assert(VFS_INODE_TYPE(a) == VFS_INODE_DIR);
  assert(VFS_INODE_TYPE(b) == VFS_INODE_DIR);

  // Walk B back to its root.
  while(true)
  {
    if(a == b)
      return true;

    if(!b->contents.inodes || b->contents.inodes[VFS_IDX_PARENT] == B)
      return false;

    B = b->contents.inodes[VFS_IDX_PARENT];
    b = vfs_get_inode_ptr(vfs, B);
    assert(b && VFS_INODE_TYPE(b) == VFS_INODE_DIR);
  }
}

/**
 * Invalidate a cached inode and all children, recursively.
 * If all of a directory's children are invalidated, it will be
 * marked invalid as well. Invalidated inodes are unlinked from their
 * parent directory and are destroyed if not currently in use.
 * If currently in use, they will remain allocated until closed.
 *
 * Returns true if all children are invalidated, otherwise false.
 */
static boolean vfs_invalidate_inode(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
  boolean is_empty_or_file = true;
  size_t i;
  if(!n)
    return true;

  vfs_seterror(vfs, 0);
  if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
  {
    for(i = 2; i < n->length; i++)
    {
      uint32_t child = n->contents.inodes[i];
      struct vfs_inode *c = vfs_get_inode_ptr(vfs, child);
      if(!c)
        continue;

      if(VFS_INODE_TYPE(c) == VFS_INODE_DIR)
      {
        if(vfs_invalidate_inode(vfs, child))
        {
          i--;
        }
        else
          is_empty_or_file = false;
      }
      else
      {
        if(VFS_IS_CACHED(c))
        {
          vfs_delete_inode(vfs, child);
          i--;
        }
        else
          is_empty_or_file = false;
      }
    }
  }

  if(is_empty_or_file && VFS_IS_CACHED(n) && inode != vfs->current)
  {
    vfs_delete_inode(vfs, inode);
    return true;
  }
  return false;
}

#if 0
static void _vfs_print_str(vfilesystem *vfs, const char *str, int level)
{
  fprintf(mzxerr, "%*.*s%s\n", level*2, level*2, "", str);
  fflush(mzxerr);
}

static void _vfs_print_len(vfilesystem *vfs, size_t len, int level)
{
  fprintf(mzxerr, "%*.*slen: %zu\n", level*2, level*2, "", len);
  fflush(mzxerr);
}

static void _vfs_print_name(vfilesystem *vfs, struct vfs_inode *n, int level)
{
  _vfs_print_str(vfs, vfs_inode_name(n), level);
}

static void _vfs_print_dir(vfilesystem *vfs, struct vfs_inode *dir, int level)
{
  size_t i;
  assert(VFS_INODE_TYPE(dir) == VFS_INODE_DIR);

  level++;
  if(level > 0)
    _vfs_print_len(vfs, dir->length, level);

  for(i = 0; i < dir->length; i++)
  {
    uint32_t inode = dir->contents.inodes[i];
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);

    if(i == VFS_IDX_SELF)
    {
      assert(n == dir);
      if(level > 0)
        _vfs_print_str(vfs, ".", level);
    }
    else

    if(i == VFS_IDX_PARENT)
    {
      if(level > 0)
        _vfs_print_str(vfs, "..", level);
    }
    else

    if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
    {
      _vfs_print_name(vfs, n, level);
      _vfs_print_str(vfs, "{", level);
      _vfs_print_dir(vfs, n, level);
      _vfs_print_str(vfs, "}", level);
    }
    else
      _vfs_print_name(vfs, n, level);
  }
}

static inline void vfs_print(vfilesystem *vfs)
{
  struct vfs_inode *roots = vfs_get_inode_ptr(vfs, 0);
  _vfs_print_dir(vfs, roots, -1);
}
#endif /* DEBUG */


/**
 * External functions.
 */

/**
 * Create a root with the given name if it does not exist.
 *
 * @param vfs     VFS handle.
 * @param name    name of the root to create. It must contain alphanumeric
 *                characters only.
 * @return        0 on success, `VFS_EEXIST` if the root already exists,
 *                `VFS_EINVAL` if the name is invalid, or other `vfs_error` codes.
 *                This function does not set `errno`.
 */
enum vfs_error vfs_make_root(vfilesystem *vfs, const char *name)
{
  struct vfs_inode *n;
  uint32_t inode;
  size_t sz;
  size_t i;

  if(!name)
    return VFS_EINVAL;
  if(name[0] == '/' && name[1] == '\0')
    return VFS_EEXIST;

  sz = strlen(name);
  if(!sz || sz + 2 >= MAX_PATH)
    return VFS_EINVAL;
  for(i = 0; i < sz; i++)
    if(!isalnum((unsigned char)name[i]))
      return VFS_EINVAL;

  if(!vfs_write_lock(vfs))
    return vfs_geterror(vfs);

  // This will check for existence and insert the new root into the roots list.
  inode = vfs_make_inode(vfs, VFS_NO_INODE, name, 0, VFS_INODE_DIR);
  if(!inode)
  {
    enum vfs_error code = vfs_geterror(vfs);
    vfs_write_unlock(vfs);
    return code;
  }

  n = vfs_get_inode_ptr(vfs, inode);
  n->create_time = vfs_get_date();
  n->modify_time = n->create_time;
  // The new root is its own parent.
  n->parent = inode;
  n->contents.inodes[VFS_IDX_PARENT] = inode;

  vfs_write_unlock(vfs);
  return 0;
}

/**
 * Create a file in the VFS at a given path if it doesn't already exist.
 * If the file does exist, this function will return `VFS_EEXIST`, similar
 * to an `open` call with O_CREAT|O_EXCL set.
 *
 * @param vfs   VFS handle.
 * @param path  path of file within `vfs` to create.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_create_file_at_path(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  boolean need_unlink = false;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    // open() sets a different error depending on the type of the existing file.
    p = vfs_get_inode_ptr(vfs, inode);
    if(p && VFS_INODE_TYPE(p) == VFS_INODE_FILE)
    {
      if(!VFS_IS_CACHED(p))
      {
        vfs_seterror(vfs, VFS_EEXIST);
        goto err;
      }
      // Special: creating a virtual file can mask a real filesystem file.
      need_unlink = true;
    }
    else
    {
      vfs_seterror(vfs, VFS_EISDIR);
      goto err;
    }
  }
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;

  // If the parent is cached and times out, ignore this call.
  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // If there's a cached file that needs to be removed, do that first.
  if(need_unlink)
  {
    if(!vfs_delete_inode(vfs, inode))
      goto err;
  }

  // Create a virtual (i.e. not real) file.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_FILE);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = n->create_time;
    p->modify_time = n->create_time;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * "Open" a file in a VFS, obtaining a file descriptor for the file and
 * incrementing its reference count by one. This is meaningless for directories,
 * and calls where the path is a directory will return `VFS_EISDIR`
 * (use `vfs_readdir` instead).
 *
 * @param vfs       VFS handle.
 * @param path      path of file within `vfs` to "open".
 * @param is_write  should be `true` if write operations are to be performed.
 * @param inode     a pointer for the file descriptor to be written to.
 * @return          0 on success, otherwise a relevant `vfs_error` code.
 *                  This function does not set `errno`.
 *                  If the file is cached, this function will behave as
 *                  normal but instead return `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_open_if_exists(vfilesystem *vfs,
 const char *path, boolean is_write, uint32_t *_inode)
{
  struct vfs_inode *n;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || n->refcount >= VFS_MAX_REFCOUNT)
    goto err;

  // Opening directories is nonsensical.
  if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
  {
    vfs_seterror(vfs, VFS_EISDIR);
    goto err;
  }

  n->refcount++;
  if(VFS_IS_CACHED(n))
    code = VFS_ERR_IS_CACHED;

  vfs_read_unlock(vfs);
  *_inode = inode;
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * "Close" a file in a VFS, decrementing its reference count by one.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to "close".
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_close(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  if(inode >= vfs->table_length)
    return VFS_EBADF;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  n = vfs_get_inode_ptr(vfs, inode);
  assert(n->refcount > 0);
  n->refcount--;

  n->modify_time = vfs_get_date();
  if(VFS_IS_CACHED(n))
  {
    if(n->refcount == 0 && VFS_IS_INVALIDATED(n))
    {
      if(vfs_elevate_lock(vfs))
      {
        vfs_delete_inode(vfs, inode);
        vfs_write_unlock(vfs);
        return 0;
      }
    }
    if(!vfs->disable_timestamp)
      n->timestamp = vfs_get_timestamp();
  }

  vfs_read_unlock(vfs);
  return 0;
}

/**
 * Truncate an "open" VFS file to length 0.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to truncate.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_truncate(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  if(inode >= vfs->table_length)
    return VFS_EBADF;

  if(!vfs_write_lock(vfs))
    return vfs_geterror(vfs);

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || !n->refcount)
  {
    vfs_write_unlock(vfs);
    return VFS_EBADF;
  }

  if(n->length_alloc > VFS_DEFAULT_FILE_SIZE)
  {
    unsigned char *tmp = (unsigned char *)realloc(n->contents.data, VFS_DEFAULT_FILE_SIZE);
    if(tmp)
    {
      // Update cache size tracking.
      if(VFS_IS_CACHED(n))
      {
        assert(vfs->cache_total >= n->length_alloc);
        vfs->cache_total -= n->length_alloc - VFS_DEFAULT_FILE_SIZE;
      }
      n->contents.data = tmp;
      n->length_alloc = VFS_DEFAULT_FILE_SIZE;
    }

  }
  n->length = 0;

  vfs_write_unlock(vfs);
  return 0;
}

/**
 * Get the length of an open VFS inode.
 * This function should NOT be called while a file is locked for read/write.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to write.
 * @return      the length of the file, or a negative number corresponding to a
 *              relevant `vfs_error` code. This function does not set `errno`.
 */
ssize_t vfs_filelength(vfilesystem *vfs, uint32_t inode)
{
  struct vfs_inode *n;
  ssize_t ret;

  if(!vfs_read_lock(vfs))
    return -(ssize_t)vfs_geterror(vfs);

  if(inode >= vfs->table_length)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n || !n->refcount)
    goto err;

  ret = n->length;
  if(ret < 0)
    goto err;

  vfs_read_unlock(vfs);
  return ret;

err:
  vfs_read_unlock(vfs);
  return -(ssize_t)VFS_EBADF;
}

/**
 * Lock an "open" VFS file for reading and retrieve its underlying data buffer.
 * Multiple reading locks can be acquired on a VFS handle simultaneously, but
 * a reading lock will block if a writing lock is held. To avoid deadlocks,
 * only one lock should ever be acquired per thread simultaneously.
 *
 * @param vfs         VFS handle.
 * @param inode       file descriptor to the file to read.
 * @param data        pointer to store a pointer to the underlying data to.
 * @param data_length pointer to store the length of the underlying data to.
 * @return            0 on success, otherwise a relevant `vfs_error` code.
 *                    This function does not set `errno`.
 */
enum vfs_error vfs_lock_file_read(vfilesystem *vfs, uint32_t inode,
 const unsigned char **data, size_t *data_length)
{
  struct vfs_inode *n;
  if(inode < vfs->table_length)
  {
    if(!vfs_read_lock(vfs))
      return vfs_geterror(vfs);

    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
    {
      vfs_read_unlock(vfs);
      return VFS_EBADF;
    }

    *data = n->contents.data;
    *data_length = n->length;
    return 0;
  }
  return VFS_EBADF;
}

/**
 * Release a held reading lock on a VFS handle.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to read.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_unlock_file_read(vfilesystem *vfs, uint32_t inode)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return VFS_EBADF;

    if(vfs_read_unlock(vfs))
      return 0;
  }
  return VFS_EBADF;
}

/**
 * Lock an "open" VFS file for writing and retrieve its underlying data buffer.
 * Only one writing lock can be acquired on a VFS handle at any given time.
 * To avoid deadlocks, only one lock should ever be acquired per thread
 * simultaneously.
 *
 * @param vfs         VFS handle.
 * @param inode       file descriptor to the file to write.
 * @param data        pointer to store a pointer to the pointer to the underlying data to.
 * @param data_length pointer to store a pointer to the the length of the underlying data to.
 * @param data_alloc  pointer to store a pointer to the the allocation size of the underlying data to.
 * @return            0 on success, otherwise a relevant `vfs_error` code.
 *                    This function does not set `errno`.
 */
enum vfs_error vfs_lock_file_write(vfilesystem *vfs, uint32_t inode,
 unsigned char ***data, size_t **data_length, size_t **data_alloc)
{
  struct vfs_inode *n;
  if(inode < vfs->table_length)
  {
    if(!vfs_write_lock(vfs))
      return vfs_geterror(vfs);

    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
    {
      vfs_write_unlock(vfs);
      return VFS_EBADF;
    }

    // Since the buffer may be reallocated, remove the old alloc size.
    if(VFS_IS_CACHED(n))
    {
      assert(vfs->cache_total >= n->length_alloc);
      vfs->cache_total -= n->length_alloc;
    }

    *data = &(n->contents.data);
    *data_length = &(n->length);
    *data_alloc = &(n->length_alloc);
    return 0;
  }
  return VFS_EBADF;
}

/**
 * Release a held writing lock on a VFS handle.
 *
 * @param vfs   VFS handle.
 * @param inode file descriptor to the file to write.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_unlock_file_write(vfilesystem *vfs, uint32_t inode)
{
  if(inode < vfs->table_length)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    if(!n || !n->refcount)
      return VFS_EBADF;

    // The size of this file may have changed, add the new size back.
    if(VFS_IS_CACHED(n))
      vfs->cache_total += n->length_alloc;

    if(vfs_write_unlock(vfs))
      return 0;
  }
  return VFS_EBADF;
}

/**
 * Set the current working directory of a VFS. This operation currently can
 * only be performed by the thread that created this VFS.
 *
 * @param vfs   VFS handle.
 * @param path  path of directory to set as the current working directory.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 *              If the directory is cached, this function will behave as
 *              normal but instead return `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_chdir(vfilesystem *vfs, const char *path)
{
  struct vfs_inode *n;
  uint32_t inode;
  uint32_t root;
  char buffer[MAX_PATH * 2];
  size_t len;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

#ifdef VIRTUAL_FILESYSTEM_PARALLEL
  // TODO: for now, only allow chdir from the thread that created this VFS.
  if(!platform_is_same_thread(vfs->origin, platform_get_thread_id()))
  {
    vfs_seterror(vfs, VFS_EACCES);
    goto err;
  }
#endif

  // Navigate the data structure first to get better errors than path_navigate.
  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  if(VFS_IS_CACHED(n))
    code = VFS_ERR_IS_CACHED;

  // If this is the same as the current inode, exit early.
  if(inode == vfs->current)
  {
    vfs_read_unlock(vfs);
    return code;
  }

  if(VFS_INODE_TYPE(n) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, VFS_ENOTDIR);
    goto err;
  }

  // Get the new working directory path string.
  if(!vfs_get_inode_path(vfs, inode, buffer, sizeof(buffer)))
  {
    vfs_seterror(vfs, VFS_ENAMETOOLONG);
    goto err;
  }
  len = strlen(buffer);
  if(len >= sizeof(vfs->current_path))
  {
    vfs_seterror(vfs, VFS_ENAMETOOLONG);
    goto err;
  }

  // Find the root this directory exists in.
  root = inode;
  while(true)
  {
    uint32_t next;
    if(!root || !n || !n->contents.inodes)
      goto err;

    next = n->contents.inodes[VFS_IDX_PARENT];
    if(next == root)
      break;

    root = next;
    n = vfs_get_inode_ptr(vfs, root);
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  memcpy(vfs->current_path, buffer, len + 1);
  vfs->current_path_len = len;
  vfs->current = inode;
  vfs->current_root = root;
  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Get the current working directory of a VFS as a path string.
 *
 * @param vfs       VFS handle.
 * @param dest      buffer to store the current working directory path to.
 * @param dest_len  size of `dest` in bytes.
 * @return          0 on success, otherwise a relevant `vfs_error` code.
 *                  This function does not set `errno`.
 *                  If the directory is cached, this function will behave as
 *                  normal but instead return `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_getcwd(vfilesystem *vfs, char *dest, size_t dest_len)
{
  struct vfs_inode *n;
  enum vfs_error code = 0;

  if(!dest || !dest_len)
    return VFS_EINVAL;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(vfs->current_path_len >= dest_len)
  {
    vfs_read_unlock(vfs);
    return VFS_ERANGE;
  }
  // This should already have been cleaned by vfs_chdir, so copy directly.
  memcpy(dest, vfs->current_path, vfs->current_path_len + 1);

  n = vfs_get_inode_ptr(vfs, vfs->current);
  if(n && VFS_IS_CACHED(n))
    code = VFS_ERR_IS_CACHED;

  vfs_read_unlock(vfs);
  return code;
}

/**
 * Create a virtual directory (i.e. not a cached copy of a real directory).
 * If this virtual directory is being created on a real path, that path must
 * be cached via `vfs_cache_directory` prior to calling this function.
 *
 * @param vfs   VFS handle.
 * @param path  path to create a directory at within `vfs`.
 * @param mode  permission bits for the created directory (ignored).
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_mkdir(vfilesystem *vfs, const char *path, int mode)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    vfs_seterror(vfs, VFS_EEXIST);
    goto err;
  }
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;

  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // Create a virtual (i.e. not real) directory.
  inode = vfs_make_inode(vfs, parent, buffer, 0, VFS_INODE_DIR);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = n->create_time;
    p->modify_time = n->create_time;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Rename a virtual or cached file or directory. If a file is moved, the
 * destination either must not exist or must be a file. If a directory is moved,
 * the destination either must not exist or must be an empty directory.
 *
 * @param vfs     VFS handle.
 * @param oldpath path of a file or directory within `vfs` to rename.
 * @param newpath new path to rename the file or directory to.
 * @return        0 on success, otherwise a relevant `vfs_error` code.
 *                This function does not set `errno`.
 *                Returns `VFS_ERR_IS_CACHED` if a cached file was renamed.
 */
enum vfs_error vfs_rename(vfilesystem *vfs, const char *oldpath, const char *newpath)
{
  char buffer[MAX_PATH];
  char buffer2[MAX_PATH];
  struct vfs_inode *old_p;
  struct vfs_inode *old_n;
  struct vfs_inode *new_p;
  struct vfs_inode *new_n;
  uint32_t old_parent;
  uint32_t new_parent;
  uint32_t old_inode;
  uint32_t new_inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, oldpath, &old_parent, &old_inode, buffer, sizeof(buffer)))
    goto err;

  // old must have both a parent and an inode and they should be different.
  if(!old_parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;
  if(!old_inode)
  {
    vfs_seterror(vfs, VFS_ENOENT);
    goto err;
  }
  if(old_parent == old_inode)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }
  old_p = vfs_get_inode_ptr(vfs, old_parent);
  old_n = vfs_get_inode_ptr(vfs, old_inode);
  if(VFS_INODE_TYPE(old_p) != VFS_INODE_DIR)
    goto err;

  // If the file being moved is cached, let the caller know.
  if(VFS_IS_CACHED(old_n))
    code = VFS_ERR_IS_CACHED;

  if(!vfs_get_inode_and_parent_by_path(vfs, newpath, &new_parent, &new_inode, buffer2, sizeof(buffer2)))
    goto err;

  // new must have a parent, and its inode should be different.
  if(!new_parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;
  if(new_parent == new_inode)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }
  new_p = vfs_get_inode_ptr(vfs, new_parent);
  new_n = new_inode ? vfs_get_inode_ptr(vfs, new_inode) : NULL;
  if(VFS_INODE_TYPE(new_p) != VFS_INODE_DIR)
    goto err;

  // Special case: if old and new are the same, return early with success.
  if(old_n == new_n)
  {
    vfs_read_unlock(vfs);
    return code;
  }

  if(VFS_INODE_TYPE(old_n) == VFS_INODE_DIR)
  {
    // If old is a dir, the new parent should not be prefixed by old.
    if(vfs_is_ancestor_inode(vfs, old_inode, new_parent))
    {
      vfs_seterror(vfs, VFS_EINVAL);
      goto err;
    }

    // If old is a dir, new must be an empty dir if it exists.
    if(new_n)
    {
      if(VFS_INODE_TYPE(new_n) != VFS_INODE_DIR)
      {
        vfs_seterror(vfs, VFS_ENOTDIR);
        goto err;
      }
      if(new_n->length > 2)
      {
        vfs_seterror(vfs, VFS_ENOTEMPTY);
        goto err;
      }
    }

    // Also, neither of them can be the current directory or an ancestor.
    if(vfs_is_ancestor_inode(vfs, old_inode, vfs->current) ||
       vfs_is_ancestor_inode(vfs, new_inode, vfs->current))
    {
      vfs_seterror(vfs, VFS_EBUSY);
      goto err;
    }
  }
  else
  {
    if(new_n)
    {
      // If old is a file, new must be a file if it exists.
      if(VFS_INODE_TYPE(new_n) == VFS_INODE_DIR)
      {
        vfs_seterror(vfs, VFS_EISDIR);
        goto err;
      }
      // New must not have any references if it exists.
      if(new_n->refcount)
      {
        // Might be incorrect, POSIX only mentions dirs.
        vfs_seterror(vfs, VFS_EBUSY);
        goto err;
      }
    }
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(new_inode && !vfs_delete_inode(vfs, new_inode))
    goto err_write;

  if(!vfs_move_inode(vfs, old_parent, new_parent, old_inode, buffer, buffer2))
    goto err_write;

  old_p->modify_time = vfs_get_date();
  new_p->modify_time = old_p->modify_time;
  vfs_write_unlock(vfs);
  return code;

err_write:
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Remove a virtual file (i.e. not a cached copy of a real file).
 * If this is a cached real file, it should be unlinked separately and
 * invalidated with `vfs_invalidate_at_path`. This function does not work on
 * directories (use `vfs_rmdir` instead).
 *
 * @param vfs   VFS handle.
 * @param path  path of file within `vfs` to remove.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 *              If the file/directory exists and is cached, `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_unlink(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  struct vfs_inode *n;
  uint32_t parent;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Both must exist and be different.
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;
  if(!inode)
  {
    vfs_seterror(vfs, VFS_ENOENT);
    goto err;
  }
  if(parent == inode)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  // This function is not applicable to cached files.
  if(VFS_IS_CACHED(n))
  {
    vfs_seterror(vfs, VFS_ERR_IS_CACHED);
    goto err;
  }

  // If this is a directory, this call should be ignored.
  if(VFS_INODE_TYPE(n) == VFS_INODE_DIR)
  {
    vfs_seterror(vfs, VFS_EPERM);
    goto err;
  }

  // If the inode is currently in use, this call should be ignored.
  if(n->refcount)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(vfs_delete_inode(vfs, inode))
  {
    struct vfs_inode *p = vfs_get_inode_ptr(vfs, parent);
    p->modify_time = vfs_get_date();
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Remove an empty virtual directory (i.e. not a cached directory).
 * If this is a cached directory, it should be removed separately and
 * invalidated with `vfs_invalidate_at_path`.
 *
 * @param vfs   VFS handle.
 * @param path  path of directory within `vfs` to remove.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 *              If the file/directory exists and is cached, `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_rmdir(vfilesystem *vfs, const char *path)
{
  char buffer[MAX_PATH];
  struct vfs_inode *n;
  uint32_t parent;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Both must exist and be different.
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;
  if(!inode)
  {
    vfs_seterror(vfs, VFS_ENOENT);
    goto err;
  }
  if(parent == inode)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  // This function is not applicable to cached files.
  if(VFS_IS_CACHED(n))
  {
    // ...unless there are virtual children somewhere. A full check could
    // be slow so just check for any children and hope cached files are correct.
    if(VFS_INODE_TYPE(n) == VFS_INODE_DIR && n->length > 2)
      vfs_seterror(vfs, VFS_ENOTEMPTY);
    else
      vfs_seterror(vfs, VFS_ERR_IS_CACHED);
    goto err;
  }

  // If this is a file, this call should be ignored.
  if(VFS_INODE_TYPE(n) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, VFS_ENOTDIR);
    goto err;
  }

  // If the inode is not empty, this call should be ignored.
  if(n->length > 2)
  {
    vfs_seterror(vfs, VFS_ENOTEMPTY);
    goto err;
  }

  // If the inode is the CWD, this call should be ignored.
  if(inode == vfs->current)
  {
    vfs_seterror(vfs, VFS_EBUSY);
    goto err;
  }

  if(!vfs_elevate_lock(vfs))
    goto err;

  if(vfs_delete_inode(vfs, inode))
  {
    struct vfs_inode *p = vfs_get_inode_ptr(vfs, parent);
    p->modify_time = vfs_get_date();
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Get access permissions for a virtual file or directory (i.e. not cached).
 * If this is a cached file or directory, it should be checked with `access`.
 * Currently, the VFS system doesn't bother with permissions, so this works
 * as long as the file exists and isn't cached.
 *
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param mode  permissions to query (ignored; always treated as `F_OK`).
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_access(vfilesystem *vfs, const char *path, int mode)
{
  struct vfs_inode *n;
  uint32_t inode;
  enum vfs_error code;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  // This function is not applicable to cached files.
  if(VFS_IS_CACHED(n))
  {
    vfs_seterror(vfs, VFS_ERR_IS_CACHED);
    goto err;
  }

  // All operations are allowed for virtual files.
  vfs_read_unlock(vfs);
  return 0;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Get extended information about any file or directory (i.e. virtual or cached).
 * This function allows usage on cached files since `stat` calls can be very
 * slow and do not modify the filesystem.
 *
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param st    destination to store queried `stat` data to. If the call is
 *              successful, every field of this struct will be initialized.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 *              If the file/directory exists and is cached, `VFS_ERR_IS_CACHED`.
 */
enum vfs_error vfs_stat(vfilesystem *vfs, const char *path, struct stat *st)
{
  struct vfs_inode *n;
  uint32_t inode;
  int mode = S_IRWXU|S_IRWXG|S_IRWXO;
  enum vfs_error code;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  n = vfs_get_inode_ptr(vfs, inode);
  if(!n)
    goto err;

  memset(st, 0, sizeof(struct stat));

  if(VFS_INODE_TYPE(n) == VFS_INODE_FILE)
  {
    st->st_mode = S_IFREG | mode;
    st->st_size = n->length;
  }
  else
    st->st_mode = S_IFDIR | mode;

  // Dummy device value to indicate this is an inode from an MZX VFS.
  st->st_dev = (dev_t)VFS_MZX_DEVICE;
  st->st_ino = inode;
  st->st_nlink = 1;
  // This is cheating a little--atime is the access time but noatime can
  // cause it to never be updated in real filesystems. ctime is for status
  // changes but this VFS treats modifications and status changes as the same.
  st->st_atime = n->create_time;
  st->st_mtime = n->modify_time;
  st->st_ctime = n->modify_time;
  code = VFS_IS_CACHED(n) ? VFS_ERR_IS_CACHED : 0;
  vfs_read_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Get a list of virtual files present in a directory.
 * The list of virtual files will be allocated to `struct vfs_dir *d`.
 *
 * @param vfs   VFS handle.
 * @param path  path within `vfs` to query.
 * @param d     destination to store queried directory data to.
 * @return      0 on success, otherwise a relevant `vfs_error` code.
 *              This function does not set `errno`.
 */
enum vfs_error vfs_readdir(vfilesystem *vfs, const char *path, struct vfs_dir *d)
{
  struct vfs_dir_file **files;
  struct vfs_inode *p;
  uint32_t *inodes;
  uint32_t inode;
  size_t num_alloc;
  size_t num_files;
  size_t i;
  enum vfs_error code;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
    goto err;

  p = vfs_get_inode_ptr(vfs, inode);
  if(!p)
    goto err;

  // This function is only applicable to files.
  if(VFS_INODE_TYPE(p) != VFS_INODE_DIR)
  {
    vfs_seterror(vfs, VFS_ENOTDIR);
    goto err;
  }

  files = NULL;
  num_files = 0;
  num_alloc = 0;
  inodes = p->contents.inodes;

  for(i = 2; i < p->length; i++)
  {
    struct vfs_dir_file *f;
    struct vfs_inode *n;
    const char *name;
    size_t len;
    int type;
    inode = inodes[i];

    // Ignore cached files; the caller should be using dirent for them.
    n = vfs_get_inode_ptr(vfs, inode);
    if(!n || VFS_IS_CACHED(n))
      continue;

    if(num_files >= num_alloc)
    {
      struct vfs_dir_file **t;
      num_alloc = num_alloc ? num_alloc << 1 : 4;

      t = (struct vfs_dir_file **)realloc(files, num_alloc * sizeof(struct vfs_dir_file *));
      if(!t)
      {
        vfs_seterror(vfs, VFS_ENOMEM);
        goto err_free;
      }
      files = t;
    }

    name = vfs_inode_name(n);
    len = n->name_length;
    switch(VFS_INODE_TYPE(n))
    {
      case VFS_INODE_DIR:
        type = DIR_TYPE_DIR;
        break;
      case VFS_INODE_FILE:
        type = DIR_TYPE_FILE;
        break;
      default:
        type = DIR_TYPE_UNKNOWN;
        break;
    }

    f = (struct vfs_dir_file *)malloc(sizeof(struct vfs_dir_file) + len);
    if(!f)
    {
      vfs_seterror(vfs, VFS_ENOMEM);
      goto err_free;
    }

    f->type = type;
    memcpy(f->name, name, len + 1);

    files[num_files++] = f;
  }

  if(num_files < num_alloc)
  {
    struct vfs_dir_file **t = (struct vfs_dir_file **)realloc(files,
     num_files * sizeof(struct vfs_dir_file *));
    if(t)
      files = t;
  }

  d->files = files;
  d->num_files = num_files;

  vfs_read_unlock(vfs);
  return 0;

err_free:
  if(files)
  {
    for(i = 0; i < num_files; i++)
      free(files[i]);
    free(files);
  }
err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Free memory allocated to a `struct vfs_dir` by `vfs_readdir`.
 *
 * @param d   `vfs_dir` struct to free data from.
 * @return    0 on success, otherwise a negative value.
 */
enum vfs_error vfs_readdir_free(struct vfs_dir *d)
{
  size_t i;
  if(d->files)
  {
    for(i = 0; i < d->num_files; i++)
      free(d->files[i]);

    free(d->files);
    d->files = NULL;
  }
  return 0;
}

/**
 * Recursively delete all cached entries at the given path.
 * Unlinks cached entries that are currently opened, but they will
 * not be destroyed until they are closed.
 *
 * @param vfs       VFS handle.
 * @param path      path of file or directory within `vfs` to invalidate.
 * @return          0 on success, otherwise a relevant `vfs_error` code.
 *                  This function does not set `errno`.
 */
enum vfs_error vfs_invalidate_at_path(vfilesystem *vfs, const char *path)
{
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  inode = vfs_get_inode_by_path(vfs, path);
  if(!inode)
  {
    code = vfs_geterror(vfs);
    vfs_read_unlock(vfs);
    return code;
  }

  if(!vfs_elevate_lock(vfs))
  {
    vfs_read_unlock(vfs);
    return VFS_ERR_UNKNOWN;
  }

  vfs_invalidate_inode(vfs, inode);
  code = vfs_geterror(vfs);
  vfs_write_unlock(vfs);
  return code;
}

struct sort_data
{
  struct vfs_inode *n;
  uint32_t inode;
};

static int invalidate_sort_fn(const void *A, const void *B)
{
  const struct sort_data *a = (const struct sort_data *)A;
  const struct sort_data *b = (const struct sort_data *)B;

  // Sort by timestamp ascending (invalidate older files first).
  // Intentionally underflow since these are 32-bit truncated timestamps.
  if(a->n->timestamp != b->n->timestamp)
    return (int32_t)b->n->timestamp - (int32_t)a->n->timestamp;

  // Sort by size descending.
  if(a->n->length_alloc != b->n->length_alloc)
    return a->n->length_alloc > b->n->length_alloc ? -1 : 1;

  // Stabilize using the unique inode number.
  return a->inode < b->inode ? -1 : a->inode > b->inode ? 1 : 0;
}

/**
 * Free cached entries until the amount of memory pointed to by
 * `amount_to_free` has been invalidated. This function ignores cached
 * entries that have active references and directories. This may be an
 * expensive operation.
 *
 * @param vfs       VFS handle
 * @param amount_to_free  a pointer to the amount of memory to be freed
 *                  by this function. If at least this amount was freed,
 *                  0 will be stored to this value; otherwise, the amount of
 *                  memory that count not be freed will be stored here.
 * @return          0 on success, otherwise a relevant `vfs_error` code.
 *                  This function does not set `errno`.
 *                  A successful return value does not guarantee any entries were freed.
 */
enum vfs_error vfs_invalidate_at_least(vfilesystem *vfs, size_t *_amount_to_free)
{
  struct sort_data fallback[64];
  struct sort_data *targets;
  size_t amount_to_free;
  size_t total_free;
  size_t max_targets;
  size_t num_targets;
  size_t i;

  if(!_amount_to_free)
    return VFS_EINVAL;
  if(*_amount_to_free == 0)
    return 0;

  amount_to_free = *_amount_to_free;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  // This is likely too large, but that's why there's a fallback table.
  max_targets = vfs->table_length;
  targets = (struct sort_data *)malloc(max_targets * sizeof(struct sort_data));
  if(!targets)
  {
    targets = fallback;
    max_targets = ARRAY_SIZE(fallback);
  }

  num_targets = 0;
  for(i = 0; i < vfs->table_length; i++)
  {
    struct vfs_inode *n = vfs->table[i];

    if(n && VFS_INODE_TYPE(n) == VFS_INODE_FILE && n->refcount == 0 &&
     VFS_IS_CACHED(n) && !VFS_IS_INVALIDATED(n))
    {
      struct sort_data tmp = { n, i };
      targets[num_targets++] = tmp;

      if(num_targets >= max_targets)
        break;
    }
  }

  if(num_targets)
    qsort(targets, num_targets, sizeof(struct sort_data), invalidate_sort_fn);

  // Can't do anything, just exit.
  if(!num_targets || !vfs_elevate_lock(vfs))
  {
    vfs_read_unlock(vfs);
    if(targets != fallback)
      free(targets);
    return 0;
  }

  // If there is an oldest accessed file of at least the requested size and no
  // open references, deleting it should be good enough.
  for(i = 0; i < num_targets; i++)
  {
    if(targets[i].n->length_alloc >= amount_to_free)
    {
      vfs_delete_inode(vfs, targets[i].inode);
      amount_to_free = 0;
      goto success;
    }
  }

  // Delete entries from oldest to newest until the threshold is reached.
  total_free = 0;
  for(i = 0; i < num_targets && total_free < amount_to_free; i++)
  {
    total_free += targets[i].n->length_alloc;
    vfs_delete_inode(vfs, targets[i].inode);
    if(total_free >= amount_to_free)
      break;
  }
  amount_to_free = amount_to_free > total_free ? amount_to_free - total_free : 0;

success:
  vfs_write_unlock(vfs);

  *_amount_to_free = amount_to_free;
  if(targets != fallback)
    free(targets);
  return 0;
}

/**
 * Free ALL cached entries in the entire VFS. Opened cached entries will be
 * unlinked by this function, but not freed until they are closed.
 *
 * @param vfs       VFS handle.
 * @return          0 on success, otherwise a relevant `vfs_error` code.
 *                  This function does not set `errno`.
 */
enum vfs_error vfs_invalidate_all(vfilesystem *vfs)
{
  if(!vfs_write_lock(vfs))
    return vfs_geterror(vfs);

  // VFS_NO_INODE = the list of all roots.
  vfs_invalidate_inode(vfs, VFS_NO_INODE);

  vfs_write_unlock(vfs);
  return 0;
}

/**
 * Create a cached directory entry at the given path.
 *
 * @param vfs       VFS handle.
 * @param path      path of directory to create a cache entry at within `vfs`.
 * @param st        stat information to create cached directory from.
 * @return          0 on success, `VFS_ENOINT` if the parent doesn't exist, or
 *                  potentially another relevant `vfs_error` code.
 *                  This function does not set `errno`.
 */
enum vfs_error vfs_cache_directory(vfilesystem *vfs, const char *path, const struct stat *st)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    vfs_seterror(vfs, VFS_EEXIST);
    goto err;
  }
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;

  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // Create a cache directory.
  inode = vfs_make_inode(vfs, parent, buffer, 0,  VFS_INODE_DIR | VFS_INODE_IS_REAL);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = st ? st->st_mtime : n->create_time;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

/**
 * Create a cached file entry at the given path.
 * An internal buffer of at least `data_length` bytes will be allocated
 * and initialized using the provided callback.
 *
 * @param vfs         VFS handle.
 * @param path        path of file to create a cache entry at within `vfs`.
 * @param readfn      function to read bytes into a buffer, or `NULL`.
 *                    Arg 1: destination buffer to read to.
 *                    Arg 2: number of bytes to attempt to read.
 *                    Arg 3: the value provided to `priv`.
 *                    Return value: the number of bytes successfully read, which
 *                    may be less than the requested number. The total number of
 *                    bytes successfully read determines the actual file length.
 * @param priv        private data for `readfn`.
 * @param data_length initial allocated size of the cached file. This function
 *                    will attempt to read `data_length` bytes using `readfn`.
 * @return            0 on success, `VFS_ENOINT` if the parent doesn't exist, or
 *                    potentially another relevant `vfs_error` code.
 *                    This function does not set `errno`.
 */
enum vfs_error vfs_cache_file_callback(vfilesystem *vfs, const char *path,
 size_t (*readfn)(void * RESTRICT, size_t, void * RESTRICT),
 void *priv, size_t data_length)
{
  struct vfs_inode *p;
  char buffer[MAX_PATH];
  uint32_t parent;
  uint32_t inode;
  enum vfs_error code = 0;

  if(!vfs_read_lock(vfs))
    return vfs_geterror(vfs);

  if(!vfs_get_inode_and_parent_by_path(vfs, path, &parent, &inode, buffer, sizeof(buffer)))
    goto err;

  // Parent must exist, but target must not exist.
  if(inode)
  {
    p = vfs_get_inode_ptr(vfs, inode);
    if(p && VFS_INODE_TYPE(p) == VFS_INODE_FILE)
      vfs_seterror(vfs, VFS_EEXIST);
    else
      vfs_seterror(vfs, VFS_EISDIR);
    goto err;
  }
  if(!parent) // Error is set by vfs_get_inode_and_parent_by_path.
    goto err;

  p = vfs_get_inode_ptr(vfs, parent);
  if(!p)
    goto err;

  if(!vfs_elevate_lock(vfs))
    goto err;

  // Create a cache file.
  inode = vfs_make_inode(vfs, parent, buffer, data_length,
   VFS_INODE_FILE | VFS_INODE_IS_REAL);
  if(inode)
  {
    struct vfs_inode *n = vfs_get_inode_ptr(vfs, inode);
    n->create_time = vfs_get_date();
    n->modify_time = n->create_time;

    if(readfn)
      n->length = readfn(n->contents.data, n->length_alloc, priv);
    else
      n->length = 0;
  }
  else
    code = vfs_geterror(vfs);

  vfs_write_unlock(vfs);
  return code;

err:
  code = vfs_geterror(vfs);
  vfs_read_unlock(vfs);
  return code;
}

struct cache_file_mem_fn_data
{
  const unsigned char *data;
  size_t left;
};

static size_t cache_file_mem_fn(void * RESTRICT dest, size_t nbytes,
 void * RESTRICT priv)
{
  struct cache_file_mem_fn_data *d = (struct cache_file_mem_fn_data *)priv;
  if(!nbytes || !d || !d->data)
    return 0;

  if(nbytes > d->left)
    nbytes = d->left;

  if(nbytes)
  {
    memcpy(dest, d->data, nbytes);
    d->data += nbytes;
    d->left -= nbytes;
  }
  return nbytes;
}

/**
 * Create a cached file entry at the given path.
 * The contents of the file will be duplicated from the provided buffer.
 *
 * @param vfs         VFS handle.
 * @param path        path of file to create a cache entry at within `vfs`.
 * @param data        initial contents of the cached file.
 * @param data_length initial length and allocated size of the cached file.
 * @return            0 on success, `VFS_ENOINT` if the parent doesn't exist, or
 *                    potentially another relevant `vfs_error` code.
 *                    This function does not set `errno`.
 */
enum vfs_error vfs_cache_file(vfilesystem *vfs, const char *path, const void *data,
 size_t data_length)
{
  struct cache_file_mem_fn_data d =
  {
    (const unsigned char *)data,
    data_length
  };
  return vfs_cache_file_callback(vfs, path, cache_file_mem_fn, &d, data_length);
}

/**
 * Get the current total amount of RAM used by cached files.
 * This does not include structure overhead.
 *
 * @param vfs         VFS handle.
 * @return            the total amount of RAM used.
 */
size_t vfs_get_cache_total_size(vfilesystem *vfs)
{
  size_t sz = 0;

  if(vfs_read_lock(vfs))
  {
    sz = vfs->cache_total;
    vfs_read_unlock(vfs);
  }
  return sz;
}

/**
 * Get the total memory usage for the entire VFS.
 * This may be a slow operation.
 *
 * @param vfs         VFS handle.
 * @return            the total amount of RAM used.
 */
size_t vfs_get_total_memory_usage(vfilesystem *vfs)
{
  size_t sz = 0;
  size_t i;

  if(vfs_read_lock(vfs))
  {
    sz += sizeof(vfilesystem);
    sz += vfs->table_alloc * sizeof(struct vfs_inode *);

    for(i = 0; i < vfs->table_length; i++)
    {
      struct vfs_inode *n = vfs->table[i];
      if(!n)
        continue;

      sz += MAX(sizeof(struct vfs_inode), sizeof(struct vfs_inode_name_alloc));
      if(n->flags & VFS_INODE_NAME_ALLOC)
        sz += n->name_length + 1;

      if(n->flags & VFS_INODE_DIR)
        sz += n->length_alloc * sizeof(uint32_t);
      else
        sz += n->length_alloc;
    }
    vfs_read_unlock(vfs);
  }
  return sz;
}

/**
 * Sets whether or not the VFS should track timestamps for files
 * created or closed AFTER this function is called. This function exists
 * mainly for the unit tests.
 *
 * @param vfs         VFS handle.
 * @param enable      if true, track timestamps.
 */
void vfs_set_timestamps_enabled(vfilesystem *vfs, boolean enable)
{
  vfs->disable_timestamp = !enable;
}

/**
 * Convert vfs_error codes to their errno equivalents.
 *
 * @param err         `vfs_error` to convert.
 * @return            equivalent `errno` value, or `EINVAL` if there isn't one.
 */
int vfs_error_to_errno(enum vfs_error err)
{
  switch(err)
  {
    case VFS_EPERM:         return EPERM;
    case VFS_ENOENT:        return ENOENT;
    case VFS_EBADF:         return EBADF;
    case VFS_ENOMEM:        return ENOMEM;
    case VFS_EACCES:        return EACCES;
    case VFS_EBUSY:         return EBUSY;
    case VFS_EEXIST:        return EEXIST;
    case VFS_ENOTDIR:       return ENOTDIR;
    case VFS_EISDIR:        return EISDIR;
    case VFS_EINVAL:        return EINVAL;
    case VFS_ENOSPC:        return ENOSPC;
    case VFS_ERANGE:        return ERANGE;
    case VFS_ENAMETOOLONG:  return ENAMETOOLONG;
    case VFS_ENOTEMPTY:     return ENOTEMPTY;
    case VFS_ERR_UNKNOWN:   return 0;
    case VFS_ERR_IS_CACHED: return 0;
  }
  if(err == 0)
    return 0;

  return EINVAL;
}

#endif /* VIRTUAL_FILESYSTEM */

/**
 * Allocate and initialize a VFS.
 * (This function is enabled even when VIRTUAL_FILESYSTEM is not defined so
 * -pedantic doesn't complain about ISO C not allowing empty compilation units.)
 *
 * @return a `vfilesystem` handle on success, otherwise `NULL`. If VFS support
 *         is disabled, this function will always return `NULL`.
 */
vfilesystem *vfs_init(void)
{
#ifdef VIRTUAL_FILESYSTEM
  vfilesystem *vfs = (vfilesystem *)malloc(sizeof(vfilesystem));
  if(vfs)
  {
    if(vfs_setup(vfs))
      return vfs;

    vfs_clear(vfs);
    free(vfs);
  }
#endif
  return NULL;
}

/**
 * Free a VFS handle created by `vfs_init`.
 *
 * @param vfs   VFS handle to free.
 */
void vfs_free(vfilesystem *vfs)
{
#ifdef VIRTUAL_FILESYSTEM
  vfs_clear(vfs);
  free(vfs);
#endif
}

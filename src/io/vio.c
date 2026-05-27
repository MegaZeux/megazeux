/* MegaZeux
 *
 * Copyright (C) 2019-2026 Alice Rowan <petrifiedrowan@gmail.com>
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
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

#include "../util.h"
#include "memfile.h"
#include "path.h"
#include "vio.h"
#include "vfs.h"
//#include "zip.h"

#ifdef _WIN32
#include "vio_win32.h"
#else
#include "vio_posix.h"
#endif

#ifndef VFILE_SMALL_BUFFER_SIZE
#define VFILE_SMALL_BUFFER_SIZE 256
#endif

#ifndef VFILE_LARGE_BUFFER_SIZE
#define VFILE_LARGE_BUFFER_SIZE 32768
#endif

// If 0 is provided as a cache maximum size, use this value instead.
#define DEFAULT_MAX_SIZE (1<<22)

struct vfile
{
  FILE *fp;

  struct memfile mf;
  // Local copy of pointer/size of a memory vfile buffer (expandable or not).
  void *local_buffer;
  size_t local_buffer_size;
  // External copy of pointer/size of expandable vfile buffer.
  void **external_buffer;
  size_t *external_buffer_size;
  // Virtual file--inode and position.
  size_t *v_length;
  size_t position;
  size_t position_writeback;
  uint32_t inode;

  // vungetc buffer for memory files.
  int tmp_chr;
  int flags;
  // To avoid arithmetic on NULL...
  char dummy[1];
};


/************************************************************************
 * Virtual filesystem support functions.
 */

// Letting the optimizer deal with this is cleaner than macros everywhere...
#ifdef VIRTUAL_FILESYSTEM
static vfilesystem *vfs_base = NULL;
#else
#define vfs_base ((vfilesystem *)NULL)
#endif
static size_t vfs_max_size = 0;
static size_t vfs_max_auto_cache_file_size = 0;
static boolean vfs_enable_auto_cache = false;

/**
 * Recursively cache the provided directory path if it doesn't exist in
 * the cache, including the root. This function will cache as much of `path`
 * as possible, starting from the root and working down the directory tree.
 * Returns true if the directory is already in the VFS or if the entire
 * path is successfully cached.
 */
static boolean vio_cache_directory_recursively(vfilesystem *vfs, const char *path)
{
  char tmp[MAX_PATH];
  char *next;
  struct stat st;
  ssize_t len;
  enum vfs_error code;
  int ret;

  // Does it already exist in the VFS?
  code = vfs_stat(vfs, path, &st);
  if(code == 0 || code == VFS_ERR_IS_CACHED)
  {
    trace("--VIO-- vio_cache_directory_recursively: '%s' exists\n", path);
    return true;
  }

/*
  // Is it a real directory?
  ret = platform_stat(path, &st);
  if(ret < 0 || !S_ISDIR(st.st_mode))
    return false;
*/

  // Initialize root.
  path_clean_copy(tmp, MAX_PATH, path);
  next = tmp;
  len = path_is_absolute(tmp);
  if(len >= 1 && !(len == 1 && isslash(tmp[0])))
  {
#if defined(PATH_DOS_STYLE_ROOTS) || defined(PATH_AMIGA_STYLE_ROOTS)
    int skip = 0;
    int colon_pos = -1;

    // Root should end in :/, unless CWD is the root, in which
    // case it may end with just a :.
    if(tmp[len - 1] == ':')
      colon_pos = len - 1;
    if(tmp[len - 2] == ':')
      colon_pos = len - 2;
    if(colon_pos < 0)
    {
      trace("--VIO-- vio_cache_directory_recursively: missing colon (%s)\n", path);
      return false;
    }

#ifdef PATH_UNC_ROOTS
    // Reject UNC host/share paths as network files should be assumed volatile.
    // Allow local file UNC prefixes.
    if(isslash(tmp[0]))
    {
      if(len > 4 && (!memcmp(tmp, "\\\\.\\", 4) || !memcmp(tmp, "\\\\?\\", 4)) &&
       strncasecmp(tmp + 4, "unc\\", 4))
      {
        next += 4;
        skip = 4;
      }
      else
      {
        trace("--VIO-- vio_cache_directory_recursively: rejecting UNC (%s)\n", path);
        return false;
      }
    }
#endif

    path_tokenize(&next);
    /* Amiga: a colon at the start signifies the current active disk. */
    if(colon_pos > 0)
    {
      tmp[colon_pos] = '\0';

      code = vfs_make_root(vfs, tmp + skip);
      if(code != 0 && code != VFS_EEXIST)
      {
        trace("--VIO-- vio_cache_directory_recursively: failed vfs_make_root '%s' (%s)\n",
         tmp + skip, path);
        return false;
      }

      // Repair current fragment.
      tmp[colon_pos] = ':';
      if(colon_pos < len - 1)
        tmp[len - 1] = DIR_SEPARATOR_CHAR;
    }
#else
    // DOS-style roots are only supported as VFS roots and should never
    // make it here!
    trace("--VIO-- vio_cache_directory_recursively: unsupported DOS-style root (%s)\n", path);
    return false;
#endif
  }
  else

  if(tmp[0] == '/')
    next++;

  // Initialize directories.
  while(next)
  {
    path_tokenize(&next);

    ret = platform_stat(tmp, &st);
    if(ret < 0 || !S_ISDIR(st.st_mode))
    {
      trace("--VIO-- vio_cache_directory_recursively: stat failed or is not dir '%s' (%s)\n",
       tmp, path);
      return false;
    }

    code = vfs_cache_directory(vfs, tmp, &st);
    if(code != 0 && code != VFS_EEXIST)
    {
      trace("--VIO-- vio_cache_directory_recursively: failed vfs_cache_directory '%s' (%s)\n",
       tmp, path);
      return false;
    }

    // Repair current fragment.
    if(next)
      next[-1] = DIR_SEPARATOR_CHAR;
  }
  trace("--VIO-- vio_cache_directory_recursively: '%s' OK\n", path);
  return true;
}

/**
 * Get the parent directory from the provided path and cache it recursively,
 * including the root. The parent must successfully stat. If there is no parent
 * component of the path, this function automatically succeeds.
 */
static boolean vio_cache_parent_recursively(vfilesystem *vfs, const char *path)
{
  char parent[MAX_PATH];

  // If there is no parent component, there's nothing else to do here.
  if(path_get_parent(parent, MAX_PATH, path) <= 0)
    return true;

  return vio_cache_directory_recursively(vfs, parent);
}

static size_t cache_file_read_fn(void * RESTRICT dest, size_t nbytes,
 void * RESTRICT priv)
{
  FILE *fp = (FILE *)priv;
  return fread(dest, 1, nbytes, fp);
}

/**
 * Cache the file at the provided path and its parent directory (recursively).
 * The file should already be opened with stdio.
 */
static boolean vio_cache_file_and_parent(vfilesystem *vfs, const char *path,
 FILE *fp, int flags)
{
  struct stat st;
  int64_t len;
  int64_t pos;
  enum vfs_error code;

  // Already cached...
  if(vfs_stat(vfs, path, &st) == 0 && S_ISREG(st.st_mode))
    return true;

  len = platform_filelength(fp);
  if(len < 0 || (size_t)len >= SIZE_MAX)
    return false;

  // Can't cache files over the individual file size limit.
  if((size_t)len >= vfs_max_auto_cache_file_size && (~flags & V_FORCE_CACHE))
    return false;

  // If there isn't enough space, try to free some.
  if(vfs_max_size)
  {
    size_t current = vfs_get_cache_total_size(vfs);
    size_t avail = vfs_max_size > current ? vfs_max_size - current : 0;
    size_t to_free = (size_t)len > avail ? len - avail : 0;

    if(to_free)
      vfs_invalidate_at_least(vfs, &to_free);

    // If it wasn't able to free enough space, just fail.
    if(to_free && (~flags & V_FORCE_CACHE))
      return false;
  }

  if(!vio_cache_parent_recursively(vfs, path))
    return false;

  pos = platform_ftell(fp);
  code = vfs_cache_file_callback(vfs, path, cache_file_read_fn, fp, len);
  platform_fseek(fp, pos, SEEK_SET);
  return code == 0;
}

/**
 * Given a relative or absolute path, convert it to an absolute path if the
 * current working directory is virtual. This is necessary because the current
 * real directory is desyncronized from the current VFS directory (the current
 * VFS directory does not exist in the real filesystem). This function will
 * always clobber the contents of the provided buffer.
 *
 * TODO: if a platform needs chdir/getcwd emulation, this would be a good
 * place to insert relative path conversion for that too, since all functions
 * which take paths need to use this when the VFS is enabled.
 */
static const char *vio_normalize_virtual_path(vfilesystem *vfs,
 char *buffer, size_t buffer_len, const char *path)
{
  // Cached current directory will return VFS_ERR_IS_CACHED instead.
  if(vfs_getcwd(vfs, buffer, buffer_len) == 0)
  {
    path_navigate_no_check(buffer, buffer_len, path);
    return buffer;
  }
  return path;
}

/**
 * Enables the virtual filesystem for all vio.c functions. The virtual
 * filesystem provides virtual files and file caching. This function
 * is not thread-safe. If the virtual filesystem is already initialized,
 * this function will update the cache settings.
 *
 * @param max_size            soft limit for the maximum total size of cached
 *                            files in the VFS. In some cases, the total size
 *                            may be allowed exceed this value. If 0 is provided
 *                            a default maximum size will be used instead.
 * @param max_file_size       maximum size of files allowed to be automatically
 *                            cached, if enabled. If 0 is provided, this will
 *                            default to `max_size`.
 * @param enable_auto_cache   if `true`, files will be automatically cached.
 * @return                    `true` on success, otherwise `false`.
 */
boolean vio_filesystem_init(size_t max_size, size_t max_file_size,
 boolean enable_auto_cache)
{
  char tmp[MAX_PATH];
  enum vfs_error code;

  trace("--VIO-- vio_filesystem_init\n");
  if(!vfs_base)
  {
#ifdef VIRTUAL_FILESYSTEM
    vfs_base = vfs_init();
#endif
    if(!vfs_base)
    {
      trace("--VIO-- vio_filesystem_init: failed vfs_init\n");
      return false;
    }
  }

  // Synchronize the current working directory.
  if(!getcwd(tmp, MAX_PATH))
  {
    trace("--VIO-- vio_filesystem_init: failed getcwd\n");
    goto err;
  }
  if(!vio_cache_directory_recursively(vfs_base, tmp))
  {
    trace("--VIO-- vio_filesystem_init: "
     "failed vio_cache_directory_recursively: %s\n", tmp);
    goto err;
  }
  code = vfs_chdir(vfs_base, tmp);
  if(code != VFS_ERR_IS_CACHED)
  {
    trace("--VIO-- vio_filesystem_init: failed vfs_chdir with code %u: %s\n",
     code, strerror(vfs_error_to_errno(code)));
    goto err;
  }

  // Immediately purge the cache if it has been disabled.
  if(vfs_enable_auto_cache && !enable_auto_cache)
    vfs_invalidate_all(vfs_base);

  vfs_max_size = max_size ? max_size : DEFAULT_MAX_SIZE;
  vfs_max_auto_cache_file_size = max_file_size ? max_file_size : vfs_max_size;
  vfs_enable_auto_cache = enable_auto_cache;

  trace("--VIO-- vio_filesystem_init: success\n");
  return true;

err:
#ifdef VIRTUAL_FILESYSTEM
  vfs_free(vfs_base);
  vfs_base = NULL;
#endif
  return false;
}

/**
 * Disable the virtual filesystem for all vio.c functions and free all memory
 * associated with it. This function is not thread-safe and should be called
 * only when all other threads are finished using vio.c functions.
 *
 * @return                    `true` on success.
 */
boolean vio_filesystem_exit(void)
{
#ifdef VIRTUAL_FILESYSTEM
  if(vfs_base)
    vfs_free(vfs_base);

  vfs_base = NULL;
#endif
  return true;
}

/**
 * Get the total memory usage for the contents of cached files ONLY
 * the vio.c virtual filesystem.
 *
 * @return                    the total amount of memory used.
 */
size_t vio_filesystem_total_cached_usage(void)
{
  if(vfs_base)
    return vfs_get_cache_total_size(vfs_base);
  return 0;
}

/**
 * Get the total memory usage of the vio.c virtual filesystem,
 * INCLUDING cached files.
 *
 * @return                    the total amount of memory used.
 */
size_t vio_filesystem_total_memory_usage(void)
{
  if(vfs_base)
    return vfs_get_total_memory_usage(vfs_base);
  return 0;
}

/**
 * Create a virtual file in the VFS which is not backed by a physical file.
 * If a physical file exists at the provided path, it will be masked.
 * If the file already exists in the virtual filesystem, this function will
 * return true.
 *
 * The parent directory of the path must be a real directory or an existing
 * virtual directory.
 *
 * @param                     path to create a new file at.
 * @return                    `true` on success, otherwise `false`.
 */
boolean vio_virtual_file(const char *path)
{
  enum vfs_error code;
  if(!vfs_base)
    return false;

  if(!vio_cache_parent_recursively(vfs_base, path))
    return false;

  code = vfs_create_file_at_path(vfs_base, path);
  if(code != 0 && code != VFS_EEXIST)
    return false;

  return true;
}

/**
 * Create a virtual directory in the VFS which is not backed by a physical file.
 * If a physical file exists at the provided path, it will be masked.
 * If the directory already exists in the virtual filesystem, this function
 * will return true.
 *
 * The parent directory of the path must be a real directory or an existing
 * virtual directory.
 *
 * @param                     path to create a new directory at.
 * @return                    `true` on success, otherwise `false`.
 */
boolean vio_virtual_directory(const char *path)
{
  enum vfs_error code;
  if(!vfs_base)
    return false;

  if(!vio_cache_parent_recursively(vfs_base, path))
    return false;

  code = vfs_mkdir(vfs_base, path, 0755);
  if(code != 0 && code != VFS_EEXIST)
    return false;

  return true;
}

/**
 * Invalidate and purge cached files in the VFS until at least
 * the amount of memory pointed to by `amount_to_free` has been invalided.
 */
boolean vio_invalidate_at_least(size_t *amount_to_free)
{
#ifdef DEBUG
  size_t init = amount_to_free ? *amount_to_free : 0;
#endif
  enum vfs_error code;
  if(!vfs_base || !amount_to_free)
    return false;

  code = vfs_invalidate_at_least(vfs_base, amount_to_free);
  if(code != 0)
    return false;

  debug("vio_invalidate_at_least freed >= %zu buffered\n", init - *amount_to_free);
  if(*amount_to_free > 0)
    return false;

  return true;
}

/**
 * Invalidate and purge ALL cached files in the VFS.
 */
boolean vio_invalidate_all(void)
{
  if(!vfs_base)
    return false;

  debug("vio_invalidate_all\n");
  if(vfs_invalidate_all(vfs_base) != 0)
    return false;

  return true;
}


/************************************************************************
 * vfile functions and stdio/unistd wrappers.
 */

/**
 * Parse vfile mode flags from a standard fopen mode string.
 */
int vfile_get_mode_flags(const char *mode)
{
  int flags = 0;

  assert(mode);

  switch(*(mode++))
  {
    case 'r':
      flags |= VF_READ;
      break;

    case 'w':
      flags |= VF_WRITE | VF_TRUNCATE;
      break;

    case 'a':
      flags |= VF_WRITE | VF_APPEND;
      break;

    default:
      return 0;
  }

  while(*mode)
  {
    switch(*(mode++))
    {
      case 'b':
        flags |= VF_BINARY;
        break;

      // Explicitly "text" mode. Does nothing, but some libcs support it.
      case 't':
        break;

      case '+':
        flags |= (VF_READ | VF_WRITE);
        break;

      default:
        return 0;
    }
  }
  return flags;
}

/**
 * Diagnostic function to get the internal flags of a vfile.
 * Doesn't have much use outside of the unit tests.
 *
 * @param  vf     vfile handle.
 * @return        the internal flags of the provided vfile.
 */
int vfile_get_flags(vfile *vf)
{
  return vf ? vf->flags : 0;
}

/**
 * Internal function for opening a file from the VFS.
 * filename should already have been normalized by the caller.
 */
static enum vfs_error vfopen_virtual(vfilesystem *vfs, vfile *vf,
 const char *filename, int flags)
{
  uint32_t inode;
  enum vfs_error code;

  code = vfs_open_if_exists(vfs_base, filename, !!(flags & VF_WRITE), &inode);
  if(code != 0 && code != VFS_ERR_IS_CACHED)
    return code;

  if(flags & VF_TRUNCATE)
    vfs_truncate(vfs_base, inode);

  vf->flags |= VF_MEMORY | VF_MEMORY_EXPANDABLE | VF_VIRTUAL;
  vf->inode = inode;
  vf->position = (flags & VF_APPEND) ? vfs_filelength(vfs, inode) : 0;
  vf->position_writeback = vf->position;
  return code;
}

/**
 * Open a file for input or output with user-defined flags.
 */
vfile *vfopen_unsafe_ext(const char *filename, const char *mode,
 int user_flags)
{
  char buffer[MAX_PATH];
  int flags = vfile_get_mode_flags(mode);
  enum vfs_error code;
  vfile *vf;
  FILE *fp;

  assert(filename && flags);
  flags |= (user_flags & VF_PUBLIC_MASK);

  vf = (vfile *)calloc(1, sizeof(vfile));
  vf->tmp_chr = EOF;
  vf->flags = flags;

  if(vfs_base)
  {
    filename = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, filename);
    code = vfopen_virtual(vfs_base, vf, filename, flags);
    // File is 100% virtual? Can exit now.
    if(code == 0)
      return vf;
    // Non-write and cached? Don't need a FILE handle for that either...
    if(code == VFS_ERR_IS_CACHED && (~flags & VF_WRITE))
      return vf;
  }

  // Open the vfile as a normal file.
  fp = platform_fopen_unsafe(filename, mode);
  if(!fp)
  {
    if(vfs_base && vf->inode)
    {
      vfs_close(vfs_base, vf->inode);
      vfs_invalidate_at_path(vfs_base, filename);
    }
    free(vf);
    return NULL;
  }

#ifndef VFILE_NO_SETVBUF
  if((flags & VF_BINARY) && (!(flags & VF_READ) || !(flags & VF_WRITE)))
  {
    if(flags & V_LARGE_BUFFER)
    {
      setvbuf(fp, NULL, _IOFBF, VFILE_LARGE_BUFFER_SIZE);
      flags &= ~V_SMALL_BUFFER;
    }
    else

    if(flags & V_SMALL_BUFFER)
      setvbuf(fp, NULL, _IOFBF, VFILE_SMALL_BUFFER_SIZE);
  }
  else
#endif
    flags &= ~(V_SMALL_BUFFER | V_LARGE_BUFFER);

  vf->fp = fp;
  vf->flags |= VF_FILE;

  if(vfs_base && !vf->inode && (~flags & V_DONT_CACHE))
  {
    if(vfs_enable_auto_cache || (flags & V_FORCE_CACHE))
    {
      if(vio_cache_file_and_parent(vfs_base, filename, fp, flags))
      {
        // In the rare case this fails, the vfile will just fall back to stdio.
        vfopen_virtual(vfs_base, vf, filename, flags);
      }
    }
  }
  return vf;
}

/**
 * Open a file for input or output.
 */
vfile *vfopen_unsafe(const char *filename, const char *mode)
{
  return vfopen_unsafe_ext(filename, mode, 0);
}

/**
 * Create a vfile from an existing fp.
 */
vfile *vfile_init_fp(FILE *fp, const char *mode)
{
  int flags = vfile_get_mode_flags(mode);
  vfile *vf = NULL;

  assert(fp && flags);

  vf = (vfile *)ccalloc(1, sizeof(vfile));
  vf->fp = fp;
  vf->tmp_chr = EOF;
  vf->flags = flags | VF_FILE;
  return vf;
}

/**
 * Create a vfile from an existing memory buffer.
 */
vfile *vfile_init_mem(void *buffer, size_t size, const char *mode)
{
  int flags = vfile_get_mode_flags(mode);
  vfile *vf = NULL;
  size_t filesize = size;

  assert((buffer && size) || (!buffer && !size));
  assert(flags);

  // "w"-based modes should start at size 0 and expand as-needed, either in
  // their fixed buffer or with an extendable buffer (see vfile_init_mem_ext).
  if(flags & VF_TRUNCATE)
    filesize = 0;

  vf = (vfile *)ccalloc(1, sizeof(vfile));
  mfopen_wr(buffer ? buffer : vf->dummy, filesize, &(vf->mf));
  vf->mf.seek_past_end = true;
  vf->tmp_chr = EOF;
  vf->flags = flags | VF_MEMORY;
  vf->local_buffer = buffer;
  vf->local_buffer_size = size;
  return vf;
}

/**
 * Create a vfile from an existing memory buffer.
 * This vfile will be resizable and, when resized, the source pointer and size
 * will be updated to match.
 *
  // vfs_init_mem_ext needs to be modified to allow readable files from
  // external buffers. attempt to sync vfile to external buffer for each op
  // also need a way to insert the inode into the vfile from here.
 */
vfile *vfile_init_mem_ext(void **external_buffer, size_t *external_buffer_size,
 const char *mode)
{
  vfile *vf = vfile_init_mem(*external_buffer, *external_buffer_size, mode);

  assert(vf->flags & VF_WRITE);

  vf->flags |= VF_MEMORY_EXPANDABLE;
  vf->external_buffer = external_buffer;
  vf->external_buffer_size = external_buffer_size;
  return vf;
}

/**
 * Create a temporary vfile.
 * If `initial_size` is non-zero, this function will attempt to allocate an
 * expandable temporary file in RAM with this size. If allocation fails or if
 * `initial_size` is zero, tmpfile() will be called instead.
 */
vfile *vtempfile(size_t initial_size)
{
  FILE *fp;
  vfile *vf;

  if(initial_size)
  {
    void *buffer = malloc(initial_size);
    if(buffer)
    {
      vf = vfile_init_mem(buffer, initial_size, "wb+");
      vf->flags |= VF_MEMORY_EXPANDABLE | VF_MEMORY_FREE;
      vf->local_buffer = buffer;
      vf->local_buffer_size = initial_size;
      return vf;
    }
  }

  fp = platform_tmpfile();
  if(fp)
  {
    setvbuf(fp, NULL, _IOFBF, VFILE_SMALL_BUFFER_SIZE);
    return vfile_init_fp(fp, "wb+");
  }

  return NULL;
}

/**
 * Force an open vfile entirely into RAM. This function will ALWAYS rewind.
 * If this is a regular vfile in READ mode, attempt to copy it into RAM.
 * If this is a cached vfile in READ mode, just close the FILE if it's open.
 * If this is a memory or virtual vfile, nothing needs to be done.
 * Returns false if the vfile can not be moved to memory or is in write mode.
 */
boolean vfile_force_to_memory(vfile *vf)
{
  assert(vf);
  vrewind(vf);
  // This only works in read mode--write mode needs to be able to write
  // back, unless it's a virtual file, in which case
  if(~vf->flags & VF_FILE)
  {
    // Already entirely in memory.
    if(vf->flags & VF_MEMORY)
      return true;
    // Broken handle :(
    return false;
  }
  // If there's an underlying FILE, it can only be closed in read mode.
  if(vf->flags & VF_WRITE)
    return false;

  // FILE and memory -> cached, close the underlying FILE.
  // If there is no memory, it needs to be loaded to memory first.
  if(~vf->flags & VF_MEMORY)
  {
    int64_t len = vfilelength(vf, false);
    void *buffer;

    if(len < 0 || (size_t)len >= SIZE_MAX)
      return false;

    buffer = malloc(len);
    if(!buffer)
      return false;

    if(!vfread(buffer, len, 1, vf))
    {
      free(buffer);
      return false;
    }

    mfopen(buffer, len, &(vf->mf));
    vf->mf.seek_past_end = true;
    vf->tmp_chr = EOF;
    vf->flags |= VF_MEMORY | VF_MEMORY_FREE;
    vf->local_buffer = buffer;
    vf->local_buffer_size = len;
  }

  fclose(vf->fp);
  vf->flags &= ~VF_FILE;
  vf->fp = NULL;
  return true;
}

/**
 * Close a file. The file pointer should not be used after using this function.
 */
int vfclose(vfile *vf)
{
  int retval = 0;

  assert(vf);

  if(vfs_base && vf->inode)
  {
    // Rewind to force writeback.
    // TODO: kind of a crap way to do this, might need to add vfflush.
    vrewind(vf);
    vfs_close(vfs_base, vf->inode);
  }

  if((vf->flags & VF_MEMORY) && (vf->flags & VF_MEMORY_FREE))
  {
    free(vf->local_buffer);
    if(vf->external_buffer)
    {
      // Make sure these didn't desync somehow...
      assert(vf->local_buffer == *(vf->external_buffer));
      *(vf->external_buffer) = NULL;
    }
  }

  if(vf->flags & VF_FILE)
    retval = fclose(vf->fp);

  free(vf);
  return retval;
}


/************************************************************************
 * Filesystem function wrappers.
 */

/**
 * Change the current working directory to path.
 */
int vchdir(const char *path)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    enum vfs_error code;
    int ret;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
    // If this is also a virtual path, this will succeed. The vfs_access
    // call filters out cached directories, which vfs_chdir DOES work on.
    if(vfs_access(vfs_base, path, R_OK) == 0 && vfs_chdir(vfs_base, path) == 0)
      return 0;

    // Special: the current directory must exist in the VFS.
    vio_cache_directory_recursively(vfs_base, path);

    // If this is a real path, this function will succeed, in which case
    // the VFS current directory should also be updated.
    ret = platform_chdir(path);
    if(ret == 0)
    {
      code = vfs_chdir(vfs_base, path);
      assert(code == VFS_ERR_IS_CACHED);
      (void)code; // silence -Wunused-but-set-variable
    }

    return ret;
  }
  return platform_chdir(path);
}

/**
 * Print the name of the current working directory to buffer 'buf' of
 * length 'size'.
 */
char *vgetcwd(char *buf, size_t size)
{
  if(vfs_base)
  {
    // If cwd is a virtual path, return the VFS cwd.
    // Cached real cwds will return VFS_ERR_IS_CACHED.
    if(vfs_getcwd(vfs_base, buf, size) == 0)
      return buf;
  }
  return platform_getcwd(buf, size);
}

/**
 * Make a directory with a given name.
 */
int vmkdir(const char *path, int mode)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    enum vfs_error code;
    int ret;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);

    // Special: this function prefers real mkdir over vfs_mkdir.
    // Use vio_virtual_directory to force creation of virtual directories.
    ret = platform_mkdir(path, mode);
    if(ret == 0 || errno != ENOENT)
      return ret;

    code = vfs_mkdir(vfs_base, path, mode);
    if(code != 0)
    {
      errno = vfs_error_to_errno(code);
      return -1;
    }
    return 0;
  }
  return platform_mkdir(path, mode);
}

/**
 * Rename a file or directory.
 */
int vrename(const char *oldpath, const char *newpath)
{
  if(vfs_base)
  {
    char buffer1[MAX_PATH];
    char buffer2[MAX_PATH];
    enum vfs_error code;
    int ret;

    oldpath = vio_normalize_virtual_path(vfs_base, buffer1, MAX_PATH, oldpath);
    newpath = vio_normalize_virtual_path(vfs_base, buffer2, MAX_PATH, newpath);

    // Preemptively cache the destination parent.
    vio_cache_parent_recursively(vfs_base, newpath);

    // Attempt vfs_rename, which will handle virtual source files
    // moving, replacing cached files, or replacing virtual files.
    // No filesystem operation needs to be done in this case.
    code = vfs_rename(vfs_base, oldpath, newpath);
    if(code == 0)
      return 0;

    if(code == VFS_EBUSY || code == VFS_ENOTDIR ||
     code == VFS_EISDIR || code == VFS_EEXIST)
    {
      errno = vfs_error_to_errno(code);
      return -1;
    }

    // Try a real rename...
    ret = platform_rename(oldpath, newpath);
    if(ret != 0)
    {
      // (Try to) roll back the vfs_rename.
      // This unfortunately won't bring back the old dir/file at newpath.
      vfs_rename(vfs_base, newpath, oldpath);
    }
    return ret;
  }
  return platform_rename(oldpath, newpath);
}

/**
 * Unlink a name (and delete a file if it was the last link to the file) if
 * the file is a real file.
 */
int vunlink(const char *path)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    enum vfs_error code;
    int ret;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
    code = vfs_unlink(vfs_base, path);
    if(code == 0)
      return 0;

    // VFS inode exists and isn't cached, but there was an error.
    if(code == VFS_EBUSY || code == VFS_EPERM)
    {
      errno = vfs_error_to_errno(code);
      return -1;
    }

    ret = platform_unlink(path);
    if(ret == 0)
      vfs_invalidate_at_path(vfs_base, path);
    return ret;
  }
  return platform_unlink(path);
}

/**
 * Remove an empty directory at a given path.
 */
int vrmdir(const char *path)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    enum vfs_error code;
    int ret;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
    code = vfs_rmdir(vfs_base, path);
    if(code == 0)
      return 0;

    // VFS inode exists and isn't cached, but there was an error.
    if(code == VFS_EBUSY || code == VFS_ENOTDIR || code == VFS_ENOTEMPTY)
    {
      errno = vfs_error_to_errno(code);
      return -1;
    }

    ret = platform_rmdir(path);
    if(ret == 0)
      vfs_invalidate_at_path(vfs_base, path);
    return ret;
  }
  return platform_rmdir(path);
}

/**
 * Check an access permission for a path for the current process.
 *
 * F_OK   (checks existence only)
 * R_OK   Read permission.
 * W_OK   Write permission.
 * X_OK   Execute permission.
 */
int vaccess(const char *path, int mode)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    enum vfs_error code;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
    code = vfs_access(vfs_base, path, mode);
    if(code == 0)
      return 0;

    // File exists and is virtual but permission was denied.
    if(code == VFS_EACCES)
    {
      errno = vfs_error_to_errno(code);
      return -1;
    }
  }
  return platform_access(path, mode);
}

/**
 * Get file information for a file at a given path.
 */
int vstat(const char *path, struct stat *buf)
{
  if(vfs_base)
  {
    char buffer[MAX_PATH];
    struct stat tmp;
    enum vfs_error code;

    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
    // Pass through success only.
    code = vfs_stat(vfs_base, path, &tmp);
    if(code == 0 || code == VFS_ERR_IS_CACHED)
    {
      memcpy(buf, &tmp, sizeof(struct stat));
      return 0;
    }
  }
  return platform_stat(path, buf);
}


/************************************************************************
 * stdio function wrappers.
 */

/* If virtual IO fails, close the VFS file (if applicable)
 * and fall back to stdio. */
static inline void vfile_cleanup_virtual(vfile *vf)
{
  if(vfs_base)
    vfs_close(vfs_base, vf->inode);

  vf->flags &= ~(VF_MEMORY | VF_VIRTUAL);
  vf->inode = 0;

  if(vf->fp)
  {
    size_t pos = vf->position;
    int buf = vf->tmp_chr;
    vrewind(vf);
    vfseek(vf, pos, SEEK_SET);
    if(buf >= 0)
      vungetc(buf, vf);
  }
  else
    vf->flags &= ~VF_FILE;
}

/* Perform a writeback for cached vfiles. This should be performed
 * after the VFS file is read locked and the memfile is synchronized.
 * Returns true on success or if no writeback was required. */
static inline boolean virt_writeback(vfile *vf)
{
  size_t count;
  size_t avail;
  size_t out;
  const unsigned char *src;

  // Detached vfiles and read-only vfiles can't perform a writeback operation.
  if(!vf->inode || !vf->fp || (~vf->flags & VF_WRITE))
    return true;

  if(vf->position_writeback == vf->position)
    return true;

  // fseek to write position (also allows switching to write mode).
  if(platform_fseek(vf->fp, vf->position_writeback, SEEK_SET) != 0)
    return false;

  // This shouldn't be able to happen--all backwards seeks call here.
  assert(vf->position_writeback < vf->position);

  // Attempt write...
  src = vf->mf.start + vf->position_writeback;
  if(src >= vf->mf.end)
    return false;

  avail = vf->mf.end - src;
  count = vf->position - vf->position_writeback;
  avail = MIN(avail, count);
  out = fwrite(src, 1, avail, vf->fp);
  fflush(vf->fp);

  if(out < count)
    return false;

  return true;
}

/**
 * Begin a read or write operation on a VFS vfile.
 * This synchronizes the internal memfile and, if the VFS is no longer
 * available, attempts to fall back to FILE io.
 */
static inline boolean virt_read(vfile *vf)
{
  const unsigned char *tmp = NULL;
  size_t len = 0;
  enum vfs_error code = VFS_EINVAL;
  if(!vf->inode)
    return false;

  assert(vf->flags & VF_MEMORY);
  if(vfs_base)
    code = vfs_lock_file_read(vfs_base, vf->inode, &tmp, &len);
  if(code != 0)
  {
    vfile_cleanup_virtual(vf);
    return false;
  }
  mfopen(tmp, len, &(vf->mf));
  vf->mf.seek_past_end = true;
  mfseek(&(vf->mf), vf->position, SEEK_SET);
  virt_writeback(vf);
  return true;
}

static inline void virt_read_end(vfile *vf)
{
  if(!vfs_base || !vf->inode)
    return;

  assert(vf->flags & VF_MEMORY);
  vfs_unlock_file_read(vfs_base, vf->inode);
  vf->position = mftell(&(vf->mf));
  vf->position_writeback = vf->position;
}

static inline boolean virt_write(vfile *vf)
{
  enum vfs_error code = VFS_EINVAL;
  if(!vf->inode)
    return false;

  assert(vf->flags & VF_MEMORY);
  if(vfs_base)
  {
    code = vfs_lock_file_write(vfs_base, vf->inode,
     (unsigned char ***)&vf->external_buffer,
     &vf->v_length, &vf->external_buffer_size);
  }
  if(code != 0)
  {
    vfile_cleanup_virtual(vf);
    return false;
  }
  vf->local_buffer = *vf->external_buffer;
  vf->local_buffer_size = *vf->external_buffer_size;
  mfopen_wr(vf->local_buffer, *vf->v_length, &(vf->mf));
  vf->mf.seek_past_end = true;
  mfseek(&(vf->mf), vf->position, SEEK_SET);
  return true;
}

static inline void virt_write_end(vfile *vf)
{
  if(!vfs_base || !vf->inode)
    return;

  assert(vf->flags & VF_MEMORY);

  *(vf->v_length) = vf->mf.end - vf->mf.start;
  vfs_unlock_file_write(vfs_base, vf->inode);
  vf->position = mftell(&(vf->mf));
}

/**
 * Ensure an amount of space exists in a memory vfile or expand the vfile
 * (if possible). This should be used for writing only.
 */
static inline boolean vfile_ensure_space(size_t amount_to_write, vfile *vf)
{
  struct memfile *mf = &(vf->mf);
  size_t new_size;

  assert(vf->flags & VF_MEMORY);

  /* In append mode, all writes should occur at the end. */
  if(vf->flags & VF_APPEND)
    mf->current = mf->end;

  if(!mfhasspace(amount_to_write, mf))
  {
    new_size = (mf->start ? (mf->current - mf->start) : 0) + amount_to_write;
    if(new_size > vf->local_buffer_size)
    {
      size_t new_size_alloc = 32;
      void *t;
      int i;

      if(!(vf->flags & VF_MEMORY_EXPANDABLE))
        return false;

      for(i = 0; i < 64 && new_size_alloc < new_size; i++)
        new_size_alloc <<= 1;

      if(i >= 64)
        return false;

      t = realloc(vf->local_buffer, new_size_alloc);
      if(!t)
        return false;

      vf->local_buffer = t;
      vf->local_buffer_size = new_size_alloc;

      if(vf->external_buffer)
        *(vf->external_buffer) = t;
      if(vf->external_buffer_size)
        *(vf->external_buffer_size) = new_size_alloc;
    }
    mfmove(vf->local_buffer, new_size, mf);

    return (mf->end - mf->start) >= (ptrdiff_t)new_size;
  }
  return true;
}

/**
 * Get a direct memory access memfile of a given length starting at the current
 * position of a memory vfile. This should pretty much not be used, but some
 * things in the zip code already relied on it. If dest is NULL, a size check
 * (and potential expansion) will still be performed.
 */
boolean vfile_get_memfile_block(vfile *vf, size_t length, struct memfile *dest)
{
  assert(vf);
  assert(vf->flags & VF_MEMORY);
  if(vf->flags & VF_WRITE)
  {
    if(!vfile_ensure_space(length, vf))
      return false;
  }
  if(!mfhasspace(length, &(vf->mf)))
    return false;

  if(dest)
    mfopen_wr(vf->mf.current, length, dest);
  return true;
}

/**
 * Read a single byte from a file.
 */
int vfgetc(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    int character = EOF;
    if(vf->tmp_chr != EOF)
    {
      character = vf->tmp_chr;
      vf->tmp_chr = EOF;
    }
    else

    if(mfhasspace(1, &(vf->mf)))
      character = mfgetc(&(vf->mf));

    virt_read_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
    return fgetc(vf->fp);

  return EOF;
}

/**
 * Read two bytes from a file as an unsigned integer (little endian).
 */
int vfgetw(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    int character = EOF;
    if(vf->tmp_chr != EOF)
    {
      if(mfhasspace(1, &(vf->mf)))
      {
        character = vf->tmp_chr | (mfgetc(&(vf->mf)) << 8);
        vf->tmp_chr = EOF;
      }
    }
    else

    if(mfhasspace(2, &(vf->mf)))
      character = mfgetw(&(vf->mf));

    virt_read_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);

    return (a != EOF) && (b != EOF) ? ((b << 8) | a) : EOF;
  }

  return EOF;
}

/**
 * Read four bytes from a file as a signed integer (little endian).
 */
int vfgetd(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    int character = EOF;
    if(vf->tmp_chr != EOF)
    {
      if(mfhasspace(3, &(vf->mf)))
      {
        character = vf->tmp_chr |
         (mfgetc(&(vf->mf)) << 8) |
         (mfgetw(&(vf->mf)) << 16);
        vf->tmp_chr = EOF;
      }
    }
    else

    if(mfhasspace(4, &(vf->mf)))
      character = mfgetd(&(vf->mf));

    virt_read_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);
    int c = fgetc(fp);
    int d = fgetc(fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
      return EOF;

    return ((uint32_t)d << 24) | (c << 16) | (b << 8) | a;
  }

  return EOF;
}

/**
 * Read eight bytes from a file as a signed integer (little endian).
 */
int64_t vfgetq(vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    int64_t character = EOF;
    if(vf->tmp_chr != EOF)
    {
      if(mfhasspace(7, &(vf->mf)))
      {
        character = vf->tmp_chr |
         ((int64_t)mfgetc(&(vf->mf)) << 8) |
         ((int64_t)mfgetw(&(vf->mf)) << 16) |
         ((int64_t)mfgetud(&(vf->mf)) << 32);
        vf->tmp_chr = EOF;
      }
    }
    else

    if(mfhasspace(8, &(vf->mf)))
      character = mfgetq(&(vf->mf));

    virt_read_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fgetc(fp);
    int b = fgetc(fp);
    int c = fgetc(fp);
    int d = fgetc(fp);
    int e = fgetc(fp);
    int f = fgetc(fp);
    int g = fgetc(fp);
    int h = fgetc(fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF) ||
     (e == EOF) || (f == EOF) || (g == EOF) || (h == EOF))
      return EOF;

    return ((uint64_t)h << 56) | ((uint64_t)g << 48) | ((uint64_t)f << 40) |
     ((uint64_t)e << 32) | ((uint32_t)d << 24) | (c << 16) | (b << 8) | a;
  }

  return EOF;
}

/**
 * Write a single byte to a file.
 */
int vfputc(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    if(vfile_ensure_space(1, vf))
      character = mfputc(character, &(vf->mf));
    else
      character = EOF;

    virt_write_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
    return fputc(character, vf->fp);

  return EOF;
}

/**
 * Write an unsigned short integer to a file (little endian).
 */
int vfputw(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    if(vfile_ensure_space(2, vf))
      mfputw(character, &(vf->mf));
    else
      character = EOF;

    virt_write_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fputc(character & 0xFF, fp);
    int b = fputc((character >> 8) & 0xFF, fp);
    return (a != EOF) && (b != EOF) ? character : EOF;
  }

  return EOF;
}

/**
 * Write a signed long integer to a file (little endian).
 */
int vfputd(int character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    if(vfile_ensure_space(4, vf))
      mfputd(character, &(vf->mf));
    else
      character = EOF;

    virt_write_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fputc(character & 0xFF, fp);
    int b = fputc((character >> 8) & 0xFF, fp);
    int c = fputc((character >> 16) & 0xFF, fp);
    int d = fputc((character >> 24) & 0xFF, fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF))
      return EOF;

    return character;
  }

  return EOF;
}

/**
 * Write a signed 64-bit integer to a file (little endian).
 */
int64_t vfputq(int64_t character, vfile *vf)
{
  assert(vf);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    if(vfile_ensure_space(8, vf))
      mfputq(character, &(vf->mf));
    else
      character = EOF;

    virt_write_end(vf);
    return character;
  }

  if(vf->flags & VF_FILE)
  {
    FILE *fp = vf->fp;
    int a = fputc(character & 0xFF, fp);
    int b = fputc((character >> 8) & 0xFF, fp);
    int c = fputc((character >> 16) & 0xFF, fp);
    int d = fputc((character >> 24) & 0xFF, fp);
    int e = fputc((character >> 32) & 0xFF, fp);
    int f = fputc((character >> 40) & 0xFF, fp);
    int g = fputc((character >> 48) & 0xFF, fp);
    int h = fputc((character >> 56) & 0xFF, fp);

    if((a == EOF) || (b == EOF) || (c == EOF) || (d == EOF) ||
     (e == EOF) || (f == EOF) || (g == EOF) || (h == EOF))
      return EOF;

    return character;
  }

  return EOF;
}

/**
 * Read an array from a file.
 */
size_t vfread(void *dest, size_t size, size_t count, vfile *vf)
{
  assert(vf);
  assert(dest);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    if(size && count && vf->tmp_chr != EOF)
    {
      char *d = (char *)dest;
      *(d++) = vf->tmp_chr;
      if(size > 1)
      {
        if(!mfread(d, size - 1, 1, &(vf->mf)))
        {
          virt_read_end(vf);
          return 0;
        }

        d += size - 1;
      }
      vf->tmp_chr = EOF;
      if(count > 1)
        count = mfread(d, size, count - 1, &(vf->mf)) + 1;
    }
    else
      count = mfread(dest, size, count, &(vf->mf));

    virt_read_end(vf);
    return count;
  }

  if(vf->flags & VF_FILE)
    return fread(dest, size, count, vf->fp);

  return 0;
}

/**
 * Write an array to a file.
 */
size_t vfwrite(const void *src, size_t size, size_t count, vfile *vf)
{
  assert(vf);
  assert(src);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    if(vfile_ensure_space(size * count, vf))
      count = mfwrite(src, size, count, &(vf->mf));
    else
      count = 0;

    virt_write_end(vf);
    return count;
  }

  if(vf->flags & VF_FILE)
  {
#ifdef __DragonFly__
    /* DragonFly BSD ignored the C standard on this: */
    if(!size)
      return 0;
#endif
    return fwrite(src, size, count, vf->fp);
  }

  return 0;
}

/**
 * Read a line from a file, safely trimming platform-specific line end chars
 * as-needed.
 */
char *vfsafegets(char *dest, int size, vfile *vf)
{
  assert(vf);
  assert(dest);
  assert(size > 1);
  assert(vf->flags & VF_READ);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    if(size && vf->tmp_chr != EOF)
    {
      int tmp = vf->tmp_chr;
      vf->tmp_chr = EOF;
      if(tmp == '\r' || tmp == '\n')
      {
        // If this \r is part of a DOS line end, consume the corresponding \n.
        if(tmp == '\r' && (mfhasspace(1, &(vf->mf)) && vf->mf.current[0] == '\n'))
          vf->mf.current++;
        dest[0] = '\0';
      }
      else
      {
        dest[0] = tmp;
        dest[1] = '\0';
        mfsafegets(dest + 1, size - 1, &(vf->mf));
      }
    }
    else
      dest = mfsafegets(dest, size, &(vf->mf));

    virt_read_end(vf);
    return dest;
  }

  if(vf->flags & VF_FILE)
  {
    if(fgets(dest, size, vf->fp))
    {
      size_t len = strlen(dest);
      while(len >= 1 && (dest[len - 1] == '\r' || dest[len - 1] == '\n'))
      {
        len--;
        dest[len] = '\0';
      }
      return dest;
    }
    return NULL;
  }

  return NULL;
}

/**
 * Write a null-terminated string to a file. The null terminator will not be
 * written to the file.
 */
int vfputs(const char *src, vfile *vf)
{
  size_t len;
  int ret;

  assert(vf);
  assert(src);
  assert(vf->flags & VF_WRITE);

  len = strlen(src);
  if(!len)
    return 0;

  ret = vfwrite(src, len, 1, vf);
  return (ret == 1) ? 0 : EOF;
}

/**
 * Print a formatted string with a variable number of parameters to a file.
 * This function internally uses vfprintf and vsnprintf.
 */
int vf_printf(vfile *vf, const char *fmt, ...)
{
  va_list args;
  int ret;

  va_start(args, fmt);
  ret = vf_vprintf(vf, fmt, args);
  va_end(args);

  return ret;
}

/**
 * Print a formatted string with a variable number of parameters in a va_list
 * to a file. This function internally uses vfprintf and vsnprintf.
 */
int vf_vprintf(vfile *vf, const char *fmt, va_list args)
{
  assert(vf);
  assert(fmt);
  assert(vf->flags & VF_WRITE);

  if(virt_write(vf) || (vf->flags & VF_MEMORY))
  {
    // Get the expected output length from the format string and args.
    // This requires a duplicate arglist so vsnprintf can be called twice.
    // va_copy is supported by MSVC 2015+.
    va_list tmp_args;
    int length;
    char _buffer[512];
    void *buffer;

    va_copy(tmp_args, args);
    length = vsnprintf(NULL, 0, fmt, tmp_args);
    va_end(tmp_args);

    // Note: vsnprintf will fail if this is actually MSVC's broken _vsnprintf.
    // This shouldn't happen since only MinGW and MSVC 2015+ are supported.
    // Alternatively, some errors cause it to return -1.
    if(length < 0)
    {
      length = -1;
      goto err;
    }

    // Write to a temporary buffer to avoid null terminators overwriting data
    // and potential side effects of allocating extra vfile space. This is a
    // little slower but less messy than the alternative.
    if((size_t)length >= sizeof(_buffer))
    {
      buffer = malloc(length + 1);
      if(!buffer)
      {
        length = -1;
        goto err;
      }
    }
    else
      buffer = _buffer;

    length = vsnprintf((char *)buffer, length + 1, fmt, args);

    if(vfile_ensure_space(length, vf))
    {
      mfwrite(buffer, length, 1, &(vf->mf));
    }
    else
      length = -1;

    if(buffer != _buffer)
      free(buffer);

  err:
    virt_write_end(vf);
    return length;
  }

  if(vf->flags & VF_FILE)
    return vfprintf(vf->fp, fmt, args);

  return -1;
}

/**
 * Place a character back into the stream. This can be used once only for
 * memory streams and is only guaranteed to be usable once for files.
 * If chr is EOF or otherwise does not represent a valid char, this function
 * will fail and the stream will be unmodified.
 */
int vungetc(int chr, vfile *vf)
{
  assert(vf);

  // Note: MSVCRT &255s non-EOF values so this may not be 100% accurate to stdio.
  // If this somehow breaks something it can be reverted later.
  if(chr < 0 || chr >= 256)
    return EOF;

  if(vf->flags & VF_MEMORY)
  {
    if(vf->tmp_chr != EOF)
      return EOF;

    vf->tmp_chr = chr;
    return chr;
  }

  if(vf->flags & VF_FILE)
    return ungetc(chr, vf->fp);

  return EOF;
}

/**
 * Seek to a position in a file.
 */
int vfseek(vfile *vf, int64_t offset, int whence)
{
  assert(vf);
  vf->tmp_chr = EOF;

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    int ret = mfseek(&(vf->mf), offset, whence);
    virt_read_end(vf);
    return ret;
  }

  if(vf->flags & VF_FILE)
    return platform_fseek(vf->fp, offset, whence);

  return -1;
}

/**
 * Return the current position in a file.
 */
int64_t vftell(vfile *vf)
{
  assert(vf);

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    long res = mftell(&(vf->mf));
    /**
     * The number of buffered chars should be subtracted from the cursor for
     * binary mode files (in text mode the behavior is unspecified.). If the
     * cursor is at position 0 the return value of this operation is undefined.
     */
    if((vf->tmp_chr != EOF) && (vf->flags & VF_BINARY) && res > 0)
      res--;

    virt_read_end(vf);
    return res;
  }

  if(vf->flags & VF_FILE)
    return platform_ftell(vf->fp);

  return -1;
}

/**
 * Rewind the file to the start (and clear EOF/errors if applicable).
 */
void vrewind(vfile *vf)
{
  assert(vf);
  vf->tmp_chr = EOF;

  if(virt_read(vf) || (vf->flags & VF_MEMORY))
  {
    mfseek(&(vf->mf), 0, SEEK_SET);
    virt_read_end(vf);
    return;
  }

  if(vf->flags & VF_FILE)
  {
    rewind(vf->fp);
    return;
  }
}

/**
 * Return the length of the file and optionally rewind to the start of it.
 * If rewind is false, this function is guaranteed to either not modify the
 * current file position or to restore it to its position prior to calling this.
 * This function is not guaranteed to work correctly on streams that have been
 * written to or on memory write streams with fixed buffers, but is probably
 * safe to use for these cases.
 */
int64_t vfilelength(vfile *vf, boolean rewind)
{
  int64_t size = -1;

  assert(vf);

  if(vfs_base && vf->inode)
  {
    size = vfs_filelength(vfs_base, vf->inode);
  }

  if((vf->flags & VF_MEMORY) && size < 0)
  {
    size = vf->mf.end - vf->mf.start;
  }

  if((vf->flags & VF_FILE) && size < 0)
  {
    // fstat (and maybe _filelength) rely on the copy on-disk being up to date.
    // The SEEK_END hack works without this, since fseek also flushes the file.
    if(vf->flags & VF_WRITE)
      fflush(vf->fp);

    size = platform_filelength(vf->fp);
    if(size < 0)
    {
      int64_t current_pos = vftell(vf);

      vfseek(vf, 0, SEEK_END);
      size = vftell(vf);

      if(!rewind)
        vfseek(vf, current_pos, SEEK_SET);
    }
  }

  if(rewind)
    vrewind(vf);

  return size;
}


/************************************************************************
 * dirent function wrappers.
 */

struct vdir
{
  struct dir_handle dh;
  struct vfs_dir virt;
  long position;
  long num_total;
  long num_real;
  int flags;
  boolean has_real;
#ifdef PLATFORM_NO_REWINDDIR
  // Path for dirent implementations with no working rewinddir.
  char path[1];
#endif
};

/**
 * Open a directory for reading.
 *
 * @param path        path to directory to open.
 * @param skip_scan   if `true`, skip the inital directory read to count
 *                    files. This breaks the read, seek, and tell functions.
 * @return            `vdir` handle on success, otherwise NULL.
 */
vdir *vdir_open_ext(const char *path, int flags)
{
  char buffer[MAX_PATH];
  boolean virt_okay = false;
  vdir *dir;

#ifdef PLATFORM_NO_REWINDDIR

  ssize_t pathlen = -1;

  // Make path absolute so this directory can be located more reliably.
  if(vgetcwd(buffer, MAX_PATH))
  {
    pathlen = path_navigate_no_check(buffer, MAX_PATH, path);
    if(pathlen >= 0)
      path = buffer;
  }
  if(pathlen < 0)
    pathlen = strlen(path);

  dir = (vdir *)calloc(1, sizeof(vdir) + pathlen);
  if(dir)
    memcpy(dir->path, path, pathlen + 1);

#else
  dir = (vdir *)calloc(1, sizeof(vdir));

  // Path needs to be made absolute only for VFS operations.
  if(vfs_base)
    path = vio_normalize_virtual_path(vfs_base, buffer, MAX_PATH, path);
#endif

  if(dir)
  {
    flags &= VDIR_PUBLIC_MASK;
    dir->flags = flags;

    // There may be virtual files in this directory, or this directory
    // may also be virtual. Both real and virtual files need to be listed.
    // TODO: overlaid files should replace the real file somehow? ugh
    if(vfs_base && vfs_readdir(vfs_base, path, &dir->virt) == 0)
    {
      virt_okay = true;
      dir->num_total += dir->virt.num_files;
    }

    if(platform_opendir(&(dir->dh), path))
    {
      // Get total real files.
      dir->has_real = true;
      if(~flags & VDIR_NO_SCAN)
      {
        while(platform_readdir(dir->dh, NULL, 0, NULL))
          dir->num_real++;

        dir->num_total += dir->num_real;
        vdir_rewind(dir);
      }
    }
    else

    if(!virt_okay)
      goto err;

    return dir;
  }

err:
  free(dir);
  return NULL;
}

/**
 * Open a directory for reading.
 */
vdir *vdir_open(const char *path)
{
  return vdir_open_ext(path, false);
}

/**
 * Close a directory.
 */
int vdir_close(vdir *dir)
{
  int ret = 0;
  if(dir->has_real)
    ret = platform_closedir(dir->dh);

  vfs_readdir_free(&dir->virt);
  free(dir);
  return ret;
}

/**
 * Read the next file from a directory.
 * Returns `true` if a file was read, otherwise `false`.
 */
boolean vdir_read(vdir *dir, char *buffer, size_t len, enum vdir_type *type)
{
  int dirent_type = -1;

  if(dir->position >= dir->num_total && (~dir->flags & VDIR_NO_SCAN))
    return false;

  /* Return a virtual file. */
  if(dir->position >= dir->num_real && dir->position < dir->num_total)
  {
    size_t i = dir->position - dir->num_real;
    struct vfs_dir_file *f = dir->virt.files[i];
    size_t sz;
    // Shouldn't happen.
    if(!f)
      return false;

    if(buffer)
    {
      sz = snprintf(buffer, len, "%s", f->name);
      if(sz >= len)
        return false;
    }
    if(type)
      *type = (enum vdir_type)f->type;

    dir->position++;
    return true;
  }

  if(!dir->has_real)
    return false;

  if(!platform_readdir(dir->dh, buffer, len, &dirent_type))
    return false;

  if(type)
  {
    switch(dirent_type)
    {
  #ifdef DT_UNKNOWN
      case DT_REG:
        *type = DIR_TYPE_FILE;
        break;

      case DT_DIR:
        *type = DIR_TYPE_DIR;
        break;
  #endif

      default:
        *type = DIR_TYPE_UNKNOWN;
        break;
    }
  }

  dir->position++;
  return true;
}

/**
 * Seek to a position in a directory.
 */
boolean vdir_seek(vdir *dir, long position)
{
  if(position < 0)
    return false;

  // If position is before current position, rewind.
  if(position < dir->position)
  {
    if(!vdir_rewind(dir))
      return false;
  }

  // Skip files until the current position is the requested position.
  while(dir->position < position)
    if(!vdir_read(dir, NULL, 0, NULL))
      break;

  return true;
}

/**
 * Rewind the directory to the first file.
 * Some platforms don't have a rewinddir implementation and need special
 * handling, hence both vdir_open and vdir_seek call this instead of rewinddir.
 */
boolean vdir_rewind(vdir *dir)
{
  dir->position = 0;
  if(!dir->num_real)
    return true;

#ifdef PLATFORM_NO_REWINDDIR
  platform_closedir(dir->dh);

  // Virtual files are still valid, only the real dir needs to be reopened.
  if(!platform_opendir(&(dir->dh), dir->path))
  {
    dir->num_total -= dir->num_real;
    dir->num_real = 0;
    dir->flags &= ~VDIR_NO_SCAN;
    return false;
  }
  return true;
#else
  return platform_rewinddir(dir->dh);
#endif
}

/**
 * Report the current position in a directory.
 */
long vdir_tell(vdir *dir)
{
  return dir->position;
}

/**
 * Report the length of a directory.
 */
long vdir_length(vdir *dir)
{
  return dir->num_total;
}


/************************************************************************
 * Volume listing function wrappers.
 * For DOS/Amiga-style volumes, this will append ':' (but no slash).
 */

struct vvolumelist
{
  char **names;
  size_t num_total;
  size_t num_alloc;
  size_t pos;
};

static vvolumelist *vvolumelist_alloc(void)
{
  vvolumelist *volumes = (vvolumelist *)malloc(sizeof(vvolumelist));
  if(!volumes)
    return NULL;

  volumes->pos = 0;
  volumes->num_total = 0;
  volumes->num_alloc = 16;
  volumes->names = (char **)malloc(volumes->num_alloc * sizeof(char *));
  if(!volumes->names)
  {
    free(volumes);
    return NULL;
  }
  return volumes;
}

static boolean vvolumelist_append(vvolumelist *volumes,
 const char *name, size_t name_len)
{
  char *n;

  trace("--VIO-- vvolumelist_append: %s\n", name);

  if(volumes->num_total == volumes->num_alloc)
  {
    char **tmp;
    size_t new_num = volumes->num_alloc << 1u;

    if(new_num <= volumes->num_alloc)
      return false;

    tmp = (char **)realloc(volumes->names, new_num * sizeof(char *));
    if(!tmp)
      return false;

    volumes->names = tmp;
    volumes->num_alloc = new_num;
  }

  n = (char *)malloc(name_len + 1);
  if(!n)
    return false;

  memcpy(n, name, name_len);
  n[name_len] = '\0';
  volumes->names[volumes->num_total++] = n;
  return true;
}

static void vvolumelist_free(vvolumelist *volumes)
{
  size_t i;
  for(i = 0; i < volumes->num_total; i++)
    free(volumes->names[i]);

  free(volumes->names);
  free(volumes);
}

/**
 * Open the system volume list for reading. This function will read the volume
 * list in one go and allocate a copy to memory, as some platforms require
 * locking the volume list (Amiga) or temporarily changing the disk (DOS).
 * For most POSIX platforms, this list will consist of "/" and nothing else.
 *
 * @return an opaque volume list on success, otherwise NULL.
 */
vvolumelist *vvolumelist_open(void)
{
  struct vvolumelist_handle vh;
  vvolumelist *ret;
  int sz;
  char buf[256]; /* Amiga BCPL strings max at 255, other platforms are less. */

  trace("--VIO-- vvolumelist_open\n");

  if(!platform_vvolumelist_open(&vh))
  {
    trace("--VIO-- vvolumelist_open: failed to open platform volume list\n");
    return NULL;
  }

  ret = vvolumelist_alloc();
  if(!ret)
    goto err;

  while((sz = platform_vvolumelist_read(&vh, buf, sizeof(buf))))
    if((size_t)sz < sizeof(buf) && !vvolumelist_append(ret, buf, sz))
      goto err2;

  platform_vvolumelist_close(&vh);

  /* TODO: virtual roots */
  return ret;

err2:
  vvolumelist_free(ret);
err:
  platform_vvolumelist_close(&vh);
  return NULL;
}

/**
 * Release resources associated with an opened system volume list and free it.
 *
 * @param volumes   system volume list to destroy.
 * @return          0 on success.
 */
int vvolumelist_close(vvolumelist *volumes)
{
  vvolumelist_free(volumes);
  return 0;
}

/**
 * Get the next volume from an opened system volume list.
 *
 * @param volumes   system volume list to read.
 * @return          the name of the next volume, or NULL (end of list).
 */
const char *vvolumelist_read(vvolumelist *volumes)
{
  if(volumes->pos >= volumes->num_total)
    return NULL;

  return volumes->names[volumes->pos++];
}

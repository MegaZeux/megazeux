/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2012-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

// Must include after <time.h> (MSVC bug)
#include "util.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WIN32__
#include "io/vio_win32.h" // for windows.h, WIDE_PATHS, and utf16_to_utf8
#endif

#include "const.h" // for MAX_PATH
#include "error.h"
#include "io/path.h"
#include "io/vio.h"
#include "platform/platform.h"

struct mzx_resource
{
  const char *const base_name;
  char *path;

  /* So mzxrun requires fewer files even in CONFIG_EDITOR=1 build */
  const boolean editor_only;

  /* Optional resource--do not abort if not found, but print a warning. */
  const boolean optional;
};

/* Using C99 initializers would be nicer here, but MSVC doesn't support
 * them and we try to allow compilation with that compiler.
 *
 * As a result, these must be in exactly the same order as the
 * enum resource_id enumeration defines them.
 */
static struct mzx_resource mzx_res[] =
{
#define ASSETS "assets/"
#ifndef ICONFILE
#define ICONFILE NULL
#endif
  { "(binpath)",                        NULL, false, true },
  { CONFFILE,                           NULL, false, false },
  { ICONFILE,                           NULL, false, true },
  { ASSETS "default.chr",               NULL, false, false },
  { ASSETS "edit.chr",                  NULL, false, false },
  { ASSETS "smzx.pal",                  NULL, false, false },
#ifdef CONFIG_EDITOR
  { ASSETS "ascii.chr",                 NULL, true,  false },
  { ASSETS "blank.chr",                 NULL, true,  false },
  { ASSETS "smzx.chr",                  NULL, true,  false },
  { ASSETS "smzx2.chr",                 NULL, true,  false },
#endif
#ifdef CONFIG_HELPSYS
  { ASSETS "help.fil",                  NULL, false, true },
#endif
#ifdef CONFIG_RENDER_GL_PROGRAM
#define GLSL_SHADERS ASSETS "glsl/"
#define GLSL_SCALERS GLSL_SHADERS "scalers/"
  { GLSL_SCALERS,                       NULL, false, false },
  { GLSL_SHADERS "scaler.vert",         NULL, false, false },
  { GLSL_SCALERS "semisoft.frag",       NULL, false, false },
  { GLSL_SHADERS "tilemap.vert",        NULL, false, false },
  { GLSL_SHADERS "tilemap.frag",        NULL, false, false },
  { GLSL_SHADERS "tilemap.smzx.frag",   NULL, false, false },
  { GLSL_SHADERS "mouse.vert",          NULL, false, false },
  { GLSL_SHADERS "mouse.frag",          NULL, false, false },
  { GLSL_SHADERS "cursor.vert",         NULL, false, false },
  { GLSL_SHADERS "cursor.frag",         NULL, false, false },
#endif
#ifdef CONFIG_GAMECONTROLLERDB
  { ASSETS "gamecontrollerdb.txt",      NULL, false, true },
#endif
};

#ifdef CONFIG_CHECK_ALLOC

static platform_thread_id main_thread_id;

void check_alloc_init(void)
{
  main_thread_id = platform_get_thread_id();
}

static void out_of_memory_check(void *p, const char *file, int line)
{
  char msgbuf[128];
  if(!p)
  {
    platform_thread_id cur_thread_id = platform_get_thread_id();
    if(platform_is_same_thread(cur_thread_id, main_thread_id))
    {
      snprintf(msgbuf, sizeof(msgbuf), "Out of memory in %s:%d", file, line);
      msgbuf[sizeof(msgbuf)-1] = '\0';
      error(msgbuf, ERROR_T_FATAL, ERROR_OPT_EXIT|ERROR_OPT_NO_HELP, 0);
    }
    else
      warn("Out of memory in in %s:%d (thread %zu)\n",
       file, line, (size_t)cur_thread_id);
  }
}

// Attempt to free buffered memory from the virtual filesystem for reuse.
static boolean check_free_buffered(size_t amount, boolean *retry)
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

void *check_calloc(size_t nmemb, size_t size, const char *file, int line)
{
  void *result = calloc(nmemb, size);
  if(!result && nmemb * size >= size)
  {
    boolean retry = true;
    while(!result && check_free_buffered(nmemb * size, &retry))
      result = calloc(nmemb, size);
  }
  out_of_memory_check(result, file, line);
  return result;
}

void *check_malloc(size_t size, const char *file, int line)
{
  void *result = malloc(size);
  if(!result)
  {
    boolean retry = true;
    while(!result && check_free_buffered(size, &retry))
      result = malloc(size);
  }
  out_of_memory_check(result, file, line);
  return result;
}

void *check_realloc(void *ptr, size_t size, const char *file, int line)
{
  void *result = realloc(ptr, size);
  if(!result)
  {
    boolean retry = true;
    while(!result && check_free_buffered(size, &retry))
      result = realloc(ptr, size);
  }
  out_of_memory_check(result, file, line);
  return result;
}

#endif /* CONFIG_CHECK_ALLOC */

// Determine the executable dir.
static ssize_t find_executable_dir(char *dest, size_t dest_len, const char *argv0)
{
#ifdef __WIN32__
  {
    // Windows may not reliably give a full path in argv[0]. Fortunately,
    // there's a Windows solution for this.
    HMODULE module = GetModuleHandle(NULL);
    char filename[MAX_PATH];

#ifdef WIDE_PATHS
    wchar_t filename_wide[MAX_PATH];
    DWORD ret = GetModuleFileNameW(module, filename_wide, MAX_PATH);

    if(ret > 0 && ret < MAX_PATH)
    {
      if(!utf16_to_utf8(filename_wide, filename, MAX_PATH))
        ret = 0;
    }
#else /* !WIDE_PATHS */
    DWORD ret = GetModuleFileNameA(module, filename, MAX_PATH);
#endif

    if(ret > 0 && ret < MAX_PATH)
    {
      ssize_t len = path_get_directory(dest, dest_len, filename);
      if(len > 0)
        return len;
    }

    warn("--RES-- Failed to get executable path from Win32\n");
  }
#endif /* __WIN32__ */

  if(argv0)
  {
#ifdef CONFIG_PSP
    // FIXME: EBOOT.PBP may be treated as a directory, always get the parent.
    ssize_t len = path_get_parent(dest, dest_len, argv0);
#else
    ssize_t len = path_get_directory(dest, dest_len, argv0);
#endif
    if(len > 0)
    {
      // Change to the executable dir and get it as an absolute path.
      if(!vchdir(dest) && vgetcwd(dest, dest_len))
        return strlen(dest);
    }

    // Might be on PATH, don't print a warning.
    debug("--RES-- Failed to get executable path from argv[0]: %s\n", argv0);
  }
  else
    warn("--RES-- Failed to get executable path from argv[0]: (null)\n");

  dest[0] = 0;
  return 0;
}

int mzx_res_init(const char *argv0, boolean editor)
{
  int i;
  ssize_t bin_path_len;
  struct stat file_info;
  char *full_path;
  char *bin_path;
  char *p_dir;
  int ret = 0;

  bin_path = cmalloc(MAX_PATH);
  p_dir = cmalloc(MAX_PATH);

  bin_path_len = find_executable_dir(bin_path, MAX_PATH, argv0);
  if(bin_path_len > 0)
  {
    // Shrink the allocation...
    bin_path = crealloc(bin_path, bin_path_len + 1);
  }
  else
  {
    free(bin_path);
    bin_path = NULL;
  }

  /* Always try to use SHAREDIR/CONFDIR first, if possible. On some
   * platforms, such as Linux, this will be something other than
   * the current working directory (i.e. '.'). If this fails, try
   * to look up the resources relative to the binary's install
   * location, which should allow the MegaZeux binary to be more
   * "portable".
   */
  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    size_t base_name_len;
    size_t full_path_len;
    size_t p_dir_len;

    if(i == MZX_EXECUTABLE_DIR)
    {
      // Special--this was already determined above if applicable.
      mzx_res[i].path = bin_path;
      continue;
    }

    if(!mzx_res[i].base_name)
      continue;

    base_name_len = strlen(mzx_res[i].base_name);

    // Path may be an absolute path.
    if(mzx_res[i].base_name[0] == '/')
    {
      full_path_len = base_name_len + 1;
      full_path = cmalloc(full_path_len);

      if(path_clean_copy(full_path, full_path_len,
       mzx_res[i].base_name) > 0)
      {
        if(!vstat(full_path, &file_info))
        {
          mzx_res[i].path = full_path;
          continue;
        }
      }
      // Do not attempt to load absolute files relatively.
      free(full_path);
      continue;
    }

    if(i == CONFIG_TXT)
      vchdir(CONFDIR);
    else
      vchdir(SHAREDIR);

    vgetcwd(p_dir, MAX_PATH);
    p_dir_len = strlen(p_dir);

    full_path_len = p_dir_len + base_name_len + 2;
    full_path = cmalloc(full_path_len);

    // The provided buffer should always be big enough, but check anyway.
    if(path_join(full_path, full_path_len, p_dir, mzx_res[i].base_name) > 0)
    {
      if(!vstat(full_path, &file_info))
      {
        mzx_res[i].path = full_path;
        continue;
      }
    }

    free(full_path);

    // Try to locate the resource relative to the bin_path
    if(bin_path)
    {
      vchdir(bin_path);
      if(!vstat(mzx_res[i].base_name, &file_info))
      {
        full_path_len = bin_path_len + base_name_len + 2;
        full_path = cmalloc(full_path_len);

        // The provided buffer should always be big enough, but check anyway.
        if(path_join(full_path, full_path_len, bin_path, mzx_res[i].base_name) > 0)
        {
          mzx_res[i].path = full_path;
          continue;
        }
        free(full_path);
      }
    }
  }

  for(i = 0; i < END_RESOURCE_ID_T; i++)
  {
    /* Skip editor resources if this isn't the editor. */
    if(!editor && mzx_res[i].editor_only)
      continue;

    if(!mzx_res[i].path)
    {
      // Finding this fails from installed builds in Linux, just ignore.
      if(i == MZX_EXECUTABLE_DIR)
        continue;

      if(mzx_res[i].optional)
      {
        if(!mzx_res[i].base_name)
        {
          trace("--RES-- %d -> NULL\n", i);
          continue;
        }
        warn("Failed to locate non-critical resource (%d) '%s'\n",
         i, mzx_res[i].base_name);
        continue;
      }

      warn("Failed to locate critical resource (%d) '%s'.\n",
       i, mzx_res[i].base_name);
      ret = 1;
    }
    else
      trace("--RES-- %d -> %s\n", i, mzx_res[i].path);
  }

  free(p_dir);
  return ret;
}

void mzx_res_free(void)
{
  int i;
  for(i = 0; i < END_RESOURCE_ID_T; i++)
    if(mzx_res[i].path)
      free(mzx_res[i].path);
}

#ifdef USERCONFFILE
#define COPY_BUFFER_SIZE  4096
static unsigned char copy_buffer[COPY_BUFFER_SIZE];
#endif

const char *mzx_res_get_by_id(enum resource_id id)
{
#ifdef USERCONFFILE
  static char userconfpath[MAX_PATH];
  if(id == CONFIG_TXT)
  {
    vfile *vf;

    // Special handling for CONFIG_TXT to allow for user
    // configuration files
#ifdef CONFIG_PSVITA
    snprintf(userconfpath, MAX_PATH, "%s", USERCONFFILE);
#else
    snprintf(userconfpath, MAX_PATH, "%s/%s", getenv("HOME"), USERCONFFILE);
#endif

    // Check if the file can be opened for reading
    vf = vfopen_unsafe(userconfpath, "rb");

    if(vf)
    {
      vfclose(vf);
      return (char *)userconfpath;
    }
    // Otherwise, try to open the file for writing
    vf = vfopen_unsafe(userconfpath, "wb");
    if(vf)
    {
      vfile *original = vfopen_unsafe(mzx_res[id].path, "rb");
      if(original)
      {
        size_t bytes_read;
        for(;;)
        {
          bytes_read = vfread(copy_buffer, 1, COPY_BUFFER_SIZE, original);
          if(bytes_read)
            vfwrite(copy_buffer, 1, bytes_read, vf);
          else
            break;
        }
        vfclose(vf);
        vfclose(original);
        return (char *)userconfpath;
      }
      vfclose(vf);
    }

    // If that's no good, just read the normal config file
  }
#endif /* USERCONFFILE */
  return mzx_res[id].path;
}

#ifdef CONFIG_STDIO_REDIRECT

FILE *mzxout_h = NULL;
FILE *mzxerr_h = NULL;

/**
 * Some platforms may not be able to display console output without extra work.
 * On these platforms, open stdout/stderr replacement files instead. The log
 * macros and any other references to `mzxout` and `mzxerr` will use these
 * files instead of stdio.
 *
 * Previously, this was implemented using `freopen` on stdout/stderr, but
 * doing this in some console SDKs does not work correctly (NDS, PS Vita).
 */
boolean redirect_stdio_init(const char *base_path, boolean require_conf)
{
  char dest_path[MAX_PATH];
  size_t dest_len;
  FILE *fp_wr;
  uint64_t t;

  if(!base_path)
    return false;

  dest_len = path_clean_copy(dest_path, MAX_PATH - 10, base_path);

  if(require_conf)
  {
    // If the config file is required, attempt to stat it.
    struct stat stat_info;

    path_append(dest_path, MAX_PATH, "config.txt");
    if(vstat(dest_path, &stat_info))
      return false;
    dest_path[dest_len] = '\0';
  }

  // Clean up existing handles from a previous attempt.
  redirect_stdio_exit();

  // Test directory for write access.
  path_append(dest_path, MAX_PATH, "stdout.txt");
  fp_wr = fopen_unsafe(dest_path, "w");
  if(!fp_wr)
  {
    fprintf(stdout, "Failed to redirect stdout\n");
    fflush(stdout);
    return false;
  }

  t = (uint64_t)time(NULL);

  // Redirect mzxout to stdout.txt.
  fprintf(stdout, "Redirecting logs to '%s'...\n", dest_path);
  fflush(stdout);
  mzxout_h = fp_wr;
  fprintf(mzxout, "MegaZeux: Logging to '%s' (%" PRIu64 ")\n", dest_path, t);
  fflush(mzxout);

  // Redirect mzxerr to stderr.txt.
  dest_path[dest_len] = '\0';
  path_append(dest_path, MAX_PATH, "stderr.txt");
  fp_wr = fopen_unsafe(dest_path, "w");
  if(!fp_wr)
  {
    fprintf(stderr, "Failed to redirect stderr\n");
    fflush(stderr);
    return false;
  }

  fprintf(stderr, "Redirecting logs to '%s'...\n", dest_path);
  fflush(stderr);
  mzxerr_h = fp_wr;
  fprintf(mzxerr, "MegaZeux: Logging to '%s' (%" PRIu64 ")\n", dest_path, t);
  fflush(mzxerr);

  return true;
}

void redirect_stdio_exit(void)
{
  if(mzxout_h)
  {
    fclose(mzxout_h);
    mzxout_h = NULL;
  }

  if(mzxerr_h)
  {
    fclose(mzxerr_h);
    mzxerr_h = NULL;
  }
}

#endif /* CONFIG_STDIO_REDIRECT */

// Random function, returns an integer [0-range)

static uint64_t rng_state;

// Seed the RNG from system time on startup
void rng_seed_init(void)
{
  uint64_t seed = (((uint64_t)time(NULL)) << 32) | clock();
  rng_set_seed(seed);
}

uint64_t rng_get_seed(void)
{
  return rng_state;
}

void rng_set_seed(uint64_t seed)
{
  rng_state = seed;
}

// xorshift*
// Implementation from https://en.wikipedia.org/wiki/Xorshift
unsigned int Random(uint64_t range)
{
  uint64_t x = rng_state;
  if(x == 0) x = 1;
  x ^= x >> 12; // a
  x ^= x << 25; // b
  x ^= x >> 27; // c
  rng_state = x;
  return (((x * 0x2545F4914F6CDD1D) >> 32) * range) >> 32;
}

#if defined(__WIN32__) && defined(__STRICT_ANSI__)

/* On WIN32 with C99 defining __STRICT_ANSI__ these POSIX.1-2001 functions
 * are not available. The stricmp/strnicmp functions are available, but not
 * to C99 programs.
 *
 * Redefine them here; these are copied from glibc, and should be optimal.
 */

int strcasecmp(const char *s1, const char *s2)
{
  while(*s1 != '\0' && tolower(*s1) == tolower(*s2))
  {
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  if(n == 0)
    return 0;

  while(n-- != 0 && tolower(*s1) == tolower(*s2))
  {
    if(n == 0 || *s1 == '\0' || *s2 == '\0')
      break;
    s1++;
    s2++;
  }

  return tolower(*(unsigned char *)s1) - tolower(*(unsigned char *)s2);
}

#endif // __WIN32__ && __STRICT_ANSI__

#if defined(__WIN32__) || defined(__amigaos__)

// strsep() stolen from glibc (GPL)

char *strsep(char **stringp, const char *delim)
{
  char *begin, *end;

  begin = *stringp;
  if(!begin)
    return NULL;

  if(delim[0] == '\0' || delim[1] == '\0')
  {
    char ch = delim[0];
    if(ch == '\0')
      end = NULL;
    else
    {
      if(*begin == ch)
        end = begin;
      else if(*begin == '\0')
        end = NULL;
      else
        end = strchr(begin + 1, ch);
    }
  }
  else
    end = strpbrk(begin, delim);

  if(end)
  {
    *end++ = '\0';
    *stringp = end;
  }
  else
    *stringp = NULL;

  return begin;
}

#endif // __WIN32__ || __amigaos__

#if defined(__amigaos__)

long __stack_chk_guard[8];

void __stack_chk_fail(void)
{
  warn("Stack overflow detected; terminated");
  exit(0);
}

#endif // __amigaos__

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

#include <inttypes.h>
#include <time.h>

#include "log.h"
#include "../io/path.h"
#include "../io/vio.h"

#ifdef CONFIG_STDIO_REDIRECT
FILE *mzxout_h = NULL;
FILE *mzxerr_h = NULL;
#endif

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
#ifdef CONFIG_STDIO_REDIRECT
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
#else
  return false;
#endif
}

void redirect_stdio_exit(void)
{
#ifdef CONFIG_STDIO_REDIRECT
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
#endif
}

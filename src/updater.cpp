/* MegaZeux
 *
 * Copyright (C) 2008 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2020-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "caption.h"
#include "const.h"
#include "core.h"
#include "error.h"
#include "graphics.h"
#include "helpsys.h"
#include "updater.h"
#include "util.h"
#include "window.h"
#include "io/memfile.h"
#include "io/path.h"
#include "io/vio.h"

#include "editor/window.h"
#include "event/event.h"
#include "platform/platform.h"

#include "network/HTTPHost.hpp"
#include "network/Manifest.hpp"
#include "network/Scoped.hpp"

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifndef PLATFORM
#error Must define a valid "friendly" platform name!
#endif

#define MAX_RETRIES   3

#define OUTBOUND_PORT 80
#define LINE_BUF_LEN  256

#define UPDATES_TXT   "updates.txt"
#define DELETE_TXT    "delete.txt"

#define UPDATES_TXT_MAX_SIZE ((size_t)1 << 20)

#define WIDGET_BUF_LEN 80

static char widget_buf[WIDGET_BUF_LEN];

static char previous_dir[MAX_PATH];

static long final_size = -1;
static boolean cancel_update;

/**
 * Functions and enum for updater startup error codes (namespace because no
 * enum classes in C++98...).
 */
namespace UpdaterInit
{
  enum StatusValue
  {
    SUCCESS,
    UNINITIALIZED,
    INCOMPATIBLE_ASSETS,
    INCOMPATIBLE_CONFDIR,
    FAILED_CHDIR_TO_EXEC_DIR,
    FAILED_TO_STAT_MANIFEST,
    FAILED_FILE_WRITE,
    FAILED_FILE_UNLINK
  };
  static StatusValue status = UpdaterInit::UNINITIALIZED;

  static boolean allow_full_updates()
  {
    return status == SUCCESS || status == FAILED_TO_STAT_MANIFEST;
  }

  static const char *status_string(StatusValue st)
  {
    switch(st)
    {
      case SUCCESS:
        return "updater initialization successful";
      case UNINITIALIZED:
        return "updater hasn't been initialized";
      case INCOMPATIBLE_ASSETS:
      case INCOMPATIBLE_CONFDIR:
        return "platform not configured for full updates";
      case FAILED_CHDIR_TO_EXEC_DIR:
        return "failed to chdir to executable dir";
      case FAILED_TO_STAT_MANIFEST:
        return "failed to stat manifest.txt";
      case FAILED_FILE_WRITE:
        return "failed to write to executable dir";
      case FAILED_FILE_UNLINK:
        return "failed to delete file from executable dir";
    }
    return "unknown error";
  }

#ifdef IS_CXX_11
  static constexpr int const_strcmp(const char *a, const char *b)
  {
    return (a[0] == b[0] && (a[0] == '\0' || const_strcmp(a + 1, b + 1))) ? 0 : -1;
  }
#else
#define const_strcmp(a,b) strcmp(a,b)
#endif

  /**
   * Compile time checks for the paranoid to disable the parts of the updater
   * that can delete things on UNIX-like platforms and Mac OS X.
   */
  static maybe_constexpr StatusValue _const_safety_checks()
  {
    /**
     * The config file should exist in the executable dir.
     * If it doesn't, this is probably a UNIX-like platform or Mac OS X.
     * TODO other checks may be possible here e.g. make sure SHAREDIR is
     * prefixed by GAMEDIR--requires GAMEDIR being added to config.h.
     */
    return const_strcmp(CONFDIR, "./") ? INCOMPATIBLE_CONFDIR : SUCCESS;
  }
  static maybe_constexpr const StatusValue const_safety_checks = _const_safety_checks();
}

enum new_version_opts
{
  OPT_EXIT,
  OPT_TRY_NEXT_HOST,
  OPT_UPDATE_CURRENT
};

/**
 * A new version has been released and there are updates available on the
 * current host for this platform. Prompt the user to either update to the
 * new version or attempt to update the current version. Returns the version
 * selected by the user, or NULL if canceled by the user.
 */
static const char *ui_new_version_available(context *ctx,
 const char *current_ver, const char *new_ver)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[6];
  struct dialog di;
  int result;

  int buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "A new version is available (%s)", new_ver);
  widget_buf[WIDGET_BUF_LEN - 1] = 0;
  buf_len = MAX(0, buf_len);

  elements[0] = construct_label((55 - buf_len) >> 1, 2, widget_buf);

  elements[1] = construct_label(2, 4,
   "You can continue to receive updates for the version\n"
   "installed (if available), or you can upgrade to the\n"
   "newest version (recommended).");

  elements[2] = construct_label(2, 8,
   "If you do not upgrade, this question will be asked\n"
   "again the next time you run the updater.\n");

  elements[3] = construct_button(9, 11, "Upgrade", 0);
  elements[4] = construct_button(21, 11, "Update Old", 1);
  elements[5] = construct_button(36, 11, "Cancel", 2);

  construct_dialog(&di, "New Version", 12, 6, 55, 14, elements, 6, 3);
  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // User pressed Escape, abort all updates
  if(result < 0 || result == 2)
    return NULL;

  // User pressed Upgrade, use new version.
  if(result == 0)
    return new_ver;

  // Check for updates on the current version.
  return current_ver;
}

/**
 * A new version has been released, but either there are no updates available
 * on the current host or something went wrong in initializing the updater.
 * Either way, the update can't be performed, so explain why and possibly
 * offer some other useful options.
 */
static enum new_version_opts ui_new_version_error(context *ctx,
 const char *new_ver, boolean platform_has_remote_manifest)
{
  char reason[LINE_BUF_LEN];
  struct world *mzx_world = ctx->world;
  struct element *elements[7];
  struct dialog di;
  int result;
  int num_elements = ARRAY_SIZE(elements) - 2;

  int buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "A new version is available (%s)", new_ver);
  widget_buf[WIDGET_BUF_LEN - 1] = '\0';
  buf_len = MAX(0, buf_len);

  elements[0] = construct_label((55 - buf_len) >> 1, 2, widget_buf);

  elements[1] = construct_label(2, 4,
   "This update can not be downloaded from the remote\n"
   "host for the following reason(s):");

  int reason_lines = 0;
  buf_len = 0;
  if(UpdaterInit::const_safety_checks != UpdaterInit::SUCCESS)
  {
    int res = snprintf(reason + buf_len, LINE_BUF_LEN - buf_len,
     "* This platform does not support updates.\n");
    buf_len += MAX(0, res);
    reason_lines += 1;
  }
  else
  {
    if(!platform_has_remote_manifest)
    {
      int res = snprintf(reason + buf_len, LINE_BUF_LEN - buf_len,
       "* The remote host does not have updates for your\n"
       "  platform (%s).\n", PLATFORM);
      buf_len += MAX(0, res);
      reason_lines += 2;
    }

    if(!UpdaterInit::allow_full_updates())
    {
      int res = snprintf(reason + buf_len, LINE_BUF_LEN - buf_len,
       "* The updater failed to initialize on startup:\n"
       "  %s.\n", UpdaterInit::status_string(UpdaterInit::status));
      buf_len += MAX(0, res);
      reason_lines += 2;
    }
  }
  elements[2] = construct_label(2, 7, reason);

  elements[3] = construct_label(2, 8 + reason_lines,
   "Check digitalmzx.com or github.com/AliceLR/megazeux\n"
   "to download the latest release for your platform.");

  if(UpdaterInit::allow_full_updates())
  {
    elements[4] = construct_button(9, 11 + reason_lines, "OK", OPT_EXIT);
    elements[5] = construct_button(16, 11 + reason_lines, "Update Old",
     OPT_UPDATE_CURRENT);
    elements[6] = construct_button(31, 11 + reason_lines, "Try next host",
     OPT_TRY_NEXT_HOST);

    num_elements += 2;
  }
  else
    elements[4] = construct_button(25, 11 + reason_lines, "OK", OPT_EXIT);

  int h = 12 + reason_lines + 2;
  int y = (25 - h) / 2;

  construct_dialog(&di, "New Version", 12, y, 55, h, elements, num_elements, 4);
  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  // User pressed "Update Old"; attempt to update the current version.
  if(result == OPT_UPDATE_CURRENT)
    return OPT_UPDATE_CURRENT;

  // User pressed "Try next host"
  if(result == OPT_TRY_NEXT_HOST)
    return OPT_TRY_NEXT_HOST;

  // User pressed escape or OK; abort.
  return OPT_EXIT;
}

/**
 * No changes have been detected between the local manifest and the remote
 * manifest. Prompt the user to either try the next host or to abort. Return
 * true if the user selected to try the next host, otherwise false.
 */
static boolean ui_version_is_current(context *ctx, boolean has_next_host)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[3];
  struct dialog di;
  int result;

  elements[0] = construct_label(2, 2, "This client is already current.");
  elements[1] = construct_button(7, 4, "OK", 0);
  elements[2] = construct_button(13, 4, "Try next host", 1);

  construct_dialog(&di, "No Updates", 22, 9, 35, 6, elements, 3, 1);
  result = run_dialog(mzx_world, &di);
  destruct_dialog(&di);

  if((result == 1) && has_next_host)
    return true;

  return false;
}

/**
 * Show the user the list of changes to be applied. Return the number of
 * changes to be applied (or 0 if canceled).
 */
static int ui_confirm_changes(context *ctx, const Manifest &removed,
 const Manifest &replaced, const Manifest &added)
{
  const ManifestEntry *e;
  char **list_entries;
  int list_entry_width = 0;
  int entries = 0;
  int result;
  int i = 0;

  for(e = removed.first(); e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);
  for(e = replaced.first(); e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);
  for(e = added.first(); e; e = e->next, entries++)
    list_entry_width = MAX(list_entry_width, 2 + (int)strlen(e->name)+1+1);

  // We don't want the listbox to be too wide
  list_entry_width = MIN(list_entry_width, 60);

  list_entries = (char **)cmalloc(entries * sizeof(char *));

  for(e = removed.first(); e; e = e->next, i++)
  {
    list_entries[i] = (char *)cmalloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "- %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  for(e = replaced.first(); e; e = e->next, i++)
  {
    list_entries[i] = (char *)cmalloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "* %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  for(e = added.first(); e; e = e->next, i++)
  {
    list_entries[i] = (char *)cmalloc(list_entry_width);
    snprintf(list_entries[i], list_entry_width, "+ %s", e->name);
    list_entries[i][list_entry_width - 1] = 0;
  }

  draw_window_box(19, 1, 59, 4, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(" Task Summary ", 33, 1, DI_TITLE, 0);
  write_string("ESC   - Cancel   [+] Add   [-] Delete", 21, 2, DI_TEXT, 0);
  write_string("ENTER - Proceed  [*] Replace  ", 21, 3, DI_TEXT, 0);

  result = list_menu((const char **)list_entries, list_entry_width,
   NULL, 0, entries, ((80 - (list_entry_width + 9)) >> 1) + 1, 4);

  for(i = 0; i < entries; i++)
    free(list_entries[i]);
  free(list_entries);

  clear_screen();
  update_screen();

  if(result >= 0)
    return entries;

  return 0;
}

/**
 * Inform the user that the update process is complete.
 */
static void ui_update_finished(context *ctx)
{
  struct world *mzx_world = ctx->world;
  struct element *elements[2];
  struct dialog di;

  elements[0] = construct_label(2, 2,
   "This client will now attempt to restart itself.");
  elements[1] = construct_button(23, 4, "OK", 0);

  construct_dialog(&di, "Update Successful", 14, 9, 51, 6, elements, 2, 1);
  run_dialog(mzx_world, &di);
  destruct_dialog(&di);
}

/**
 * Clear the screen.
 */
static void display_clear(void)
{
  clear_screen();
  update_screen();
}

/**
 * Indicate that the network is currently being initialized.
 */
static void display_initializing()
{
  static const char str[] = "Initializing network...";
  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(str, (WIDGET_BUF_LEN - strlen(str)) / 2, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate that the client is currently connecting to a remote host.
 */
static void display_connecting(const char *host_name)
{
  size_t buf_len;

  buf_len = snprintf(widget_buf, WIDGET_BUF_LEN,
   "Connecting to \"%s\". Please wait..", host_name);
  widget_buf[WIDGET_BUF_LEN - 1] = 0;

  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(widget_buf, (WIDGET_BUF_LEN - buf_len) >> 1, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate that MZX is currently trying to determine if a remote manifest
 * exists for this platform.
 */
static void display_manifest_check()
{
  static const char str[] = "Checking for remote " MANIFEST_TXT "..";
  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(str, (WIDGET_BUF_LEN - strlen(str)) / 2, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate that the manifest is currently being processed.
 */
static void display_manifest(void)
{
  static const char *str = "Fetching remote " MANIFEST_TXT "..";
  m_hide();
  draw_window_box(3, 11, 76, 13, DI_MAIN, DI_DARK, DI_CORNER, 1, 1);
  write_string(str, (WIDGET_BUF_LEN - strlen(str)) / 2, 12, DI_TEXT, 0);
  update_screen();
  m_show();
}

/**
 * Indicate the current download.
 */
static void display_download_init(const char *filename, int cur, int total)
{
  char name[72];

  m_hide();
  snprintf(name, 72, "%s (%ldb) [%u/%u]", filename, final_size, cur, total);
  meter(name, 0, final_size);
  update_screen();
  m_show();
}

/**
 * Update the download progress bar.
 */
static void display_download_update(long progress)
{
  m_hide();
  meter_interior(progress, final_size);
  update_screen();
  m_show();
}

static boolean check_prune_basedir(const char *file)
{
  char path[MAX_PATH];
  ssize_t ret;

  ret = path_get_directory(path, MAX_PATH, file);
  if(ret < 0)
  {
    error_message(E_UPDATE, 0, "Failed to prune directories (path too long)");
    return false;
  }

  // This file has no base directory
  if(ret == 0)
    return true;

  // Attempt to remove the directory.
  while(!vrmdir(path))
  {
    ssize_t len = strlen(path);
    info("--UPDATER-- Pruned empty directory '%s'\n", path);

    // If that worked, also try to remove the parent directory recursively.
    ssize_t ret = path_navigate(path, MAX_PATH, PATH_PARENT_DIR);
    if(ret < 0 || ret >= len)
      break;
  }
  return true;
}

static boolean check_create_basedir(const char *file)
{
  enum path_create_error ret = path_create_parent_recursively(file);
  if(ret)
    warn("Failed to mkdir() parent directory of '%s'\n", file);

  switch(ret)
  {
    case PATH_CREATE_SUCCESS:
      return true;

    case PATH_CREATE_ERR_BUFFER:
      error_message(E_UPDATE, 1, "Failed to mkdir(); path is too long");
      break;

    case PATH_CREATE_ERR_STAT_ERROR:
      error_message(E_UPDATE, 1, "Failed to mkdir(); unknown stat error occurred");
      break;

    case PATH_CREATE_ERR_MKDIR_FAILED:
      error_message(E_UPDATE, 1, "Failed to mkdir(); check permissions");
      break;

    case PATH_CREATE_ERR_FILE_EXISTS:
    {
      error_message(E_UPDATE, 1,
       "Failed to mkdir(); file exists with the specified name");
      break;
    }
  }
  return false;
}

static void check_cancel_update(void)
{
  update_event_status();
  if(get_key(keycode_internal) == IKEY_ESCAPE
   || get_exit_status())
    cancel_update = true;
}

static void recv_cb(long offset)
{
  check_cancel_update();

  if(final_size > 0 && offset > final_size)
  {
    error_message(E_UPDATE, 4,
     "Transferred more than expected uncompressed size.");
    cancel_update = true;
    return;
  }

  display_download_update(offset);
}

static boolean cancel_cb(void)
{
  return cancel_update;
}

static boolean swivel_current_dir(boolean have_video)
{
  const char *executable_dir = mzx_res_get_by_id(MZX_EXECUTABLE_DIR);
  char base_path[MAX_PATH];

  if(!executable_dir)
  {
    if(have_video)
      error_message(E_UPDATE, 25,
       "Updater: couldn't determine install directory.");
    else
      warn("--UPDATER-- Couldn't determine install directory.\n");
    return false;
  }

  // Store the user's current directory, so we can get back to it
  vgetcwd(previous_dir, MAX_PATH);

  if(vchdir(executable_dir))
  {
    info("--UPDATER-- getcwd(): %s\n", previous_dir);
    info("--UPDATER-- attempted chdir() to: %s\n", base_path);
    if(have_video)
      error_message(E_UPDATE, 5,
       "Updater: failed to change into install directory.");
    else
      warn("--UPDATER-- Failed to change into install directory.\n");
    return false;
  }
  trace("--UPDATER-- chdir() to exec dir successful: %s\n", executable_dir);
  return true;
}

static boolean swivel_current_dir_back(boolean have_video)
{
  if(vchdir(previous_dir))
  {
    if(have_video)
      error_message(E_UPDATE, 6,
       "Updater: failed to change back to user directory.");
    else
      warn("--UPDATER-- Failed to change back to user directory.\n");
    return false;
  }
  trace("--UPDATER-- chdir() to user dir successful: %s\n", previous_dir);
  return true;
}

/**
 * Replace the old manifest with the new manifest.
 * This should only be called if the update was successful.
 */
static boolean replace_manifest(void)
{
  struct stat s;

  // The new manifest should exist...
  if(vstat(REMOTE_MANIFEST_TXT, &s))
    return false;

  // Remove the old manifest.
  if(vunlink(LOCAL_MANIFEST_TXT))
  {
    error_message(E_UPDATE, 7,
     "Failed to remove " LOCAL_MANIFEST_TXT ". Check permissions.");
    return false;
  }

  // Rename the new manifest.
  if(vrename(REMOTE_MANIFEST_TXT, LOCAL_MANIFEST_TXT))
  {
    error_message(E_UPDATE, 8,
     "Failed to rename " REMOTE_MANIFEST_TXT ". Check permissions.");
    return false;
  }

  return true;
}

static boolean write_delete_list(const Manifest &delete_list)
{
  if(!delete_list.write_to_file(DELETE_TXT))
  {
    error_message(E_UPDATE, 9,
     "Failed to create \"" DELETE_TXT "\". Check permissions.");
    return false;
  }
  return true;
}

static void apply_delete_list(Manifest &delete_list)
{
  ManifestEntry *e_next = delete_list.first();
  ManifestEntry *e_prev = NULL;
  ManifestEntry *e;
  int retry_times = 0;
  struct stat s;
  boolean files_failed = false;

  while(e_next)
  {
    e = e_next;
    e_next = e->next;

    if(!vstat(e->name, &s))
    {
      if(e->validate())
      {
        if(vunlink(e->name))
          goto err_delete_failed;

        info("--UPDATER-- Deleted '%s'\n", e->name);
        // Also try to delete the base directory. If it's empty, this won't
        // do anything.
        check_prune_basedir(e->name);
      }
      else
        info("--UPDATER-- Skipping invalid entry '%s'\n", e->name);
    }

    // File was removed, doesn't exist, or is non-applicable; remove from list
    if(delete_list.head == e)
      delete_list.head = e_next;

    // Keep the link on the last failed file up-to-date.
    if(e_prev)
      e_prev->next = e_next;

    delete e;
    continue;

err_delete_failed:
    {
      int errval;

      warn("--UPDATER-- Failed to delete '%s'\n", e->name);

      switch(retry_times)
      {
        case 0:
        {
          if(!strcmp(e->name, "mzx_help.fil") ||
           !strcmp(e->name, "assets/help.fil"))
          {
            // HACK: Older MZX versions do not properly close these files
            // because that would have been too easy. Silently skip them for
            // now; they'll be deleted the next time MZX is started.
            errval = ERROR_OPT_FAIL;
            break;
          }

          // 1st failure: delay a little bit and then retry automatically.
          delay(200);
          errval = ERROR_OPT_RETRY;
          break;
        }

        case 1:
        {
          // 2nd failure: give user the option to either retry or fail.
          char buf[72];
          snprintf(buf, 72, "Failed to delete \"%.30s\". Retry?", e->name);
          buf[71] = 0;

          errval = error_message(E_UPDATE_RETRY, 10, buf);
          break;
        }

        default:
        {
          // 3rd failure: auto fail so we're not here all day. Also, display
          // an error message when this loop is finished.
          errval = ERROR_OPT_FAIL;
          files_failed = true;
          break;
        }
      }

      if(errval == ERROR_OPT_RETRY)
      {
        info("--UPDATER-- Retrying '%s'...\n", e->name);
        retry_times++;

        // Set the next file to this file to try again...
        e_next = e;
      }
      else
      {
        info("--UPDATER-- Skipping '%s'...\n", e->name);
        retry_times = 0;

        // Track this file so its link can be updated.
        e_prev = e;
      }
      continue;
    }
  }

  if(files_failed)
    error_message(E_UPDATE, 24,
     "Failed to delete files; check permissions and restart MegaZeux");
}

static boolean reissue_connection(HTTPHost &http, const char *host_name,
 boolean is_automatic)
{
  boolean ret = false;

  /**
   * The provided host may have an existing connection; in this case, close the
   * connection so a new one can be opened.
   */
  http.close();

  // If this is an automatic check, make the timeout duration shorter than the
  // default so it's less annoying.
  if(is_automatic)
    http.set_timeout_ms(1000);

  display_connecting(host_name);

  if(!http.connect(host_name, OUTBOUND_PORT))
  {
    if(!is_automatic)
    {
      snprintf(widget_buf, WIDGET_BUF_LEN,
       "Connection to \"%s\" failed.", host_name);
      widget_buf[WIDGET_BUF_LEN - 1] = 0;
      error_message(E_UPDATE, 12, widget_buf);
    }
  }
  else
  {
    trace("--UPDATER-- Successfully connected to %s:%d.\n", host_name, OUTBOUND_PORT);
    ret = true;
  }

  display_clear();
  return ret;
}

/**
 * Run a synchronous update check.
 * @param ctx           Current context
 * @param is_automatic  Disable more annoying UI displays for automated checks.
 * @return              true if the update completed, otherwise false.
 */
static boolean __check_for_updates(context *ctx, boolean is_automatic)
{
  const struct config_info *conf = get_config();
  int cur_host;
  const char *update_host;
  boolean in_exec_dir = false;
  boolean delete_remote_manifest = false;
  boolean try_next_host = true;
  boolean ret = false;

  set_error_suppression(E_UPDATE, false);

  if(conf->update_host_count < 1)
  {
    error_message(E_UPDATE, 14, "No updater hosts defined! Aborting.");
    return false;
  }

  display_initializing();
  if(!Host::host_layer_init_check())
  {
    error_message(E_UPDATE, 15, "Failed to initialize network layer.");
    display_clear();
    return false;
  }
  display_clear();

  set_context(CTX_UPDATER);

  ScopedBuffer<char> updates_txt(LINE_BUF_LEN);
  ScopedBuffer<char> url_base(LINE_BUF_LEN);

  for(cur_host = 0; (cur_host < conf->update_host_count) && try_next_host; cur_host++)
  {
    HTTPHost http(HOST_TYPE_TCP, conf->network_address_family);
    HTTPRequestInfo request;
    HTTPHostStatus status;

    Manifest removed, replaced, added, delete_list;
    ManifestEntry *e;

    char buffer[LINE_BUF_LEN];
    char *value = nullptr;
    int i = 0;
    int entries = 0;
    char update_branch[LINE_BUF_LEN];
    const char *version = VERSION;
    unsigned int retries;
    boolean reconnect = false;

    update_host = conf->update_hosts[cur_host];

    for(retries = 0; retries < MAX_RETRIES; retries++)
    {
      if(!reissue_connection(http, update_host, is_automatic))
        goto err_try_next_host;

      // Grab the file containing the names of the current Stable and Unstable
      request.clear();
      strcpy(request.url, "/" UPDATES_TXT);
      request.allowed_types = HTTPRequestInfo::plaintext_types;

      status = http.get(request, updates_txt, updates_txt.size());

      if(status == HOST_SUCCESS)
        break;

      trace("--UPDATER-- Failed to fetch " UPDATES_TXT ".\n");

      // Stop early on redirect and client error codes
      if(status == HOST_HTTP_REDIRECT || status == HOST_HTTP_CLIENT_ERROR)
      {
        retries = MAX_RETRIES;
        break;
      }

      /**
       * In the unlikely event this legitimately failed because updates.txt is
       * too big for the buffer, expand it. Use x2 the reported Content-Length
       * since update hosts should almost always be serving gzipped files.
       */
      if(status == HOST_FWRITE_FAILED)
      {
        size_t new_size = MAX(request.content_length, updates_txt.size()) * 2;
        if(!updates_txt.resize(MIN(new_size, UPDATES_TXT_MAX_SIZE)))
        {
          retries = MAX_RETRIES;
          break;
        }
      }
    }

    if(retries == MAX_RETRIES)
    {
      if(!is_automatic)
      {
        warn("Failed to download \"" UPDATES_TXT "\" (code %d; error: %s)\n",
         request.status_code, HTTPHost::get_error_string(status));
        request.print_response();

        snprintf(widget_buf, WIDGET_BUF_LEN, "Failed to download \""
         UPDATES_TXT "\" (%d/%d).\n", request.status_code, status);
        widget_buf[WIDGET_BUF_LEN - 1] = 0;
        error_message(E_UPDATE, 16, widget_buf);
      }
      goto err_try_next_host;
    }

    trace("--UPDATER-- Checking " UPDATES_TXT " for updates.\n");
    snprintf(update_branch, LINE_BUF_LEN, "Current-%.240s",
     conf->update_branch_pin);

    // Walk this list (of two, hopefully)
    struct memfile mf;
    mfopen(updates_txt, request.final_length, &mf);
    while(mfsafegets(buffer, LINE_BUF_LEN, &mf))
    {
      char *m = buffer;
      char *key;
      value = nullptr;

      key = strsep(&m, ":\n");
      if(!key)
        break;

      value = strsep(&m, ":\n");
      if(!value)
        break;

      if(strcmp(key, update_branch) == 0)
        break;
    }

    /* There was no "Current-XXX: Version" found; we cannot proceed with the
     * update because we cannot compute an update URL below.
     */
    if(!value)
    {
      if(!is_automatic)
        error_message(E_UPDATE, 17,
         "Failed to identify applicable update version.");
      goto err_try_next_host;
    }

    /* There's likely to be a space prepended to the version number.
     * Skip it here.
     */
    while(isspace(*value))
      value++;

    /* We found the latest update version, but we should check to see if that
     * matches the version we're already using. The user may choose to receive
     * "stability" updates for their current major version, or upgrade to the
     * newest one.
     */
    if(strcmp(value, version) != 0)
    {
      // Notify the user that updates are available.
      caption_set_updates_available(true);

      // If this is an auto check and silent mode is enabled, we can stop here.
      if(is_automatic && conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
      {
        try_next_host = false;
        goto err_try_next_host;
      }

      /**
       * Compute a temporary url base and use it to check if the current branch
       * supports full updates for this platform.
       */
      display_manifest_check();

      snprintf(url_base, LINE_BUF_LEN, "/%s/" PLATFORM, value);
      boolean platform_has_remote_manifest =
       Manifest::check_if_remote_exists(http, request, url_base);

      display_clear();

      if(!platform_has_remote_manifest || !UpdaterInit::allow_full_updates())
      {
        switch(ui_new_version_error(ctx, value, platform_has_remote_manifest))
        {
          case OPT_EXIT:
            version = nullptr;
            break;
          case OPT_UPDATE_CURRENT:
            version = VERSION;
            break;
          case OPT_TRY_NEXT_HOST:
            goto err_try_next_host;
        }
      }
      else
        version = ui_new_version_available(ctx, version, value);

      reconnect = true;

      // Abort if no version was selected.
      if(version == NULL)
      {
        try_next_host = false;
        goto err_try_next_host;
      }
    }

    /**
     * To continue, the updater needs to switch to the executable directory.
     * Platforms that don't pass the startup checks should stop here. Explicitly
     * checking the constexpr check result here lets the compiler optimize out
     * large sections of inaccessible code for platforms like Linux.
     */
    if(UpdaterInit::const_safety_checks != UpdaterInit::SUCCESS ||
     !UpdaterInit::allow_full_updates())
      break;

    if(!in_exec_dir)
    {
      if(!swivel_current_dir(true))
        break;

      in_exec_dir = true;
    }

    /* We can now compute a unique URL base for the updater. This will
     * be composed of a user-selected version and a static platform-archicture
     * name.
     */
    snprintf(url_base, LINE_BUF_LEN, "/%s/" PLATFORM, version);
    debug("Update base URL: %s\n", url_base.data());

    for(retries = 0; retries < MAX_RETRIES; retries++)
    {
      if(reconnect && !reissue_connection(http, update_host, false))
        goto err_try_next_host;

      display_manifest();

      status = Manifest::get_updates(http, request, url_base,
       removed, replaced, added);
      delete_remote_manifest = true;

      display_clear();

      if(status == HOST_SUCCESS)
        break;

      // Unsupported platform.
      if(status == HOST_HTTP_REDIRECT || status == HOST_HTTP_CLIENT_ERROR)
      {
        error_message(E_UPDATE, 19, "No updates available for this platform.");
        goto err_try_next_host;
      }
      reconnect = true;
    }

    if(retries == MAX_RETRIES)
    {
      error_message(E_UPDATE, 20, "Failed to fetch or process update manifest");
      goto err_try_next_host;
    }

    // At this point, we have a successful manifest, so we won't need another host
    try_next_host = false;

    if(!removed.has_entries() && !replaced.has_entries() && !added.has_entries())
    {
      boolean has_next_host = (cur_host < conf->update_host_count);

      if(is_automatic)
        goto err_try_next_host;

      // The user may want to attempt an update from the next host.
      try_next_host = ui_version_is_current(ctx, has_next_host);
      goto err_try_next_host;
    }

    // Set the updates available notification if it hasn't been set already.
    caption_set_updates_available(true);

    // Switch back to the normal checking timeout for the rest of the process.
    if(is_automatic)
    {
      if(conf->update_auto_check == UPDATE_AUTO_CHECK_SILENT)
        goto err_try_next_host;

      http.set_timeout_ms(Host::TIMEOUT_DEFAULT);
      is_automatic = 0;
    }

    /* Show the user the list of changes to perform and prompt the user to
     * confirm or cancel.
     */
    entries = ui_confirm_changes(ctx, removed, replaced, added);
    reconnect = true;

    if(entries <= 0)
      goto err_try_next_host;

    /* Defer deletions until we restart; any of these files may still be
     * in use by this (old) process. Reduce the number of entries by the
     * number of removed items for the progress meter below. Since the
     * local manifest might be wrong, generate new manifest entries for this.
     */
    for(e = removed.first(); e; e = e->next, entries--)
      delete_list.append(ManifestEntry::create_from_file(e->name));

    /* Since the operations for adding and replacing a file are identical,
     * move the added list to the end of the replaced list.
     */
    replaced.append(added);

    cancel_update = false;
    http.set_callbacks(NULL, recv_cb, cancel_cb);

    i = 1;
    for(e = replaced.first(); e; e = e->next, i++)
    {
      for(retries = 0; retries < MAX_RETRIES; retries++)
      {
        boolean m_ret;

        if(reconnect)
        {
          if(reissue_connection(http, update_host, 0))
          {
            http.set_callbacks(NULL, recv_cb, cancel_cb);
            reconnect = false;
          }
          else
            goto err_try_next_host;
        }

        if(!check_create_basedir(e->name))
          goto err_try_next_host;

        final_size = (long)e->size;
        display_download_init(e->name, i, entries);

        m_ret = Manifest::download_and_replace_entry(http, request, url_base,
         e, delete_list);

        display_clear();

        if(m_ret)
          break;

        if(cancel_update)
        {
          error_message(E_UPDATE, 21, "Download was cancelled; update aborted.");
          goto err_try_next_host;
        }
        reconnect = true;
      }

      if(retries == MAX_RETRIES)
      {
        snprintf(widget_buf, WIDGET_BUF_LEN,
         "Failed to download \"%s\" (after %d attempts).", e->name, retries);
        widget_buf[WIDGET_BUF_LEN - 1] = 0;
        error_message(E_UPDATE, 22, widget_buf);
        goto err_try_next_host;
      }
    }

    if(!write_delete_list(delete_list))
      goto err_try_next_host;

    delete_remote_manifest = false;
    if(!replace_manifest())
      goto err_try_next_host;

    try_next_host = false;
    ret = true;
err_try_next_host:
    http.close();
  } //end host for loop

  if(delete_remote_manifest)
    vunlink(REMOTE_MANIFEST_TXT);

  if(in_exec_dir)
    swivel_current_dir_back(true);

  pop_context();

  if(ret)
  {
    // Inform the user the update was successful.
    ui_update_finished(ctx);

    // Signal core to exit and restart MZX.
    core_full_restart(ctx);
    return true;
  }
  return false;
}

/**
 * Perform some sanity checks to make sure certain parts of this code will
 * never run on platforms that don't support it (e.g. Linux).
 */
static UpdaterInit::StatusValue updater_init_safety_checks()
{
  /**
   * The assets should should be relative to the executable dir.
   * If they aren't, this is probably the same as above. Use the protected
   * charset to test this (though this really should apply to all assets).
   */
  const char *executable_dir = mzx_res_get_by_id(MZX_EXECUTABLE_DIR);
  const char *edit_chr = mzx_res_get_by_id(MZX_EDIT_CHR);
  size_t len = strlen(executable_dir);
  if(strncmp(executable_dir, edit_chr, len))
    return UpdaterInit::INCOMPATIBLE_ASSETS;

  return UpdaterInit::SUCCESS;
}

/**
 * Perform several steps to determine whether full updater capabilities are
 * available in the exec dir.
 *
 * 1) File read + make sure manifest.txt exists.
 * 2) File create.
 * 3) File unlink.
 */
static UpdaterInit::StatusValue updater_init_exec_dir_check()
{
  struct stat stat_info;

  vfile *vf = vfopen_unsafe(REMOTE_MANIFEST_TXT "~", "wb");
  if(!vf)
    return UpdaterInit::FAILED_FILE_WRITE;

  vfclose(vf);
  if(vunlink(REMOTE_MANIFEST_TXT "~"))
    return UpdaterInit::FAILED_FILE_UNLINK;

  if(vstat(LOCAL_MANIFEST_TXT, &stat_info))
    return UpdaterInit::FAILED_TO_STAT_MANIFEST;

  return UpdaterInit::SUCCESS;
}

/**
 * Check for a delete.txt manifest indicating files to delete from a prior
 * update. If found, delete the files listed and update (or remove) delete.txt.
 */
static void updater_init_delete_txt_check()
{
  Manifest delete_list;
  struct stat stat_info;

  if(vstat(DELETE_TXT, &stat_info))
    return;

  if(!delete_list.create(DELETE_TXT))
    return;

  apply_delete_list(delete_list);
  vunlink(DELETE_TXT);

  if(delete_list.has_entries())
    write_delete_list(delete_list);
}

boolean updater_init(void)
{
  const struct config_info *conf = get_config();
  if(!conf->updater_enabled)
    return false;

  check_for_updates = __check_for_updates;

  /**
   * Explicitly compare against the constexpr checks first so the rest gets
   * optimized out as inaccessible code for platforms like Linux.
   */
  UpdaterInit::status = UpdaterInit::const_safety_checks;
  if(UpdaterInit::const_safety_checks != UpdaterInit::SUCCESS)
    return true;

  /**
   * More checks to filter out platforms where the full update process is
   * entirely non-applicable.
   */
  UpdaterInit::status = updater_init_safety_checks();
  if(UpdaterInit::status != UpdaterInit::SUCCESS)
    return true;

  if(swivel_current_dir(false))
  {
    UpdaterInit::status = updater_init_exec_dir_check();

    if(UpdaterInit::allow_full_updates())
      updater_init_delete_txt_check();

    swivel_current_dir_back(false);
  }
  else
    UpdaterInit::status = UpdaterInit::FAILED_CHDIR_TO_EXEC_DIR;

  if(!UpdaterInit::allow_full_updates())
  {
    warn("Updater startup checks failed; full updates will not be available (%s).\n",
     UpdaterInit::status_string(UpdaterInit::status));
  }

  return true;
}

boolean is_updater(void)
{
  return true;
}

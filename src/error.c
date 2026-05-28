/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2017-2024 Alice Rowan <petrifiedrowan@gmail.com>
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
// Error code

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "core.h"
#include "error.h"
#include "graphics.h"
#include "helpsys.h"
#include "util.h"
#include "window.h"
#include "world.h"
#include "event/event.h"
#include "platform/platform.h"

// Error type names by type code:
static const char *const error_type_names[] =
{
  " WARNING: ",         // 0
  " ERROR: ",           // 1
  " FATAL ERROR: ",     // 2
  " CRITICAL ERROR: ",  // 3, unused
};

// Note: ERROR_OPT_SUPPRESS should only be used by error_message().

int error(const char *string, enum error_type type, unsigned int options,
 unsigned int code)
{
  boolean skip_error_ui = false;
  const char *type_name;
  int t1 = 9, ret = 0;
  int joystick_key;
  int x;

  // Find the name of this error type.
  if((unsigned int)type >= ARRAY_SIZE(error_type_names))
    type = ERROR_T_WARNING;

  type_name = error_type_names[type];

  // If graphics couldn't initialize, print the error to stderr and abort.
  if(!has_video_initialized())
    skip_error_ui = true;

#ifdef __EMSCRIPTEN__
  // TODO: For Emscripten, some paths here are safe and others will crash.
  // These can be detected through their error codes for now.
  {
    static const enum error_type safe_codes[] = { 0x3101, 0x2C01, 0x0000 };
    skip_error_ui = true;

    for(x = 0; x < (int)ARRAY_SIZE(safe_codes); x++)
      if(code == safe_codes[x])
        skip_error_ui = false;
  }
#endif

  if(skip_error_ui)
  {
    int scode = code ? (int)code : -1;

    fprintf(stderr, "%s%s\n", type_name, string);
    fflush(stderr);

    // Attempt to automatically handle this error if possible.
    if(options & ERROR_OPT_EXIT) exit(scode);
    if(options & ERROR_OPT_SUPPRESS) return ERROR_OPT_SUPPRESS;
    if(options & ERROR_OPT_OK) return ERROR_OPT_OK;
    if(options & ERROR_OPT_FAIL) return ERROR_OPT_FAIL;
    exit(scode);
    return ERROR_OPT_FAIL; // __EMSCRIPTEN__
  }

  // Use the high byte of the error code to link to a context in the help file.
  set_context(((code >> 8) & 0xFF) + 700);

  m_hide();
  save_screen();

  dialog_fadein();

  draw_window_box(1, 10, 78, 14, 76, 64, 72, 1, 1);
  // Add title and error name
  x = 40 - (int)strlen(type_name)/2;
  write_string(type_name, x, 10, 78, 0);

  write_string(string, 40 - ((unsigned int)strlen(string) / 2), 11, 79, 0);

  // Add options
  write_string("Press", 4, 13, 78, 0);

  if(options & ERROR_OPT_FAIL)
  {
    write_string(", F for Fail", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & ERROR_OPT_RETRY)
  {
    write_string(", R to Retry", t1, 13, 78, 0);
    t1 += 12;
  }
  if(options & ERROR_OPT_EXIT)
  {
    write_string(", E to Exit", t1, 13, 78, 0);
    t1 += 11;
  }
  if(options & ERROR_OPT_OK)
  {
    write_string(", O for OK", t1, 13, 78, 0);
    t1 += 10;
  }
  if(options & ERROR_OPT_SUPPRESS)
  {
    write_string(", S for Suppress", t1, 13, 78, 0);
    t1 += 16;
  }
  // FIXME no context or mzx_world here
  if(!(options & ERROR_OPT_NO_HELP) && false)
  {
    write_string(", F1 for Help", t1, 13, 78, 0);
    t1 += 13;
  }

  draw_char('.', 78, t1, 13);
  draw_char(':', 78, 9, 13);

  // Add code if not 0

  if(code != 0)
  {
    char temp[32];
    snprintf(temp, 32, " Debug code:%4.4xh ", code);
    write_string(temp, 30, 14, 64, 0);
  }

  // Get key
  do
  {
    update_screen();
    update_event_status_delay();
    t1 = get_key(keycode_internal_wrt_numlock);

    joystick_key = get_joystick_ui_key();
    if(joystick_key)
      t1 = joystick_key;

    //Exit event--mimic Escape
    if(get_exit_status())
      t1 = IKEY_ESCAPE;

    //Process
    switch(t1)
    {
      case IKEY_f:
        fail:
        if(!(options & ERROR_OPT_FAIL)) break;
        ret = ERROR_OPT_FAIL;
        break;
      case IKEY_r:
        retry:
        if(!(options & ERROR_OPT_RETRY)) break;
        ret = ERROR_OPT_RETRY;
        break;
      case IKEY_e:
        exit:
        if(!(options & ERROR_OPT_EXIT)) break;
        ret = ERROR_OPT_EXIT;
        break;
      case IKEY_o:
        ok:
        if(!(options & ERROR_OPT_OK)) break;
        ret = ERROR_OPT_OK;
        break;
      case IKEY_s:
        if(!(options & ERROR_OPT_SUPPRESS)) break;
        ret = ERROR_OPT_SUPPRESS;
        break;
      case IKEY_F1:
        if(options & ERROR_OPT_NO_HELP) break;
        // Call help
        // FIXME context, no mzx_world here
        //help_system(NULL, mzx_world);
        break;
      case IKEY_ESCAPE:
        // Escape. Order of options this applies to-
        // Fail, Ok, Retry, Exit.
        if(options & ERROR_OPT_FAIL ) goto fail;
        if(options & ERROR_OPT_OK   ) goto ok;
        if(options & ERROR_OPT_RETRY) goto retry;
        goto exit;
      case IKEY_RETURN:
        // Enter. Order of options this applies to-
        // OK, Retry, Fail, Exit.
        if(options & ERROR_OPT_OK   ) goto ok;
        if(options & ERROR_OPT_RETRY) goto retry;
        if(options & ERROR_OPT_FAIL ) goto fail;
        goto exit;
    }
  } while(ret == 0);

  pop_context();

  // Restore screen and exit appropriately
  dialog_fadeout();

  restore_screen();
  m_show();
  if(ret == ERROR_OPT_EXIT) // Exit the program
  {
#ifdef CONFIG_STDIO_REDIRECT
    redirect_stdio_exit();
#endif
    platform_quit();
    exit(-1);
  }

  return ret;
}


static boolean suppress_errors[NUM_ERROR_CODES];
static int error_count = 0;

// Wrapper for error().
int error_message(enum error_code id, int parameter, const char *string)
{
  char error_mesg[80];
  char version_string[16];
  int hi = (parameter & 0xFF00) >> 8;
  int lo = (parameter & 0xFF);
  int opts = ERROR_OPT_OK | ERROR_OPT_SUPPRESS;
  int severity = ERROR_T_ERROR;
  int code = id;
  int result;

  get_version_string(version_string, parameter);
  error_count++;

  switch (id)
  {
    case E_INVOKE_SELF_FAILED:
      sprintf(error_mesg, "Attempt to invoke self failed!");
      code = 0xADA1;
      break;

    case E_CORE_FATAL_BUG:
      sprintf(error_mesg, "Context code bug");
      severity = ERROR_T_FATAL;
      opts = ERROR_OPT_EXIT;
      code = 0x2B00 | (parameter & 0xFF);
      break;

    case E_FILE_DOES_NOT_EXIST:
      sprintf(error_mesg, "File doesn't exist");
      break;

    case E_IO_READ:
      sprintf(error_mesg, "Unknown error reading from file");
      break;

    case E_IO_WRITE:
      sprintf(error_mesg, "Unknown error writing to file");
      break;

    case E_SAVE_FILE_INVALID:
      sprintf(error_mesg, "File is not a valid .SAV file or is corrupt");
      code = 0x2101;
      break;

    case E_SAVE_VERSION_OLD:
      sprintf(error_mesg,
       ".SAV files from older versions of MZX (%s) are not supported",
       version_string);
      code = 0x2101;
      break;

    case E_SAVE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       ".SAV files from newer versions of MZX (%s) are not supported",
       version_string);
      code = 0x2101;
      break;

    case E_WORLD_FILE_INVALID:
      sprintf(error_mesg, "File is not a valid world file or is corrupt");
      code = 0x0D02;
      break;

    case E_WORLD_FILE_VERSION_OLD:
      sprintf(error_mesg,
       "Invalid world version (%s)",
       version_string);
      code = 0x0D02;
      break;

    case E_WORLD_FILE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       "World is from a more recent version (%s)",
       version_string);
      code = 0x0D02;
      break;

    case E_WORLD_DECRYPT_WRITE_PROTECTED:
      sprintf(error_mesg, "Cannot decrypt write-protected world; "
       "check permissions");
      code = 0x0DD5;
      break;

    case E_WORLD_LOCKED:
      sprintf(error_mesg, "Cannot load password protected world");
      code = 0x0D02;
      break;

    case E_WORLD_IO_POST_VALIDATION:
      sprintf(error_mesg, "Post validation IO error occurred");
      code = 0x0D01;
      break;

    case E_WORLD_IO_SAVING:
      sprintf(error_mesg,
       "Error saving; file/directory may be write protected");
      code = 0x0D01;
      break;

    case E_WORLD_BOARD_MISSING:
      sprintf(error_mesg,
       "Board @ %Xh could not be found", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_BOARD_CORRUPT:
      sprintf(error_mesg,
       "Board @ %Xh is irrecoverably truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_BOARD_TRUNCATED_SAFE:
      sprintf(error_mesg,
       "Board @ %Xh is truncated, but could be partially recovered", parameter);
      code = 0x0D03;
      break;

    case E_WORLD_ROBOT_MISSING:
      sprintf(error_mesg, "Robot @ %Xh could not be found", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_FILE_INVALID:
      sprintf(error_mesg, "File is not a board file or is corrupt");
      code = 0x4040;
      break;

    case E_BOARD_FILE_FUTURE_VERSION:
      sprintf(error_mesg, "Board file is from a future version (%s)",
       version_string);
      code = 0x4041;
      break;

    case E_BOARD_ROBOT_CORRUPT:
      sprintf(error_mesg, "Robot @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SCROLL_CORRUPT:
      sprintf(error_mesg, "Scroll @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SENSOR_CORRUPT:
      sprintf(error_mesg, "Sensor @ %Xh is truncated or corrupt", parameter);
      code = 0x0D03;
      break;

    case E_BOARD_SUMMARY:
      sprintf(error_mesg, "Board @ %Xh summary", parameter);
      code = 0x4041;
      break;

    case E_MZM_DOES_NOT_EXIST:
      sprintf(error_mesg, "MZM doesn't exist");
      code = 0x0D01;
      break;

    case E_MZM_FILE_INVALID:
      sprintf(error_mesg, "File is not an MZM or is corrupt");
      code = 0x6660;
      break;

    case E_MZM_FILE_FROM_SAVEGAME:
      sprintf(error_mesg, "MZM contains runtime robots; dummying out");
      code = 0x6661;
      break;

    case E_MZM_FILE_VERSION_TOO_RECENT:
      sprintf(error_mesg,
       "MZM from newer version (%s); dummying out robots",
       version_string);
      code = 0x6661;
      break;

    case E_MZM_ROBOT_CORRUPT:
      sprintf(error_mesg, "MZM is missing robots or contains corrupt robots");
      code = 0x6662;
      break;

    case E_LOAD_BC_CORRUPT:
      sprintf(error_mesg, "Bytecode file failed validation check");
      code = 0xD0D0;
      break;

    case E_NO_LAYER_RENDERER:
      sprintf(error_mesg, "Current renderer lacks advanced graphical features; "
       "features disabled");
      code = 0x2563;
      break;

    case E_NO_EXTENDED_CHARSETS:
      sprintf(error_mesg, "Limited/missing extended charset support; "
       "some features may not work");
      code = 0x2564;
      break;

    case E_ZIP_BOARD_CORRUPT:
      sprintf(error_mesg, "Board # %d is corrupt", lo);
      code = 0x9000;
      break;

    case E_ZIP_BOARD_MISSING_DATA:
      sprintf(error_mesg, "Board # %d is missing data:", lo);
      code = 0x9001;
      break;

    case E_ZIP_ROBOT_CORRUPT:
      sprintf(error_mesg, "Robot # %d on board # %d is corrupt", lo, hi);
      code = 0x9002;
      break;

    case E_ZIP_SCROLL_CORRUPT:
      sprintf(error_mesg, "Scroll # %d on board # %d is corrupt", lo, hi);
      code = 0x9003;
      break;

    case E_ZIP_SENSOR_CORRUPT:
      sprintf(error_mesg, "Sensor # %d on board # %d is corrupt", lo, hi);
      code = 0x9004;
      break;

    case E_ZIP_ROBOT_MISSING_FROM_BOARD:
      sprintf(error_mesg, "Robot # %d does not exist on board # %d", lo, hi);
      code = 0x9005;
      break;

    case E_ZIP_ROBOT_MISSING_FROM_DATA:
      sprintf(error_mesg,
       "Robot # %d exists on board # %d, but was not found", lo, hi);
      code = 0x9006;
      break;

    case E_ZIP_ROBOT_DUPLICATED:
      sprintf(error_mesg,
       "Robot # %d contains duplicates on board # %d", lo, hi);
      code = 0x9007;
      break;

#ifdef CONFIG_EDITOR
    case E_CANT_OVERWRITE_PLAYER:
      sprintf(error_mesg, "Cannot overwrite the player- move it first");
      severity = ERROR_T_WARNING;
      code = 0x0000;
      break;

    case E_ANSI_IMPORT:
      sprintf(error_mesg, "Error importing ANSi");
      code = 0x1901;
      break;

    case E_ANSI_EXPORT:
      sprintf(error_mesg, "Error exporting ANSi");
      code = 0x0F01;
      break;

    case E_TEXT_EXPORT:
      sprintf(error_mesg, "Error exporting text");
      code = 0x1401;
      break;

    case E_SFX_IMPORT:
      sprintf(error_mesg, "File is invalid or is not an SFX file");
      code = 0;
      break;

    case E_SFX_EXPORT:
      sprintf(error_mesg, "Error exporting SFX file");
      code = 0;
      break;

    case E_IMAGE_EXPORT:
      sprintf(error_mesg, "Error exporting image");
      code = 0;
      break;

    case E_IMAGE_EXPORT_CANCELED:
      sprintf(error_mesg, "Error exporting image or export was canceled");
      code = 0;
      break;
#endif

#ifdef CONFIG_UPDATER
    case E_UPDATE_RETRY:
      opts = ERROR_OPT_RETRY | ERROR_OPT_FAIL;

      /* fallthrough */

    case E_UPDATE:
      snprintf(error_mesg, 79, "%s", string);
      code = 0xA200 | (parameter & 0xFF);
      string = NULL;
      break;
#endif

#ifdef CONFIG_DEBYTECODE
    case E_DBC_WORLD_OVERWRITE_OLD:
      sprintf(error_mesg,
       "Save would overwrite older version (%s); aborted",
       version_string);
      code = 0x0fac;
      break;

    case E_DBC_SAVE_ROBOT_UNSUPPORTED:
      sprintf(error_mesg,
       "SAVE_BC is no longer supported");
      code = 0x0fac;
      break;
#endif

    case E_ZIP:
      code = 0xEEEE;
      /* fallthrough */

    case E_DEFAULT:
    default:
      snprintf(error_mesg, 79, "%s", string);
      string = NULL;
      break;
  }

  // This needs to happen after the switch, unfortunately, since we need to
  // check the options. Suppress errors that allow OK only.
  if(suppress_errors[id] && (opts & ERROR_OPT_OK))
    return ERROR_OPT_OK;

  if(string)
  {
    int offset = strlen(error_mesg);
    int left = 79 - offset;
    if(left > 0)
    {
      snprintf(error_mesg + offset, left, ": %s", string);
    }
  }

  result = error(error_mesg, severity, opts, code);

  if(result == ERROR_OPT_SUPPRESS)
  {
    suppress_errors[id] = true;
    return ERROR_OPT_OK;
  }

  return result;
}

int get_and_reset_error_count(void)
{
  int count = error_count;
  error_count = 0;
  return count;
}

void set_error_suppression(enum error_code id, boolean enable)
{
  suppress_errors[id] = enable;
}

void reset_error_suppression(void)
{
  int i;
  for(i = 0; i < NUM_ERROR_CODES; i++)
    suppress_errors[i] = false;
}

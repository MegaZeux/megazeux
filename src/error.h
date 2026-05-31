/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
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

/* Declaration for ERROR.CPP */

#ifndef MEGAZEUX_ERROR_H
#define MEGAZEUX_ERROR_H

#include "compat.h"

#define ERROR_OPT_FAIL      1
#define ERROR_OPT_RETRY     2
#define ERROR_OPT_EXIT      4
#define ERROR_OPT_OK        8
#define ERROR_OPT_NO_HELP  16
#define ERROR_OPT_SUPPRESS 32

MEGAZEUX_BEGIN_DECLS

enum error_type
{
  ERROR_T_WARNING   = 0,
  ERROR_T_ERROR     = 1,
  ERROR_T_FATAL     = 2,
};

enum error_code
{
  E_DEFAULT,
  E_INVOKE_SELF_FAILED,
  E_CORE_FATAL_BUG,
  E_FILE_DOES_NOT_EXIST,
  E_IO_READ,
  E_IO_WRITE,
  E_SAVE_FILE_INVALID,
  E_SAVE_VERSION_OLD,
  E_SAVE_VERSION_TOO_RECENT,
  E_WORLD_FILE_INVALID,
  E_WORLD_FILE_VERSION_OLD,
  E_WORLD_FILE_VERSION_TOO_RECENT,
  E_WORLD_DECRYPT_WRITE_PROTECTED,
  E_WORLD_LOCKED,
  E_WORLD_IO_POST_VALIDATION,
  E_WORLD_IO_SAVING,
  E_WORLD_BOARD_MISSING,
  E_WORLD_BOARD_CORRUPT,
  E_WORLD_BOARD_TRUNCATED_SAFE,
  E_WORLD_ROBOT_MISSING,
  E_BOARD_FILE_INVALID,
  E_BOARD_FILE_FUTURE_VERSION,
  E_BOARD_ROBOT_CORRUPT,
  E_BOARD_SCROLL_CORRUPT,
  E_BOARD_SENSOR_CORRUPT,
  E_BOARD_SUMMARY,
  E_MZM_DOES_NOT_EXIST,
  E_MZM_FILE_INVALID,
  E_MZM_FILE_FROM_SAVEGAME,
  E_MZM_FILE_VERSION_TOO_RECENT,
  E_MZM_ROBOT_CORRUPT,
  E_LOAD_BC_CORRUPT,
  E_NO_LAYER_RENDERER,
  E_NO_EXTENDED_CHARSETS,
  E_ZIP,
  E_ZIP_BOARD_CORRUPT,
  E_ZIP_BOARD_MISSING_DATA,
  E_ZIP_ROBOT_CORRUPT,
  E_ZIP_SCROLL_CORRUPT,
  E_ZIP_SENSOR_CORRUPT,
  E_ZIP_ROBOT_MISSING_FROM_BOARD,
  E_ZIP_ROBOT_MISSING_FROM_DATA,
  E_ZIP_ROBOT_DUPLICATED,
#ifdef CONFIG_EDITOR
  E_CANT_OVERWRITE_PLAYER,
  E_ANSI_IMPORT,
  E_ANSI_EXPORT,
  E_TEXT_EXPORT,
  E_SFX_IMPORT,
  E_SFX_EXPORT,
  E_IMAGE_EXPORT,
  E_IMAGE_EXPORT_CANCELED,
#endif
#ifdef CONFIG_UPDATER
  E_UPDATE,
  E_UPDATE_RETRY,
#endif
#ifdef CONFIG_DEBYTECODE
  E_DBC_WORLD_OVERWRITE_OLD,
  E_DBC_SAVE_ROBOT_UNSUPPORTED,
#endif
  NUM_ERROR_CODES
};

CORE_LIBSPEC int error(const char *string, enum error_type type,
 unsigned int options, unsigned int code);

CORE_LIBSPEC int error_message(enum error_code id, int parameter,
 const char *string);

CORE_LIBSPEC void set_error_suppression(enum error_code id, boolean enable);

int get_and_reset_error_count(void);
void reset_error_suppression(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_ERROR_H */

/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
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

#ifndef MEGAZEUX_EVENT_3DS_H
#define MEGAZEUX_EVENT_3DS_H

#include "../../compat.h"

MEGAZEUX_BEGIN_DECLS

enum bottom_screen_mode
{
  BOTTOM_SCREEN_MODE_PREVIEW,
  BOTTOM_SCREEN_MODE_KEYBOARD,
  BOTTOM_SCREEN_MODE_MAX
};

enum focus_mode
{
  FOCUS_FORBID,
  FOCUS_ALLOW, // checks if position changed
  FOCUS_PASS // ignores all checks and check updates - for touchscreen
};

enum bottom_screen_mode get_bottom_screen_mode(void);
enum focus_mode get_allow_focus_changes(void);
int ctr_get_subscreen_height(void);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_EVENT_3DS_H */

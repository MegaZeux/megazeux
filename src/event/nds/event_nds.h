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

#ifndef __NDS_EVENT_H__
#define __NDS_EVENT_H__

#include "../../compat.h"

__M_BEGIN_DECLS

enum focus_mode
{
  FOCUS_FORBID,
  FOCUS_ALLOW, // checks if position changed
  FOCUS_PASS // ignores all checks and check updates - for touchscreen
};

enum focus_mode get_allow_focus_changes(void);

__M_END_DECLS

#endif /* __NDS_EVENT_H__ */

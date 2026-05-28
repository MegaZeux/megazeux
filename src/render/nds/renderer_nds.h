/* MegaZeux
 *
 * Copyright (C) 2007 Kevin Vance <kvance@kvance.com>
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

#ifndef __RENDER_NDS_H
#define __RENDER_NDS_H

#include "../../compat.h"

__M_BEGIN_DECLS

// The subscreen can display different information.
enum Subscreen_Mode
{
  SUBSCREEN_SCALED = 0,
  SUBSCREEN_KEYBOARD,
  SUBSCREEN_MODE_COUNT,
  SUBSCREEN_MODE_INVALID
};

extern enum Subscreen_Mode subscreen_mode;

boolean is_scaled_mode(enum Subscreen_Mode mode);

// Call these 4 functions every vblank.
void nds_sleep_check(void);
void nds_video_jitter(void);
void nds_video_rasterhack(void);
void nds_video_do_transition(void);

// Toggle to the next subscreen mode.
boolean nds_subscreen_preview(void);
boolean nds_subscreen_keyboard(void);

__M_END_DECLS

#endif // __RENDER_NDS_H

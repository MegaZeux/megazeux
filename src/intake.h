/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2018-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_INTAKE_H
#define MEGAZEUX_INTAKE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"

enum intake_exit_type
{
  INTK_EXIT_ENTER,
  INTK_EXIT_ENTER_ESC,
  INTK_EXIT_ANY
};

enum intake_event_type
{
  INTK_NO_EVENT,
  INTK_MOVE,            /* Cursor moved within the intake line. */
  INTK_MOVE_WORDS,      /* Cursor moved a number of words within the intake line. */
  INTK_INSERT,          /* Text inserted. */
  INTK_OVERWRITE,       /* Text overwritten. */
  INTK_DELETE,          /* Text deleted with Delete. */
  INTK_BACKSPACE,       /* Text deleted with Backspace. */
  INTK_BACKSPACE_WORDS, /* Text deleted with Ctrl+Backspace (value=#words). */
  INTK_CLEAR,           /* Text deleted with Alt+Backspace. */
  INTK_INSERT_BLOCK,    /* Text block inserted via intake_input_string. */
  INTK_OVERWRITE_BLOCK, /* Text block overwritten via intake_input_string. */
};

CORE_LIBSPEC boolean intake_get_insert(void);
CORE_LIBSPEC void intake_set_insert(boolean new_insert_state);

CORE_LIBSPEC int intake(struct world *mzx_world, char *string, int max_len, int display_len,
 int x, int y, char color, enum intake_exit_type exit_type, boolean mask_colors,
 int *return_x_pos);

CORE_LIBSPEC subcontext *intake2(context *parent, char *dest, int max_length,
 int *pos_external, int *length_external);

CORE_LIBSPEC void intake_sync(subcontext *intk);
CORE_LIBSPEC const char *intake_input_string(subcontext *intk, const char *src,
 int linebreak_char);
CORE_LIBSPEC void intake_event_callback(subcontext *intk, void *priv,
 boolean (*event_cb)(void *priv, subcontext *sub, enum intake_event_type type,
 int old_pos, int new_pos, int value, const char *data));

CORE_LIBSPEC boolean intake_apply_event_fixed(subcontext *sub,
 enum intake_event_type type, int new_pos, int value, const char *data);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_INTAKE_H */


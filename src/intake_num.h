/* MegaZeux
 *
 * Copyright (C) 2018-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_INTAKE_NUM_H
#define MEGAZEUX_INTAKE_NUM_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"

/**
 * Create a context for number entry. On exit if the number was changed, the
 * callback will be executed using the parent context and the final value as
 * inputs.
 *
 * @param parent    Calling context/context to receive result.
 * @param value     Initial number value.
 * @param min_val   Minimum possible value.
 * @param max_val   Maximum possible value.
 * @param x         X position to display the number onscreen.
 * @param y         Y position to display the number onscreen.
 * @param min_width Minimum width of the number display.
 * @param color     Color of the number display.
 * @param callback  Function to be called after a number is entered.
 * @return          The new intake_num context.
 */

CORE_LIBSPEC
context *intake_num(context *parent, int value, int min_val, int max_val,
 int x, int y, int min_width, int color, void (*callback)(context *, int));

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_INTAKE_NUM_H */

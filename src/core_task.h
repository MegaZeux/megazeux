/* MegaZeux
 *
 * Copyright (C) 2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_CORE_TASK_H
#define MEGAZEUX_CORE_TASK_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "core.h"

CORE_LIBSPEC boolean core_task_tick(context *ctx,
 int progress, int progress_max, const char *status);

CORE_LIBSPEC void core_task_context(context *parent, const char *title,
 boolean (*task_callback)(context *ctx, void *priv),
 void (*task_complete)(void *priv, boolean ret), void *priv);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_CORE_TASK_H */

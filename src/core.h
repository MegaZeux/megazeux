/* MegaZeux
 *
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#ifndef MEGAZEUX_CORE_H
#define MEGAZEUX_CORE_H

#include "compat.h"

MEGAZEUX_BEGIN_DECLS

#include "world_struct.h"

/**
 * The type of a given context. Used to identify particular contexts and to open
 * help files contextually. Do not sort this enum by value.
 *
 * Numbers >0 correspond directly to a hyperlink in the help system.
 * Numbers <=0 are used by contexts with no unique link in the help system.
 */
enum context_type
{
  // Core contexts.
  CTX_TITLE_SCREEN          = -1,
  CTX_MAIN                  = 72,
  CTX_PLAY_GAME             = 91,
  CTX_CONFIGURE             = 92,
  CTX_DIALOG_BOX            = 98,
  CTX_HELP_SYSTEM           = -2,
  CTX_MAIN_MENU             = -3,
  CTX_GAME_MENU             = -4,
  CTX_INTAKE_NUM            = -6,
  CTX_TASK                  = -7,

  // Network contexts.
  CTX_UPDATER               = 99,

  // Editor contexts.
  CTX_EDITOR                = -100,
  CTX_EDITOR_VIEW_BOARD     = -101,
  CTX_THING_MENU            = -102,
  CTX_BLOCK_CMD             = 73,
  CTX_BLOCK_TYPE            = 74,
  CTX_CHOOSE_CHARSET        = 75,
  CTX_IMPORTEXPORT_TYPE     = 77,
  CTX_CHAR_EDIT             = 79,
  CTX_STATUS_COUNTERS       = 82,
  CTX_BOARD_EXITS           = 83,
  CTX_BOARD_SIZES           = 84,
  CTX_BOARD_INFO            = 85,
  CTX_CHANGE_CHAR_IDS       = 89,
  CTX_CHANGE_DAMAGE         = 90,
  CTX_GLOBAL_SETTINGS       = 86,
  CTX_GLOBAL_SETTINGS_2     = 88,
  CTX_ROBO_ED               = 87,
  CTX_PALETTE_EDITOR        = 93,
  CTX_SENSOR_EDITOR         = 94,
  CTX_SUPER_MEGAZEUX        = 95,
  CTX_SFX_EDITOR            = 97,
  CTX_COUNTER_DEBUG         = 100,
  CTX_ROBOT_DEBUG           = 101,
  CTX_BREAKPOINT_EDITOR     = 102,
  CTX_VLAYER_SIZES          = 103,
};

/** The framerate rule that should be used for the active context. */
enum framerate_type
{
  FRAMERATE_UI,             // Standard delay time.
  FRAMERATE_UI_INTERRUPT,   // Standard delay time or until an event occurs.
  FRAMERATE_MZX_SPEED,      // Delay according to mzx_speed.
};

/**
 * Core types.
 * "subcontext" is an alternate type for context that should be used for
 * functions that deal with subcontexts instead of contexts.
 */
typedef struct context context;
typedef struct context subcontext;
typedef struct context_data context_data;
typedef struct core_context core_context;

typedef void context_callback_param;
typedef void (*context_callback_function)(context *, context_callback_param *);

/**
 * Contains information related to the current MegaZeux state/interface/etc.
 */
struct context
{
  struct world *world;
  core_context *root;
  context_data *internal_data;
};

/**
 * Contains extra function/variable information for a new context or subcontext.
 * All variables should be set to NULL or zero unless explicitly provided.
 * A return value of true from the draw function will update the screen (this
 * is almost always what you want to return, for an exception see draw_world).
 * A return value of true to an update function will treat the indicated input
 * type(s) as having been fully handled. If no functions aside from destroy are
 * provided, an error will display.
 *
 * resume           Optional function called when this context enters focus.
 * draw             Optional function to draw this context every frame.
 * idle             Update function called every frame (all).
 * key              Update function called to handle a keypress (key).
 * joystick         Update function called to handle a joystick (joystick).
 * click            Update function called to handle a mouse click (mouse).
 * drag             Update function called to handle a mouse drag (mouse).
 * destroy          Optional function to be called on destruction.
 * framerate_mode   Framerate mode this context should use (context only).
 * priority         Alters the update order. higher => updates earlier.
 */
struct context_spec
{
  void (*resume)(context *);
  boolean (*draw)(context *);
  boolean (*idle)(context *);
  boolean (*key)(context *, int *key);
  boolean (*joystick)(context *, int *key, int action);
  boolean (*click)(context *, int *key, int button, int x, int y);
  boolean (*drag)(context *, int *key, int button, int x, int y);
  void (*destroy)(context *);
  enum framerate_type framerate_mode;
  int priority;
};

/**
 * Sets up a new context and adds it to the context stack. At least one
 * update function must be provided. If "parent" is a subcontext, this function
 * will use its parent context as the parent instead.
 *
 * @param ctx               Optional newly allocated context to be initialized.
 * @param parent            The context which created this context.
 * @param ctx_spec          Specification for context functions/variables.
 * @param context_type      Used to identify contexts (also, by the help system)
 */
CORE_LIBSPEC void create_context(context *ctx, context *parent,
 struct context_spec *ctx_spec, enum context_type context_type);

/**
 * Sets up a new subcontext and adds it to a parent context. A subcontext will
 * be executed at the same time as its parent context and will be terminated if
 * its parent context is terminated. Each context operation will execute as
 * follows:
 *
 * Resume:  parent, oldest subcontext -> newest subcontext
 * Draw:    parent, oldest subcontext -> newest subcontext
 * Update:  parent, oldest subcontext -> newest subcontext (see priority above)
 * Destroy: newest subcontext -> oldest subcontext, parent
 *
 * If "parent" is a subcontext, this function will use the parent of that
 * subcontext as the parent of the new subcontext instead.
 *
 * @param sub               Optional newly allocated subcontext.
 * @param parent            The context which created this context.
 * @param sub_spec          Specification for subcontext functions/variables.
 */
CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 struct context_spec *sub_spec);

/**
 * Destroys a context and its subcontexts. This will call the destroy function
 * for every subcontext destroyed, and finally for this context itself.
 * Subcontexts will be destroyed from the top of the stack down. If given a
 * subcontext, this will destroy the given subcontext only.
 *
 * @param ctx           The context or subcontext to destroy.
 */
CORE_LIBSPEC void destroy_context(context *ctx);

/**
 * Determine if a context change has occurred.
 *
 *
 * @param ctx           Current context
 * @return              true if a context change has occurred.
 */
CORE_LIBSPEC boolean has_context_changed(context *ctx);

/**
 * Determine if the given context matches an expected type.
 *
 * @param ctx           A context
 * @param context_type  A context type
 * @return              true if the context has the given context type.
 */
CORE_LIBSPEC boolean is_context(context *ctx, enum context_type context_type);

/**
 * Add a callback to a context if a context change has occurred. This callback
 * will be executed when focus returns to the given context, but before the
 * resume function of the context is executed. Multiple callbacks may be
 * provided to a context; they will be executed in the order they were added.
 * If this function receives a subcontext pointer, this callback will be added
 * to the parent context but the subcontext will be used as the parameter.
 *
 * If no change of context has occurred, this function is executed immediately
 * instead of during the context resume process.
 *
 * @param ctx           The context to assign this callback to.
 * @param param         A parameter to be provided to the callback.
 * @param func          The callback to be executed.
 */
CORE_LIBSPEC void context_callback(context *ctx, context_callback_param *param,
 context_callback_function func);

/**
 * Sets the framerate mode of the current context. See enum framerate_type for
 * a list of valid values. This function will error if given a subcontext.
 *
 * @param ctx           The current context.
 * @param framerate     The new framerate mode.
 */
void set_context_framerate_mode(context *ctx, enum framerate_type framerate);

/**
 * Initializes data for the core.
 *
 * @param mzx_world     MegaZeux world information struct.
 * @param data          Global information struct.
 * @return              A core context struct.
 */
CORE_LIBSPEC core_context *core_init(struct world *mzx_world);

/**
 * Run MegaZeux using given context stack information.
 *
 * @param root          The core context to run.
 */
CORE_LIBSPEC void core_run(core_context *root);

/**
 * Signal the core to abort all contexts and exit the current game loop.
 *
 * @param ctx           The current context.
 */
CORE_LIBSPEC void core_full_exit(context *ctx);

/**
 * Signal the core to abort all contexts, exit the loop, and restart MegaZeux.
 *
 * @param ctx           The current context.
 */
CORE_LIBSPEC void core_full_restart(context *ctx);

/**
 * Determine if the a restart was requested.
 *
 * @param root          The core context.
 * @return              True if a restart was requested.
 */
CORE_LIBSPEC boolean core_restart_requested(core_context *root);

/**
 * Clean up MegaZeux all context data for a given core context.
 *
 * @param root          The core context to free.
 */
CORE_LIBSPEC void core_free(core_context *root);

// Deprecated functions.

CORE_LIBSPEC enum context_type get_context(context *);
CORE_LIBSPEC void set_context(enum context_type idx);
CORE_LIBSPEC void pop_context(void);

// Editor external function pointers.

CORE_LIBSPEC extern void (*edit_world)(context *parent,
 boolean reload_curr_file);
CORE_LIBSPEC extern void (*debug_counters)(context *ctx);
CORE_LIBSPEC extern void (*draw_debug_box)(struct world *mzx_world,
 int x, int y, int d_x, int d_y, int show_keys);
CORE_LIBSPEC extern int (*debug_robot_break)(context *ctx,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern int (*debug_robot_watch)(context *ctx,
 struct robot *cur_robot, int id, int lines_run);
CORE_LIBSPEC extern void (*debug_robot_config)(struct world *mzx_world);
CORE_LIBSPEC extern void (*debug_robot_reset)(struct world *mzx_world);

// Network external function pointers.

CORE_LIBSPEC extern boolean (*check_for_updates)(context *ctx,
 boolean is_automatic);

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_CORE_H */

/* MegaZeux
 *
 * Copyright (C) 2018-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

/* MegaZeux main loop and context handling code. */

#include <stdlib.h>
#include <string.h>

#include "caption.h"
#include "counter.h"
#include "configure.h"
#include "core.h"
#include "error.h"
#include "game_menu.h"
#include "graphics.h"
#include "helpsys.h"
#include "settings.h"
#include "util.h"
#include "world.h"
#include "world_struct.h"

#include "event/event.h"
#include "platform/platform.h"

#define MAX_NUM_CALLBACKS 8

static int unique = 0;

struct context_stack
{
  context **contents;
  int alloc;
  int size;
  int pos;
  boolean removed;
};

/**
 * Used as a context to help create the first actual context.
 * Stores the context stack.
 */
struct core_context
{
  context ctx;
  boolean first_run;
  boolean full_exit;
  boolean restart_on_exit;
  boolean context_changed;
  struct context_stack stack;
};

/**
 * Contains data for callbacks added to a context.
 */
struct context_callback_data
{
  context *ctx_for_callback;
  context_callback_param *param_for_callback;
  context_callback_function callback;
};


/**
 * Contains internal context data used to run contexts.
 */
struct context_data
{
  context *parent;
  boolean is_subcontext;
  enum context_type context_type;
  enum framerate_type framerate;
  struct context_stack stack;
  struct context_spec functions;
  struct context_callback_data callbacks[MAX_NUM_CALLBACKS];
  int cur_callback;
  int num_callbacks;
  int unique;
  int priority;
};

#ifdef CONFIG_FPS
#define FPS_HISTORY_SIZE 5
#define FPS_INTERVAL 1000
#include <math.h>

struct fps_history_entry
{
  int frames;
  int delta;
  double ratio;
};

static double average_fps;

/**
 * Track the current speed MegaZeux is running in cycles.
 */
static void update_fps(int64_t current_ticks)
{
  static struct fps_history_entry history[FPS_HISTORY_SIZE];
  static int64_t fps_previous_ticks = -1;
  static int frames_counted;
  static int pos;
  int i;

  int delta_ticks = current_ticks - fps_previous_ticks;
  frames_counted++;

  if(fps_previous_ticks == -1)
  {
    fps_previous_ticks = current_ticks;
  }
  else

  if(delta_ticks >= FPS_INTERVAL)
  {
    double min_ratio = INFINITY;
    double max_ratio = -INFINITY;
    int min_pos = 0;
    int max_pos = 0;
    int total_frames = 0;
    int total_delta = 0;
    int count = 0;

    history[pos].frames = frames_counted;
    history[pos].delta = delta_ticks;
    history[pos].ratio = (double)frames_counted / (double)delta_ticks;
    if((++pos) >= FPS_HISTORY_SIZE)
      pos = 0;

    for(i = 0; i < FPS_HISTORY_SIZE; i++)
    {
      if(history[i].delta <= 0)
        continue;

      if(history[i].ratio > max_ratio)
      {
        max_ratio = history[i].ratio;
        max_pos = i;
      }
      if(history[i].ratio < min_ratio)
      {
        min_ratio = history[i].ratio;
        min_pos = i;
      }
      total_frames += history[i].frames;
      total_delta += history[i].delta;
      count++;
    }
    if(count > 2)
    {
      // Subtract off highest and lowest scores (outliers)
      total_frames -= history[min_pos].frames + history[max_pos].frames;
      total_delta -= history[min_pos].delta + history[max_pos].delta;
      average_fps = (double)total_frames * 1000.0 / (double)total_delta;

      caption_set_fps(average_fps);
    }
    fps_previous_ticks = current_ticks;
    frames_counted = 0;
  }
}
#endif

#define CTX_NAME_MAX_SIZE 16

/**
 * Get the name of a context from its ID (see core.h)
 */
static const char *get_ctx_name(enum context_type id)
{
  switch(id)
  {
    // Core contexts.
    case CTX_TITLE_SCREEN:      return "Title screen";
    case CTX_MAIN:              return "(help main page)";
    case CTX_PLAY_GAME:         return "Gameplay";
    case CTX_CONFIGURE:         return "Settings ed.";
    case CTX_DIALOG_BOX:        return "Dialog";
    case CTX_HELP_SYSTEM:       return "Help system";
    case CTX_MAIN_MENU:         return "Main menu";
    case CTX_GAME_MENU:         return "Game menu";
    case CTX_INTAKE_NUM:        return "(intake number)";
    case CTX_TASK:              return "(running task)";

    // Network contexts.
    case CTX_UPDATER:           return "Updater";

    // Editor contexts.
    case CTX_EDITOR:            return "Editor";
    case CTX_EDITOR_VIEW_BOARD: return "(view board)";
    case CTX_THING_MENU:        return "Thing menu";
    case CTX_BLOCK_CMD:         return "Block command";
    case CTX_BLOCK_TYPE:        return "Block type";
    case CTX_CHOOSE_CHARSET:    return "Select charset";
    case CTX_IMPORTEXPORT_TYPE: return "Import/export";
    case CTX_CHAR_EDIT:         return "Char editor";
    case CTX_STATUS_COUNTERS:   return "Status counters";
    case CTX_BOARD_EXITS:       return "Board exits";
    case CTX_BOARD_SIZES:       return "Board sizes";
    case CTX_BOARD_INFO:        return "Board info";
    case CTX_CHANGE_CHAR_IDS:   return "Char ID table";
    case CTX_CHANGE_DAMAGE:     return "Damage table";
    case CTX_GLOBAL_SETTINGS:   return "Global settings1";
    case CTX_GLOBAL_SETTINGS_2: return "Global settings2";
    case CTX_ROBO_ED:           return "Robot editor";
    case CTX_PALETTE_EDITOR:    return "Palette editor";
    case CTX_SENSOR_EDITOR:     return "Sensor param";
    case CTX_SUPER_MEGAZEUX:    return "Select SMZX mode";
    case CTX_SFX_EDITOR:        return "SFX editor";
    case CTX_COUNTER_DEBUG:     return "Counter debugger";
    case CTX_ROBOT_DEBUG:       return "Robot debugger";
    case CTX_BREAKPOINT_EDITOR: return "Robot dbg. conf.";
    case CTX_VLAYER_SIZES:      return "Vlayer sizes";
  }
  return "?????";
}

/**
 * Debug: print a line of the context stack.
 */
static void print_ctx_line(context_data *ctx_data)
{
  char name[CTX_NAME_MAX_SIZE + 1] = "  -> subcontext";
  char cbs_str[12] = "";
  char pri_str[12] = "";
  const char *framerate_str = "-";
  boolean click_drag_same = false;

  if(!ctx_data->is_subcontext)
  {
    snprintf(name, CTX_NAME_MAX_SIZE, "%s",
     get_ctx_name(ctx_data->context_type));

    if(ctx_data->num_callbacks)
      snprintf(cbs_str, 12, "%d", ctx_data->num_callbacks);

    switch(ctx_data->framerate)
    {
      case FRAMERATE_UI:            framerate_str = "UI "; break;
      case FRAMERATE_UI_INTERRUPT:  framerate_str = "Int"; break;
      case FRAMERATE_MZX_SPEED:     framerate_str = "MZX"; break;
      default:                      framerate_str = "???"; break;
    }
  }

  if(ctx_data->priority)
  {
    snprintf(pri_str, 12, "%d", ctx_data->priority);
    pri_str[11] = 0;
  }

  if(ctx_data->functions.click == ctx_data->functions.drag)
    click_drag_same = true;

  fprintf(mzxerr, "%-*.*s | %3s %3s %3s %3s %3s %3s %3s %3s | %-3s %3s %s\n",
    16, 16, name,
    ctx_data->functions.resume    ? "Yes" : "",
    ctx_data->functions.draw      ? "Yes" : "",
    ctx_data->functions.idle      ? "Yes" : "",
    ctx_data->functions.key       ? "Yes" : "",
    ctx_data->functions.joystick  ? "Yes" : "",
    ctx_data->functions.click     ? "Yes" : "",
    ctx_data->functions.drag      ? click_drag_same ? "<- " : "Yes" : "",
    ctx_data->functions.destroy   ? "Yes" : "",
    framerate_str,
    cbs_str,
    pri_str
  );
}

/**
 * Debug: print the entire context stack to the terminal.
 */

#define print_core_stack(ctx) __print_core_stack(ctx, __FILE__, __LINE__)

static void __print_core_stack(context *_ctx, const char *file, int line)
{
  core_context *root;
  context_data *ctx_data;
  context_data *sub_data;
  int i;
  int i2;

  if(file)
    fprintf(mzxerr, "%s line %d:\n", file, line);

  if(!_ctx)
  {
    fprintf(mzxerr, "Context is NULL!\n");
    fflush(mzxerr);
    return;
  }
  else

  if(!_ctx->root)
  {
    fprintf(mzxerr, "Context root is NULL!\n");
    fflush(mzxerr);
    return;
  }

  root = _ctx->root;

  fprintf(mzxerr,
   "CONTEXT STACK    | Res Drw Idl Key Joy Clk Drg Dst | Fr. CbQ Pri\n"
   "-----------------|---------------------------------|-------------\n");

  for(i = root->stack.size - 1; i >= 0; i--)
  {
    ctx_data = root->stack.contents[i]->internal_data;

    for(i2 = ctx_data->stack.size - 1; i2 >= 0; i2--)
    {
      sub_data = ctx_data->stack.contents[i2]->internal_data;
      print_ctx_line(sub_data);
    }

    print_ctx_line(ctx_data);
  }
  fprintf(mzxerr, "\n");
  fflush(mzxerr);
}

/**
 * Add an element to a stack
 */
static void add_stack(struct context_stack *stack, context *add)
{
  if(stack->size >= stack->alloc)
  {
    if(stack->alloc == 0)
      stack->alloc = 8;

    while(stack->size >= stack->alloc)
      stack->alloc *= 2;

    stack->contents = crealloc(stack->contents, stack->alloc * sizeof(void *));
  }

  stack->contents[stack->size] = add;
  stack->size++;
}

/**
 * Remove an element from a stack.
 */
static int remove_stack(struct context_stack *stack, context *del)
{
  int i;

  for(i = stack->size - 1; i >= 0; i--)
  {
    if(stack->contents[i] == del)
    {
      if(i < stack->size - 1)
        memmove(stack->contents + i, stack->contents + i + 1,
         stack->size - i - 1);

      stack->size--;
      stack->removed = true;
      return i;
    }
  }

  error_message(E_CORE_FATAL_BUG, 6, NULL);
  return -1;
}

/**
 * Configures a provided context and adds it to the context stack to be run
 * by the main loop. The assumption here is that ctx will be some other local
 * struct casted to a context.
 *
 * At least one update function needs to be provided capable of destroying the
 * context; otherwise, MegaZeux would hang up trying to execute this context.
 */
void create_context(context *ctx, context *parent,
 struct context_spec *ctx_spec, enum context_type context_type)
{
  core_context *root;
  context_data *ctx_data;

  if(parent == NULL || !ctx_spec ||
   (ctx_spec->resume == NULL && ctx_spec->draw == NULL &&
    ctx_spec->key == NULL && ctx_spec->joystick == NULL &&
    ctx_spec->click == NULL && ctx_spec->drag == NULL &&
    ctx_spec->idle == NULL))
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 1, NULL);
    return;
  }

  // If the parent is a subcontext, try to find the real parent context.
  while(parent->internal_data && parent->internal_data->parent &&
   parent->internal_data->is_subcontext)
    parent = parent->internal_data->parent;

  // Root needs to exist so this context can be added to the stack.
  if(!parent->root)
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 7, NULL);
    return;
  }

  if(!ctx) ctx = cmalloc(sizeof(struct context));
  ctx_data = cmalloc(sizeof(struct context_data));

  ctx->root = parent->root;
  ctx->internal_data = ctx_data;
  ctx->world = parent->world;
  ctx_data->parent = NULL;
  ctx_data->context_type = context_type;
  ctx_data->framerate = ctx_spec->framerate_mode;
  ctx_data->is_subcontext = false;
  ctx_data->cur_callback = 0;
  ctx_data->num_callbacks = 0;
  ctx_data->unique = unique++;
  ctx_data->priority = ctx_spec->priority;
  memset(&(ctx_data->stack), 0, sizeof(struct context_stack));
  memcpy(&(ctx_data->functions), ctx_spec, sizeof(struct context_spec));

  // Add the new context to the stack.
  root = parent->root;
  add_stack(&(root->stack), ctx);

  root->context_changed = true;
}

/**
 * Creates a subcontext and adds it to the parent context.
 */
CORE_LIBSPEC void create_subcontext(subcontext *sub, context *parent,
 struct context_spec *sub_spec)
{
  core_context *root;
  context_data *parent_data;
  context_data *sub_data;

  // If the parent is a subcontext, try to find the real parent context.
  while(parent && parent->internal_data && parent->internal_data->is_subcontext)
    parent = parent->internal_data->parent;

  // Root context must exit, parent must not be the root context, and make sure
  // this isn't some weird glitched context.
  if(!parent || !parent->root || parent == (context *)(parent->root) ||
   !parent->internal_data || parent->internal_data->parent || !sub_spec)
  {
    print_core_stack(parent);
    error_message(E_CORE_FATAL_BUG, 8, NULL);
    return;
  }

  root = parent->root;
  parent_data = parent->internal_data;

  if(!sub) sub = cmalloc(sizeof(struct context));
  sub_data = cmalloc(sizeof(struct context_data));

  sub->root = root;
  sub->internal_data = sub_data;
  sub->world = parent->world;
  sub_data->parent = parent;
  sub_data->is_subcontext = true;
  sub_data->unique = unique++;
  sub_data->priority = sub_spec->priority;
  memcpy(&(sub_data->functions), sub_spec, sizeof(struct context_spec));

  // Add the subcontext to the parent's stack.
  add_stack(&(parent_data->stack), sub);
}

/**
 * Destroy the target context or subcontext from its parent stack.
 * Flag the core_context to abort further execution of the cycle if the root
 * context stack is changed.
 */
void destroy_context(context *ctx)
{
  core_context *root = ctx->root;
  context_data *ctx_data = ctx->internal_data;

  if(!ctx_data)
    return;

  if(!ctx_data->parent || !ctx_data->is_subcontext)
  {
    // This is a root-level context, so remove it from the context stack.

    // If the context isn't on the stack, this will error.
    remove_stack(&(root->stack), ctx);
    root->context_changed = true;

    // Also, destroy all children.
    if(ctx_data->stack.size)
    {
      int i;
      for(i = ctx_data->stack.size - 1; i >= 0; i--)
        destroy_context(ctx_data->stack.contents[i]);
    }

    free(ctx_data->stack.contents);
  }
  else
  {
    int removed;
    // This is a subcontext, so remove it from its parent context.
    context_data *parent_data = ctx_data->parent->internal_data;

    // If the subcontext isn't on the stack, this will error.
    removed = remove_stack(&(parent_data->stack), ctx);

    // Adjust the current position in case this is mid-iteration.
    if(removed <= parent_data->stack.pos)
      parent_data->stack.pos--;
  }

  if(ctx_data->functions.destroy)
    ctx_data->functions.destroy(ctx);

  free(ctx_data);
  free(ctx);
}

/**
 * Find out if a context change has occurred.
 */
boolean has_context_changed(context *ctx)
{
  if(!ctx || !ctx->root)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 9, NULL);
    return true;
  }

  return (ctx->root->context_changed);
}

/**
 * Find out if the given context is a particular context.
 */
boolean is_context(context *ctx, enum context_type context_type)
{
  if(!ctx || !ctx->internal_data)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 2, NULL);
    return false;
  }

  if(ctx->internal_data->is_subcontext)
    return false;

  return (ctx->internal_data->context_type == context_type);
}

/**
 * Add a resume callback to the context (or its parent). Alternatively, call
 * it immediately if no context change has occurred.
 */
void context_callback(context *ctx, context_callback_param *param,
 context_callback_function func)
{
  context *target = ctx;

  // If the context is a subcontext, try to find its parent context.
  while(target && target->internal_data && target->internal_data->is_subcontext)
    target = target->internal_data->parent;

  if(!target || !target->root || !target->internal_data || !func ||
   target->internal_data->num_callbacks >= MAX_NUM_CALLBACKS)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 10, NULL);
    return;
  }

  // If there has been no context change, execute this callback immediately.
  if(!target->root->context_changed)
    func(ctx, param);

  else
  {
    struct context_data *ctx_data = target->internal_data;
    struct context_callback_data *cb_data;
    int id = (ctx_data->cur_callback + ctx_data->num_callbacks) %
     MAX_NUM_CALLBACKS;

    cb_data = &(ctx_data->callbacks[id]);

    cb_data->callback = func;
    cb_data->ctx_for_callback = ctx;
    cb_data->param_for_callback = param;
    ctx_data->num_callbacks++;
  }
}

/**
 * Set the framerate mode for the current context.
 */
void set_context_framerate_mode(context *ctx, enum framerate_type framerate)
{
  if(!ctx || !ctx->internal_data || ctx->internal_data->is_subcontext)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 3, NULL);
    return;
  }

  ctx->internal_data->framerate = framerate;
}

/**
 * Generate the core_context struct. This struct imitates a regular context so
 * it can be used as a parent, but it should not be started like a context.
 */
core_context *core_init(struct world *mzx_world)
{
  core_context *root = cmalloc(sizeof(core_context));
  context *ctx = (context *)root;

  ctx->root = root;
  ctx->world = mzx_world;
  ctx->internal_data = NULL;

  memset(&(root->stack), 0, sizeof(struct context_stack));
  root->first_run = true;
  root->full_exit = false;
  root->restart_on_exit = false;
  root->context_changed = false;

  return root;
}

/**
 * Execute the next callback for a context and remove it from the list.
 */
static void execute_next_context_callback(context_data *ctx_data)
{
  struct context_callback_data *cb_data;
  int i = ctx_data->cur_callback;

  cb_data = &(ctx_data->callbacks[i]);
  ctx_data->cur_callback = (ctx_data->cur_callback + 1) % MAX_NUM_CALLBACKS;
  ctx_data->num_callbacks--;

  cb_data->callback(cb_data->ctx_for_callback, cb_data->param_for_callback);
}

/**
 * Execute the resume function for the current context after a context change.
 */
static void core_resume(core_context *root)
{
  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;

  // Execute callbacks before doing anything else.
  while(ctx_data->num_callbacks)
  {
    execute_next_context_callback(ctx_data);

    if(root->context_changed || root->full_exit)
      return;
  }

  if(ctx_data->functions.resume)
    ctx_data->functions.resume(ctx);

  if(root->context_changed || root->full_exit)
    return;

  ctx_data->stack.pos = 0;

  while(ctx_data->stack.pos < ctx_data->stack.size)
  {
    sub = ctx_data->stack.contents[ctx_data->stack.pos];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.resume)
      sub_data->functions.resume((context *)sub);

    if(root->context_changed || root->full_exit)
      return;

    ctx_data->stack.pos++;
  }
}

/**
 * Draw the current context.
 */
static boolean core_draw(core_context *root)
{
  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *sub_data;
  subcontext *sub;
  boolean ret = false;

  if(ctx_data->functions.draw)
    ret |= ctx_data->functions.draw(ctx);

  if(root->context_changed || root->full_exit)
    return ret;

  ctx_data->stack.pos = 0;

  while(ctx_data->stack.pos < ctx_data->stack.size)
  {
    sub = ctx_data->stack.contents[ctx_data->stack.pos];
    sub_data = ((context *)sub)->internal_data;

    if(sub_data->functions.draw)
    {
      select_layer(UI_LAYER);
      ret |= sub_data->functions.draw((context *)sub);
    }

    if(root->context_changed || root->full_exit)
      return ret;

    ctx_data->stack.pos++;
  }

#ifdef CONFIG_FPS
  if(video_is_fullscreen() && ctx_data->context_type != CTX_EDITOR)
  {
    // If we're in fullscreen mode, draw an onscreen FPS display.
    char fpsbuf[32];
    snprintf(fpsbuf, 32, "  %.2f  ", average_fps);

    select_layer(UI_LAYER);
    write_string(fpsbuf, 0, 0, 0x0f, false);
    return true;
  }
#endif
  return ret;
}

/**
 * Determine if a given context exists on the stack.
 */
static boolean is_on_stack(core_context *root, enum context_type type)
{
  int i = root->stack.size - 1;
  context *ctx;

  for(; i >= 0; i--)
  {
    ctx = root->stack.contents[i];
    if(ctx->internal_data->context_type == type)
      return true;
  }

  return false;
}

#ifdef CONFIG_HELPSYS
/**
 * Determine if the help system is currently allowed. In addition to the
 * regular checks, this should not be allowed to open if it's already open!
 */
static boolean core_allow_help_system(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  boolean is_titlescreen = !is_on_stack(root, CTX_PLAY_GAME);

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  if(mzx_world->active && !allow_help_system(mzx_world, is_titlescreen))
    return false;

  return true;
}
#endif

/**
 * Determine if the settings menu is currently allowed. In addition to the
 * regular checks, this should not be allowed to open if it's already open!
 */
static boolean core_allow_settings_menu(core_context *root)
{
  struct world *mzx_world = ((context *)root)->world;
  boolean is_titlescreen = !is_on_stack(root, CTX_PLAY_GAME);
  boolean is_override =
   get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal);

  if(is_on_stack(root, CTX_CONFIGURE))
    return false;

  if(is_on_stack(root, CTX_HELP_SYSTEM))
    return false;

  if(mzx_world->active &&
   !allow_settings_menu(mzx_world, is_titlescreen, is_override))
    return false;

  return true;
}

/**
 * Update sort function.
 */
static int core_update_sort_fn(const void *A, const void *B)
{
  const context *a = *((const context **)A);
  const context *b = *((const context **)B);
  const context_data *a_data = a->internal_data;
  const context_data *b_data = b->internal_data;
  int diff = b_data->priority - a_data->priority;
  return diff ? diff : a_data->unique - b_data->unique;
}

/**
 * Update the current context, handling input and idle activity.
 * If any subcontexts are attached to the context, they will also be updated.
 * Notes:
 *
 * 1) Execution order: Idle -> Click OR Drag -> Joystick -> Key
 * 2) A true return value of idle cancels key, mouse, and joystick handling.
 * 3) A true return value of key cancels key handling.
 * 4) A true return value of click or drag cancels mouse handling.
 * 5) A true return value of joystick handling cancels joystick handling.
 *
 * If a subcontext is added during the update, it will not be updated.
 * If a subcontext is removed during the update, the update will terminate.
 */
static void core_update(core_context *root)
{
  context *_update_order[8];
  context **update_order = _update_order;

  context *ctx = root->stack.contents[root->stack.size - 1];
  context_data *ctx_data = ctx->internal_data;
  context_data *cur_data;
  context *cur;

  struct context_stack *stack = &(ctx_data->stack);

  boolean joystick_handled = false;
  boolean mouse_handled = false;
  boolean key_handled = false;
  boolean sorted = true;
  int update_count = stack->size + 1;
  int last_priority = ctx_data->priority;

  boolean exit_status = get_exit_status();
  int key = get_key(keycode_internal_wrt_numlock);
  int joystick = get_joystick_ui_action();
  int mouse_press = get_mouse_press_ext();
  int mouse_drag_state = get_mouse_drag();
  int mouse_x;
  int mouse_y;
  int i;

  get_mouse_position(&mouse_x, &mouse_y);

  // The key handler needs to be called when there is text input but no
  // regular key input. In this situation, use the IKEY_UNICODE keycode.
  if(!key && has_unicode_input())
    key = IKEY_UNICODE;

  if(update_count > (int)ARRAY_SIZE(_update_order))
    update_order = cmalloc(update_count * sizeof(context *));

  update_order[0] = ctx;
  for(i = 0; i < stack->size; i++)
  {
    cur = stack->contents[i];
    update_order[i + 1] = cur;

    if(cur->internal_data->priority > last_priority)
      sorted = false;

    last_priority = cur->internal_data->priority;
  }

  if(!sorted)
    qsort(update_order, update_count, sizeof(context *), core_update_sort_fn);

  stack->removed = false;
  stack->pos = 0;

  for(i = 0; i < update_count; i++)
  {
    cur = update_order[i];
    cur_data = cur->internal_data;

    if(cur_data->functions.idle)
    {
      if(cur_data->functions.idle(cur))
      {
        joystick_handled = true;
        mouse_handled = true;
        key_handled = true;
      }

      if(root->context_changed || root->full_exit || stack->removed)
        break;
    }

    if(!mouse_handled)
    {
      if(mouse_drag_state && cur_data->functions.drag)
      {
        mouse_handled |=
         cur_data->functions.drag(cur, &key, mouse_press, mouse_x, mouse_y);
      }
      else

      if(mouse_press && cur_data->functions.click)
      {
        mouse_handled |=
         cur_data->functions.click(cur, &key, mouse_press, mouse_x, mouse_y);
      }

      if(root->context_changed || root->full_exit || stack->removed)
        break;
    }

    if(!joystick_handled)
    {
      if(joystick && cur_data->functions.joystick)
        joystick_handled |= cur_data->functions.joystick(cur, &key, joystick);

      if(root->context_changed || root->full_exit || stack->removed)
        break;
    }

    if(!key_handled)
    {
      if((key || exit_status) && cur_data->functions.key)
        key_handled |= cur_data->functions.key(cur, &key);

      if(root->context_changed || root->full_exit || stack->removed)
        break;
    }
  }

  if(update_order != _update_order)
    free(update_order);

  // Global key handler.
  if(!root->context_changed && !root->full_exit && !key_handled)
  {
    switch(key)
    {
#ifdef CONFIG_HELPSYS
      case IKEY_F1:
      {
        // Display help.
        if(core_allow_help_system(root))
          help_system(ctx, ctx->world);

        break;
      }
#endif

      case IKEY_F2:
      {
        // Display settings menu.
        if(core_allow_settings_menu(root))
          game_settings(ctx->world);

        break;
      }

      case IKEY_F12:
      {
#ifdef DEBUG
        if(get_alt_status(keycode_internal_wrt_numlock))
        {
          print_core_stack(ctx);
          break;
        }
#endif

#ifdef CONFIG_ENABLE_SCREENSHOTS
        // Take screenshot.
        if(get_config()->allow_screenshots)
          dump_screen();
#endif
        break;
      }
    }
  }
}

/**
 * Run the main game loop.
 */
void core_run(core_context *root)
{
  const struct config_info *conf = get_config();
  context *ctx;
  // FIXME: Because not everything can be converted to use the main loop at
  // once, this function needs to be used from places other than main(). So
  // this doesn't break MZX, this function stops once the number of contexts
  // on the stack has dropped below the initial value.
  int initial_stack_size = root->stack.size;
  int64_t start_ticks;
  int64_t delta_ticks;
  int64_t total_ticks;
  boolean need_update_screen = true;
#ifdef __EMSCRIPTEN__
  uint64_t emscripten_prev_ticks = get_ticks();
#endif

  // If there aren't any contexts on the stack, there's no reason to be here.
  if(initial_stack_size <= 0)
    return;

  // First run-- only terminate if all contexts are terminated.
  if(root->first_run)
  {
    initial_stack_size = 1;
    root->first_run = false;
  }

  // FIXME hack
  enable_f12_hack = conf->allow_screenshots;

  do
  {
    // Resume might trigger additional context changes, exit, or empty the
    // context stack, so continue the main loop after handling the resume.
    if(root->context_changed)
    {
      root->context_changed = false;
      force_release_all_keys();
      core_resume(root);
      continue;
    }

    // Gameplay framerate timing starts immediately before the board update,
    // which must be done in the draw function!
    start_ticks = get_ticks();

#ifdef CONFIG_FPS
    update_fps(start_ticks);
#endif

    need_update_screen = core_draw(root);

    // Context changed or an exit occurred? Skip the screen update and delay
    if(root->context_changed || root->full_exit)
      continue;

    if(need_update_screen)
      update_screen();

    // Delay and then handle events.
    ctx = root->stack.contents[root->stack.size - 1];

    // Special- enable joystick gameplay bindings if this is the gameplay ctx.
    joystick_set_game_mode(ctx->internal_data->context_type == CTX_PLAY_GAME);

    // FIXME legacy loop hacks; remove when legacy loops are gone
    joystick_set_legacy_loop_hacks(false);
    enable_f12_hack = false;
    // FIXME end legacy loop hacks

    switch(ctx->internal_data->framerate)
    {
      case FRAMERATE_UI:
      {
        // Delay for a standard (fixed) amount of time.
        update_event_status_delay();
        break;
      }

      case FRAMERATE_UI_INTERRUPT:
      {
        // Delay for the standard amount of time or until an event is detected.
        // Use for interfaces that need precise keypress detection, like typing.
        update_event_status_intake();
        break;
      }

      case FRAMERATE_MZX_SPEED:
      {
        // Delay according to mzx_speed and execution time.
        if(ctx->world->mzx_speed > 1)
        {
          // Number of ms the update cycle took
          delta_ticks = get_ticks() - start_ticks;
          total_ticks = (16 * (ctx->world->mzx_speed - 1)) - delta_ticks;
          if(total_ticks < 0)
            total_ticks = 0;

          // Delay for 16 * (speed - 1) since the beginning of the update
          delay(total_ticks);
        }
#ifdef __EMSCRIPTEN__
        else
        {
          // Emscripten must yield occasionally, or else the asynchronous queue
          // will never get processed, causing a freeze.
          delta_ticks = get_ticks() - emscripten_prev_ticks;
          if(delta_ticks >= 8) // 16 - 8 = 8
          {
            delay(8);
            emscripten_prev_ticks = get_ticks();
          }
        }
#endif

        update_event_status();
        break;
      }

      default:
      {
        print_core_stack(ctx);
        error_message(E_CORE_FATAL_BUG, 5, NULL);
        break;
      }
    }

    // FIXME legacy loop hacks; remove when legacy loops are gone
    joystick_set_game_mode(false);
    joystick_set_legacy_loop_hacks(true);
    enable_f12_hack = conf->allow_screenshots;
    // FIXME end legacy loop hacks

    core_update(root);
  }
  while(!root->full_exit && root->stack.size >= initial_stack_size);

  // Reset first run variable if the stack empties.
  if(!root->stack.size)
    root->first_run = true;
}

/**
 * Indicate that the core loop should exit.
 */
void core_full_exit(context *ctx)
{
  if(!ctx || !ctx->root)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 4, NULL);
    return;
  }

  ctx->root->full_exit = true;
}

/**
 * Exit the core loop and restart MegaZeux.
 */
void core_full_restart(context *ctx)
{
  if(!ctx || !ctx->root)
  {
    print_core_stack(ctx);
    error_message(E_CORE_FATAL_BUG, 11, NULL);
    return;
  }

  ctx->root->full_exit = true;
  ctx->root->restart_on_exit = true;
}

/**
 * Find out if an exit has been requested.
 */
boolean core_restart_requested(core_context *root)
{
  if(!root)
  {
    error_message(E_CORE_FATAL_BUG, 12, NULL);
    return false;
  }

  return root->restart_on_exit;
}

/**
 * Free all context data that still exists.
 */
void core_free(core_context *root)
{
  int i;

  // Destroy all contexts on the stack.
  for(i = root->stack.size - 1; i >= 0; i--)
    destroy_context(root->stack.contents[i]);

  free(root->stack.contents);
  free(root);
}

// Deprecated.
static enum context_type indices[128] = { CTX_MAIN };
static int curr_index = 0;

/**
 * Get the most recent context ID associated with the help system.
 * TODO rename after the deprecated features are no longer necessary.
 */
enum context_type get_context(context *ctx)
{
  if(!curr_index && ctx)
  {
    core_context *root = ctx->root;
    enum context_type ctx_type;
    int i;

    for(i = root->stack.size - 1; i >= 0; i--)
    {
      ctx_type = root->stack.contents[i]->internal_data->context_type;

      // Help system only cares about positive context values.
      if(ctx_type > 0)
        return ctx_type;
    }

    return CTX_MAIN;
  }

  return indices[curr_index];
}

// Deprecated.
void set_context(enum context_type idx)
{
  curr_index++;
  indices[curr_index] = idx;
}

// Deprecated.
void pop_context(void)
{
  if(curr_index > 0)
    curr_index--;
}

// Editor external function pointers (NULL by default).

void (*edit_world)(context *parent, boolean reload_curr_file);
void (*debug_counters)(context *ctx);
void (*draw_debug_box)(struct world *mzx_world, int x, int y, int d_x, int d_y,
 int show_keys);
int (*debug_robot_break)(context *ctx, struct robot *cur_robot,
 int id, int lines_run);
int (*debug_robot_watch)(context *ctx, struct robot *cur_robot,
 int id, int lines_run);
void (*debug_robot_config)(struct world *mzx_world);
void (*debug_robot_reset)(struct world *mzx_world);

// Network external function pointers (NULL by default).

boolean (*check_for_updates)(context *ctx, boolean is_automatic);

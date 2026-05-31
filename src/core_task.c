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

/* Context that blocks and draws a progress meter while waiting for a task.
 * If possible, the task will be run asynchronously in a second thread. */

#include "core_task.h"

#include "graphics.h"
#include "util.h"
#include "window.h"
#include "event/event.h"
#include "platform/platform.h"

#ifndef PLATFORM_NO_THREADING
#define TASK_THREADED
#endif

struct task_context
{
  context ctx;
#ifdef TASK_THREADED
  platform_thread thd;
  platform_mutex lock;
#endif
  uint64_t ticks;
  char title[80];
  int progress;
  int progress_max;
  boolean async;
  boolean redraw;
  boolean cancel;
  boolean first_draw;
  boolean done;
  boolean return_status;
  boolean (*task_callback)(context *ctx, void *priv);
  void (*task_complete)(void *priv, boolean ret);
  void *task_priv;
};

static boolean task_lock(struct task_context *task)
{
#ifdef TASK_THREADED
  if(task->async)
    return platform_mutex_lock(&task->lock);
#endif
  return true;
}

static boolean task_unlock(struct task_context *task)
{
#ifdef TASK_THREADED
  if(task->async)
    return platform_mutex_unlock(&task->lock);
#endif
  return true;
}

static boolean task_draw(context *ctx)
{
  struct task_context *task = (struct task_context *)ctx;
  char title[80];
  int progress;
  int progress_max;
  boolean done;
  boolean no_draw;
  boolean redraw;

  task_lock(task);
  done = task->done;
  no_draw = task->first_draw || task->cancel;
  redraw = task->redraw;
  progress = task->progress;
  progress_max = task->progress_max;

  if(no_draw)
  {
    task->first_draw = false;
  }
  else

  if(redraw)
  {
    memcpy(title, task->title, sizeof(title));
    task->redraw = false;
  }
  task_unlock(task);

  if(done)
  {
    destroy_context((context *)task);
    return true;
  }

  /* Modern users expect no save/load meter: skip the first would-be frame. */
  if(no_draw)
    return true;

  if(redraw)
    meter(title, progress, progress_max);
  else
    meter_interior(progress, progress_max);

  return true;
}

static boolean task_key(context *ctx, int *key)
{
  struct task_context *task = (struct task_context *)ctx;

  // Request cancelation--the task may or may not check this.
  if(get_exit_status() || *key == IKEY_ESCAPE)
  {
    task_lock(task);
    task->cancel = true;
    task_unlock(task);
  }

  // capture all other inputs, including help file, settings
  if(*key != IKEY_F12)
    return true;

  return false;
}

static void task_destroy(context *ctx)
{
  struct task_context *task = (struct task_context *)ctx;
#ifdef TASK_THREADED
  if(task->async)
  {
    task_lock(task);
    task->cancel = true;
    task_unlock(task);
    platform_thread_join(&task->thd);
    platform_mutex_destroy(&task->lock);
  }
#endif
  task->task_complete(task->task_priv, task->return_status);
}

/**
 * Call during a task to update the meter. Called from task thread if threaded.
 * If the task is running synchronously, either because the task initialization
 * failed to create a thread or because the platform does not support
 * threading, this function may update the screen and poll events. Because of
 * this, tasks should call this function regularly.
 *
 * @param ctx           the task context provided to the task callback.
 * @param progress      the current progress of the task.
 * @param progress_max  expected final value of the progress of the task.
 * @param status        new title for the progress bar (`NULL` for no change).
 * @returns             `true` if the task should continue, otherwise `false`.
 */
boolean core_task_tick(context *ctx,
 int progress, int progress_max, const char *status)
{
  struct task_context *task = (struct task_context *)ctx;

  if(!task_lock(task))
    return false;

  task->progress = MIN(progress, progress_max);
  task->progress_max = progress_max;
  if(status)
  {
    snprintf(task->title, sizeof(task->title), "%s", status);
    task->redraw = true;
  }
  task_unlock(task);

#ifdef TASK_THREADED
  if(!task->async)
#endif
  {
    uint64_t ticks = get_ticks();
    if(ticks - task->ticks >= UPDATE_DELAY)
    {
      int key;
      task->ticks = ticks;
      task_draw(ctx);
      update_screen();
      update_event_status();
      key = get_key(keycode_internal_wrt_numlock);
      task_key(ctx, &key);
    }
  }
  return (task->cancel == false);
}

static THREAD_RES task_execute(void *priv)
{
  struct task_context *task = (struct task_context *)priv;
  boolean status = task->task_callback((context *)task, task->task_priv);

  task_lock(task);
  task->return_status = status;
  task->done = true;
  task_unlock(task);

  THREAD_RETURN;
}

/**
 * Create a context which blocks and executes a task. This context displays a
 * simple progress meter and allows the task to be canceled. The task itself
 * will be executed asynchronous from the main loop, if possible. Because of
 * this, the caller should ensure the task owns ALL data it will use during
 * execution. The task should regularly call `core_task_tick` to provide
 * progress information to the user and to update the screen (if synchronous).
 *
 * @param parent          parent context for the new task.
 * @param title           initial title of the progress bar.
 * @param task_callback   called to start execution of the task. Caller
 *                        should assume this will be called in a new thread.
 * @param task_complete   called after the execution of the task by the
 *                        main thread. Caller should should assume the task
 *                        context has been destroyed.
 * @param priv            private data for the task callbacks.
 */
void core_task_context(context *parent, const char *title,
 boolean (*task_callback)(context *ctx, void *priv),
 void (*task_complete)(void *priv, boolean ret), void *priv)
{
  struct task_context *task =
   (struct task_context *)ccalloc(1, sizeof(struct task_context));
  struct context_spec spec;
  if(!task)
    return;

  task->redraw = true;
  task->first_draw = true;
  task->ticks = get_ticks();
  task->task_callback = task_callback;
  task->task_complete = task_complete;
  task->task_priv = priv;

#ifdef TASK_THREADED
  if(platform_mutex_init(&task->lock))
    task->async = true;
#endif

  memset(&spec, 0, sizeof(spec));
  spec.draw     = task_draw;
  spec.key      = task_key;
  spec.destroy  = task_destroy;

  create_context((context *)task, parent, &spec, CTX_TASK);
  core_task_tick((context *)task, 0, 1, title);

#ifdef TASK_THREADED
  if(task->async)
  {
   if(platform_thread_create(&task->thd, task_execute, task))
      return;

    platform_mutex_destroy(&task->lock);
    task->async = false;
  }
  warn("falling back to synchronous task execution--report this!\n");
#endif
  /* Do not allow synchronous tasks in HTML5 */
#ifndef __EMSCRIPTEN__
  task_execute(task);
#endif
  destroy_context((context *)task);
}

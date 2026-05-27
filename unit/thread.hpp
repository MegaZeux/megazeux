/* MegaZeux
 *
 * Copyright (C) 2023-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * Lazy tests to verify thread.h wrappers are implemented correctly.
 * This needs to be in a header so the full test (SDL, pthread, etc.) and the
 * limited Win32 test (utils only) can include different headers.
 */

#include "Unit.hpp"

#ifndef WIN32_FALLBACK_TEST
#undef SKIP_SDL
#endif

#include "../src/platform.h"
#include "../src/util.h"

#define NUM_THREADS 3
#define REPEAT_TIMES 100

static THREAD_RES thread_basic_fn(void *opaque)
{
  boolean *it_worked = reinterpret_cast<boolean *>(opaque);
  *it_worked = true;
  THREAD_RETURN;
}

#ifdef WIN32_FALLBACK_TEST
UNITTEST(ThreadWin32)
#else
UNITTEST(Thread)
#endif
{
  platform_thread thread;
  boolean check = false;
  boolean ret;

  ret = platform_thread_create(&thread, thread_basic_fn, &check);
  ASSERT(ret, "thread create failed");
  platform_thread_join(&thread);

  ASSERTEQ(check, true, "thread failed to run or thread join failed");
}


struct mutex_data
{
  platform_mutex lock;
  boolean lock_ret_false;
  boolean unlock_ret_false;
  boolean bad_value;
  int check;
};

static THREAD_RES mutex_fn(void *opaque)
{
  struct mutex_data *d = reinterpret_cast<struct mutex_data *>(opaque);

  for(int i = 0; i < REPEAT_TIMES; i++)
  {
    if(!platform_mutex_lock(&(d->lock)))
      d->lock_ret_false = true;

    // nonatomic without mutex
    int tmp = d->check;
    if((++d->check) != (tmp + 1))
      d->bad_value = true;

    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;
  }
  THREAD_RETURN;
}

#ifdef WIN32_FALLBACK_TEST
UNITTEST(MutexWin32)
#else
UNITTEST(Mutex)
#endif
{
  platform_thread threads[NUM_THREADS];
  struct mutex_data d{};
  boolean ret;
  int i;

  ret = platform_mutex_init(&(d.lock));
  ASSERT(ret, "failed to init lock");

  for(i = 0; i < NUM_THREADS; i++)
  {
    ret = platform_thread_create(&threads[i], mutex_fn, &d);
    ASSERT(ret, "thread %d", i);
  }
  for(i = 0; i < NUM_THREADS; i++)
    platform_thread_join(&threads[i]);

  platform_mutex_destroy(&(d.lock));

  ASSERTEQ(d.lock_ret_false, false, "lock() returned false");
  ASSERTEQ(d.unlock_ret_false, false, "unlock() returned false");
  ASSERTEQ(d.bad_value, false, "race condition in increment");
  ASSERTEQ(d.check, NUM_THREADS * REPEAT_TIMES, "incorrect final value");
}


struct cond_data
{
  platform_mutex lock;
  platform_cond cond;

  uint8_t buffer[277];
  unsigned sum[NUM_THREADS];
  unsigned expected;
  unsigned iter_read[NUM_THREADS];
  unsigned iter_write;
  unsigned iter_max;
  unsigned num_readers;
  unsigned thread_idx;
  boolean is_write;

  boolean lock_ret_false;
  boolean unlock_ret_false;
  boolean wait_ret_false;
  boolean broadcast_ret_false;
};

static THREAD_RES cond_read_fn(void *opaque)
{
  struct cond_data *d = reinterpret_cast<struct cond_data *>(opaque);
  size_t i;
  int num;

  if(!platform_mutex_lock(&(d->lock)))
    d->lock_ret_false = true;

  num = d->thread_idx;
  d->thread_idx++;

  while(d->iter_read[num] < d->iter_max)
  {
    while(d->is_write || d->iter_read[num] >= d->iter_write)
    {
      if(!platform_cond_wait(&(d->cond), &(d->lock)))
        d->wait_ret_false = true;
    }
    d->num_readers++;

    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;

    for(i = 0; i < ARRAY_SIZE(d->buffer); i++)
      d->sum[num] += d->buffer[i];

    if(!platform_mutex_lock(&(d->lock)))
      d->lock_ret_false = true;

    d->iter_read[num]++;
    d->num_readers--;

    if(!platform_cond_broadcast(&(d->cond)))
      d->broadcast_ret_false = true;
  }
  platform_mutex_unlock(&(d->lock));
  THREAD_RETURN;
}

static THREAD_RES cond_write_fn(void *opaque)
{
  struct cond_data *d = reinterpret_cast<struct cond_data *>(opaque);
  unsigned current = 0;
  size_t i;

  if(!platform_mutex_lock(&(d->lock)))
    d->lock_ret_false = true;

  while(d->iter_write < d->iter_max)
  {
    while(d->num_readers)
    {
      if(!platform_cond_wait(&(d->cond), &(d->lock)))
        d->wait_ret_false = true;
    }
    boolean up_to_date = true;
    for(i = 0; i < NUM_THREADS; i++)
      if(d->iter_read[i] < d->iter_write)
        up_to_date = false;

    if(!up_to_date)
    {
      if(!platform_cond_wait(&(d->cond), &(d->lock)))
        d->wait_ret_false = true;
      continue;
    }
    d->is_write = true;

    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;

    for(i = 0; i < ARRAY_SIZE(d->buffer); i++)
    {
      d->buffer[i] = (current++);
      d->expected += d->buffer[i];
    }

    if(!platform_mutex_lock(&(d->lock)))
      d->lock_ret_false = true;

    d->iter_write++;
    d->is_write = false;

    if(!platform_cond_broadcast(&(d->cond)))
      d->broadcast_ret_false = true;
  }
  platform_mutex_unlock(&(d->lock));
  THREAD_RETURN;
}

#ifdef WIN32_FALLBACK_TEST
UNITTEST(CondWin32)
#else
UNITTEST(Cond)
#endif
{
  platform_thread workers[NUM_THREADS];
  platform_thread ctrl;
  struct cond_data d{};
  boolean ret;
  int i;

#if defined(_WIN32) && defined(SKIP_SDL)
  // Not currently implemented.
  SKIP();
#endif

  d.iter_max = REPEAT_TIMES;

  ret = platform_mutex_init(&(d.lock));
  ASSERT(ret, "failed to init lock");
  ret = platform_cond_init(&(d.cond));
  ASSERT(ret, "failed to init cond");

  ret = platform_thread_create(&ctrl, cond_write_fn, &d);
  ASSERT(ret, "ctrl");

  for(i = 0; i < NUM_THREADS; i++)
  {
    ret = platform_thread_create(&workers[i], cond_read_fn, &d);
    ASSERT(ret, "worker %d", i);
  }

  platform_thread_join(&ctrl);
  for(i = 0; i < NUM_THREADS; i++)
    platform_thread_join(&workers[i]);

  platform_cond_destroy(&(d.cond));
  platform_mutex_destroy(&(d.lock));

  for(i = 0; i < NUM_THREADS; i++)
    ASSERTEQ(d.sum[i], d.expected, "incorrect value");

  ASSERTEQ(d.lock_ret_false, false, "lock() returned false");
  ASSERTEQ(d.unlock_ret_false, false, "unlock() returned false");
  ASSERTEQ(d.wait_ret_false, false, "wait() returned false");
  ASSERTEQ(d.broadcast_ret_false, false, "broadcast() returned false");
}


#define NUM_WORK (NUM_THREADS) * 2
struct semaphore_data
{
  platform_mutex lock;
  platform_sem worker_sem;
  platform_sem ctrl_sem;

  boolean ready[NUM_WORK];
  int buffer[NUM_WORK];
  int next_idx;
  int input_idx;
  boolean need_exit;

  boolean lock_ret_false;
  boolean unlock_ret_false;
  boolean wait_ret_false;
  boolean post_ret_false;
};

static THREAD_RES semaphore_worker_fn(void *opaque)
{
  struct semaphore_data *d = reinterpret_cast<struct semaphore_data *>(opaque);
  int i;

  if(!platform_mutex_lock(&(d->lock)))
    d->lock_ret_false = true;

  while(!d->need_exit)
  {
    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;
    if(!platform_sem_wait(&(d->worker_sem)))
      d->wait_ret_false = true;
    if(!platform_mutex_lock(&(d->lock)))
      d->lock_ret_false = true;

    if(d->need_exit)
      break;

    int idx = d->next_idx;
    d->next_idx++;
    if(d->next_idx >= NUM_WORK)
      d->next_idx = 0;

    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;

    // Do "work"
    for(i = 0; i < REPEAT_TIMES; i++)
      d->buffer[idx]++;

    if(!platform_mutex_lock(&(d->lock)))
      d->lock_ret_false = true;

    d->ready[idx] = true;

    if(!platform_sem_post(&(d->ctrl_sem)))
      d->post_ret_false = true;
  }
  platform_mutex_unlock(&(d->lock));
  THREAD_RETURN;
}

static THREAD_RES semaphore_ctrl_fn(void *opaque)
{
  struct semaphore_data *d = reinterpret_cast<struct semaphore_data *>(opaque);
  int num_out = 0;
  int num_in = 0;
  int i;

  while(num_out < NUM_WORK * REPEAT_TIMES)
  {
    if(!platform_mutex_lock(&d->lock))
      d->lock_ret_false = true;

    // Consume finished work in-order (y4m2smzx does this)
    int idx = d->input_idx;
    boolean ready = d->ready[idx];

    if(num_in >= NUM_WORK && !ready)
    {
      if(!platform_mutex_unlock(&(d->lock)))
        d->unlock_ret_false = true;
      if(!platform_sem_wait(&(d->ctrl_sem)))
        d->wait_ret_false = true;
      continue;
    }

    d->input_idx++;
    if(d->input_idx >= NUM_WORK)
      d->input_idx = 0;

    if(!platform_mutex_unlock(&(d->lock)))
      d->unlock_ret_false = true;

    if(num_in >= NUM_WORK)
    {
      // Consume
      num_out++;
    }

    if(num_in >= NUM_WORK * REPEAT_TIMES)
      continue;

    // Prepare new "work"
    d->buffer[idx]++;
    d->ready[idx] = false;
    num_in++;

    if(!platform_sem_post(&(d->worker_sem)))
      d->post_ret_false = true;
  }

  if(!platform_mutex_lock(&(d->lock)))
    d->lock_ret_false = true;
  d->need_exit = true;
  if(!platform_mutex_unlock(&(d->lock)))
    d->unlock_ret_false = true;

  for(i = 0; i < NUM_THREADS; i++)
  {
    if(!platform_sem_post(&(d->worker_sem)))
      d->post_ret_false = true;
  }
  THREAD_RETURN;
}

#ifdef WIN32_FALLBACK_TEST
UNITTEST(SemaphoreWin32)
#else
UNITTEST(Semaphore)
#endif
{
  platform_thread workers[NUM_THREADS];
  platform_thread ctrl;
  struct semaphore_data d{};
  boolean ret;
  int i;

  ret = platform_mutex_init(&(d.lock));
  ASSERT(ret, "failed to init lock");
  ret = platform_sem_init(&(d.worker_sem), 0);
  ASSERT(ret, "failed to init worker semaphore");
  ret = platform_sem_init(&(d.ctrl_sem), 0);
  ASSERT(ret, "failed to init ctrl semaphore");

  for(i = 0; i < NUM_THREADS; i++)
  {
    ret = platform_thread_create(&workers[i], semaphore_worker_fn, &d);
    ASSERT(ret, "worker %d", i);
  }
  for(i = 0; i < NUM_WORK; i++)
  {
    ASSERTEQ(d.buffer[i], 0, "worker did not block");
    d.ready[i] = true;
  }
  ret = platform_thread_create(&ctrl, semaphore_ctrl_fn, &d);
  ASSERT(ret, "ctrl");

  platform_thread_join(&ctrl);
  for(i = 0; i < NUM_THREADS; i++)
    platform_thread_join(&workers[i]);

  platform_sem_destroy(&(d.worker_sem));
  platform_sem_destroy(&(d.ctrl_sem));
  platform_mutex_destroy(&(d.lock));

  for(i = 0; i < NUM_WORK; i++)
    ASSERTEQ(d.buffer[i], REPEAT_TIMES * (REPEAT_TIMES + 1), "incorrect value");

  ASSERTEQ(d.lock_ret_false, false, "lock() returned false");
  ASSERTEQ(d.unlock_ret_false, false, "unlock() returned false");
  ASSERTEQ(d.wait_ret_false, false, "wait() returned false");
  ASSERTEQ(d.post_ret_false, false, "post() returned false");
}

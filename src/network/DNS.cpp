/* MegaZeux
 *
 * Copyright (C) 2018-2023 Alice Rowan <petrifiedrowan@gmail.com>
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

// Asynchronous wrapper for getaddrinfo. This was easier to implement than
// trying to work with platform-dependent solutions.

#include <assert.h>
#include <stdlib.h>

#include "DNS.hpp"
#include "Socket.hpp"

#include "../configure.h"
#include "../util.h"
#include "../platform/platform.h"

#define LOCK(d)         platform_mutex_lock(&(d->mutex))
#define UNLOCK(d)       platform_mutex_unlock(&(d->mutex))
#define WAIT(d)         platform_cond_wait(&(d->cond), &(d->mutex))
#define TIMED_WAIT(d,t) platform_cond_timedwait(&(d->cond), &(d->mutex), t)
#define SIGNAL(d)       platform_cond_signal(&(d->cond))

enum dns_state
{
  STATE_INIT = 0,
  STATE_EXIT,
  STATE_STANDBY,
  STATE_LOOKUP,
  STATE_SUCCESS,
  STATE_ABORT,
  NUM_DNS_STATES
};

struct dns_data
{
  platform_thread thread;
  platform_mutex mutex;
  platform_cond cond;
  enum dns_state state;
  char *node;
  char *service;
  struct addrinfo hints;
  struct addrinfo *res;
  int ret;
  boolean has_hints;
};

static struct dns_data *threads;
static int threads_count;
static int threads_max;
static int init_ref_count;

static void set_dns_thread_data(struct dns_data *data,
 const char *node, const char *service, const struct addrinfo *hints)
{
  size_t node_len = strlen(node) + 1;
  size_t service_len = strlen(service) + 1;

  data->node = (char *)cmalloc(node_len);
  data->service = (char *)cmalloc(service_len);
  data->has_hints = false;
  memcpy(data->node, node, node_len);
  memcpy(data->service, service, service_len);
  memset(&(data->hints), 0, sizeof(struct addrinfo));
  if(hints)
  {
    data->hints.ai_flags = hints->ai_flags;
    data->hints.ai_family = hints->ai_family;
    data->hints.ai_socktype = hints->ai_socktype;
    data->hints.ai_protocol = hints->ai_protocol;
    data->has_hints = true;
  }
  data->res = NULL;
}

static void free_dns_thread_data(struct dns_data *data, boolean free_result)
{
  free(data->node);
  free(data->service);
  if(free_result && data->res)
    Socket::freeaddrinfo(data->res);

  data->node = NULL;
  data->service = NULL;
  data->res = NULL;
}

static THREAD_RES run_dns_thread(void *_data)
{
  struct dns_data *data = (struct dns_data *)_data;
  int ret = -1;

  trace("--DNS-- New thread running.\n");
  LOCK(data);
  data->state = STATE_STANDBY;

  do
  {
    SIGNAL(data);
    WAIT(data);
    UNLOCK(data);

    if(data->state == STATE_LOOKUP)
    {
      trace("--DNS-- Starting lookup.\n");
      ret = Socket::getaddrinfo(data->node, data->service,
       data->has_hints ? &(data->hints) : nullptr, &(data->res));
    }

    if(data->state == STATE_EXIT)
    {
      trace("--DNS-- Thread exited.\n");
      THREAD_RETURN;
    }

    LOCK(data);

    switch(data->state)
    {
      case(STATE_LOOKUP):
      {
        trace("--DNS-- Lookup completed.\n");
        data->state = STATE_SUCCESS;
        data->ret = ret;
        break;
      }

      default:
      {
        trace("--DNS-- Lookup completed (discarding; %d).\n", data->state);
        free_dns_thread_data(data, true);
        data->state = STATE_STANDBY;
        break;
      }
    }
  }
  while(true);
}

static void create_dns_thread(struct dns_data *data)
{
  platform_mutex_init(&(data->mutex));
  platform_cond_init(&(data->cond));

  LOCK(data);

  if(!platform_thread_create(&(data->thread),
   (platform_thread_fn)run_dns_thread, (void *)data))
  {
    UNLOCK(data);
    platform_mutex_destroy(&(data->mutex));
    platform_cond_destroy(&(data->cond));
  }
  else
  {
    // Allow thread to initialize.
    WAIT(data);
    UNLOCK(data);
  }
}

static void destroy_dns_thread(struct dns_data *data)
{
  LOCK(data);
  data->state = STATE_EXIT;
  SIGNAL(data);
  UNLOCK(data);

  platform_thread_join(&(data->thread));
  platform_mutex_destroy(&(data->mutex));
  platform_cond_destroy(&(data->cond));
  free_dns_thread_data(data, true);
}

int DNS::lookup(const char *node, const char *service,
 const struct addrinfo *hints, struct addrinfo **res, uint32_t timeout)
{
  int ret;
  int i;

  for(i = 0; i < threads_max; i++)
  {
    struct dns_data *data = threads + i;

    if(data->state == STATE_INIT)
    {
      trace("--DNS-- Starting DNS thread %d.\n", threads_count);
      create_dns_thread(data);
      threads_count++;
    }

    if(data->state == STATE_STANDBY)
    {
      trace("--DNS-- Using DNS thread %d.\n", i);
      set_dns_thread_data(data, node, service, hints);
      data->state = STATE_LOOKUP;

      LOCK(data);
      SIGNAL(data);

      trace("--DNS-- Waiting for response.\n");
      TIMED_WAIT(data, timeout);

      if(data->state == STATE_SUCCESS)
      {
        ret = data->ret;
        *res = data->res;
        trace("--DNS-- Received response (return code %d)\n", ret);
        free_dns_thread_data(data, false);
        data->state = STATE_STANDBY;
      }
      else
      {
        trace("--DNS-- Timed out.\n");
        data->state = STATE_ABORT;
        ret = EAI_AGAIN;
        *res = NULL;
      }

      UNLOCK(data);
      return ret;
    }
  }

  // Try to run manually instead.
  trace("--DNS-- No DNS threads available; running lookup in main thread.\n");
  ret = Socket::getaddrinfo(node, service, hints, res);
  trace("--DNS-- Completed successfully (return code %d)\n", ret);
  return ret;
}

boolean DNS::init(struct config_info *conf)
{
  if(!init_ref_count)
  {
    if(!Socket::init(conf))
      return false;

    threads_max = DNS::DEFAULT_MAX_THREADS;

#ifdef CONFIG_UPDATER
    if(threads_max < conf->update_host_count)
      threads_max = conf->update_host_count;
#endif

    threads = (struct dns_data *)ccalloc(threads_max, sizeof(struct dns_data));
    threads_count = 0;
  }
  init_ref_count++;
  return true;
}

void DNS::exit(void)
{
  assert(init_ref_count > 0);
  init_ref_count--;
  if(init_ref_count != 0)
    return;

  for(int i = 0; i < threads_count; i++)
  {
    if(threads[i].state != STATE_INIT)
    {
      trace("--DNS-- Waiting for DNS thread %d\n", i);
      destroy_dns_thread(threads + i);
      trace("--DNS-- Thread %d joined.\n", i);
    }
  }

  free(threads);
  threads = nullptr;
  threads_count = 0;
  Socket::exit();
}

/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2009 Kevin Vance <kvance@kvance.com>
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

/* Simple event queue for NDS input events */

#include "evq.h"

static NDSEvent *evlist_front = NULL;
static NDSEvent *evlist_back = NULL;

bool nds_event_poll(NDSEvent *dest)
{
  NDSEvent *curr_event;
  bool retval = false;

  // Mask new input events while modifying the queue.
  irqDisable(IRQ_TIMER2);

  curr_event = evlist_front;
  if(curr_event)
  {
    // Copy the event to the destination.
    memcpy(dest, curr_event, sizeof(NDSEvent));
    dest->_next = NULL;

    // Advance to the next event.
    if(evlist_front != evlist_back)
    {
      evlist_front = evlist_front->_next;
    }
    else
    {
      evlist_front = NULL;
      evlist_back = NULL;
    }

    // Dispose of the old event.
    free(curr_event);
    retval = true;
  }

  // Allow new events again.
  irqEnable(IRQ_TIMER2);

  return retval;
}

void nds_event_push(NDSEvent *src)
{
  // Copy the source event.
  NDSEvent *new_event = cmalloc(sizeof(NDSEvent));
  memcpy(new_event, src, sizeof(NDSEvent));
  new_event->_next = NULL;

  // Add it to the list.
  if(evlist_back)
  {
    evlist_back->_next = new_event;
    evlist_back = new_event;
  }
  else
  {
    evlist_front = new_event;
    evlist_back = new_event;
  }
}

// Fill in a key down event.
void nds_event_fill_key_down(NDSEvent *dest, int key)
{
  dest->type = NDS_EVENT_KEY_DOWN;
  dest->key = key;
}

// Fill in a key up event.
void nds_event_fill_key_up(NDSEvent *dest, int key)
{
  dest->type = NDS_EVENT_KEY_UP;
  dest->key = key;
}

// Fill in a software keyboard key down event.
void nds_event_fill_keyboard_down(NDSEvent *dest, int key)
{
  dest->type = NDS_EVENT_KEYBOARD_DOWN;
  dest->key = key;
}

// Fill in a software keyboard key up event.
void nds_event_fill_keyboard_up(NDSEvent *dest, int key)
{
  dest->type = NDS_EVENT_KEYBOARD_UP;
  dest->key = key;
}

// Fill in a touch event.
void nds_event_fill_touch_down(NDSEvent *dest)
{
  dest->type = NDS_EVENT_TOUCH_DOWN;
}

// Fill in a touch event.
void nds_event_fill_touch_move(NDSEvent *dest, int x, int y)
{
  dest->type = NDS_EVENT_TOUCH_MOVE;
  dest->x = x;
  dest->y = y;
}

// Fill in a touch event.
void nds_event_fill_touch_up(NDSEvent *dest)
{
  dest->type = NDS_EVENT_TOUCH_UP;
}


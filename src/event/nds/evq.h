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

#ifndef __ARCH_NDS_EVQ_H
#define __ARCH_NDS_EVQ_H

#include "../../compat.h"

__M_BEGIN_DECLS

enum NDSEventType
{
  NDS_EVENT_NOP = 0,
  NDS_EVENT_KEY_DOWN,           // Hardware key down
  NDS_EVENT_KEY_UP,             // Hardware key up
  NDS_EVENT_KEYBOARD_DOWN,      // Software key down
  NDS_EVENT_KEYBOARD_UP,        // Software key up
  NDS_EVENT_TOUCH_DOWN,         // Touchscreen pen down
  NDS_EVENT_TOUCH_MOVE,         // Touchscreen pen move
  NDS_EVENT_TOUCH_UP,           // Touchscreen pen up
};

typedef struct NDSEvent_tag
{
  enum NDSEventType type;

  // On NDS_EVENT_KEY_DOWN and NDS_EVENT_KEY_UP, a keyboard bit e.g. KEY_LEFT.
  // On NDS_EVENT_KEYBOARD_PRESSED, the ASCII character pressed (not held).
  int key;

  // NDS_EVENT_TOUCH_MOVE touch coordinates.
  int x;
  int y;

  // Implementation: next event in the list.
  struct NDSEvent_tag *_next;
} NDSEvent;


// Return true if an event is available, fill dest with this event.
bool nds_event_poll(NDSEvent *dest);

// Push a copy of this event onto the queue.
void nds_event_push(NDSEvent *src);

// Fill in different event structures.
void nds_event_fill_key_down(NDSEvent *dest, int key);
void nds_event_fill_key_up(NDSEvent *dest, int key);
void nds_event_fill_keyboard_down(NDSEvent *dest, int key);
void nds_event_fill_keyboard_up(NDSEvent *dest, int key);
void nds_event_fill_touch_down(NDSEvent *dest);
void nds_event_fill_touch_move(NDSEvent *dest, int x, int y);
void nds_event_fill_touch_up(NDSEvent *dest);

__M_END_DECLS

#endif /* __ARCH_NDS_EVQ_H */

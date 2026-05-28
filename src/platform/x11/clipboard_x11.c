/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../platform_clipboard.h"

#if defined(CONFIG_SDL)
#include "../sdl/SDLmzx.h"
#include "../../render/sdl/render_sdl.h"
#endif

static char **copy_buffer;
static int copy_buffer_lines;
static int copy_buffer_total_length;

/* Forward declaration so the SDL parts can stay separate. */
static int copy_buffer_to_X11_selection(Display *display, XEvent *xevent);

#ifdef CONFIG_SDL
#if SDL_VERSION_ATLEAST(1,2,0) && !SDL_VERSION_ATLEAST(3,0,0)

static inline boolean get_X11_display_and_window(SDL_Window *window,
 Display **display, Window *xwindow)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);

  if(!window)
    window = sdl_get_current_window();

  if(!window || !SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_X11)
    return false;

  if(display)
    *display = info.info.x11.display;
  if(window)
    *xwindow = info.info.x11.window;
  return true;
}

#if SDL_VERSION_ATLEAST(2,0,0)
static inline int event_callback(void *userdata, SDL_Event *event)
#else
static inline int event_callback(const SDL_Event *event)
#endif
{
#if SDL_VERSION_ATLEAST(2,0,0)
  SDL_Window *window = (SDL_Window *)userdata;
#else
  SDL_Window *window = NULL;
#endif
  Display *display;
  XEvent *xevent;
  if(event->type != SDL_SYSWMEVENT)
    return 0;

  if(!get_X11_display_and_window(window, &display, NULL))
    return 0;

#if SDL_VERSION_ATLEAST(2,0,0)
  xevent = &(event->syswm.msg->msg.x11.event);
#else
  xevent = &(event->syswm.msg->event.xevent);
#endif

  return copy_buffer_to_X11_selection(display, xevent);
}

static inline void set_X11_event_callback(void)
{
  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  SDL_SetEventFilter(event_callback, sdl_get_current_window());
}

#endif /* SDL_VERSION_ATLEAST(1,2,0) && !SDL_VERSION_ATLEAST(3,0,0) */
#endif /* CONFIG_SDL */


static int copy_buffer_to_X11_selection(Display *display, XEvent *xevent)
{
  XSelectionRequestEvent *request;
  char *dest_data, *dest_ptr;
  XEvent response;
  int i, line_length;

  if(xevent->type != SelectionRequest || !copy_buffer)
    return 0;

  dest_data = cmalloc(copy_buffer_total_length + 1);
  dest_ptr = dest_data;

  for(i = 0; i < copy_buffer_lines - 1; i++)
  {
    line_length = strlen(copy_buffer[i]);
    memcpy(dest_ptr, copy_buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\n';
    dest_ptr++;
  }

  line_length = strlen(copy_buffer[i]);
  memcpy(dest_ptr, copy_buffer[i], line_length);
  dest_ptr[line_length] = 0;

  request = &(xevent->xselectionrequest);
  response.xselection.type = SelectionNotify;
  response.xselection.display = request->display;
  response.xselection.selection = request->selection;
  response.xselection.target = XA_STRING;
  response.xselection.property = request->property;
  response.xselection.requestor = request->requestor;
  response.xselection.time = request->time;

  XChangeProperty(display, request->requestor, request->property,
    XA_STRING, 8, PropModeReplace, (const unsigned char *)dest_data,
    copy_buffer_total_length);

  free(dest_data);

  XSendEvent(display, request->requestor, True, 0, &response);
  XFlush(display);
  return 0;
}

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  Display *display;
  Window xwindow;

  if(!get_X11_display_and_window(NULL, &display, &xwindow))
    return;

  copy_buffer = buffer;
  copy_buffer_lines = lines;
  copy_buffer_total_length = total_length;

  XSetSelectionOwner(display, XA_PRIMARY, xwindow, CurrentTime);
  set_X11_event_callback();
}

char *get_clipboard_buffer(void)
{
  int selection_format, ret_type;
  unsigned long int nbytes, overflow;
  unsigned char *src_data;
  char *dest_data = NULL;
  Window xwindow, owner;
  Atom selection_type;
  Display *display;

  if(!get_X11_display_and_window(NULL, &display, &xwindow))
    return NULL;

  owner = XGetSelectionOwner(display, XA_PRIMARY);

  if((owner == None) || (owner == xwindow))
    return NULL;

  XConvertSelection(display, XA_PRIMARY, XA_STRING, None,
    owner, CurrentTime);

  XLockDisplay(display);

  ret_type =
    XGetWindowProperty(display, owner,
    XA_STRING, 0, INT_MAX / 4, False, AnyPropertyType, &selection_type,
    &selection_format, &nbytes, &overflow, &src_data);

  if((ret_type != Success) || ((selection_type != XA_STRING) &&
    (selection_type != XInternAtom(display, "TEXT", False)) &&
    (selection_type != XInternAtom(display, "COMPOUND_TEXT", False)) &&
    (selection_type != XInternAtom(display, "UTF8_STRING", False))))
    goto err_unlock;

  dest_data = cmalloc(nbytes + 1);
  memcpy(dest_data, src_data, nbytes);
  dest_data[nbytes] = 0;

  XFree(src_data);

err_unlock:
  XUnlockDisplay(display);
  return dest_data;
}

void free_clipboard_buffer(char *buffer)
{
  free(buffer);
}

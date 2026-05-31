/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../platform_clipboard.h"

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  HANDLE global_memory;
  size_t line_length;
  char *dest_data;
  char *dest_ptr;
  int i;

  // Room for \rs
  total_length += lines - 1;

  global_memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, total_length);
  if(!global_memory)
    return;

  if(!OpenClipboard(NULL))
    return;

  dest_data = (char *)GlobalLock(global_memory);
  dest_ptr = dest_data;

  for(i = 0; i < lines - 1; i++)
  {
    line_length = strlen(buffer[i]);
    memcpy(dest_ptr, buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\r';
    dest_ptr[1] = '\n';
    dest_ptr += 2;
  }

  line_length = strlen(buffer[i]);
  memcpy(dest_ptr, buffer[i], line_length);
  dest_ptr[line_length] = 0;

  GlobalUnlock(global_memory);
  EmptyClipboard();
  SetClipboardData(CF_TEXT, global_memory);

  CloseClipboard();
}

char *get_clipboard_buffer(void)
{
  HANDLE global_memory;
  char *dest_data;
  char *src_data;
  size_t length;

  if(!IsClipboardFormatAvailable(CF_TEXT) || !OpenClipboard(NULL))
    return NULL;

  global_memory = GetClipboardData(CF_TEXT);

  if(global_memory != NULL)
  {
    src_data = (char *)GlobalLock(global_memory);

    // CF_TEXT is guaranteed to be null-terminated.
    length = strlen(src_data);

    dest_data = (char *)cmalloc(length + 1);
    strcpy(dest_data, src_data);

    GlobalUnlock(global_memory);
    CloseClipboard();

    return dest_data;
  }

  return NULL;
}

void free_clipboard_buffer(char *buffer)
{
  free(buffer);
}

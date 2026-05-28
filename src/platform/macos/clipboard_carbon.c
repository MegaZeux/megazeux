/* MegaZeux
 *
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2017 Ian Burgmyer <spectere@gmail.com>
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

#include "../platform_clipboard.h"

#define decimal decimal_
#define Random Random_
#include <Carbon/Carbon.h>

static const CFStringRef PLAIN = CFSTR("com.apple.traditional-mac-plain-text");

void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  PasteboardSyncFlags syncFlags;
  char *dest_data, *dest_ptr;
  PasteboardRef clipboard;
  CFDataRef textData;
  int i, line_length;

  if(PasteboardCreate(kPasteboardClipboard, &clipboard) != noErr)
    return;

  if(PasteboardClear(clipboard) != noErr)
    goto err_release;

  syncFlags = PasteboardSynchronize(clipboard);
  if((syncFlags & kPasteboardModified) ||
    !(syncFlags & kPasteboardClientIsOwner))
    goto err_release;

  dest_data = cmalloc(total_length + 1);
  dest_ptr = dest_data;

  for(i = 0; i < lines - 1; i++)
  {
    line_length = strlen(buffer[i]);
    memcpy(dest_ptr, buffer[i], line_length);
    dest_ptr += line_length;
    dest_ptr[0] = '\n';
    dest_ptr++;
  }

  line_length = strlen(buffer[i]);
  memcpy(dest_ptr, buffer[i], line_length);
  dest_ptr[line_length] = 0;

  textData = CFDataCreate(kCFAllocatorDefault,
   (const UInt8 *)dest_data, total_length + 1);
  free(dest_data);

  if(!textData)
    goto err_release;

  if(PasteboardPutItemFlavor(clipboard, (PasteboardItemID)1,
   PLAIN, textData, 0) != noErr)
    goto err_release;

err_release:
  CFRelease(clipboard);
}

char *get_clipboard_buffer(void)
{
  PasteboardRef clipboard;
  ItemCount itemCount;
  UInt32 itemIndex;
  char *dest_data = NULL;

  if(PasteboardCreate(kPasteboardClipboard, &clipboard) != noErr)
    return NULL;

  if(PasteboardGetItemCount(clipboard, &itemCount) != noErr)
    goto err_release;

  for(itemIndex = 1; itemIndex <= itemCount; itemIndex++)
  {
    CFIndex flavorCount, flavorIndex;
    CFArrayRef flavorTypeArray;
    PasteboardItemID itemID;

    if(PasteboardGetItemIdentifier(clipboard, itemIndex, &itemID) != noErr)
      goto err_release;

    if(PasteboardCopyItemFlavors(clipboard, itemID, &flavorTypeArray) != noErr)
      goto err_release;

    flavorCount = CFArrayGetCount(flavorTypeArray);

    for(flavorIndex = 0; flavorIndex < flavorCount; flavorIndex++)
    {
      CFDataRef flavorData;
      char *src_data;
      size_t length;

      CFStringRef flavorType =
       (CFStringRef)CFArrayGetValueAtIndex(flavorTypeArray, flavorIndex);

      if(!UTTypeConformsTo(flavorType, PLAIN))
        continue;

      if(PasteboardCopyItemFlavorData(clipboard, itemID,
       flavorType, &flavorData) != noErr)
        continue;

      src_data = (char *)CFDataGetBytePtr(flavorData);
      length = (size_t)CFDataGetLength(flavorData);

      dest_data = (char *)cmalloc(length + 1);
      memcpy(dest_data, src_data, length);
      dest_data[length] = 0;

      CFRelease(flavorData);
    }
  }

err_release:
  CFRelease(clipboard);
  return dest_data;
}

void free_clipboard_buffer(char *buffer)
{
  free(buffer);
}

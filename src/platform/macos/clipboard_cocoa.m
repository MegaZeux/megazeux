/* MegaZeux
 *
 * Copyright (C) 2021-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <Foundation/Foundation.h>
#include <AppKit/NSPasteboard.h>

#include <stdlib.h>
#include <string.h>

#include "../platform_clipboard.h"

/**
 * Native MacOS clipboard handler using AppKit. Unlike the SDL2 clipboard
 * handler, this handler treats the input text as being encoded in Mac OS Roman
 * (unlike ISO Latin-1, Windows 1292, etc, all 256 codepoints of Mac OS Roman
 * will be encoded as valid UTF-8 symbols).
 *
 * Notes:
 * - Literal arrays require LLVM 3.1, so explicitly allocate NSArrays.
 * - @autoreleasepool requires LLVM 3.0, so use NSAutoreleasePool directly.
 */

/**
 * Send an array of extended ASCII lines to the system clipboard via Cocoa.
 * These lines will be re-encoded to UTF-8 using Mac OS Roman codepoints.
 */
void copy_buffer_to_clipboard(char **buffer, int lines, int total_length)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSPasteboard *pasteboard = NULL;
  NSString *string = NULL;

  char *buf = cmalloc(total_length + 1);
  char *pos = buf;
  for(int i = 0; i < lines; i++)
  {
    size_t len = strlen(buffer[i]);
    memcpy(pos, buffer[i], len);
    pos += len;
    *(pos++) = '\n';
  }
  buf[total_length] = '\0';

  /* Convert to NSString. NOTE: this function requires 10.3+, haven't
   * looked for a compelling way to do this for 10.0.
   */
  string = [[[NSString alloc] initWithBytes:buf length:total_length
   encoding:NSMacOSRomanStringEncoding] autorelease];
  free(buf);

  pasteboard = [NSPasteboard generalPasteboard];

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

  [pasteboard clearContents];
  [pasteboard writeObjects:[NSArray arrayWithObject:string]];

#else /* VERSION_MIN < 1060 */

  [pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
  [pasteboard setString:string forType:NSStringPboardType];

#endif /* VERSION_MIN < 1060 */

  [pool release];
}

/**
 * Fetch an extended ASCII buffer from the system clipboard via Cocoa.
 * These lines will be re-encoded from UTF-8 using Mac OS Roman codepoints.
 */
char *get_clipboard_buffer(void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
  NSString *string = NULL;
  NSData *chrdata = NULL;
  size_t buf_len;
  char *buf;

#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060

  NSArray *for_classes = [NSArray arrayWithObject:[NSString class]];
  NSArray *items = [pasteboard readObjectsForClasses:for_classes options:nil];

  string = items && [items count] ? [items objectAtIndex:0] : nil;
  if(!string)
    goto err;

#else /* VERSION_MIN < 1060 */

  NSArray *types = [NSArray arrayWithObject:NSStringPboardType];
  if(![pasteboard availableTypeFromArray:types])
    goto err;

  string = [pasteboard stringForType:NSStringPboardType];
  if(!string)
    goto err;

#endif /* VERSION_MIN < 1060 */

  chrdata = [string dataUsingEncoding:NSMacOSRomanStringEncoding
   allowLossyConversion:true];
  if(!chrdata)
    goto err;

  buf_len = [chrdata length];
  buf = (char *)cmalloc(buf_len + 1);

  [chrdata getBytes:buf length:buf_len];
  buf[buf_len] = '\0';
  [pool release];
  return buf;

err:
  [pool release];
  return NULL;
}

void free_clipboard_buffer(char *buffer)
{
  free(buffer);
}

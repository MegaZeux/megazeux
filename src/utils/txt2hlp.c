/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2004 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2012-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "../compat.h"
#include "utils_alloc.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

// Cleaned up/revised txt2hlp.cpp for compiling outside of DOS

static void fputw(int src, FILE *fp)
{
  fputc(src & 0xFF, fp);
  fputc(src >> 8, fp);
}

// Custom fgetc function to dump \r's and replace \xFF's and /x1's.

static int fgetc2(FILE *fp)
{
  int current_char = fgetc(fp);
  switch(current_char)
  {
    case '\r':
    {
      return fgetc2(fp);
    }

    case 255:
    {
      return 32;
    }

    case 1:
    {
      return 2;
    }

    default:
    {
      return current_char;
    }
  }
}

#define fgetc(f) fgetc2(f)

int main(int argc, char *argv[])
{
  // Temp vars
  char previous_char;
  char current_char;
  int colon_count;

  int str_len;
  char tstr[81] = "";
  int tlong;
  int i;

  // Source/dest files open. Now scan source for number of filenames.
  // A new file is designated by a # as the first character of a line.

  int num_files = 1;

  // Count context sensitive links too. A link is designated by a : as
  // the first character of a line, followed by three numerals, followed
  // by the end of the line. (a \n)

  int num_links = 0;

  FILE *source;
  FILE *dest;

  char curr_file[13] = "MAIN.HLP";
  char max_file[13] = "(none)";
  long file_offset;
  long file_len = 1;          // For first byte
  long curr_file_storage = 2; // Location to store current file's info

  // Links info-
  int curr_link_ref = 0;      // Ref- not actual link number
  int link_numbers[1000];     // Actual numbers of the links
  long link_offsets[1000];    // Offsets within current file

  // Current line info-
  int display_line;           // Show text?
  int line_display_len = 0;   // Used for warnings
  int current_line_num = 1;   // For error/warning info

  // Global info-
  int end_of_file = 0;        // Have we hit @ yet?
  long biggest_file = 0;      // For help allocation
  int global_line_num = 0;    // For error/warning info

  printf("\n\n");

  if(argc < 3)
  {
    printf("Usage: TXT2HLP source.txt output.fil\n");
    return 1;
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if(unveil(argv[1], "r") || unveil(argv[2], "cw") || unveil(NULL, NULL))
  {
    fprintf(stderr, "ERROR: Failed unveil!\n");
    return 1;
  }
#endif

  if(pledge(PROMISES, ""))
  {
    fprintf(stderr, "ERROR: Failed pledge!\n");
    return 1;
  }
#endif

  source = fopen_unsafe(argv[1], "rb");
  if(source == NULL)
  {
    printf("Error opening %s for input.\n", argv[1]);
    return -1;
  }

  dest = fopen_unsafe(argv[2],"wb");
  if(dest == NULL)
  {
    fclose(source);
    printf("Error opening %s for output.\n", argv[2]);
    return 1;
  }

  // Count the # of files and links
  do
  {
    current_char = fgetc(source);

    switch(current_char)
    {
      case '#':
      {
        num_files++;
        break;
      }

      case ':':
      {
        int link;

        // Get upcoming link
        if(!fscanf(source, "%3d", &link))
          break;

        // Get next char
        current_char = fgetc(source);

        if(((current_char == '\n') || (current_char == ':')) &&
         link >= num_links)
          num_links = link + 1;

      }
    }

    // Skip the rest of the line
    while((current_char != '\n') && !feof(source))
      current_char = fgetc(source);

  } while(!feof(source));

  printf("Files: %d  Links: %d\n", num_files, num_links);

  // Number of files obtained. Write header, with room, to dest. This
  // includes number of links and blank spaces to add in filenames and
  // link info.

  fputw(num_files, dest);
  for(i = 0; i < num_files; i++)
    fwrite("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 21, 1, dest);

  fputw(num_links, dest);
  if(num_links > 0)
    for(i = 0; i < num_links; i++)
      fwrite("\0\0\0\0\0\0\0\0\0\0\0\0", 12, 1, dest);

  file_offset = ftell(dest);

  // Reset position on source...
  fseek(source, 0, SEEK_SET);
  // ...and begin our parse! New files start with a 1.
  fputc(1, dest);
  printf("Processing file %s...\n", curr_file);

  do
  {
    //Our loop. Read first char of current line
    line_display_len = 0;

    // We only use this var for counting line lengths w/ display chars
    previous_char = '\0';
    current_char = fgetc(source);

    // Generally we'll want to have text after our commands, but we may not
    display_line = 1;

    switch(current_char)
    {
      // Centered line- write prefix...
      case '$':
      {
        fputc(255, dest);
        fputc('$', dest);
        file_len += 2;
        // we can't stop here this is bat country
      }

      /* fallthrough */



      // Normal line that can start with reserved char
      case '.':
      {
        current_char = fgetc(source);
        break;
      }



      // Choice of some type
      case '>':
      {
        fputc(255, dest);
        file_len++;

        // See if dest is normal or file
        current_char = fgetc(source);

        if(current_char == '#')
        {
          // File.
          fputc('<', dest);
          file_len++;

          current_char = fgetc(source);
          colon_count = 0;
        }
        else
        {
          // Label.
          fputc('>',dest);
          file_len++;

          colon_count = 1;
        }

        //Grab name.
        str_len = 0;
        do
        {
          if(current_char == '\n')
            break;

          if(current_char == ':')
          {
             if(colon_count < 1)
               colon_count++;
             else
               break;
          }

          tstr[str_len] = current_char;
          str_len++;

          current_char = fgetc(source);

        } while(!feof(source));

        tstr[str_len] = '\0';

        if(str_len == 1) // 1 len not allowed
          printf("Error- Link target length of 1 on line %d (g:%d), file %s.\n",
                 current_line_num, global_line_num, curr_file);

        if(str_len == 10) // 10 len not allowed
          printf("Error- Link target length of 10 on line %d (g:%d), file %s.\n",
                 current_line_num, global_line_num, curr_file);

        fputc(str_len, dest);
        file_len++;

        fwrite(tstr, (str_len + 1), 1, dest);
        file_len += (str_len + 1);

        //Now do actual message after the label/file.
        current_char = fgetc(source);
        break;
      }



      //Label, possibly with a message attached
      case ':':
      {
        fputc(255, dest);
        fputc(':', dest);
        file_len += 2;

        //Grab name.
        str_len = 0;
        do
        {
          current_char = fgetc(source);

          if((current_char == ':') || (current_char == '\n'))
            break;

          tstr[str_len] = current_char;
          str_len++;

        } while(!feof(source));

        tstr[str_len] = '\0';

        if(str_len == 1) //1 len not allowed
          printf("Error- Label length of 1 on line %d (g:%d), file %s.\n",
                 current_line_num, global_line_num, curr_file);

        if(str_len == 10) //10 len not allowed
          printf("Error- Label length of 10 on line %d (g:%d), file %s.\n",
                 current_line_num, global_line_num, curr_file);

        fputc(str_len, dest);
        file_len++;

        fwrite(tstr, (str_len + 1), 1, dest);
        file_len += (str_len + 1);

        //Is this a context-link?
        if((str_len == 3) &&
         isdigit((unsigned char)tstr[0]) &&
         isdigit((unsigned char)tstr[1]) &&
         isdigit((unsigned char)tstr[2]))
        {
          //Yep.
          link_numbers[curr_link_ref] = atoi(tstr);
          link_offsets[curr_link_ref] = file_len - 7;
          curr_link_ref++;
        }

        // If there isn't a :, don't put a message, end line
        if(current_char != ':')
        {
          fputc('\n', source);
          file_len++;
          display_line = 0;
        }

        current_char = fgetc(source);
        break;
      }


      // End
      case '@':
      {
        save_last_file:
        end_of_file = 1;
      }

      /* fallthrough */

      // New file
      case '#':
      {
        // End current file
        fputc(0, dest);
        file_len++;

        if(file_len > biggest_file)
        {
          biggest_file = file_len;
          memcpy(max_file, curr_file, 12);
          max_file[12] = '\0';
        }

        // Put new file's info at the start
        tlong = ftell(dest);
        fseek(dest, curr_file_storage, SEEK_SET);
        fwrite(curr_file, 1, 13, dest);
        fwrite(&file_offset, 1, 4, dest);
        fwrite(&file_len, 1, 4, dest);

        if(file_len > 65535)
          printf("Warning- File %s over 64k bytes in length.\n",
                 curr_file);

        //Write in links info
        for(i = 0; i < curr_link_ref; i++)
        {
          fseek(dest, 4 + num_files * 21 + link_numbers[i] * 12, SEEK_SET);
          fwrite(&file_offset, 1, 4, dest);
          fwrite(&file_len, 1, 4, dest);
          fwrite(&link_offsets[i], 1, 4, dest);
        }

        if(end_of_file)
          goto alldone;

        //Start next file
        fseek(dest, tlong, SEEK_SET);
        file_offset = tlong;
        current_line_num = 1;

        curr_file_storage += 21;
        curr_link_ref = 0;

        fputc(1,dest);
        file_len = 1;

        //Get new filename
        str_len = 0;
        do
        {
          current_char = fgetc(source);
          if(current_char == '\n')
            break;

          tstr[str_len] = current_char;
          str_len++;

        } while(!feof(source));

        tstr[str_len] = '\0';
        tstr[12] = '\0';

        //Copy over new filename
        memcpy(curr_file, tstr, 12);
        curr_file[12] = '\0';

        //Next file ready to roar!
        printf("Processing file %s...\n", curr_file);

        display_line = 0;
        break;
      }
    }

    // Now parse the line for display text.
    if(display_line)
    {
      while(!feof(source) && (current_char != '\n') && display_line)
      {
        fputc(current_char, dest);
        file_len++;

        // This won't be true unless there are two non ~/@ chars
        // adjacent, two ~s adjacent, or two @s adjacent
        if(
         ((current_char == '~') ^ (previous_char != '~')) &&
         ((current_char == '@') ^ (previous_char != '@')))
        {
          // Clear this so we don't miscount the next char
          previous_char = '\0';
         line_display_len++;
        }
        else
          previous_char = current_char;

        current_char = fgetc(source);
      }

      // End the line!
      fputc('\n', dest);
      file_len++;
    }

    if(line_display_len > 64)
      printf("Warning: Line %d (g:%d) over 64 chars in file %s.\n",
       current_line_num, global_line_num, curr_file);

    //Done with this line.
    current_line_num++;
    global_line_num++;

  } while(!feof(source));

  // Abrupt file end!
  printf("Warning: Encountered end of file before end of file marker '@'\n");
  goto save_last_file;

  alldone:

  //All done! Close files.
  fclose(dest);
  fclose(source);
  printf("Done! Biggest file- %ld bytes. (%s)\n\n",
         biggest_file, max_file);

  return 0;
}


/* MegaZeux
 *
 * Copyright (C) 2018-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Defines so hlp2html builds when this is included.
#define CORE_LIBSPEC
#include "../compat.h"
#include "../hashtable.h"
#include "../algorithm/string_compare.h"
#include "../io/memfile.h"

#include "utils_alloc.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

#ifndef VERSION_DATE
#define VERSION_DATE
#endif

#define EOL "\n"
#define MZXFONT_OFFSET (0xE000)

#define USAGE \
 "hlp2html - Convert help text file to HTML." EOL \
 "usage: hlp2html input.txt output.html [options]..." EOL EOL \
 "options:" EOL \
 "    -c : color+printable version (default)" EOL \
 "    -p : printable-only version" EOL

#define RESOURCES "contrib/hlp2html/"
static const char * const font_file =         RESOURCES "fonts.css";
static const char * const style_file =        RESOURCES "style.css";
static const char * const style_extras_file = RESOURCES "style_color.css";

#define MAX_FILES 256
#define MAX_LINE 256
#define MAX_FILE_NAME 16
#define MAX_ANCHOR_NAME 32

struct html_buffer
{
  char *data;
  int length;
  int alloc;
};

struct help_file
{
  // HTML ID- needs to be unique. Use the real internal file name.
  char name[MAX_FILE_NAME];
  int name_length;
  uint32_t hash;

  struct html_buffer html;
};

struct help_anchor
{
  struct help_file *parent;

  // HTML ID- needs to be unique. Use the real link name.
  char name[MAX_ANCHOR_NAME];
  int name_length;
  uint32_t hash;
};

struct help_link
{
  struct help_file *parent;
  // HTML ID of target anchor's enclosing file.
  char target_file[MAX_FILE_NAME];
  // HTML ID of target anchor.
  char target_anchor[MAX_ANCHOR_NAME];

  // HTML ID- needs to be unique. Intended to be used to inject JS, but not
  // used for anything right now.
  char name[MAX_ANCHOR_NAME];
  int name_length;
  uint32_t hash;
};

HASH_SET_INIT(FILES, struct help_file *, name, name_length)
HASH_SET_INIT(ANCHORS, struct help_anchor *, name, name_length)
HASH_SET_INIT(LINKS, struct help_link *, name, name_length)

static hash_t(FILES) *files_table = NULL;
static hash_t(ANCHORS) *anchors_table = NULL;
static hash_t(LINKS) *links_table = NULL;

static struct help_file *help_file_list[MAX_FILES];
static int num_help_files = 0;

static void load_file(char **_output, size_t *_output_len, const char *filename)
{
  FILE *fp = fopen_unsafe(filename, "rb");
  char *output = NULL;
  size_t output_len = 0;

  if(fp)
  {
    fseek(fp, 0, SEEK_END);
    output_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    output = cmalloc(output_len + 1);
    if(!fread(output, output_len, 1, fp))
      fprintf(stderr, "ERROR: could not read input file\n");
    output[output_len] = 0;
    fclose(fp);
  }
  else
    fprintf(stderr, "ERROR: failed to read input file '%s'\n", filename);

  *_output = output;
  *_output_len = output_len;
}

/**
 * Help file management.
 */

static char *expand_buffer(struct html_buffer *buffer, int required_length)
{
  if(buffer->alloc == 0)
    buffer->alloc = 1024;

  while(buffer->alloc < required_length)
    buffer->alloc *= 2;

  buffer->data = crealloc(buffer->data, buffer->alloc);
  return buffer->data;
}

static void append_html(struct help_file *current, const char *html)
{
  struct html_buffer *buffer = &(current->html);
  int html_length = strlen(html);
  int new_length = buffer->length + html_length;

  if(new_length + 1 > buffer->alloc)
    expand_buffer(buffer, new_length + 1);

  memcpy(buffer->data + buffer->length, html, html_length);
  buffer->length = new_length;
  buffer->data[new_length] = 0;
}

static void append_file(struct help_file *current, const char *filename)
{
  char *buffer;
  size_t length;

  load_file(&buffer, &length, filename);
  if(buffer)
  {
    append_html(current, buffer);
    append_html(current, EOL);
    free(buffer);
  }
}

static struct help_file *new_help_file(const char *title)
{
  struct help_file *check;
  struct help_file *current = ccalloc(1, sizeof(struct help_file));
  snprintf(current->name, MAX_FILE_NAME, "%s", title);
  current->name_length = strlen(current->name);

  HASH_FIND(FILES, files_table, current->name, current->name_length, check);
  if(check)
  {
    fprintf(stderr, "WARNING: Duplicate file '%s' found. Please debug with "
     "'make help_check'.\n", current->name);

    free(current);
    return NULL;
  }
  else
  {
    HASH_ADD(FILES, files_table, current);
    help_file_list[num_help_files] = current;
    num_help_files++;
    return current;
  }
}

static void free_help_files(void)
{
  struct help_file *current;

  HASH_ITER(FILES, files_table, current, {
    free(current->html.data);
    free(current);
  });

  HASH_CLEAR(FILES, files_table);
}

/**
 * Anchor management.
 */

static void new_anchor(struct help_file *parent, const char *name)
{
  struct help_anchor *check;
  struct help_anchor *anchor = ccalloc(1, sizeof(struct help_anchor));
  snprintf(anchor->name, MAX_ANCHOR_NAME, "%s", name);
  anchor->name_length = strlen(anchor->name);
  anchor->parent = parent;

  HASH_FIND(ANCHORS, anchors_table, anchor->name, anchor->name_length, check);
  if(check)
  {
    fprintf(stderr, "WARNING: Duplicate anchor '%s' found. Please debug with "
     "'make help_check'.\n", anchor->name);

    free(anchor);
  }
  else
  {
    HASH_ADD(ANCHORS, anchors_table, anchor);
  }
}

static void free_anchors(void)
{
  struct help_anchor *current;

  HASH_ITER(ANCHORS, anchors_table, current, {
    free(current);
  });

  HASH_CLEAR(ANCHORS, anchors_table);
}

/**
 * Links management.
 */

static void new_link(struct help_file *parent, const char *name,
 const char *target_file, const char *target_anchor)
{
  struct help_link *link = ccalloc(1, sizeof(struct help_link));
  snprintf(link->name, MAX_ANCHOR_NAME, "%s", name);
  link->name_length = strlen(link->name);
  link->parent = parent;

  snprintf(link->target_file, MAX_FILE_NAME, "%s", target_file);
  snprintf(link->target_anchor, MAX_ANCHOR_NAME, "%s", target_anchor);

  // The name is generated with a unique number; don't bother checking.
  HASH_ADD(LINKS, links_table, link);
}

static void validate_links(void)
{
  // FIXME should implement this.
}

static void free_links(void)
{
  struct help_link *current;

  HASH_ITER(LINKS, links_table, current, {
    free(current);
  });

  HASH_CLEAR(LINKS, links_table);
}

/**
 * Start of line commands:
 *
 * #FILE.HLP (start of new file-- note MAIN.HLP is the first and implicit)
 * :AAA (anchor with name "AAA", does not display a new line)
 * :AAA:Text (anchor with name "AAA", displays a new line with "Text")
 * >AAA:Text (link targeting anchor "AAA" in current file)
 * >#FILE.HLP:AAA:Text (external link targeting "AAA" in "FILE.HLP")
 * $Centered Text
 * .$Text (escape command; displays "$Text")
 *
 * Anywhere in the line commands:
 *
 * ~A color text foreground (0-9, A-F, a-f)
 * @A color text background (0-9, A-F, a-f)
 *
 * Note: there's a special case for the scroll char where, when
 * encountered as 228, it should be displayed as 232.
 *
 * Note: If an anchor is followed by anything (even a single space), this
 * means the line should be displayed. Otherwise, hide the line.
 */

static boolean write_printable_html = false;
static boolean color_code_active = false;
static unsigned char current_fg_color_char;
static unsigned char current_bg_color_char;

static char validate_color_char(char input)
{
  if(input >= '0' && input <= '9')
    return input;

  if(toupper(input) >= 'A' && toupper(input) <= 'F')
    return input;

  return 0;
}

static void set_current_fg_color(unsigned char input)
{
  input = validate_color_char(input);
  if(input > 0)
    current_fg_color_char = input;
}

static void set_current_bg_color(unsigned char input)
{
  input = validate_color_char(input);
  if(input > 0)
    current_bg_color_char = input;
}

static void reset_current_color(void)
{
  current_fg_color_char = 0;
  current_bg_color_char = 0;
  color_code_active = false;
}

static void apply_color_codes(struct help_file *current)
{
  char buffer[4];

  if(write_printable_html)
    return;

  if(current_fg_color_char == 0 && current_bg_color_char == 0)
    return;

  if(color_code_active)
    append_html(current, "</span>");

  append_html(current, "<span class=\"");

  if(current_fg_color_char > 0)
  {
    buffer[0] = 'f';
    buffer[1] = current_fg_color_char;
    buffer[2] = 0;

    if(current_bg_color_char > 0)
    {
      buffer[2] = ' ';
      buffer[3] = 0;
    }
    append_html(current, buffer);
  }

  if(current_bg_color_char > 0)
  {
    buffer[0] = 'b';
    buffer[1] = current_bg_color_char;
    buffer[2] = 0;
    append_html(current, buffer);
  }

  append_html(current, "\">");
  color_code_active = true;
}

static void close_color_codes(struct help_file *current)
{
  if(color_code_active)
    append_html(current, "</span>");

  reset_current_color();
}

static struct help_file *parse_line(struct help_file *current, char *line)
{
  boolean is_new_color = false;
  boolean is_text = false;
  boolean is_centered = false;
  boolean is_anchor = false;
  unsigned char cur;
  char buffer[MAX_ANCHOR_NAME];

  if(!current)
    return NULL;

  // Check for commands.
  switch(line[0])
  {
    case '#':
    {
      // Start a new help file.
      return new_help_file(line + 1);
    }

    case '@':
    {
      // End of help file.
      return NULL;
    }

    case ':':
    {
      // Anchor. If there is a ':' after the label name, display text.
      char *label = line + 1;
      size_t label_len;

      line = strchr(label, ':');
      if(line)
      {
        *line = 0;
        line++;
      }

      label_len = strlen(label);

      if(label_len == 1 || label_len == 10)
        fprintf(stderr, "WARNING: label length of 1 or 10 found. "
         "Please debug with 'make help_check'.\n");

      snprintf(buffer, MAX_ANCHOR_NAME, "%s__%s", current->name, label);
      new_anchor(current, buffer);
      append_html(current, "<a class=\"hA\" name=\"");
      append_html(current, buffer);
      append_html(current, "\">");
      is_anchor = true;
      break;
    }

    case '>':
    {
      // Link.
      static int link_count = 0;
      char target_anchor[MAX_ANCHOR_NAME];
      char *target_file = current->name;
      char *label;

      line++;
      if(line[0] == '#')
      {
        // External link.
        target_file = line + 1;

        line = strchr(target_file, ':');
        if(line)
        {
          *line = 0;
          line++;
        }
      }

      label = line;
      line = strchr(label, ':');
      if(line)
      {
        *line = 0;
        line++;
      }

      snprintf(target_anchor, MAX_ANCHOR_NAME, "%s__%s", target_file, label);
      snprintf(buffer, MAX_ANCHOR_NAME, "%d__%s", (++link_count), label);
      new_link(current, buffer, target_file, target_anchor);
      append_html(current, "<a class=\"hL\" href=\"#");
      append_html(current, target_anchor);
      append_html(current, "\">");
      is_anchor = true;
      break;
    }

    case '$':
    {
      // Centered line.
      append_html(current, "<p class=\"hC\">");
      is_centered = true;
      line++;
      break;
    }

    case '.':
    {
      // Escape command.
      line++;
      break;
    }
  }

  reset_current_color();

  if(line)
  {
    while((cur = *(line++)))
    {
      if(cur == '~')
      {
        cur = *(line++);
        if(cur != '~')
        {
          set_current_fg_color(cur);
          is_new_color = true;
          continue;
        }
      }

      if(cur == '@')
      {
        cur = *(line++);
        if(cur != '@')
        {
          set_current_bg_color(cur);
          is_new_color = true;
          continue;
        }
      }

      is_text = true;
      if(is_new_color)
      {
        apply_color_codes(current);
        is_new_color = false;
      }

      if(cur < 32 || cur > 126)
      {
        // Special case for scroll char.
        if(cur == 228)
          cur = 232;

        snprintf(buffer, 32, "&#x%X;", MZXFONT_OFFSET + cur);
        append_html(current, buffer);
      }
      else
      {
        // Certain chars need to be escaped to entities to display reliably.
        switch(cur)
        {
          case '&':
            append_html(current, "&amp;");
            break;

          case '<':
            append_html(current, "&lt;");
            break;

          case '>':
            append_html(current, "&gt;");
            break;

          default:
            buffer[0] = cur;
            buffer[1] = 0;
            append_html(current, buffer);
            break;
        }
      }
    }
  }

  close_color_codes(current);

  if(is_anchor)
  {
    // Append an extra space if this anchor is supposed to be consuming a line.
    if(line && !is_text)
      append_html(current, " ");
    append_html(current, "</a>");
  }

  if(is_centered)
    append_html(current, "</p>");
  else
    append_html(current, EOL);

  return current;
}

static void parse_file(char *file_buffer, size_t file_length)
{
  struct help_file *current = new_help_file("MAIN.HLP");
  char line_buffer[MAX_LINE];
  struct memfile mf;

  mfopen(file_buffer, file_length, &mf);

  while(current && mfsafegets(line_buffer, MAX_LINE, &mf))
    current = parse_line(current, line_buffer);

  // Make sure all links connect to a valid anchor.
  validate_links();
}

#define TITLE "MegaZeux " VERSION VERSION_DATE " - Help File"

static void append_nav_url(struct help_file *current, const char *target_file,
 const char *target_anchor, const char *text)
{
  char buffer[512];
  snprintf(buffer, 512, "<li><a href=\"#%s__%s\">%s</a></li>",
   target_file, target_anchor, text);

  append_html(current, buffer);
}

static void write_html(const char *output)
{
  FILE *fp_out;
  struct help_file root;
  struct help_file *current;
  int i;

  memset(&root, 0, sizeof(struct help_file));

  append_html(&root, "<!DOCTYPE html>" EOL);
  append_html(&root, "<html>" EOL "<head>" EOL);
  append_html(&root, "<title>" TITLE "</title>" EOL);

  append_html(&root, "<meta charset=\"UTF-8\">" EOL);
  append_html(&root, "<meta name=\"title\" content=\"" TITLE "\">" EOL);
  append_html(&root, "<meta name=\"twitter:card\" content=\"summary\">" EOL);
  append_html(&root, "<meta name=\"twitter:title\" content=\"" TITLE "\">" EOL);

  append_html(&root, "<style>" EOL);
  append_file(&root, font_file);
  append_html(&root, "</style>" EOL);

  append_html(&root, "<style>" EOL);
  append_file(&root, style_file);
  append_html(&root, "</style>" EOL);

  if(!write_printable_html)
  {
    append_html(&root, "<style>" EOL);
    append_file(&root, style_extras_file);
    append_html(&root, "</style>" EOL);
  }

  append_html(&root, "</head>" EOL EOL "<body>" EOL);

  if(!write_printable_html)
  {
    append_html(&root, "<div id=\"helpnav\">" EOL);
    append_html(&root, "<h1>" TITLE "</h1>" EOL);
    append_html(&root, "<div class=\"helpnavcentered\"><ul>" EOL);
    append_nav_url(&root, "MAIN.HLP", "072", "Contents");
    append_nav_url(&root, "EDITINGK.HLP", "080", "Editor Keys");
    append_nav_url(&root, "ROBOTICR.HLP", "087", "Robotic Manual");
    append_nav_url(&root, "COMMANDR.HLP", "1st", "Commands");
    append_nav_url(&root, "COUNTERS.HLP", "1st", "Counters");
    append_nav_url(&root, "NEWINVER.HLP", "1st", "Changelog");
    append_html(&root, "</ul></div>" EOL "</div>" EOL EOL);
  }
  else
  {
    append_html(&root, "<div id=\"helpnavprint\"><h1>" TITLE "</h1></div>" EOL);
  }

  append_html(&root, "<div id=\"helpcontainer\">" EOL);

  for(i = 0; i < num_help_files; i++)
  {
    current = help_file_list[i];
    append_html(&root, "<div class=\"hF\" id=\"");
    append_html(&root, current->name);
    append_html(&root, "\">" EOL);
    append_html(&root, current->html.data);
    append_html(&root, EOL "<hr>" "</div>" EOL EOL);
  }

  append_html(&root, "</div>" EOL "</body>" EOL "</html>" EOL);

  fp_out = fopen_unsafe(output, "wb");
  if(!fp_out || !fwrite(root.html.data, root.html.length, 1, fp_out))
    fprintf(stderr, "ERROR: could not write output file\n");
  fclose(fp_out);

  free(root.html.data);
}

int main(int argc, char *argv[])
{
  char *file_buffer;
  size_t file_buffer_len;
  int i;

  if(argc < 3)
  {
    fprintf(stdout, USAGE);
    exit(0);
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if(unveil(argv[1], "r") || unveil(argv[2], "cw") ||
   unveil(RESOURCES, "r") || unveil(NULL, NULL))
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

  for(i = 3; i < argc; i++)
  {
    if(!strcasecmp(argv[i], "-c"))
    {
      write_printable_html = false;
      continue;
    }

    if(!strcasecmp(argv[i], "-p"))
    {
      write_printable_html = true;
      continue;
    }
  }

  load_file(&file_buffer, &file_buffer_len, argv[1]);

  if(file_buffer)
  {
    parse_file(file_buffer, file_buffer_len);
    write_html(argv[2]);

    free(file_buffer);
    free_help_files();
    free_anchors();
    free_links();
  }
  return 0;
}

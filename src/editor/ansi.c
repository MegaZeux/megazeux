/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 1998 Matthew D. Williams - dbwilli@scsn.net
 * Copyright (C) 2020-2025 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ansi.h"
#include "buffer.h"
#include "edit.h"

#include "../error.h"
#include "../idput.h"
#include "../robot.h"
#include "../util.h"
#include "../world_struct.h"
#include "../io/vio.h"
#include "../platform/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum sauce_flags
{
  SAUCE_NO_BLINK      = (1<<0), // aka "iCE colors".
  SAUCE_DEFAULT_FONT  = (0<<1),
  SAUCE_8_PIXEL_FONT  = (1<<1),
  SAUCE_9_PIXEL_FONT  = (2<<1),
  SAUCE_DEFAULT_RATIO = (0<<3),
  SAUCE_CLASSIC_RATIO = (1<<3),
  SAUCE_MODERN_RATIO  = (2<<3),
  // Flags that correspond (roughly) to how MZX displays graphics.
  SAUCE_MZX_FLAGS = SAUCE_NO_BLINK | SAUCE_8_PIXEL_FONT | SAUCE_MODERN_RATIO,
};

static const char ansi_csi_prefix[3] = "\x1b[";

/**
 * Convert a reserved character into the closest CP-437 equivalent.
 *
 * @param chr     character to convert, 0-255.
 * @return        `chr`, or the closest equivalent if `chr` is reserved.
 */
static int convert_reserved_character(int chr)
{
  switch(chr)
  {
    case '\0':
      return ' ';   // blank (nul) -> space
    case '\a':
      return 249;   // bullet (bell) -> interpunct
    case '\b':
      return 219;   // inverted bullet (backspace) -> block
    case '\t':
      return 'o';   // circle (tab) -> lowercase o
    case '\n':
      return 219;   // inverted cricle (line feed) -> block
    /* TODO: MegaZeux wasn't stripping this, should it be?
    case '\f':
      return 157;   // female sign (form feed) -> yen
    */
    case '\r':
      return 14;    // single 8th note (carriage return) -> double 8th note
    /* TODO: need more investigation for this one. */
    case 26:
      return '-';   // right thin arrow (EOF for some implementations) -> dash
    case 27:
      return '-';   // left thin arrow (escape) -> dash
  }
  return chr;
}

/**
 * Output meta codes to transform the current color into the destination color.
 *
 * @param curr    color to change from.
 * @param dest    color to change to.
 * @param vf      vfile handle.
 * @return        size of the written meta code (0 if none), or -1 on error.
 */
static ssize_t issue_color_meta_codes(int curr, int dest, vfile *vf)
{
  static const char ega_to_ansi_colors[8] =
  {
    '0', '4', '2', '6', '1', '5', '3', '7'
  };

  int size = 2;
  boolean reset = false;

  if(curr == dest)
    return 0;

  vfwrite(ansi_csi_prefix, 1, 2, vf);

  if((curr & 128) && !(dest & 128))
    reset = true;

  if((curr & 8) && !(dest & 8))
    reset = true;

  if(reset)
  {
    curr = 7;
    vfputc('0', vf);
    size++;
  }

  if(dest & 128)
  {
    // Blink
    if(size > 2)
    {
      vfputc(';', vf);
      size++;
    }
    vfputc('5', vf);
    size++;
  }

  if(dest & 8)
  {
    // Bold
    if(size > 2)
    {
      vfputc(';', vf);
      size++;
    }
    vfputc('1', vf);
    size++;
  }

  if((curr & 7) != (dest & 7))
  {
    // FG color
    if(size > 2)
    {
      vfputc(';', vf);
      size++;
    }
    vfputc('3', vf);
    vfputc(ega_to_ansi_colors[dest & 7], vf);
    size += 2;
  }

  if((curr & 112) != (dest & 112))
  {
    // BG color
    if(size > 2)
    {
      vfputc(';', vf);
      size++;
    }
    vfputc('4', vf);
    vfputc(ega_to_ansi_colors[(dest >> 4) & 7], vf);
    size += 2;
  }

  if(vfputc('m', vf) < 0)
    return -1;

  return size;
}

/**
 * Export a block of ANSi (or plaintext) from the current board. This is
 * assumed to not go over the edges.
 *
 * @param mzx_world   MZX world.
 * @param filename    filename of ANSi or plaintext to write to.
 * @param start_x     position of block to write to ANSi.
 * @param start_y     position of block to write to ANSi.
 * @param width       width of block to write to ANSi.
 * @param height      height of block to write to ANSi.
 * @param text_only   `true`: write plaintext; `false`: write ANSi.
 * @return            `true` on success, otherwise `false`.
 */
boolean export_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 boolean text_only, boolean doorway_mode, const char *title, const char *author)
{
  struct board *cur_board = mzx_world->current_board;
  vfile *vf;
  ssize_t color_size;
  int x, y, line_size;
  int terminal_width = -1;
  int curr_color;
  int col, chr, replace_chr;
  int offset;
  int board_width;
  int line_skip;

  trace("--ANSI-- export_ansi\n");

  vf = vfopen_unsafe_ext(filename, "wb", V_SMALL_BUFFER);
  if(!vf)
    goto err;

  /**
   * NOTE: DOS versions limited the dimensions to 78x23 here but there's not
   * much reason to do this anymore (terminals are variable width now).
   */

  if(mode == EDIT_VLAYER)
    board_width = mzx_world->vlayer_width;
  else
    board_width = cur_board->board_width;

  offset = start_x + start_y * board_width;
  line_skip = board_width - width;

  curr_color = 7;
  line_size = 0;

  if(!text_only)
  {
    vfwrite(ansi_csi_prefix, 1, 2, vf);
    vfwrite("0m", 1, 2, vf);
    vfwrite(ansi_csi_prefix, 1, 2, vf);
    vfwrite("2J", 1, 2, vf);
    line_size += 8;

    if(doorway_mode)
    {
      /* Enable Doorway mode. */
      vfwrite(ansi_csi_prefix, 1, 2, vf);
      vfwrite("=255h", 1, 5, vf);
      line_size += 7;
    }

    /**
     * TODO: it might be useful to allow a user-specified width here so e.g.
     * line ends are inserted.
     */
    terminal_width = width;
  }
  else
    doorway_mode = false;

  // Longest meta code issuable is \x1b[0;5;1;30;40m, or 14 characters.
  for(y = 0; y < height; y++)
  {
    for(x = 0; x < width; x++, offset++)
    {
      switch(mode)
      {
        case EDIT_BOARD:
          chr = get_id_char(cur_board, offset);
          col = get_id_color(cur_board, offset);
          break;

        case EDIT_OVERLAY:
          chr = cur_board->overlay[offset];
          col = cur_board->overlay_color[offset];
          break;

        case EDIT_VLAYER:
          chr = mzx_world->vlayer_chars[offset];
          col = mzx_world->vlayer_colors[offset];
          break;

        default:
          goto err;
      }

      if(!text_only)
      {
        color_size = issue_color_meta_codes(curr_color, col, vf);
        if(color_size < 0)
          goto err;
        line_size += color_size;
      }
      curr_color = col;

      // Print alternate characters for reserved chars.
      replace_chr = convert_reserved_character(chr);
      if(replace_chr != chr && doorway_mode)
      {
        /* Doorway mode: write nul to escape reserved character. */
        vfputc(0, vf);
        line_size++;
      }
      else
        chr = replace_chr;

      vfputc(chr, vf);
      line_size++;

      /**
       * If the line length is near 256, use save/restore to start a new line.
       * TODO: it's not 100% clear why this was added (it was present in 2.51)
       * but it was likely due to a DOS ANSi viewer or editor using fixed size
       * line buffer. More information on this would be great.
       */
      if(!text_only && line_size > 230)
      {
        // Issue save/restore
        vfwrite(ansi_csi_prefix, 1, 2, vf);
        vfputc('s', vf);
        vfputc('\r', vf);
        vfputc('\n', vf);
        vfwrite(ansi_csi_prefix, 1, 2, vf);
        vfputc('u', vf);
        line_size = 3;
      }
    }
    // Issue CR/LF if this isn't the edge of the terminal.
    if(width != terminal_width)
    {
      vfputc('\r', vf);
      vfputc('\n', vf);
      line_size = 0;
    }
    offset += line_skip;
  }

  if(!text_only)
  {
    time_t t;
    struct tm *tm;
    long total_len;
    char datebuf[9];
    char buffer[192];

    color_size = issue_color_meta_codes(curr_color, 7, vf);
    if(color_size < 0)
      goto err;

    if(doorway_mode)
    {
      /* Disable Doorway mode. */
      vfwrite(ansi_csi_prefix, 1, 2, vf);
      vfwrite("=255l", 1, 5, vf);
    }

    t = time(NULL);
    tm = localtime(&t);
    strftime(datebuf, ARRAY_SIZE(datebuf), "%Y%m%d", tm);

    total_len = vftell(vf);

// macOS makes snprintf a macro for some reason so define this separately.
#ifdef VERSION_DATE
#define COMMENT_LINE "Created with MegaZeux " VERSION VERSION_DATE "."
#else
#define COMMENT_LINE "Created with MegaZeux " VERSION "."
#endif

    // SAUCE record.
    vfputc(0x1A, vf);
    snprintf(buffer, ARRAY_SIZE(buffer),
      "COMNT%-64.64sSAUCE00%-35.35s%-20.20s%-20.20s%-8.8s",
      COMMENT_LINE, // Comment line 1.
      title,
      author,
      "",     // Group
      datebuf
    );
    vfputs(buffer, vf);
    vfputd(total_len, vf);
    vfputc(1, vf);                // DataType (Character)
    vfputc(1, vf);                // FileType (ANSi)
    vfputw(terminal_width, vf);   // TInfo1 (terminal width in characters)
    vfputw(height, vf);           // TInfo2 (height in rows)
    vfputw(0, vf);                // TInfo3
    vfputw(0, vf);                // TInfo4
    vfputc(1, vf);                // Comment line count.
    vfputc(SAUCE_MZX_FLAGS, vf);  // TFlags
    snprintf(buffer, ARRAY_SIZE(buffer),
      "%-22.22s",
      "IBM EGA"
    );
    vfputs(buffer, vf);           // TInfoS
  }
  vfclose(vf);
  return true;

err:
  if(vf)
    vfclose(vf);

  error_message(text_only ? E_TEXT_EXPORT : E_ANSI_EXPORT, 0, NULL);
  return false;
}

enum ansi_eol_type
{
  EOL_UNKNOWN,
  EOL_DOS,
  EOL_UNIX,
  EOL_MAC,
};

enum ansi_retvals
{
  ANSI_EOF    = -1,
  ANSI_ESCAPE = -2,
  ANSI_ERROR  = -3,
};

enum ansi_erase
{
  ANSI_NO_ERASE       = -1,
  ANSI_ERASE_TO_END   = 0,
  ANSI_ERASE_TO_START = 1,
  ANSI_ERASE_ALL      = 2,
};

struct ansi_data
{
  int color;
  int x;
  int y;
  int terminal_width;
  int scan_width;
  int scan_height;
  int saved_x;
  int saved_y;
  enum ansi_eol_type eol;
  enum ansi_erase erase_display;
  enum ansi_erase erase_line;
  boolean is_scan;
  boolean doorway_mode;
};

static const struct ansi_data default_state =
{
  7,  // Color
  0,  // x
  0,  // y
  -1, // terminal_width
  0,  // scan_width
  0,  // scan_height,
  0,  // saved_x
  0,  // saved_y,
  EOL_UNKNOWN,
  ANSI_NO_ERASE,
  ANSI_NO_ERASE,
  false,
  false
};

/**
 * Apply an SGR (Select Graphic Rendition) parameter. Only colors are handled.
 *
 * @param ansi    ANSi data.
 * @param param   SGR parameter to apply.
 */
static void apply_ansi_sgr_code(struct ansi_data *ansi, int param)
{
  static const int ansi_to_ega_color[8] =
  {
    0, 4, 2, 6, 1, 5, 3, 7
  };

  switch(param)
  {
    case 0:
      ansi->color = 7;
      break;

    case 1:
      // Bold enable.
      ansi->color |= 8;
      break;

    case 5:
      // Blink enable.
      ansi->color |= 128;
      break;

    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
      // FG color.
      ansi->color = (ansi->color & 248) | ansi_to_ega_color[param - 30];
      break;

    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
      // BG color.
      ansi->color = (ansi->color & 143) | (ansi_to_ega_color[param - 40] << 4);
      break;
  }
}

/**
 * Read a single char or escape code from an ANSI file.
 *
 * @param ansi  ANSi data.
 * @param vf    vfile handle.
 * @return      -1 for EOF, -2 for escape code, -3 for error, otherwise a char.
 */
static int read_ansi(struct ansi_data *ansi, vfile *vf)
{
  int sym = vfgetc(vf);
  if(sym < 0)
    return ANSI_EOF;

  if(ansi->terminal_width > 0 && ansi->x >= ansi->terminal_width && (sym == '\r' || sym == '\n'))
  {
    trace("--ANSI-- line end at %d (term width %d)\n", ansi->x, ansi->terminal_width);
  }

  if(ansi->eol == EOL_UNKNOWN && (sym == '\r' || sym == '\n'))
  {
    int tmp = vfgetc(vf);
    if(sym == '\r' && tmp == '\n')
    {
      trace("--ANSI-- Detected DOS line ends.\n");
      ansi->eol = EOL_DOS;
    }
    else

    if(sym == '\r')
    {
      trace("--ANSI-- Detected MAC line ends.\n");
      ansi->eol = EOL_MAC;
    }
    else
    {
      trace("--ANSI-- Detected Unix line ends.\n");
      ansi->eol = EOL_UNIX;
    }

    vungetc(tmp, vf);
  }

  if(sym == '\r')
  {
    ansi->x = 0;
    if(ansi->eol == EOL_MAC)
      ansi->y++;

    return ANSI_ESCAPE;
  }
  else

  if(sym == '\n')
  {
    if(ansi->eol == EOL_UNIX)
      ansi->x = 0;

    ansi->y++;
    return ANSI_ESCAPE;
  }
  else

  // Nul (escape if Doorway mode is active; otherwise print a space).
  if(sym == '\0')
  {
    if(ansi->doorway_mode)
    {
      sym = vfgetc(vf);
      return sym >= 0 ? sym : ANSI_EOF;
    }
  }
  else

  // TODO: handle bell/backspace/form feed/tab instead of printing them?

  if(sym == 0x1A)
  {
    // TODO: TYPE treats this as an unconditional EOF, PabloDraw refuses to
    // load it, and other sources claim it's just a display character.
    return ANSI_EOF;
  }
  else

  // ANSi escape sequence.
  if(sym == 0x1B)
  {
    if(ansi->eol != EOL_DOS)
    {
      trace("--ANSI-- Detected ANSi, forcing DOS line ends.\n");
      ansi->eol = EOL_DOS;
    }

    sym = vfgetc(vf);
    if(sym == '[')
    {
      /**
       * CSI sequence.
       *
       * TODO: not following the spec, which is:
       * Format: any number of characters 0x30 - 0x3f;
       *         any number of characters 0x20 - 0x2f;
       *         terminating byte         0x40 - 0x7e;
       *         any sequence which includes <=>? or ends in p-z{|}~ is
       *         "private" (the only supported one here is Doorway mode).
       *         If the parameter doesn't start with <=>?, it should only
       *         contain 0-9 and ;.
       */
      int param[32] =
      {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      };
      int cur_param = 0;
      int i;

      sym = vfgetc(vf);
      while(1)
      {
        if(sym == '=')
        {
          // The only extension commands supported in this format are
          // =255h (begin Doorway mode) and =255l (end Doorway mode).
          if(vfgetc(vf) != '2')
            return ANSI_ERROR;
          if(vfgetc(vf) != '5')
            return ANSI_ERROR;
          if(vfgetc(vf) != '5')
            return ANSI_ERROR;

          sym = vfgetc(vf);
          if(sym == 'h')
          {
            trace("--ANSI-- enabled Doorway mode.\n");
            ansi->doorway_mode = true;
            return ANSI_ESCAPE;
          }
          else

          if(sym == 'l')
          {
            trace("--ANSI-- disabled Doorway mode.\n");
            ansi->doorway_mode = false;
            return ANSI_ESCAPE;
          }
          else
            return ANSI_ERROR;
        }
        else

        if(sym == ';')
        {
          // End param.
          if(param[cur_param] < 0)
            return ANSI_ERROR;

          if(cur_param >= (int)ARRAY_SIZE(param))
            return ANSI_ERROR;

          cur_param++;
          sym = vfgetc(vf);
          continue;
        }
        else

        if(sym == 'm')
        {
          for(i = 0; i <= cur_param; i++)
            apply_ansi_sgr_code(ansi, param[i]);

          return ANSI_ESCAPE;
        }
        else

        if(sym == 's')
        {
          // Save position.
          ansi->saved_x = ansi->x;
          ansi->saved_y = ansi->y;
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'u')
        {
          // Restore position.
          ansi->x = ansi->saved_x;
          ansi->y = ansi->saved_y;
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'J')
        {
          // Erase display.
          param[0] = CLAMP(param[0], 0, 2);

          ansi->erase_display = param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'K')
        {
          // Erase line.
          param[0] = CLAMP(param[0], 0, 2);

          ansi->erase_line = param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'A' || sym == 'F')
        {
          // Move up (F is non-standard).
          if(sym == 'F')
            ansi->x = 0;

          if(param[0] <= 0)
            param[0] = 1;

          if(ansi->y >= param[0])
            ansi->y -= param[0];
          else
            ansi->y = 0;

          return ANSI_ESCAPE;
        }
        else

        if(sym == 'B' || sym == 'E')
        {
          // Move down (E is non-standard).
          if(sym == 'E')
            ansi->x = 0;

          if(param[0] <= 0)
            param[0] = 1;

          ansi->y += param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'C')
        {
          // Move right.
          if(param[0] <= 0)
            param[0] = 1;

          ansi->x += param[0];
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'D')
        {
          // Move left.
          if(param[0] <= 0)
            param[0] = 1;

          if(ansi->x >= param[0])
            ansi->x -= param[0];
          else
            ansi->x = 0;

          return ANSI_ESCAPE;
        }
        else

        if(sym == 'G')
        {
          // Cursor column (non-standard).
          if(param[0] <= 0)
            param[0] = 1;

          ansi->x = param[0] - 1;
          return ANSI_ESCAPE;
        }
        else

        if(sym == 'H' || sym == 'f')
        {
          // Cursor position.
          if(param[0] <= 0)
            param[0] = 1;
          if(param[1] <= 0)
            param[1] = 1;

          ansi->x = param[0] - 1;
          ansi->y = param[1] - 1;
          return ANSI_ESCAPE;
        }
        else

        // Ignore unsupported CSI sequences.
        if((sym < '0') || (sym > '9'))
          return ANSI_ESCAPE;

        // Read parameter.
        param[cur_param] = 0;
        while(sym >= '0' && sym <= '9')
        {
          param[cur_param] = (param[cur_param] * 10) + (sym - '0');
          sym = vfgetc(vf);
        }
      }
    }
  }
  return sym;
}

/**
 * Read SAUCE from an incoming file, if present.
 * `ansi->terminal_width` should be initialized prior to calling this function.
 * This function will set `ansi->terminal_width` depending on the SAUCE width
 * and the initial value of `ansi->terminal_width`. If no SAUCE record is found,
 * no values in the ANSi data will be modified.
 *
 * @param ansi  ANSi data.
 * @param vf    vfile handle.
 * @return      `true` if relevant SAUCE data was found, otherwise `false`.
 */
static boolean read_sauce(struct ansi_data *ansi, vfile *vf)
{
  long file_len = vfilelength(vf, false);
  char magic[7];

  trace("--ANSI-- read_sauce\n");

  // Check for a SAUCE record. If found and valid, prefer its requested
  // wrap width over the provided wrap width.
  if(file_len > 128)
  {
    vfseek(vf, file_len - 128, SEEK_SET);

    if(vfread(magic, 7, 1, vf) && !strncmp(magic, "SAUCE00", 7))
    {
      int data_type;
      int file_type;
      int sauce_width;
      int sauce_height;

      // Skip optional fields.
      vfseek(vf, 35+20+20+8+4, SEEK_CUR);

      data_type = vfgetc(vf);
      file_type = vfgetc(vf);
      sauce_width = vfgetw(vf);
      sauce_height = vfgetw(vf);

      if(sauce_height >= 0 && data_type == 1 /* Character */ && file_type == 1 /* ANSi */)
      {
        /*
        // The width value is the terminal width and the line count is
        // apparently unreliable, so don't set the scan dimensions from either.
        ansi->scan_width = sauce_width;
        ansi->scan_height = sauce_height;
        */
        trace("--ANSI-- SAUCE width=%d, height=%d\n", sauce_width, sauce_height);

        if(sauce_width &&
         (ansi->terminal_width < 1 || ansi->terminal_width > sauce_width))
        {
          ansi->terminal_width = sauce_width;
          trace("--ANSI-- using SAUCE width as ANSi terminal width.\n");
        }
        else

        if(!sauce_width && ansi->terminal_width < 1)
        {
          ansi->terminal_width = 80;
          trace("--ANSI-- SAUCE record with width 0--using default width "
           "of 80 as ANSi terminal width.\n");
        }
        return true;
      }
    }
  }
  trace("--ANSI-- no relevant SAUCE data found.\n");
  return false;
}

/**
 * Test an ANSi file for correctness and determine its output dimensions.
 * The expected output dimensions are required to correctly load the ANSi later.
 *
 * @param filename    filename of ANSi or plaintext file.
 * @param wrap_width  force line wrap after this many columsn (<=0 for none).
 * @param width       pointer to store width at.
 * @param height      pointer to store height at.
 * @return            `true` if the ANSi file is valid, otherwise `false`.
 */
boolean validate_ansi(const char *filename, int wrap_width, int *width, int *height)
{
  struct ansi_data ansi = default_state;
  vfile *vf;
  int chr;

  trace("--ANSI-- validate_ansi\n");

  *width = -1;
  *height = -1;

  vf = vfopen_unsafe_ext(filename, "rb", V_SMALL_BUFFER);
  if(!vf)
    goto err;

  ansi.is_scan = true;
  ansi.terminal_width = wrap_width;

  read_sauce(&ansi, vf);
  vrewind(vf);

  while(1)
  {
    chr = read_ansi(&ansi, vf);
    if(chr == ANSI_EOF)
      break;

    if(chr == ANSI_ERROR)
      goto err;

    if(chr >= 0)
    {
      if(ansi.x + 1 > ansi.scan_width)
        ansi.scan_width = ansi.x + 1;
      if(ansi.y + 1 > ansi.scan_height)
        ansi.scan_height = ansi.y + 1;

      ansi.x++;
    }

    if(ansi.terminal_width > 0 && ansi.x >= ansi.terminal_width)
    {
      ansi.x = 0;
      ansi.y++;
    }
  }
  trace("--ANSI-- scan width=%d, height=%d\n", ansi.scan_width, ansi.scan_height);

  if(ansi.terminal_width <= 0 || ansi.scan_width < ansi.terminal_width)
  {
    /**
     * If no terminal width was used, a line end was present at the end of the
     * longest line. Add one to the width to avoid line wrapping. This should
     * only affect TXT files or ANSi files in the case where 1) no SAUCE record
     * exists and 2) a wrap width wasn't explicitly provided by the user.
     */
    trace("--ANSI-- adding 1 to final width to avoid line wrapping: %d, %d\n",
     ansi.scan_width + 1, ansi.scan_height);
    ansi.scan_width++;
  }

  vfclose(vf);
  *width = ansi.scan_width;
  *height = ansi.scan_height;
  return true;

err:
  if(vf)
    vfclose(vf);

  error_message(E_ANSI_IMPORT, 0, NULL);
  return false;
}

/**
 * Import an ANSi or text graphic into the board as the selected type.
 * Undo history should be managed separately for this block. This function
 * clips the ANSi to the size of the destination board/vlayer.
 *
 * @param mzx_world   MZX world.
 * @param filename    filename of ANSi or plaintext file.
 * @param mode        location to load ANSi to (board, overlay, or vlayer).
 * @param start_x     position to load ANSi to.
 * @param start_y     position to load ANSi to.
 * @param width       width of the ANSi to load (see `validate_ansi`).
 *                    Note this isn't the width of the area to load to.
 * @param height      height of the ANSi to load (see `validate_ansi`).
 *                    Note this isn't the height of the area to load to.
 * @param convert_id  thing to load ANSi chars as (board only).
 * @return            `true` on success, otherwise `false`.
 */
boolean import_ansi(struct world *mzx_world, const char *filename,
 enum editor_mode mode, int start_x, int start_y, int width, int height,
 enum thing convert_id)
{
  struct ansi_data ansi = default_state;
  struct buffer_info ansi_buffer;
  struct buffer_info clear_buffer;
  struct board *cur_board = mzx_world->current_board;
  int board_width = cur_board->board_width;
  int board_height = cur_board->board_height;
  int wrap_width = width;
  boolean need_erase = true;
  vfile *vf;
  int chr;

  trace("--ANSI-- import_ansi\n");

  // Bound the area to the board (or vlayer).
  if(mode == EDIT_VLAYER)
  {
    board_width = mzx_world->vlayer_width;
    board_height = mzx_world->vlayer_height;
  }

  if(start_x >= board_width || start_y >= board_height ||
   start_x < 0 || start_y < 0 || width < 1 || height < 1)
    return false;

  trace("--ANSI-- image is %d by %d\n", width, height);

  if(start_x + width > board_width)
    width = board_width - start_x;

  if(start_y + height > board_height)
    height = board_height - start_y;

  memset(&ansi_buffer, 0, sizeof(struct buffer_info));
  memset(&clear_buffer, 0, sizeof(struct buffer_info));

  if(!is_storageless(convert_id))
    convert_id = CUSTOM_BLOCK;

  ansi_buffer.id = convert_id;
  ansi_buffer.color = ansi.color;
  clear_buffer.id = convert_id;
  clear_buffer.color = 7;
  clear_buffer.param = ' ';

  vf = vfopen_unsafe_ext(filename, "rb", V_SMALL_BUFFER);
  if(!vf)
    goto err;

  if(!read_sauce(&ansi, vf))
  {
    // If no SAUCE terminal width is found, the provided width from the scan
    // should be the correct width.
    ansi.terminal_width = wrap_width;
  }
  vrewind(vf);

  trace("--ANSI-- importing @ %d,%d (%d by %d) with terminal width=%d\n",
   start_x, start_y, width, height, ansi.terminal_width);

  while(1)
  {
    if(need_erase)
    {
      // MZX ANSI files contain an initial display clear but many others do not.
      // Inject an erase display escape at the start of the file so the drawing
      // area is guaranteed to be filled with the selected ID.
      ansi.erase_display = ANSI_ERASE_ALL;
      chr = ANSI_ESCAPE;
      need_erase = false;
    }
    else
      chr = read_ansi(&ansi, vf);

    if(chr == ANSI_EOF)
      break;

    if(chr == ANSI_ERROR) // Shouldn't happen.
      goto err;

    if(chr == ANSI_ESCAPE)
    {
      if(ansi.erase_display != ANSI_NO_ERASE)
      {
        int x1 = 0;
        int x2 = width - 1;
        int y1 = 0;
        int y2 = height - 1;

        switch(ansi.erase_display)
        {
          case ANSI_ERASE_TO_START:
            x2 = ansi.x;
            y2 = ansi.y;
            break;
          case ANSI_ERASE_TO_END:
            x1 = ansi.x;
            y1 = ansi.y;
            break;
          default:
            ansi.x = 0;
            ansi.y = 0;
            break;
        }

        while(1)
        {
          if(x1 < width && y1 < height)
          {
            place_current_at_xy(mzx_world, &clear_buffer,
             start_x + x1, start_y + y1, mode, NULL);
          }

          if(x1 >= x2)
          {
            if(y1 >= y2)
              break;
            x1 = 0;
            y1++;
          }
          else
            x1++;
        }
      }
      else

      if(ansi.erase_line != ANSI_NO_ERASE)
      {
        int x1 = (ansi.erase_line == ANSI_ERASE_TO_END) ? ansi.x : 0;
        int x2 = (ansi.erase_line == ANSI_ERASE_TO_START) ? ansi.x : width - 1;

        while(x1 <= x2)
        {
          if(x1 < width && ansi.y < height)
            place_current_at_xy(mzx_world, &clear_buffer,
             start_x + x1, start_y + ansi.y, mode, NULL);
          x1++;
        }
      }

      ansi_buffer.color = ansi.color;
      ansi.erase_line = ANSI_NO_ERASE;
      ansi.erase_display = ANSI_NO_ERASE;
    }
    else
    {
      if(ansi.x < width && ansi.y < height)
      {
        ansi_buffer.param = chr;

        if(place_current_at_xy(mzx_world, &ansi_buffer,
         start_x + ansi.x, start_y + ansi.y, mode, NULL) == -1)
        {
          goto err; // Shouldn't ever happen--can't place storage objects.
        }
      }
      ansi.x++;
    }

    if(ansi.terminal_width > 0 && ansi.x >= ansi.terminal_width)
    {
      //trace("force wrap at %d,%d\n", ansi.x, ansi.y);
      ansi.x = 0;
      ansi.y++;
    }
  }
  vfclose(vf);
  return true;

err:
  if(vf)
    vfclose(vf);

  error_message(E_ANSI_IMPORT, 0, NULL);
  return false;
}

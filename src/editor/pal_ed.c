/* MegaZeux
 *
 * Copyright (C) 1996 Alexis Janson
 * Copyright (C) 2017-2026 Alice Rowan <petrifiedrowan@gmail.com>
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

/* Palette editor */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../core.h"
#include "../data.h"
#include "../graphics.h"
#include "../intake_num.h"
#include "../util.h"
#include "../window.h"
#include "../io/vio.h"

#include "../event/event.h"

#include "configure.h"
#include "graphics.h"
#include "pal_ed.h"
#include "window.h"

// Color editor (right) and 16-color palette (left)

//----------------------------------//------------------------------------------/
// ##..##..##..##..##..##..##..##.. // Color # 000           RGB  HSL  CIELAB   /
// ##..##..##..##..##..#1.1#1.1#1.1 // Red   0 [----|----|----|----|----|----|] /
// #0.1#2.3#4.5#6.7#8.9#0.1#2.3#4.5 // G     0 [----|----|----|----|----|----|] /
// ##..##..##..##..##..##..##..##.. // B     0 [----|----|----|----|----|----|] /
//-^^-------------------------------//------------------------------------------/

// 16-color help

//------------------------------------------------------------------/
//
//  %- Select color    Alt+D- Default pal.      PgUp- Prev. mode
//  R- Increase Red    Alt+R- Decrease Red      PgDn- Next mode
//  G- Increase Green  Alt+G- Decrease Green
//  B- Increase Blue   Alt+B- Decrease Blue        Q- Quit editing  /
//  A- Increase All    Alt+A- Decrease All
//  0- Blacken color   Alt+H- Hide menu          Tab- Switch to
//  F2- Store color    Alt+I- Import                  temp palette  /
//  F3- Place color    Alt+X- Export                                /
//                                                   /--------------/
//  Left click-       edit component/activate        | Buffer ##### /
//  Right click+Drag- change color                   |        ##### /
//                                                   |        ##### /
//---------------------------------------------------+--------------/

// 256-color subpalette

//----------------------------------/
// Subpal.  1:123 2:123 3:123 4:123 /
//  # 000   ##### ##### ##### ##### /
//          ##### ##### ##### ##### /
// Hex: FF  ##### ##### ##### ##### /
//----------------------------------/

// 256-color palette

//----------------------------------/ 36*18, color pos + 8
// [][][][][][][][][][][][][][][][] /
// ... 16 + 2

// 256-color help

//------------------------------------------/
// %%- Select color   Alt+D- Default pal.   /
// R- Increase Red    Alt+R- Decrease Red   /
// G- Increase Green  Alt+G- Decrease Green /
// B- Increase Blue   Alt+B- Decrease Blue  /
// A- Increase All    Alt+A- Decrease All   /
// 0- Blacken color   Alt+H- Hide menu      /
// F2- Store color       F5- Store colors
// F3- Place color       F6- Load colors
// Ins- Cursors (off)  PgUp- Prev. mode
// Tab- Switch to      PgDn- Next mode
//      temp. palette Alt+I- Import
// Q- Quit editing    Alt+X- Export
//

// Mode 3- this replaces 'Hide menu'/'Import'/'Export'...
//
//Space- Subpalette
//   F5- Store colors
//   F6- Place colors
//   F7- Store indices
//   F8- Place indices
//  1-4- Current index
//       to subpalette

//                           /--------------/
// Left click- activate      | Buffer ##### /
// Right click- color (drag) | 1####3 ##### /
// Middle/wheel- subpalette  | 2####4 ##### /
//---------------------------+--------------/

// Note: the help menu mouse functionality has been broken since 2.80.

#define CHAR_LINE_HORIZ '\xC4'
#define CHAR_CORNER_TL  '\xDA'
#define CHAR_CORNER_TR  '\xBF'
#define CHAR_CORNER_BL  '\xC0'
#define CHAR_CORNER_BR  '\xD9'

struct color_status
{
  unsigned char r;
  unsigned char g;
  unsigned char b;

  unsigned int h;
  unsigned char s;
  unsigned char l;

  unsigned char CL;
  int Ca;
  int Cb;
};

static boolean saved_color_active = false;
static boolean saved_subpalette_active = false;
static boolean saved_subpalette_display = false;

static struct color_status saved_color;
static struct color_status saved_subpal[4];

static int saved_indices[4] =
{
  0, 1, 2, 3
};

static boolean startup = false;
static boolean minimal_help = false;
static boolean subpalette_cursors = true;

static unsigned int current_id = 0;
static unsigned int current_subpalette = 0;
static int current_mode_id = 0;

static int temp_palette_mode = -1;
static char temp_palette[SMZX_PAL_SIZE * 3];
static char temp_indices[SMZX_PAL_SIZE * 4];

static char pal_filename[MAX_PATH];
static char palidx_filename[MAX_PATH];

struct pal_ed_context
{
  context ctx;
  struct color_status internal_palette[SMZX_PAL_SIZE];
  char game_palette_backup[SMZX_PAL_SIZE * 3];
  char game_indices[SMZX_PAL_SIZE * 4];
  char *current_indices;
  enum
  {
    GAME_PALETTE,
    TEMP_PALETTE,
  } current;
  //uint32_t cursor_fg_layer[5];
  //uint32_t cursor_bg_layer[5];
  int editing_component;
};

struct pal_ed_subcontext
{
  subcontext ctx;
  struct pal_ed_context *pal_ed;
  // Border corner coordinates
  int border_x;
  int border_y;
  int border_x2;
  int border_y2;
  // Start of contents coordinates
  int x;
  int y;
};

static const char *palette_labels[] =
{
  "game palette",
  "temp palette"
};

static const char *const pal_ext[] = { ".PAL", NULL };
static const char *const idx_ext[] = { ".PALIDX", NULL };


// -----------------------------------------------------------------------------


/**
 * RGB-HSL conversion functions.
 */

static void rgb_to_hsl(struct color_status *current)
{
  double r = (double)current->r / 63.0;
  double g = (double)current->g / 63.0;
  double b = (double)current->b / 63.0;

  double M = MAX(r, MAX(g, b));
  double m = MIN(r, MIN(g, b));
  double c = M - m;

  double h =
   (c == 0) ? 0 :
   (M == r) ? fmod((g - b)/c + 6, 6.0)  : // Add 6 to bypass C's shit modulo
   (M == g) ? ((b - r)/c) + 2           :
   (M == b) ? ((r - g)/c) + 4           : 0;

  double l = (M + m) / 2.0;

  current->h = (unsigned int) round( fmod(h * 60, 360.0) );

  current->s = (unsigned int) round( l < 1  ?  c/(1 - fabs(M+m-1)) * 100 : 0 );

  current->l = (unsigned int) round( l * 100 );
}

static void hsl_to_rgb(struct color_status *current)
{
  double l = (double)current->l / 100.0;
  double s = (double)current->s / 100.0;
  double h = (double)current->h / 60.0;

  double c = (1 - fabs(2 * l - 1)) * s;

  double m = l - c / 2.0;

  double x = c * (1 - fabs(fmod(h, 2.0) - 1));

  int fh = floor(h);

  double r =
   (fh == 0) || (fh == 5) ? c+m :
   (fh == 1) || (fh == 4) ? x+m : m;

  double g =
   (fh == 1) || (fh == 2) ? c+m :
   (fh == 0) || (fh == 3) ? x+m : m;

  double b =
   (fh == 3) || (fh == 4) ? c+m :
   (fh == 2) || (fh == 5) ? x+m : m;

  current->r = (unsigned char) round(r * 63.0);
  current->g = (unsigned char) round(g * 63.0);
  current->b = (unsigned char) round(b * 63.0);
}

/**
 * RGB-CIELAB conversion functions.
 */

static const double Xn = 0.9505;
static const double Yn = 1.0000;
static const double Zn = 1.0890;

static inline double srgb_comp_to_linear(double c)
{
  return (c <= 0.04045)  ?  c / 12.92  :  pow((c + 0.055)/(1.055), 2.4);
}

static inline double srgb_linear_to_comp(double l)
{
  return (l <= 0.0031308)  ?  l * 12.92  :  (1.055)*pow(l, 1/2.4) - 0.055;
}

static inline double xyz_to_lab_comp_transform(double c)
{
  return (c > 0.008856)  ?  cbrt(c)  :  (903.3 * c + 16)/116.0;
}

static inline double lab_to_xyz_comp_transform(double c)
{
  return (c > 0.205893)  ?  pow(c, 3.0)  :  0.128414 * (c - 0.137931);
}

static void rgb_to_lab(struct color_status *current)
{
  double r = srgb_comp_to_linear( (double)(current->r) / 63.0 );
  double g = srgb_comp_to_linear( (double)(current->g) / 63.0 );
  double b = srgb_comp_to_linear( (double)(current->b) / 63.0 );

  double x = 0.4124564*r + 0.3575761*g + 0.1804375*b;
  double y = 0.2126729*r + 0.7151522*g + 0.0721750*b;
  double z = 0.0193339*r + 0.1191920*g + 0.9503041*b;

  double fx = xyz_to_lab_comp_transform( x / Xn );
  double fy = xyz_to_lab_comp_transform( y / Yn );
  double fz = xyz_to_lab_comp_transform( z / Zn );

  current->CL = (unsigned int) round( 116 * fy - 16 );
  current->Ca = (int) round( 500 * (fx - fy) );
  current->Cb = (int) round( 200 * (fy - fz) );
}

static void lab_to_rgb(struct color_status *current)
{
  double CL = (double)(current->CL);
  double Ca = current->Ca;
  double Cb = current->Cb;

  double fy = (CL + 16) / 116.0;
  double fx = fy + Ca / 500.0;
  double fz = fy - Cb / 200.0;

  double x = Xn * lab_to_xyz_comp_transform( fx );
  double y = Yn * lab_to_xyz_comp_transform( fy );
  double z = Zn * lab_to_xyz_comp_transform( fz );

  double r =  3.2404542*x + -1.5371385*y + -0.4985314*z;
  double g = -0.9692660*x +  1.8760108*y +  0.0415560*z;
  double b =  0.0556434*x + -0.2040259*y +  1.0572252*z;

  r = srgb_linear_to_comp( r );
  g = srgb_linear_to_comp( g );
  b = srgb_linear_to_comp( b );

  current->r = (unsigned int) round(CLAMP(r, 0.0, 1.0) * 63.0);
  current->g = (unsigned int) round(CLAMP(g, 0.0, 1.0) * 63.0);
  current->b = (unsigned int) round(CLAMP(b, 0.0, 1.0) * 63.0);
}

/**
 * Color management functions.
 */

static void load_color(struct color_status *current, int id)
{
  get_rgb(id, &(current->r), &(current->g), &(current->b));
  rgb_to_hsl(current);
  rgb_to_lab(current);
}

static void store_color_pos(struct color_status *current, int id)
{
  set_rgb(id, current->r, current->g, current->b);
}

static void store_color_rgb(struct color_status *current)
{
  rgb_to_hsl(current);
  rgb_to_lab(current);
  set_rgb(current_id, current->r, current->g, current->b);
}

static void store_color_hsl(struct color_status *current)
{
  hsl_to_rgb(current);
  rgb_to_lab(current);
  set_rgb(current_id, current->r, current->g, current->b);
}

static void store_color_lab(struct color_status *current)
{
  lab_to_rgb(current);
  rgb_to_hsl(current);
  set_rgb(current_id, current->r, current->g, current->b);
}

static int get_color_rgb(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->r;
    case 1: return (int)current->g;
    case 2: return (int)current->b;
  }
  return -1;
}

static int get_color_hsl(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->h;
    case 1: return (int)current->s;
    case 2: return (int)current->l;
  }
  return -1;
}

static int get_color_lab(struct color_status *current, int component)
{
  switch(component)
  {
    case 0: return (int)current->CL;
    case 1: return (int)current->Ca;
    case 2: return (int)current->Cb;
  }
  return -1;
}

static void set_color_rgb(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->r = value;
      break;

    case 1:
      current->g = value;
      break;

    case 2:
      current->b = value;
      break;
  }
  store_color_rgb(current);
}

static void set_color_hsl(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->h = value;
      break;

    case 1:
      current->s = value;
      break;

    case 2:
      current->l = value;
      break;
  }
  store_color_hsl(current);
}

static void set_color_lab(struct color_status *current, int component,
 int value)
{
  switch(component)
  {
    case 0:
      current->CL = value;
      break;

    case 1:
      current->Ca = value;
      break;

    case 2:
      current->Cb = value;
      break;
  }
  store_color_lab(current);
}

struct color_mode_component
{
  const char *name_long;  // Long name for menu, max 5 chars
  const char *name_short; // Short name for selector, max 3 chars
  const char *name_key;   // Key name, max 1 char
  enum keycode key;       // Keycode to trigger inc/dec
  char left_col;
  char right_col;
  int min_val;
  int max_val;
  boolean wrap;
};

struct color_mode
{
  const char *name;
  int draw_x;
  struct color_mode_component components[3];
  void (*store_function)(struct color_status *);
  int (*get_function)(struct color_status *, int);
  void (*set_function)(struct color_status *, int, int);
  enum keycode all_key;
};

static const struct color_mode mode_list[] =
{
  { "RGB", 22,
    {
      { "Red",   "Red ", "R", IKEY_r, 12,  0,    0,  63, false },
      { "Green", "Grn ", "G", IKEY_g, 10,  0,    0,  63, false },
      { "Blue",  "Blu ", "B", IKEY_b,  9,  0,    0,  63, false },
    },
    store_color_rgb,
    get_color_rgb,
    set_color_rgb,
    IKEY_a
  },

  { "HSL", 27,
    {
      { "Hue",   "Hue",  "C", IKEY_c,  4,  4,    0, 359, true  },
      { "Sat.",  "Sat",  "S", IKEY_s,  7,  0,    0, 100, false },
      { "Light", "Lgt",  "V", IKEY_v, 15,  0,    0, 100, false },
    },
    store_color_hsl,
    get_color_hsl,
    set_color_hsl,
    IKEY_UNKNOWN
  },

  { "CIELAB", 32,
    {
      { "L*",    "L*",   "V", IKEY_v, 15,  0,    0, 100, false },
      { "a*",    "a*",   "A", IKEY_a,  4,  2, -128, 128, false },
      { "b*",    "b*",   "B", IKEY_b,  6,  1, -128, 128, false },
    },
    store_color_lab,
    get_color_lab,
    set_color_lab,
    IKEY_UNKNOWN
  },
};

/**
 * Utility functions.
 */

static struct color_status *get_current_color(struct pal_ed_context *pal_ed)
{
  return &(pal_ed->internal_palette[current_id]);
}

static struct color_mode *get_current_mode(struct pal_ed_context *pal_ed)
{
  return (struct color_mode *)&(mode_list[current_mode_id]);
}

static boolean get_current_indices(struct pal_ed_context *pal_ed,
 int *a, int *b, int *c, int *d)
{
  int screen_mode = get_screen_mode();
  if(screen_mode < 2)
    return false;

  if(screen_mode == 2)
  {
    int lo = current_id & (0x0F);
    int hi = (current_id & (0xF0)) >> 4;
    *a = (hi << 4) | hi;
    *b = (lo << 4) | hi;
    *c = (hi << 4) | lo;
    *d = (lo << 4) | lo;
  }
  else
  {
    if(!pal_ed->current_indices)
      return false;

    *a = pal_ed->current_indices[current_subpalette * 4 + 0];
    *b = pal_ed->current_indices[current_subpalette * 4 + 2];
    *c = pal_ed->current_indices[current_subpalette * 4 + 1];
    *d = pal_ed->current_indices[current_subpalette * 4 + 3];
  }
  return true;
}

#define MOUSE_IN(b_x, b_y, b_w, b_h) \
  ((mouse_x >= (b_x)) && (mouse_x < (b_x) + (b_w)) && \
   (mouse_y >= (b_y)) && (mouse_y < (b_y) + (b_h)))


// -----------------------------------------------------------------------------


static char hue_chars[32] =
{
  0xDB, 0xB0, 0xB0, 0xB1, 0xB2,  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,
  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,  0xDB, 0xB0, 0xB1, 0xB1, 0xB2,
  0xB2, 0xDB, 0xB0, 0xB1, 0xB2,  0xDB, 0xDB, 0xB0, 0xB1, 0xB2,
  0xDB, 0xDB
};

static char hue_colors[32] =
{
  0xCC, 0xC6, 0xCE, 0xCE, 0xCE,  0xEE, 0xEE, 0xEA, 0xEA, 0xEA,
  0xAA, 0xAA, 0xAB, 0xAB, 0xAB,  0xBB, 0xB3, 0xB3, 0xB9, 0xB9,
  0x39, 0x99, 0x9D, 0x9D, 0x9D,  0xDD, 0xDD, 0xDC, 0xDC, 0xDC,
  0xCC, 0xCC
};

static void draw_hue_bar(int value, unsigned int x, unsigned int y,
 int min_val, int max_val)
{
  int i;

  value = ((value - min_val)*31 + (max_val - min_val)/2) / (max_val - min_val);

  for(i = 0; i < 32; i++)
  {
    draw_char(hue_chars[i], hue_colors[i], x+i, y);
  }

  // TODO this could be a layer
  draw_char(0xDB, 0xFF, x+value, y);
}

static void draw_color_bar(int value, unsigned int x, unsigned int y,
 int min_val, int max_val, char left_col, char right_col)
{
  char bg_col;
  char fg_col;
  int i;
  int j;

  value = ((value - min_val)*63 + (max_val - min_val)/2) / (max_val - min_val);

  for(i = 0, j = 0; i < 32; i++)
  {
    bg_col =
     (value > j) ? left_col  :
     (value < j) ? right_col : 15;
    j++;

    fg_col =
     (value > j) ? left_col  :
     (value < j) ? right_col : 15;
    j++;

    draw_char('\xDE', (bg_col << 4) + fg_col, x+i, y);
  }
}

/**
 * Draw the components inside of the color editor.
 */

static void draw_color_components(struct pal_ed_subcontext *current)
{
  struct pal_ed_context *pal_ed = current->pal_ed;
  struct color_status *current_color = get_current_color(pal_ed);
  struct color_mode *current_mode = get_current_mode(pal_ed);

  const struct color_mode_component *c;
  char buffer[5];
  int value;
  int i;

  for(i = 0; i < 3; i++)
  {
    value = current_mode->get_function(current_color, i);
    c = &(current_mode->components[i]);

    sprintf(buffer, "%4d", value);

    write_string(
     buffer,
     current->x + 3,
     current->y + 1 + i,
     DI_GREY_NUMBER,
     WR_NONE
    );

    write_string(
     c->name_short,
     current->x,
     current->y + 1 + i,
     DI_GREY_TEXT,
     WR_NONE
    );

    if(c->min_val == 0 && c->max_val == 359)
    {
      // Special case - the hue bar
      draw_hue_bar(value, current->x + 8, current->y + 1 + i,
       c->min_val, c->max_val);
    }

    else
    {
      // Otherwise, draw a regular bar
      draw_color_bar(value, current->x + 8, current->y + 1 + i,
       c->min_val, c->max_val, c->left_col, c->right_col);
    }
  }
}

/**
 * Draw the color editor.
 */

static boolean color_editor_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *current = (struct pal_ed_subcontext *)ctx;
  char color;
  int i;

  draw_window_box(
   current->border_x,  current->border_y,
   current->border_x2, current->border_y2,
   DI_GREY, DI_GREY_DARK, DI_GREY_CORNER, true, true
  );

  // Write Color #
  write_string(
   "Color #",
   current->x,
   current->y,
   DI_GREY_TEXT,
   WR_NONE
  );

  write_number(current_id, DI_GREY_TEXT,
   current->x + 10, current->y, 3, 1, 10);

  // Modes
  for(i = 0; i < (int)ARRAY_SIZE(mode_list); i++)
  {
    color = (i == current_mode_id) ? DI_GREY : DI_GREY_DARK;

    write_string(
     mode_list[i].name,
     current->x + mode_list[i].draw_x,
     current->y,
     color,
     WR_NONE
    );
  }

  draw_color_components(current);
  return true;
}

static void inc_component(struct color_status *current_color,
 struct color_mode *current_mode, int component, int mod)
{
  struct color_mode_component *c = &(current_mode->components[component]);
  int value = current_mode->get_function(current_color, component) + mod;

  if(value < c->min_val)
  {
    if(c->wrap)
      value = c->max_val;
    else
      value = c->min_val;
  }
  else

  if(value > c->max_val)
  {
    if(c->wrap)
      value = c->min_val;
    else
      value = c->max_val;
  }

  current_mode->set_function(current_color, component, value);
}

/**
 * Key function for the color editor.
 * These keys are mostly configured on a per-color mode basis and need
 * to be handled specially.
 */

static boolean color_editor_key(subcontext *ctx, int *key)
{
  struct pal_ed_subcontext *current = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = current->pal_ed;
  struct color_status *current_color = get_current_color(pal_ed);
  struct color_mode *current_mode = get_current_mode(pal_ed);
  int i;

  // Components
  for(i = 0; i < 3; i++)
  {
    if(*key == (int)current_mode->components[i].key)
    {
      if(get_alt_status(keycode_internal))
        inc_component(current_color, current_mode, i, -1);

      else
        inc_component(current_color, current_mode, i, 1);

      return true;
    }
  }

  // All
  if(current_mode->all_key && (int)current_mode->all_key == *key)
  {
    if(get_alt_status(keycode_internal))
    {
      inc_component(current_color, current_mode, 0, -1);
      inc_component(current_color, current_mode, 1, -1);
      inc_component(current_color, current_mode, 2, -1);
    }
    else
    {
      inc_component(current_color, current_mode, 0, 1);
      inc_component(current_color, current_mode, 1, 1);
      inc_component(current_color, current_mode, 2, 1);
    }
    return true;
  }

  switch(*key)
  {
    case IKEY_F2:
    {
      // Ignore Alt+F2, Ctrl+F2
      if(get_alt_status(keycode_internal) || get_ctrl_status(keycode_internal))
        break;

      memcpy(&saved_color, current_color, sizeof(struct color_status));
      saved_subpalette_display = false;
      saved_color_active = true;
      return true;
    }

    case IKEY_F3:
    {
      if(saved_color_active)
      {
        memcpy(current_color, &saved_color, sizeof(struct color_status));
        store_color_rgb(current_color);
        saved_subpalette_display = false;
      }
      return true;
    }

    case IKEY_0:
    {
      current_color->r = 0;
      current_color->g = 0;
      current_color->b = 0;
      store_color_rgb(current_color);
      return true;
    }

    case IKEY_PAGEUP:
    {
      if(current_mode_id == 0)
        current_mode_id = ARRAY_SIZE(mode_list);

      current_mode_id--;
      return true;
    }

    case IKEY_PAGEDOWN:
    {
      current_mode_id++;

      if(current_mode_id == ARRAY_SIZE(mode_list))
        current_mode_id = 0;

      return true;
    }
  }
  return false;
}

/**
 * Callback for component editing.
 */

static void edit_component_callback(subcontext *ctx, int new_value)
{
  struct pal_ed_context *pal_ed = ((struct pal_ed_subcontext *)ctx)->pal_ed;
  struct color_status *current_color = get_current_color(pal_ed);
  struct color_mode *current_mode = get_current_mode(pal_ed);
  int component = pal_ed->editing_component;

  current_mode->set_function(current_color, component, new_value);
}

/**
 * Click function for color editor.
 */

static boolean color_editor_click(subcontext *ctx, int *key, int button,
 int mouse_x, int mouse_y)
{
  struct pal_ed_subcontext *current = (struct pal_ed_subcontext *)ctx;

  if(button == MOUSE_BUTTON_LEFT)
  {
    // Component numbers

    if(MOUSE_IN(current->x, current->y + 1, 7, 3))
    {
      int component = mouse_y - current->y - 1;
      struct pal_ed_context *pal_ed = current->pal_ed;
      struct color_status *current_color = get_current_color(pal_ed);
      struct color_mode *current_mode = get_current_mode(pal_ed);
      struct color_mode_component *c = &(current_mode->components[component]);
      int value;

      current->pal_ed->editing_component = component;
      value = current_mode->get_function(current_color, component);

      intake_num(ctx, value, c->min_val, c->max_val,
       current->x + 3, mouse_y, 3, DI_GREY_EDIT, edit_component_callback);

      return true;
    }
    else

    // Mode select

    if(mouse_y == current->y)
    {
      int len;
      int i;

      for(i = 0; i < (int)ARRAY_SIZE(mode_list); i++)
      {
        const struct color_mode *m = &(mode_list[i]);
        len = strlen(m->name);

        if(MOUSE_IN(current->x + m->draw_x, current->y, len, 1))
        {
          current_mode_id = i;
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Drag function for color editor.
 */

static boolean color_editor_drag(subcontext *ctx, int *key, int button,
 int mouse_x, int mouse_y)
{
  struct pal_ed_subcontext *current = (struct pal_ed_subcontext *)ctx;
  struct color_status *current_color = get_current_color(current->pal_ed);
  struct color_mode *current_mode = get_current_mode(current->pal_ed);

  int mouse_px;
  int mouse_py;

  if(get_mouse_held(MOUSE_BUTTON_LEFT))
  {
    // Component bars

    // extra char on each side of the bar past the actual bounds to
    // make setting to the minimum and maximum values easier.

    get_mouse_pixel_position(&mouse_px, &mouse_py);

    if(MOUSE_IN(current->x + 7, current->y + 1, 34, 3))
    {
      const struct color_mode_component *c;
      int component = mouse_y - current->y - 1;

      double value =
       CLAMP((mouse_x - current->x - 8) + ((mouse_px % 8) / 8.0), 0, 32);

      c = &(current_mode->components[component]);

      value = (value * (c->max_val - c->min_val) + 16) / 32.0 + c->min_val;

      current_mode->set_function(current_color, component, (int)value);

      // Snap the mouse to the center of the bar.
      warp_mouse_pixel_y(mouse_y * 14 + 7);
      return -1;
    }
  }
  return false;
}

/**
 * Start the color editor subcontext.
 */

static subcontext *create_color_editor(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 36;
  sub->border_y = 0;
  sub->border_x2 = 79;
  sub->border_y2 = 5;

  sub->x = sub->border_x + 2;
  sub->y = sub->border_y + 1;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw   = color_editor_draw;
  spec.key    = color_editor_key;
  spec.click  = color_editor_click;
  spec.drag   = color_editor_drag;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------

/**
 * Draw the buffer portion of the menu window.
 */
static void menu_buffer_draw(int x, int y, boolean show_indices)
{
  static unsigned int timer = 0;
  int mode = get_screen_mode();
  int i;

  draw_window_box(
    x, y, x + 15, y + 4,
    DI_GREY, DI_GREY_DARK, DI_GREY_CORNER, false, false
  );

  write_string("Buffer", x + 2, y + 1, DI_GREY_TEXT, WR_NONE);
  write_string("     \n     \n     ", x + 9, y + 1, 0x55, WR_NEWLINE);

  if(mode >= 2 && saved_subpalette_display)
  {
    unsigned int which = ((timer++) / 60) % 4;
    uint8_t fg_color;
    struct color_status *col = &saved_subpal[which];

    set_protected_rgb(5, col->r, col->g, col->b);

    fg_color =
     (get_color_luma(graphics.protected_pal_position + 5) < 128) ? 31 : 16;
    draw_char(which + 49, (0x50) | fg_color, x + 11, y + 1);
  }
  else
  {
    set_protected_rgb(5, saved_color.r, saved_color.g, saved_color.b);
    timer = 0;
  }

  if(show_indices && mode >= 2)
  {
    char c = (mode == 2) ? CHAR_SMZX_C2 : 0;
    int x2;
    int y2;

    write_string("1    3\n2    4", x + 2, y + 2, DI_GREY_CORNER, WR_NEWLINE);
    for(i = 0; i < 4; i++)
    {
      x2 = x + 3 + (i/2*2);
      y2 = y + 2 + (i%2);

      select_layer(UI_LAYER);
      erase_char(x2, y2);
      erase_char(x2 + 1, y2);

      select_layer(GAME_UI_LAYER);
      draw_char(c, saved_indices[i], x2, y2);
      draw_char(c, saved_indices[i], x2 + 1, y2);
    }
  }
  select_layer(UI_LAYER);
}

/**
 * Draw the 16 color editor menu window.
 */

static boolean menu_16_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *menu = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = menu->pal_ed;
  struct color_mode *current_mode = get_current_mode(menu->pal_ed);
  struct color_mode_component *c;
  int x;
  int y;
  int i;

  if(minimal_help)
    return false;

  draw_window_box(
    menu->border_x,  menu->border_y,
    menu->border_x2, menu->border_y2,
    DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, true, true
  );

  // Write menu
  write_string(
    "\x1d- Select color    Alt+D- Default pal.     PgUp- Prev. mode\n"
    " - Increase        Alt+ - Decrease         PgDn- Next mode\n"
    " - Increase        Alt+ - Decrease            \n"
    " - Increase        Alt+ - Decrease            Q- Quit editing\n"
    "\n"
    "0- Blacken color   Alt+H- Hide menu         Tab- Switch to\n"
    "F2- Store color    Alt+I- Import                 XXXX palette\n"
    "F3- Place color    Alt+X- Export\n"
    "\n"
    "Left click-       edit component/activate\n"
    "Right click+Drag- change color\n",
    menu->x,
    menu->y,
    DI_GREY_TEXT,
    WR_NEWLINE
  );

  write_string(palette_labels[!pal_ed->current], menu->x + 49, menu->y + 6,
   DI_GREY_TEXT, WR_NONE);

  // Component instructions
  y = menu->y + 1;
  for(i = 0; i < 3; i++, y++)
  {
    c = &(current_mode->components[i]);
    x = menu->x;

    write_string(c->name_key, x, y, DI_GREY_TEXT, WR_NONE);
    x += 12;

    write_string(c->name_long, x, y, DI_GREY_TEXT, WR_NONE);
    x += 11;

    write_string(c->name_key, x, y, DI_GREY_TEXT, WR_NONE);
    x += 12;

    write_string(c->name_long, x, y, DI_GREY_TEXT, WR_NONE);
  }

  // All
  if(current_mode->all_key)
  {
    write_string(
     "A- Increase All    Alt+A- Decrease All",
     menu->x,
     menu->y + 4,
     DI_GREY_TEXT,
     WR_NEWLINE
    );
  }

  // Buffer
  menu_buffer_draw(menu->border_x2 - 15, menu->border_y2 - 4, false);
  return true;
}

/**
 * Create the menu window for the 16 color editor.
 */

static subcontext *create_menu_16(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 7;
  sub->border_y = 7;
  sub->border_x2 = 73;
  sub->border_y2 = 21;

  sub->x = sub->border_x + 3;
  sub->y = sub->border_y + 2;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw = menu_16_draw;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------


/**
 * Draw the palette window for the 16 color editor.
 */

static boolean palette_16_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *pal = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = pal->pal_ed;
  int screen_mode = get_screen_mode();
  unsigned int bg_color;
  unsigned int fg_color;
  unsigned int chr;
  int i;
  int x;
  int y;

  draw_window_box(
    pal->border_x,  pal->border_y,
    pal->border_x2, pal->border_y2,
    DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, true, true
  );

  // Temporary palette warning
  if(pal_ed->current != GAME_PALETTE)
  {
    write_string("TEMPORARY -- WILL NOT BE SAVED", pal->border_x + 3,
     pal->border_y, 0x8E, WR_NONE);
  }

  // Draw palette bars
  for(bg_color = 0; bg_color < 16; bg_color++)
  {
    x = bg_color * 2 + pal->x;
    y = pal->y;

    // The foreground color is white or black in the protected palette.

    fg_color = (get_color_luma(bg_color) < 128) ? 31 : 16;

    // Draw the palette colors
    for(i = 0; i < 8; i++)
    {
      chr = ' ';

      // These look ugly in SMZX mode 1, so only draw them in regular mode.
      if(!screen_mode)
      {
        if((i == 5) && (bg_color >= 10))
          chr = '1';

        if(i == 6)
          chr = '0' + (bg_color % 10);
      }

      select_layer(UI_LAYER);
      erase_char(x + (i/4), y + (i%4));

      select_layer(GAME_UI_LAYER);
      draw_char_mixed_pal_ext(chr, bg_color, fg_color,
       x + (i/4), y + (i%4), PRO_CH);
    }

    select_layer(UI_LAYER);

    // Clear the bottom
    draw_char(CHAR_LINE_HORIZ, DI_GREY, x, y + 4);
    draw_char(CHAR_LINE_HORIZ, DI_GREY, x+1, y + 4);

    if(bg_color == current_id)
    {
      // Draw '^^'
      write_string("\x1e\x1e",
       x, y + 4, DI_GREY_TEXT, WR_NONE);
    }
  }
  return true;
}

/**
 * Key input for the 16 color palette window.
 */

static boolean palette_16_key(subcontext *ctx, int *key)
{
  switch(*key)
  {
    case IKEY_LEFT:
    case IKEY_MINUS:
    case IKEY_KP_MINUS:
    {
      if(current_id > 0)
        current_id--;
      return true;
    }

    case IKEY_RIGHT:
    case IKEY_EQUALS:
    case IKEY_KP_PLUS:
    {
      if(current_id < 15)
        current_id++;
      return true;
    }
  }
  return false;
}

/**
 * Translate mouse movement into a keypress and move the mouse
 * cursor back to its original position if it moved.
 */

static void mouse_slide_cursor(int *_key, int mouse_x, int mouse_y)
{
  int delta_x;
  int delta_y;
  int key = IKEY_UNKNOWN;

  get_mouse_movement(&delta_x, &delta_y);

  if(abs(delta_x) > abs(delta_y))
  {
    if(delta_x > 0)
      key = IKEY_RIGHT;

    if(delta_x < 0)
      key = IKEY_LEFT;
  }
  else
  {
    if(delta_y > 0)
      key = IKEY_DOWN;

    if(delta_y < 0)
      key = IKEY_UP;
  }

  if(key != IKEY_UNKNOWN)
  {
    // Only warp the mouse to its old spot if there's movement registered.
    // Otherwise, it will be unresponsive with precise mouse movements.
    warp_mouse(mouse_x - delta_x, mouse_y - delta_y);

    *_key = key;
  }
}

/**
 * Mouse input for the 16 color palette window.
 */

static boolean palette_16_mouse(subcontext *ctx, int *key, int button,
 int mouse_x, int mouse_y)
{
  struct pal_ed_subcontext *pal = (struct pal_ed_subcontext *)ctx;

  if(button == MOUSE_BUTTON_LEFT &&
   MOUSE_IN(pal->x, pal->y, 32, 4))
  {
    current_id = (mouse_x - pal->x) / 2;
    return true;
  }
  else

  if(get_mouse_status() & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
  {
    mouse_slide_cursor(key, mouse_x, mouse_y);
    return true;
  }
  return false;
}

/**
 * Create the 16 color palette window.
 */

static subcontext *create_palette_16(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 0;
  sub->border_y = 0;
  sub->border_x2 = 35;
  sub->border_y2 = 5;

  sub->x = sub->border_x + 2;
  sub->y = sub->border_y + 1;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw   = palette_16_draw;
  spec.key    = palette_16_key;
  spec.click  = palette_16_mouse;
  spec.drag   = palette_16_mouse;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------


/**
 * Draw the 256 color palette editor menu window.
 */

static boolean menu_256_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *menu = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = menu->pal_ed;
  struct color_mode *current_mode = get_current_mode(menu->pal_ed);
  struct color_mode_component *c;
  int smzx_mode = get_screen_mode();
  int i;
  int x;
  int y;

  // Always enable help during smzx_mode 3; the preview area is useless.
  if(minimal_help && smzx_mode == 2)
    return false;

  draw_window_box(
    menu->border_x,  menu->border_y,
    menu->border_x2, menu->border_y2,
    DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, true, true
  );

  // Write menu
  write_string(
    "\x12\x1d- Select color   Alt+D- Default pal.   \n"
    " - Increase        Alt+ - Decrease       \n"
    " - Increase        Alt+ - Decrease       \n"
    " - Increase        Alt+ - Decrease       \n"
    "\n"
    "0- Blacken color\n"
    "F2- Store color\n"
    "F3- Place color\n"
    "Ins- Cursors (off)\n"
    "Tab- Switch to\n"
    "     XXXX palette\n"
    "Q- Quit editing\n"
    "\n"
    "Left click- activate\n"
    "Right click+Drag- color\n",
    menu->x,
    menu->y,
    DI_GREY_TEXT,
    WR_NEWLINE
  );

  write_string(palette_labels[!pal_ed->current], menu->x + 5, menu->y + 10,
   DI_GREY_TEXT, WR_NONE);

  if(subpalette_cursors)
    write_string("(on) ", menu->x + 13, menu->y + 8, DI_GREY_TEXT, WR_NONE);

  if(smzx_mode == 2)
  {
    // SMZX mode 2 specific help
    write_string(
      "Alt+H- Hide menu\n"
      "   F5- Store colors\n"
      "   F6- Place colors\n"
      " PgUp- Prev. mode\n"
      " PgDn- Next mode\n"
      "Alt+I- Import\n"
      "Alt+X- Export\n",
      menu->x + 19,
      menu->y + 5,
      DI_GREY_TEXT,
      WR_NEWLINE
    );
  }
  else
  {
    // SMZX mode 3 specific help
    write_string(
      "Space- Subpalette\n"
      "   F5- Store colors\n"
      "   F6- Place colors\n"
      "   F7- Store indices\n"
      "   F8- Place indices\n"
      "  1-4- Current index\n"
      "       to subpalette\n",
      menu->x + 19,
      menu->y + 5,
      DI_GREY_TEXT,
      WR_NEWLINE
    );
    write_string(
      "Middle/wheel- subpalette\n",
      menu->x,
      menu->y + 15,
      DI_GREY_TEXT,
      WR_NEWLINE
    );
  }

  // Component instructions
  y = menu->y + 1;
  for(i = 0; i < 3; i++, y++)
  {
    c = &(current_mode->components[i]);
    x = menu->x;

    write_string(c->name_key, x, y, DI_GREY_TEXT, WR_NONE);
    x += 12;

    write_string(c->name_long, x, y, DI_GREY_TEXT, WR_NONE);
    x += 11;

    write_string(c->name_key, x, y, DI_GREY_TEXT, WR_NONE);
    x += 12;

    write_string(c->name_long, x, y, DI_GREY_TEXT, WR_NONE);
  }

  // All
  if(current_mode->all_key)
  {
    write_string(
      "A- Increase All    Alt+A- Decrease All",
      menu->x,
      menu->y + 4,
      DI_GREY_TEXT,
      WR_NEWLINE
    );
  }

  // Buffer
  menu_buffer_draw(menu->border_x2 - 15, menu->border_y2 - 4, smzx_mode==3);
  return true;
}

/**
 * Create the 256 color editor menu window.
 */

static subcontext *create_menu_256(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 36;
  sub->border_y = 7;
  sub->border_x2 = 79;
  sub->border_y2 = 24;

  sub->x = sub->border_x + 2;
  sub->y = sub->border_y + 1;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw = menu_256_draw;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------


static void draw_unbound_cursor_chars(uint32_t layer, int color)
{
  select_layer(layer);
  set_layer_mode(layer, 0);
  // Note: mode 0, so offset by 16 (PAL_SIZE) for the protected palette.
  draw_char_to_layer(CHAR_CORNER_TL,  color, 0, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(CHAR_CORNER_TR,  color, 2, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(CHAR_CORNER_BL,  color, 0, 1, PRO_CH, PAL_SIZE);
  draw_char_to_layer(CHAR_CORNER_BR,  color, 2, 1, PRO_CH, PAL_SIZE);
  draw_char_to_layer(CHAR_LINE_HORIZ, color, 1, 0, PRO_CH, PAL_SIZE);
  draw_char_to_layer(CHAR_LINE_HORIZ, color, 1, 1, PRO_CH, PAL_SIZE);
}

static void draw_unbound_cursor(int x, int y, int fg, int bg, int offset)
{
  // Cursor for mode 2/3 palette selector
  uint32_t layer;
  int order;

  x = x * CHAR_W - 3;
  y = y * CHAR_H - 5;
  fg += 0xD0;
  bg += 0xD0;

  // Shadow
  order = LAYER_DRAWORDER_UI + offset*2 + 1;
  layer = create_layer(x, y, 3, 2, order, 0x10D, 0, 1);
  draw_unbound_cursor_chars(layer, bg);

  // FG
  x -= 2;
  y--;
  order++;
  layer = create_layer(x, y, 3, 2, order, 0x10D, 0, 1);
  draw_unbound_cursor_chars(layer, fg);
}

static void destroy_unbound_cursors(struct pal_ed_context *pal_ed)
{
  // TODO: this destroys everything, which might not be okay eventually.
  destruct_extra_layers(0);
}

/**
 * Draw the 256 color palette window.
 */

static boolean palette_256_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *pal = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = pal->pal_ed;
  int blink_cursor = graphics.cursor_flipflop;
  char chr;
  int col;
  int x;
  int y;
  int subcursor[4] = { -1, -1, -1, -1 };
  int ignore;

  int lo = current_id & 0x0F;
  int hi = (current_id & 0xF0) >> 4;

  draw_window_box(
    pal->border_x,  pal->border_y,
    pal->border_x2, pal->border_y2,
    DI_GREY, DI_GREY_DARK, DI_GREY_CORNER, true, true
  );

  // Temporary palette warning
  if(pal_ed->current != GAME_PALETTE)
  {
    write_string("TEMPORARY -- WILL NOT BE SAVED", pal->border_x + 3,
     pal->border_y, 0x8E, WR_NONE);
  }

  // Erase the spot where the palette will go.
  for(y = 0; y < 16; y++)
    for(x = 0; x < 32; x++)
      erase_char(x + pal->x, y + pal->y);

  // Destroy the cursors if they already exist.
  destroy_unbound_cursors(pal->pal_ed);

  select_layer(GAME_UI_LAYER);

  if(get_screen_mode() == 2)
  {
    // Draw the palette
    for(y = 0; y < 16; y++)
    {
      for(x = 0; x < 16; x++)
      {
        col = y << 4 | x;
        chr = CHAR_SMZX_C2;
        draw_char_ext(chr, col, (x*2 + pal->x), (y + pal->y), PRO_CH, 0);
        draw_char_ext(chr, col, (x*2+1 + pal->x), (y + pal->y), PRO_CH, 0);
      }
    }

    get_current_indices(pal_ed,
     &subcursor[0], &subcursor[1], &ignore, &subcursor[2]);
  }

  // Mode 3
  else
  {
    // Draw the palette
    for(y = 0; y < 16; y++)
    {
      for(x = 0; x < 16; x++)
      {
        col = y << 4 | x;
        draw_char_ext(0, col, (x*2 + pal->x), (y + pal->y), PRO_CH, 0);
        draw_char_ext(0, col, (x*2+1 + pal->x), (y + pal->y), PRO_CH, 0);
      }
    }

    if(pal_ed->current_indices)
    {
      get_current_indices(pal_ed,
       &subcursor[0], &subcursor[1], &subcursor[2], &subcursor[3]);
    }
  }

  // Subpalette cursors
  if(subpalette_cursors)
  {
    unsigned char cursor_fg = blink_cursor ? 0xC : 0x4;
    unsigned char cursor_bg = blink_cursor ? 0x4 : 0x0;
    int i;

    for(i = 0; i < 4; i++)
    {
      if(subcursor[i] >= 0)
      {
        draw_unbound_cursor(
          (subcursor[i] & 15) * 2 + pal->x,
          (subcursor[i] >> 4) + pal->y,
          cursor_fg, cursor_bg, 0
        );
      }
    }
  }

  // Main cursor
  if(blink_cursor)
  {
    draw_unbound_cursor(lo * 2 + pal->x, hi + pal->y, 0xF, 0x8, 4);
  }
  else
  {
    draw_unbound_cursor(lo * 2 + pal->x, hi + pal->y, 0x7, 0x0, 4);
  }

  select_layer(UI_LAYER);
  return true;
}

/**
 * Key input for the 256 color palette window.
 */

static boolean palette_256_key(subcontext *ctx, int *key)
{
  switch(*key)
  {
    case IKEY_INSERT:
    {
      subpalette_cursors = !subpalette_cursors;
      break;
    }

    case IKEY_UP:
    {
      if(current_id / 16 > 0)
        current_id -= 16;
      return true;
    }

    case IKEY_DOWN:
    {
      if(current_id / 16 < 15)
        current_id += 16;
      return true;
    }

    case IKEY_LEFT:
    {
      if(current_id % 16 > 0)
        current_id--;
      return true;
    }

    case IKEY_RIGHT:
    {
      if(current_id % 16 < 15)
        current_id++;
      return true;
    }
  }
  return false;
}

/**
 * Mouse input for the 256 color palette window (click + drag).
 */

static boolean palette_256_mouse(subcontext *ctx, int *key, int button,
 int mouse_x, int mouse_y)
{
  struct pal_ed_subcontext *pal = (struct pal_ed_subcontext *)ctx;

  if(get_mouse_status() & MOUSE_BUTTON(MOUSE_BUTTON_RIGHT))
  {
    mouse_slide_cursor(key, mouse_x, mouse_y);
    return true;
  }

  if(button == MOUSE_BUTTON_LEFT &&
   MOUSE_IN(pal->x, pal->y, 32, 16))
  {
    // Palette- select color
    current_id = (mouse_x - pal->x)/2 + (mouse_y - pal->y) * 16;
    return true;
  }

  return false;
}

/**
 * Clean up the cursor layers on exit.
 */

static void palette_256_destroy(subcontext *ctx)
{
  struct pal_ed_subcontext *pal = (struct pal_ed_subcontext *)ctx;
  destroy_unbound_cursors(pal->pal_ed);
}

/**
 * Create the 256 color palette window.
 */

static subcontext *create_palette_256(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 0;
  sub->border_y = 7;
  sub->border_x2 = 35;
  sub->border_y2 = 24;

  sub->x = sub->border_x + 2;
  sub->y = sub->border_y + 1;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw     = palette_256_draw;
  spec.key      = palette_256_key;
  spec.click    = palette_256_mouse;
  spec.drag     = palette_256_mouse;
  spec.destroy  = palette_256_destroy;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------


/**
 * Draw the 256 color subpalette window.
 */

static boolean subpalette_256_draw(subcontext *ctx)
{
  struct pal_ed_subcontext *spal = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = spal->pal_ed;
  int subpalette_num;
  int c0 = 0;
  int c1 = 0;
  int c2 = 0;
  int c3 = 0;
  int x;
  int y;

  draw_window_box(
    spal->border_x,  spal->border_y,
    spal->border_x2, spal->border_y2,
    DI_GREY_DARK, DI_GREY, DI_GREY_CORNER, true, true
  );

  // Fill the char missed by the shadow (it looks bad, but also shows the
  // board which probably had its indices overwritten in mode 3!)
  draw_char(0, 0, spal->border_x, spal->border_y2 + 1);

  // Palette View contents
  write_string(
   "Subpal.\n #\n\nHex:",
   spal->x,
   spal->y,
   DI_GREY_TEXT,
   WR_NEWLINE
  );

  write_string(
   "1:    2:    3:    4:",
   spal->x + 9,
   spal->y,
   DI_GREY_CORNER,
   WR_NONE
  );

  // Erase spots where palette colors will go
  for(y = 0; y < 3; y++)
  {
    for(x = 0; x < 5; x++)
    {
      erase_char(spal->x + x + 9, spal->y + y + 1);
      erase_char(spal->x + x + 15, spal->y + y + 1);
      erase_char(spal->x + x + 21, spal->y + y + 1);
      erase_char(spal->x + x + 27, spal->y + y + 1);
    }
  }

  select_layer(GAME_UI_LAYER);

  if(get_screen_mode() == 2)
  {
    subpalette_num = current_id;
    get_current_indices(pal_ed, &c0, &c1, &c2, &c3);

    // Draw the current subpalette
    for(y = 0; y < 3; y++)
    {
      for(x = 0; x < 5; x++)
      {
        int x2 = spal->x + x;
        int y2 = spal->y + y + 1;
        draw_char_ext(CHAR_SMZX_C0, current_id, (x2 + 9), y2, PRO_CH, 0);
        draw_char_ext(CHAR_SMZX_C1, current_id, (x2 + 15), y2, PRO_CH, 0);
        draw_char_ext(CHAR_SMZX_C2, current_id, (x2 + 21), y2, PRO_CH, 0);
        draw_char_ext(CHAR_SMZX_C3, current_id, (x2 + 27), y2, PRO_CH, 0);
      }
    }
  }
  else
  {
    // Note indices 1 and 2 are swapped to make sense.
    subpalette_num = current_subpalette;
    if(pal_ed->current_indices)
      get_current_indices(pal_ed, &c0, &c1, &c2, &c3);

    // Draw the current subpalette
    for(y = 0; y < 3; y++)
    {
      for(x = 0; x < 5; x++)
      {
        draw_char_ext(0, c0, (spal->x + x + 9), (spal->y + y + 1), PRO_CH, 0);
        draw_char_ext(0, c1, (spal->x + x + 15), (spal->y + y + 1), PRO_CH, 0);
        draw_char_ext(0, c2, (spal->x + x + 21), (spal->y + y + 1), PRO_CH, 0);
        draw_char_ext(0, c3, (spal->x + x + 27), (spal->y + y + 1), PRO_CH, 0);
      }
    }
  }

  select_layer(UI_LAYER);

  // Current subpalette number
  write_number(subpalette_num, DI_GREY_TEXT, spal->x + 3, spal->y + 1, 3, false, 10);
  write_number(subpalette_num, DI_GREY_TEXT, spal->x + 5, spal->y + 3, 2, false, 16);

  // Subpalette indices
  write_number(c0, DI_GREY_TEXT, spal->x + 11, spal->y, 3, false, 10);
  write_number(c1, DI_GREY_TEXT, spal->x + 17, spal->y, 3, false, 10);
  write_number(c2, DI_GREY_TEXT, spal->x + 23, spal->y, 3, false, 10);
  write_number(c3, DI_GREY_TEXT, spal->x + 29, spal->y, 3, false, 10);
  return true;
}

/**
 * Key input for the 256 color subpalette window.
 */

static boolean subpalette_256_key(subcontext *ctx, int *key)
{
  struct pal_ed_subcontext *spal = (struct pal_ed_subcontext *)ctx;
  struct pal_ed_context *pal_ed = spal->pal_ed;
  struct color_status *palette = pal_ed->internal_palette;
  int smzx_mode = get_screen_mode();

  switch(*key)
  {
    case IKEY_SPACE:
    {
      if(smzx_mode == 3)
      {
        // Mode 3- select subpalette
        int new_subpal;

        // Make sure there aren't layers displaying over the UI.
        destroy_unbound_cursors(spal->pal_ed);

        // Load the current active indices
        load_backup_indices(pal_ed->current_indices);

        new_subpal = color_selection(current_subpalette, false);

        // Reset indices for editor
        set_screen_mode(3);

        if(new_subpal >= 0)
          current_subpalette = new_subpal;

        return true;
      }
      break;
    }

    case IKEY_MINUS:
    case IKEY_KP_MINUS:
    {
      if(smzx_mode == 3)
      {
        if(current_subpalette > 0)
          current_subpalette--;
      }
      else
      {
        if(current_id > 0)
          current_id--;
      }
      return true;
    }

    case IKEY_EQUALS:
    case IKEY_KP_PLUS:
    {
      if(smzx_mode == 3)
      {
        if(current_subpalette < 255)
          current_subpalette++;
      }
      else
      {
        if(current_id < 255)
          current_id++;
      }
      return true;
    }

    case IKEY_1:
    case IKEY_2:
    case IKEY_3:
    case IKEY_4:
    {
      if(smzx_mode == 3 && pal_ed->current_indices)
      {
        // Assign current color to subpalette index
        int index = (*key - IKEY_1);

        // Indices 1 and 2 are swapped to make actual sense.
        if(index == 1)      index = 2;
        else if(index == 2) index = 1;

        index += current_subpalette * 4;

        pal_ed->current_indices[index] = current_id;
      }
      return true;
    }

    case IKEY_F5:
    {
      // Store subpalette colors
      int c0, c1, c2, c3;
      if(get_current_indices(pal_ed, &c0, &c1, &c2, &c3))
      {
        memcpy(&saved_subpal[0], &palette[c0], sizeof(struct color_status));
        memcpy(&saved_subpal[1], &palette[c1], sizeof(struct color_status));
        memcpy(&saved_subpal[2], &palette[c2], sizeof(struct color_status));
        memcpy(&saved_subpal[3], &palette[c3], sizeof(struct color_status));
        saved_subpalette_active = true;
        saved_subpalette_display = true;
        return true;
      }
      break;
    }

    case IKEY_F6:
    {
      // Place subpalette colors
      int c0, c1, c2, c3;
      if(saved_subpalette_active &&
       get_current_indices(pal_ed, &c0, &c1, &c2, &c3))
      {
        memcpy(&palette[c0], &saved_subpal[0], sizeof(struct color_status));
        memcpy(&palette[c1], &saved_subpal[1], sizeof(struct color_status));
        memcpy(&palette[c2], &saved_subpal[2], sizeof(struct color_status));
        memcpy(&palette[c3], &saved_subpal[3], sizeof(struct color_status));
        store_color_pos(&palette[c0], c0);
        store_color_pos(&palette[c1], c1);
        store_color_pos(&palette[c2], c2);
        store_color_pos(&palette[c3], c3);
        saved_subpalette_display = true;
        return true;
      }
      break;
    }

    case IKEY_F7:
    {
      if(smzx_mode == 3 && pal_ed->current_indices)
      {
        // Store subpalette indices
        get_current_indices(pal_ed,
         &saved_indices[0], &saved_indices[1],
         &saved_indices[2], &saved_indices[3]);
      }
      break;
    }

    case IKEY_F8:
    {
      if(smzx_mode == 3 && pal_ed->current_indices)
      {
        // Place subpalette indices
        // NOTE: "get_current_indices" swaps 1 and 2 to make more sense to the
        // user, so swap them here too.
        int index = current_subpalette * 4;
        pal_ed->current_indices[index + 0] = saved_indices[0];
        pal_ed->current_indices[index + 2] = saved_indices[1];
        pal_ed->current_indices[index + 1] = saved_indices[2];
        pal_ed->current_indices[index + 3] = saved_indices[3];
      }
      break;
    }
  }
  return false;
}

/**
 * Mouse input for the 256 color subpalette window (click only).
 */

static boolean subpalette_256_click(subcontext *ctx, int *key, int button,
 int mouse_x, int mouse_y)
{
  struct pal_ed_subcontext *spal = (struct pal_ed_subcontext *)ctx;
  int smzx_mode = get_screen_mode();

  if(button == MOUSE_BUTTON_WHEELUP)
  {
    // Previous palette
    *key = IKEY_MINUS;
    return true;
  }

  if(button == MOUSE_BUTTON_WHEELDOWN)
  {
    // Next palette
    *key = IKEY_EQUALS;
    return true;
  }

  if(button == MOUSE_BUTTON_MIDDLE)
  {
    // Palette selector
    *key = IKEY_SPACE;
    return true;
  }

  if(smzx_mode == 3)
  {
    if(button == MOUSE_BUTTON_LEFT &&
     MOUSE_IN(spal->x - 1, spal->y, 9, 4))
    {
      // Select subpalette
      *key = IKEY_SPACE;
      return true;
    }

    if(button == MOUSE_BUTTON_LEFT &&
     MOUSE_IN(spal->x + 9, spal->y, 24, 4) &&
     (mouse_x - spal->x - 8)%6 != 0)
    {
      // Assign current color to a subpalette index
      // Fake a press of the 1-4 keys
      *key = IKEY_1 + (mouse_x - spal->x - 8)/6;
      return true;
    }
  }

  return false;
}

/**
 * Create the 256 color subpalette window.
 */

static subcontext *create_subpalette_256(struct pal_ed_context *pal_ed)
{
  struct pal_ed_subcontext *sub = cmalloc(sizeof(struct pal_ed_subcontext));
  struct context_spec spec;

  sub->pal_ed = pal_ed;
  sub->border_x = 0;
  sub->border_y = 0;
  sub->border_x2 = 35;
  sub->border_y2 = 5;

  sub->x = sub->border_x + 2;
  sub->y = sub->border_y + 1;

  memset(&spec, 0, sizeof(struct context_spec));
  spec.draw   = subpalette_256_draw;
  spec.key    = subpalette_256_key;
  spec.click  = subpalette_256_click;

  create_subcontext((subcontext *)sub, (context *)pal_ed, &spec);
  return (subcontext *)sub;
}


// -----------------------------------------------------------------------------


/**
 * Calculate color mode information for every color in the current palette.
 */
static struct color_status *rebuild_palette(struct color_status *palette)
{
  int palette_size = (get_screen_mode() >= 2) ? 256 : 16;
  int i;

  for(i = 0; i < palette_size; i++)
  {
    load_color(&(palette[i]), i);
  }

  current_id %= palette_size;

  return palette;
}

/**
 * Swap the current active palette.
 */
static void swap_palettes(struct pal_ed_context *pal_ed)
{
  if(pal_ed->current == GAME_PALETTE)
  {
    // Save the game palette, load the temp palette.
    store_backup_palette(pal_ed->game_palette_backup);
    load_backup_palette(temp_palette);

    pal_ed->current_indices = temp_indices;
    pal_ed->current = TEMP_PALETTE;
  }
  else
  {
    // Save the temp palette, load the game palette.
    store_backup_palette(temp_palette);
    load_backup_palette(pal_ed->game_palette_backup);

    pal_ed->current_indices = pal_ed->game_indices;
    pal_ed->current = GAME_PALETTE;
  }

  rebuild_palette(pal_ed->internal_palette);
}

/**
 * Reset the palettes to their state before the palette editor started.
 */
static void reset_pal_ed_palettes(struct pal_ed_context *pal_ed)
{
  if(pal_ed->current != GAME_PALETTE)
  {
    // Reload the game palette.
    swap_palettes(pal_ed);
  }

  if(get_screen_mode() == 3)
  {
    // Make the game indices active.
    load_backup_indices(pal_ed->game_indices);
  }

  // Fix protected color 5
  default_protected_palette();
}

/**
 * Initialize the palette editor's backup palettes (and indices).
 */
static void init_pal_ed_palettes(struct pal_ed_context *pal_ed)
{
  int screen_mode = get_screen_mode();
  int i;
  int j;

  pal_ed->current = GAME_PALETTE;
  pal_ed->current_indices = pal_ed->game_indices;

  // Rebuild the internal palette off the currently loaded (game) palette.
  rebuild_palette(pal_ed->internal_palette);

  if(screen_mode == 3)
  {
    // Special: mode 3 editing doesn't work while the game indices are actually
    // loaded, so they need to be backed up and edited from the backup.
    store_backup_indices(pal_ed->game_indices);

    // Reset the indices to default for display purposes.
    set_screen_mode(3);
  }

  if(screen_mode != temp_palette_mode)
  {
    // Initialize the temp palette.
    temp_palette_mode = screen_mode;
    memset(temp_palette, 0, sizeof(temp_palette));

    if(screen_mode < 2)
    {
      // Copy in the protected palette...
      for(i = 0, j = 0; i < PAL_SIZE; i++)
      {
        temp_palette[j++] = graphics.protected_palette[i].r * 63 / 255;
        temp_palette[j++] = graphics.protected_palette[i].g * 63 / 255;
        temp_palette[j++] = graphics.protected_palette[i].b * 63 / 255;
      }
    }
    else
    {
      // Load the external SMZX palette.
      vfile *vf = vfopen_unsafe(mzx_res_get_by_id(SMZX_PAL), "rb");
      if(vf)
      {
        if(!vfread(temp_palette, sizeof(temp_palette), 1, vf))
          memset(temp_palette, 0, sizeof(temp_palette));

        vfclose(vf);
      }
    }

    if(screen_mode == 3)
    {
      // The active indices will be the default mode 3 indices since the
      // mode changed above.
      store_backup_indices(temp_indices);
    }
  }
}

/**
 * Basic joystick handler for the palette editor.
 * This one intentionally does not do much because there's no way to get here
 * without a keyboard right now.
 */
static boolean pal_ed_joystick(context *ctx, int *key, int action)
{
  enum keycode ui_key = get_joystick_ui_key();
  if(ui_key)
  {
    *key = ui_key;
    return true;
  }
  return false;
}

/**
 * Basic key handler for the palette editor.
 */
static boolean pal_ed_key(context *ctx, int *key)
{
  struct pal_ed_context *pal_ed = (struct pal_ed_context *)ctx;
  int screen_mode = get_screen_mode();

  // Exit event -- mimic Escape
  if(get_exit_status())
    *key = IKEY_ESCAPE;

  switch(*key)
  {
    case IKEY_ESCAPE:
    case IKEY_q:
    {
      destroy_context(ctx);
      return true;
    }

    case IKEY_F1:
    case IKEY_F2:
    {
      // Defer to the global handler for the help/settings interfaces.
      // There might be layers displaying over the UI, which we don't want.
      destroy_unbound_cursors(pal_ed);
      return false;
    }

    case IKEY_h:
    {
      if(get_alt_status(keycode_internal))
      {
        // SMZX mode 3 always has the help enabled, so don't allow toggling
        if(get_screen_mode() != 3)
        {
          restore_screen();
          save_screen();
          minimal_help ^= 1;
        }

        return true;
      }
      break;
    }

    case IKEY_d:
    {
      if(get_alt_status(keycode_internal))
      {
        // SMZX modes 0 and 1 use the default palette
        if(screen_mode <= 1)
        {
          default_palette();
        }
        // SMZX modes 2 and 3 need the externally stored SMZX palette
        else
        {
          load_palette(mzx_res_get_by_id(SMZX_PAL));
        }

        rebuild_palette(pal_ed->internal_palette);
        return true;
      }
      break;
    }

    case IKEY_TAB:
    {
      swap_palettes(pal_ed);
      return true;
    }

    case IKEY_i:
    {
      if(get_alt_status(keycode_internal))
      {
        // Make sure there aren't layers displaying over the UI.
        destroy_unbound_cursors(pal_ed);

        if(screen_mode == 3)
        {
          // Set these to active in case the user doesn't load them.
          // Then, they'll just copy over themselves when stored again.
          load_backup_indices(pal_ed->current_indices);

          import_palette(ctx);

          // Save the indices and reset them for display.
          store_backup_indices(pal_ed->current_indices);
          set_screen_mode(3);
        }
        else
          import_palette(ctx);

        rebuild_palette(pal_ed->internal_palette);
        return true;
      }
      break;
    }

    case IKEY_x:
    {
      if(get_alt_status(keycode_internal))
      {
        // Make sure there aren't layers displaying over the UI.
        destroy_unbound_cursors(pal_ed);

        if(screen_mode == 3)
        {
          // Temporarily make these the active indices while exporting...
          load_backup_indices(pal_ed->current_indices);

          export_palette(ctx);

          // Reset the indices for display.
          set_screen_mode(3);
        }
        else
          export_palette(ctx);

        return true;
      }
      break;
    }
  }

  return false;
}

/**
 * Clean up palette editor info and restore the screen on exit.
 */
static void pal_ed_destroy(context *ctx)
{
  reset_pal_ed_palettes((struct pal_ed_context *)ctx);
  restore_screen();
}

/**
 * Create and run the palette editor.
 */
void palette_editor(context *parent)
{
  struct pal_ed_context *pal_ed = ccalloc(1, sizeof(struct pal_ed_context));
  int smzx_mode = get_screen_mode();
  struct context_spec spec;

  init_pal_ed_palettes(pal_ed);

  memset(&spec, 0, sizeof(struct context_spec));
  spec.joystick = pal_ed_joystick;
  spec.key      = pal_ed_key;
  spec.destroy  = pal_ed_destroy;

  create_context((context *)pal_ed, parent, &spec, CTX_PALETTE_EDITOR);

  if(smzx_mode >= 2)
  {
    create_subpalette_256(pal_ed);
    create_palette_256(pal_ed);
    create_color_editor(pal_ed);
    create_menu_256(pal_ed);
  }
  else
  {
    create_palette_16(pal_ed);
    create_color_editor(pal_ed);
    create_menu_16(pal_ed);
  }

  if(startup == false)
  {
    minimal_help = get_editor_config()->palette_editor_hide_help;
    startup = true;
  }

  m_show();
  cursor_off();
  save_screen();
}

/**
 * Prompt the user to import a .PAL (and/or .PALIDX) file.
 * NOTE: This is used by both the main editor and the palette editor.
 */
void import_palette(context *ctx)
{
  char filename_buffer[MAX_PATH];

  // Palette
  strcpy(filename_buffer, pal_filename);
  if(!choose_file(ctx->world, pal_ext, filename_buffer,
   "Choose palette to import", ALLOW_ALL_DIRS))
  {
    strcpy(pal_filename, filename_buffer);
    load_palette(filename_buffer);
  }

  // Indices (mode 3 only)
  strcpy(filename_buffer, palidx_filename);
  if((get_screen_mode() == 3) &&
   !choose_file(ctx->world, idx_ext, filename_buffer,
    "Choose indices to import (.PALIDX)", ALLOW_ALL_DIRS))
  {
    strcpy(palidx_filename, filename_buffer);
    load_index_file(filename_buffer);
  }
}

/**
 * Prompt the user to export a .PAL (and/or .PALIDX) file.
 * NOTE: This is used by both the main editor and the palette editor.
 */
void export_palette(context *ctx)
{
  char filename_buffer[MAX_PATH];

  // Palette
  strcpy(filename_buffer, pal_filename);
  if(!new_file(ctx->world, pal_ext, ".pal", filename_buffer,
   "Export palette", ALLOW_ALL_DIRS))
  {
    strcpy(pal_filename, filename_buffer);
    save_palette(filename_buffer);
  }

  // Indices (mode 3 only)
  strcpy(filename_buffer, palidx_filename);
  if((get_screen_mode() == 3) &&
   !new_file(ctx->world, idx_ext, ".palidx", filename_buffer,
    "Export indices (.PALIDX)", ALLOW_ALL_DIRS))
  {
    strcpy(palidx_filename, filename_buffer);
    save_index_file(filename_buffer);
  }
}

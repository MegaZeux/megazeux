/* MegaZeux
 *
 * Copyright (C) 2007-2009 Kevin Vance <kvance@kvance.com>
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

// LIMITATIONS:
// - No SMZX mode support.
// - No extended charset support. In theory, 512 characters could be
//   supported, after fixing the issue below.
// - The "1-to-1" renderer assumes that non-protected characters always use
//   the non-protected palette, and vice versa. This can be fixed by adding
//   a separate background layer for protected characters with its own
//   tileset, but it will also be a mild performance detriment.
// - Runtime changes to the protected palette will only display on the
//   "1-to-1" screen, not the "scaled" screen.

/* NOMENCLATURE NOTICE:
 * What we call the "subscreen" is called the "main screen" by libnds.
 * But in terms of functionality, it only contains auxiliary screen modes.
 */

#include <string.h>

#include "renderer_nds.h"
#include "../../event/nds/event_nds.h"
#include "../../platform/nds/platform_nds.h"

#include "../render.h"
#include "../renderers.h"

#include <nds/arm9/keyboard.h>

/* FIXME: */
#include "../../../arch/nds/protected_palette_bin.h"

#define SCALED_USE_CELL_CACHE /* 5% penalty on full redraws, up to 20x speed increase on no redraws */

// These variables control the panning along the 1:1 "main" screen.
DTCM_BSS
static int cell_pan_x = 0;
DTCM_BSS
static int cell_pan_y = 0;
DTCM_BSS
static int mainscr_x_33rd = 0;
DTCM_BSS
static int mainscr_y_max = 0;

// When set to >= 0, the upper screen will attempt to focus to this position
// the next  time it is drawn and then reset them to -1.
static int last_focus_x = -1;
static int last_focus_y = -1;
static int focus_x = -1;
static int focus_y = -1;

// This table stores scroll register values for each scanline.
static u16 scroll_table[192];

// This table maps palette color combinations to their index in BG_PALETTE.
static u8 palette_idx_table[256];

#define PALETTE_NORMAL_START (16)
#define PALETTE_PROTECTED_START (PALETTE_NORMAL_START + 16 + 120)
#define palette_protected_idx_table protected_palette_bin

// Y offset information for subscreen
static s16 subscreen_offset_y = 0;
u16 subscreen_height;

// The current transition state.
static struct {
  enum { TRANSITION_NONE = 0, TRANSITION_IN, TRANSITION_OUT } state;
  int time;
} transition;

// The subscreen can display different information.
enum Subscreen_Mode last_subscreen_mode;
enum Subscreen_Mode subscreen_mode;

// Initialized with a pointer to static libnds memory via keyboardInit.
static Keyboard *kb;

// Forward declarations
static void nds_keyboard_scroll_in(void);
static void nds_keyboard_scroll_out(void);

// Do this every vblank irq:
static void nds_on_vblank(void)
{
  /* Handle sleep mode. */
  nds_sleep_check();

  /* Do all special video handling. */
  nds_video_rasterhack();
  nds_video_jitter();
  nds_video_do_transition();

#ifdef CONFIG_AUDIO
  /* Handle PC speaker audio. */
  nds_audio_vblank();
#endif
}

boolean is_scaled_mode(enum Subscreen_Mode mode)
{
  return (mode == SUBSCREEN_SCALED);
}

static void palette_idx_table_init(void)
{
  // Start the palette at 16, reserving colors 0-16 for the overlay.
  const int offset = PALETTE_NORMAL_START;
  int idx;

  // Store the normal palette sequentially.
  for(idx = 0; idx < 16; idx++)
    palette_idx_table[idx * 17] = idx + offset;

  // Store the blends after that.
  idx += offset;
  for(int a = 0; a < 16; a++)
  {
    for(int b = a+1; b < 16; b++)
    {
      palette_idx_table[(a << 4) | b] = idx;
      palette_idx_table[(b << 4) | a] = idx;
      idx++;
    }
  }
}

static void nds_subscreen_scaled_init(void)
{
  int xscale = (int)(320 * 256 / 256);
  int yscale = (int)(350 * 256 / subscreen_height);

  /* Use banks A and B for the MZX screen. */
  videoSetMode(MODE_5_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE |
               DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_BG);

  /* Use flickered alpha lerp to scale 320x350 to 256x192. */
  /* (Bump the BG up to base 5 (+0x14000) to make room for the keyboard and console.) */
  REG_BG2CNT = BG_BMP8_512x512 | BG_BMP_BASE(5) | BG_PRIORITY(1);
  REG_BG2PA  = xscale;
  REG_BG2PB  = 0;
  REG_BG2PC  = 0;
  REG_BG2PD  = yscale;
  REG_BG2X   = 0;
  REG_BG2Y   = subscreen_offset_y;

  REG_BG3CNT = BG_BMP8_512x512 | BG_BMP_BASE(5) | BG_PRIORITY(1);
  REG_BG3PA  = xscale;
  REG_BG3PB  = 0;
  REG_BG3PC  = 0;
  REG_BG3PD  = yscale;
  REG_BG3X   = 0;
  REG_BG3Y   = subscreen_offset_y;

  /* Enable BG2/BG3 blending. */
  REG_BLDCNT   = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
  REG_BLDALPHA = (8 << 8) | 8; /* 50% / 50% */

  /* Enable the console. */
  consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
  bgSetPriority(1, 1);

  update_palette();
  update_screen();
}

static void nds_subscreen_keyboard_init(void)
{
  // BG0: Keyboard 256x512 text, map following tiles (45056 bytes total)
  // Clear the keyboard area before drawing it.
  dmaFillWords(0, BG_MAP_RAM(20), 4096);
  kb = keyboardInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x512, 20, 0, true, true);
  bgSetPriority(0, 1);
  kb->scrollSpeed = 7;
  transition.time = 0;
  transition.state = TRANSITION_IN;
}

static void nds_subscreen_keyboard_exit(void)
{
  transition.time = 0;
  transition.state = TRANSITION_OUT;
}

static void nds_mainscreen_init(struct graphics_data *graphics)
{
  u16 *vram;
  int i;

  graphics->fullscreen = true;
  graphics->resolution_width = 640;
  graphics->resolution_height = 350;

  /* Use banks H and I for the text screens. */
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
  vramSetBankH(VRAM_H_SUB_BG);
  vramSetBankI(VRAM_I_SUB_BG_0x06208000);

  /* BG0: foreground characters. */
  REG_BG0CNT_SUB  = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(0) |
                    BG_TILE_BASE(1);
  REG_BG0HOFS_SUB = 0;
  REG_BG0VOFS_SUB = 0;

  /* BG1: background characters. */
  REG_BG1CNT_SUB  = BG_64x32 | BG_COLOR_16 | BG_MAP_BASE(2) |
                    BG_TILE_BASE(0);
  REG_BG1HOFS_SUB = 0;
  REG_BG1VOFS_SUB = 0;

  // By default, pan to the center of the screen.
  focus_x = 640/2;
  focus_y = 350/2;

  // Add a solid tile for background colors.
  vram = (u16 *)BG_TILE_RAM_SUB(0) + 256 * 16;
  for(i = 0; i < 32; i++)
    *(vram++) = (1 << 12 | 1 << 8 | 1 << 4 | 1) * (1+(i >> 4));
}

void nds_video_jitter(void)
{
  const u16 jitterF4[] = {
    0x060, 0x000,   /* 0.375, 0.000 */
    0x020, 0x100,   /* 0.125, 1.000 */
    0x0e0, 0x000,   /* 0.875, 0.000 */
    0x0a0, 0x100,   /* 0.625, 1.000 */
  };
  static size_t jidx = 0;    /* jitter table index */

  /* Jitter the backgrounds for scaled mode. */
//  if(jidx >= sizeof(jitterF4)/sizeof(*jitterF4))
//    jidx = 0;

  REG_BG2X = jitterF4[jidx];
  REG_BG2Y = jitterF4[jidx+1] + subscreen_offset_y;
  REG_BG3X = jitterF4[jidx+2];
  REG_BG3Y = jitterF4[jidx+3] + subscreen_offset_y;
  jidx ^= 4;
}

/* Use HBlank DMA to load row scroll values in for each line. */
void nds_video_rasterhack(void)
{
  /* Prepare DMA transfer. */
#ifdef CONFIG_NDS_BLOCKSDS
  dmaStopSafe(1);
  REG_BG0VOFS_SUB = scroll_table[0];
  dmaSetParams(1, (scroll_table + 1), (void *)&REG_BG0VOFS_SUB,
                    DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1);

  dmaStopSafe(2);
  REG_BG1VOFS_SUB = scroll_table[0];
  dmaSetParams(2, (scroll_table + 1), (void *)&REG_BG1VOFS_SUB,
                    DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1);
#else
  DMA1_CR         = 0;
  REG_BG0VOFS_SUB = scroll_table[0];
  DMA1_SRC        = (u32)(scroll_table + 1);
  DMA1_DEST       = (u32)&REG_BG0VOFS_SUB;
  DMA1_CR         = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1;

  DMA2_CR         = 0;
  REG_BG1VOFS_SUB = scroll_table[0];
  DMA2_SRC        = (u32)(scroll_table + 1);
  DMA2_DEST       = (u32)&REG_BG1VOFS_SUB;
  DMA2_CR         = DMA_DST_FIX | DMA_SRC_INC | DMA_REPEAT | DMA_16_BIT |
                    DMA_START_HBL | DMA_ENABLE | 1;
#endif
}

// Handle the transition animation.
void nds_video_do_transition(void)
{
  // On transitioning in, use the current screen.  Out, use the last screen.
  enum Subscreen_Mode mode;
  if(transition.state == TRANSITION_NONE)
    return;
  if(transition.state == TRANSITION_IN)
    mode = subscreen_mode;
  else                // TRANSITION_OUT
    mode = last_subscreen_mode;

  if(mode == SUBSCREEN_KEYBOARD)
  {
    if(transition.state == TRANSITION_IN)
      nds_keyboard_scroll_in();
    else
      nds_keyboard_scroll_out();
  }
}

static void nds_keyboard_scroll_in(void)
{
  if(!kb)
    return;
  if(transition.time == 0)
  {
    // Show the background.
    kb->visible = 1;
    bgSetScroll(kb->background, 0, -192);
    bgUpdate();
    bgShow(kb->background);
  }
  else
  {
    // Scroll 80 pixels.
    if(transition.time < 80)
    {
      bgSetScroll(kb->background, 0, -192 + transition.time);
    }
    else
    {
      bgSetScroll(kb->background, 0, kb->offset_y);
      transition.state = TRANSITION_NONE;
    }
    bgUpdate();
  }

  transition.time += kb->scrollSpeed;
}

static void nds_keyboard_scroll_out(void)
{
  if(!kb)
    return;
  if(transition.time == 0)
    kb->visible = 0;

  if(transition.time >= 80)
  {
    // Hide the background.
    bgHide(kb->background);
    transition.state = TRANSITION_NONE;
  }
  else
  {
    // Scroll 80 pixels.
    bgSetScroll(kb->background, 0, kb->offset_y - transition.time);
    bgUpdate();
  }

  transition.time += kb->scrollSpeed;
}

void nds_sleep_check(void)
{
  static boolean asleep = false;

  // Check if we were just woken up.
  if(asleep)
  {
    asleep = false;

    // Wait a bit until returning power.
    while(REG_VCOUNT != 0);
    while(REG_VCOUNT == 0);
    while(REG_VCOUNT != 0);
    powerOn(POWER_ALL);
  }

  // Check if it's time to go to sleep.
  if(keysDown() & KEY_LID)
  {
    // Cancel DMA from raster effects.
#ifdef CONFIG_NDS_BLOCKSDS
    dmaStopSafe(1);
    dmaStopSafe(2);
#else
    DMA1_CR = 0;
    DMA2_CR = 0;
#endif

    // Power everything off.
    powerOff(POWER_ALL);
    asleep = true;
  }
}

#ifdef SCALED_USE_CELL_CACHE
static u32 graph_cache[80*25];
static bool graph_cache_invalid = false;

static void nds_clear_graph_cache(void)
{
  graph_cache_invalid = true;
}
#else
#define nds_clear_graph_cache() {}
#endif

static boolean nds_init_video(struct graphics_data *graphics,
 struct config_info *config)
{
  u32 i;

  lcdMainOnBottom();
  nds_clear_graph_cache();

  // Build the palette blend mapping table.
  palette_idx_table_init();

  // Copy protected palette to BG_PALETTE.
  for(i = 0; i < protected_palette_bin_size - 256; i += 2)
    BG_PALETTE[PALETTE_PROTECTED_START + (i>>1)] =
     *((u16 *)&protected_palette_bin[i + 256]);

  // Start with scaled mode.
  switch(config->video_ratio)
  {
    case RATIO_CLASSIC_4_3:
    case RATIO_STRETCH:
      subscreen_height = 192;
      break;
    default:
      subscreen_height = 140;
      break;
  }
  subscreen_offset_y = -((192 - subscreen_height) * 320 /* 1/2 * 256 * (1/0.4) */);

  subscreen_mode = SUBSCREEN_SCALED;
  last_subscreen_mode = SUBSCREEN_MODE_INVALID;
  nds_subscreen_scaled_init();

  // Initialize the 1:1 scaled "main" screen.
  nds_mainscreen_init(graphics);

  transition.state = TRANSITION_NONE;
  transition.time = 0;

  // Now that we're initialized, install the vblank handler.
  irqSet(IRQ_VBLANK, nds_on_vblank);

  graphics->bits_per_pixel = 8;
  return true;
}

static boolean nds_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;	// stub
}

// Focus on a given screen position (in pixels up to 640x350).
static void nds_mainscreen_focus(int x, int y)
{
  static int old_x = -1;
  static int old_y = -1;
  int scroll_x, scroll_y, i, ypos, ycounter;
  u16 *sptr;

  if(x == old_x && y == old_y)
  {
    // Already focused there, quick abort.
    return;
  }
  old_x = x;
  old_y = y;

  // Clamp the coordinates so we stay within the screen.
  if(x < 132) x = 132;
  if(x > 508) x = 508;
  if(y <  96) y =  96;
  if(y > 253) y = 253;

  // Move to the top-left corner.
  x -= 264/2;
  y -= 192/2;

  // Find the relevant cell coordinates and scroll offsets into those cells.
  cell_pan_x = x / 8;
  scroll_x   = x % 8;
  cell_pan_y = y / 14;
  scroll_y   = y % 14;

  mainscr_x_33rd = scroll_x > 0 ? 1 : 0;
  mainscr_y_max = scroll_y >= 4 ? 15 : 14;

  // Adjust the X scroll registers now.
  REG_BG0HOFS_SUB = scroll_x;
  REG_BG1HOFS_SUB = scroll_x;

  // Recalculate the scroll table.
  sptr     = scroll_table;
  ypos     = scroll_y;
  ycounter = scroll_y;
  for(i = 0; i < 192; i++)
  {
    (*sptr++) = ypos;

    ycounter++;
    if(ycounter == 14)
    {
      ycounter = 0;
      ypos += 2;
    }
  }
}

// Render the scaled screen.
__attribute__((optimize("-O3")))
ITCM_CODE
static void nds_render_graph_scaled(struct graphics_data *graphics)
{
  int const WIDTH_IN_PIXELS  = 80 * 4;  // Screen width in pixels
  int const WIDTH_IN_CELLS   = 80;      // Screen width in chars
  int const CHARACTER_HEIGHT = 14;      // Character height in pixels
  int const VRAM_WIDTH       = 512;     // HW surface width in pixels

  struct char_element *text_cell = graphics->text_video;
  u32 *vram_ptr  = (u32 *)BG_BMP_RAM(5);
  int chars, lines, columns;
  int chr, bg, fg;
  u8 *line;
  u32 fourpx;

  DTCM_BSS static u8 pal_tbl[4];

#ifdef SCALED_USE_CELL_CACHE
  if(graph_cache_invalid)
  {
    dmaFillWords(0xFFFFFFFF, graph_cache, 80*25*4);
    graph_cache_invalid = false;
  }
#endif

  /* Iterate over every character in text memory. */
  columns = 0;
  for(chars = 0; chars < 80*25; chars++)
  {
    u32 *vram_next = vram_ptr + 1;
#ifdef SCALED_USE_CELL_CACHE
    u32 key = *((u32 *)text_cell);

    if(graph_cache[chars] != key)
    {
      graph_cache[chars] = key;
      // Avoid reading from RAM to cache twice.
      chr = key & 0x1FF;
      bg  = (key >> 16) & 0x0F;
      fg  = (key >> 24) & 0x0F;
#else
    {
      chr = (*text_cell).char_value & 0x1FF;
      bg  = (*text_cell).bg_color   & 0x0F;
      fg  = (*text_cell).fg_color   & 0x0F;
#endif

      // Construct a table mapping charset two-bit pairs to palette entries.
      if(chr & 0x100) // Protected palette
      {
        u8 bg_idx = bg + PALETTE_PROTECTED_START;
        u8 fg_idx = fg + PALETTE_PROTECTED_START;
        u8 mix_idx = palette_protected_idx_table[(bg << 4) | fg];
        pal_tbl[0] = bg_idx;
        pal_tbl[1] = mix_idx;
        pal_tbl[2] = mix_idx;
        pal_tbl[3] = fg_idx;
      }
      else // Regular palette
      {
        u8 bg_idx = bg + PALETTE_NORMAL_START;
        u8 fg_idx = fg + PALETTE_NORMAL_START;
        u8 mix_idx = palette_idx_table[(bg << 4) | fg];
        pal_tbl[0] = bg_idx;
        pal_tbl[1] = mix_idx;
        pal_tbl[2] = mix_idx;
        pal_tbl[3] = fg_idx;
      }

      /* Iterate over every line in the character. */
      line = graphics->charset + CHARACTER_HEIGHT * chr;
      #pragma GCC unroll CHARACTER_HEIGHT
      for(lines = 0; lines < CHARACTER_HEIGHT; lines++)
      {
        /* Plot four pixels using the two-bit pair table. */
        fourpx  = pal_tbl[((*line) & 0xC0) >> 6]      ;
        fourpx |= pal_tbl[((*line) & 0x30) >> 4] <<  8;
        fourpx |= pal_tbl[((*line) & 0x0C) >> 2] << 16;
        fourpx |= pal_tbl[((*line) & 0x03)     ] << 24;
        *((u32 *)vram_ptr) = fourpx;

        /* Advance to the next line. */
        line++;
        vram_ptr += VRAM_WIDTH/4;
      }
    }

    /* Advance to the next character. */
    text_cell++;
    vram_ptr = vram_next;

    /* Insert padding at the end of a line. */
    columns++;
    if(columns == WIDTH_IN_CELLS)
    {
      columns = 0;
      vram_ptr += (VRAM_WIDTH - WIDTH_IN_PIXELS)/4;
      vram_ptr += VRAM_WIDTH * 13 / 4;
    }
  }
}

__attribute__((optimize("-O3")))
static void nds_render_graph_1to1(struct graphics_data *graphics)
{
  struct char_element *text_start = graphics->text_video;
  struct char_element *text_cell;
  u16 *vram_bg, *vram_fg;
  int x, y;

  // Before drawing the screen, check if the focus needs to be updated!
  if(focus_x > -1 && focus_y > -1)
  {
    nds_mainscreen_focus(focus_x, focus_y);
    focus_x = -1;
    focus_y = -1;
  }

  text_start += cell_pan_x + 80*cell_pan_y;
  text_cell = text_start;

  /* Plot the foreground and background tiles. */
  vram_fg = (u16 *)BG_MAP_RAM_SUB(0);
  vram_bg = (u16 *)BG_MAP_RAM_SUB(2);

  for(y = 0; y < mainscr_y_max; y++)
  {
    // Draw the top halves of this line for tile_offset=0, then the bottom
    // halves for tile_offset=1.
    for(int tile_offset = 0; tile_offset < 2; tile_offset++)
    {
      int chr;
      int bg;
      int fg;

      for(x = 0; x < 32; x++)
      {
        // Extract the cell data.
        chr = (*text_cell).char_value & 0x1FF;
        // Colors are not explicitly "& 0x0F" - this is done by the shift below.
        bg  = (*text_cell).bg_color;
        fg  = (*text_cell).fg_color;

        *vram_fg = (2*chr + tile_offset) | (fg << 12);
        *vram_bg = (bg << 12) | (chr >> 8) | 0x100;

        text_cell++;
        vram_bg++;
        vram_fg++;
      }

      if(mainscr_x_33rd)
      {
        // Plot the 33rd column (in the next plane)
        chr = (*text_cell).char_value & 0x1FF;
        bg  = (*text_cell).bg_color;
        fg  = (*text_cell).fg_color;
        *(vram_fg+992) = (2*chr + tile_offset) | (fg << 12);
        *(vram_bg+992) = (bg << 12) | (chr >> 8) | 0x100;
      }

      // Move back.
      text_cell -= 32;
    }

    // Move to the next row.
    text_cell += 80;
  }
}

static void nds_render_graph(struct graphics_data *graphics)
{
  // Always render the scaled screen.
  nds_render_graph_scaled(graphics);

  // Always render the 1:1 "main" screen.
  nds_render_graph_1to1(graphics);
}

static void nds_update_palette_entry(struct rgb_color *palette, unsigned int idx)
{
  struct rgb_color color1 = palette[idx];
  int idx2;

  if(idx < 16)
  {
    // Update the mainscreen palette.
    BG_PALETTE_SUB[16 * idx + 1] = RGB15(color1.r / 8, color1.g / 8, color1.b / 8);

    // Update the subscreen palette.
    /* Iterate over an entire column of the palette, blending this color
     * with all 16 other colors (including itself). */
    for(idx2 = 0; idx2 < 16; idx2++)
    {
      /* Average the colors while reducing their accuracy to 5 bits. */
      struct rgb_color color2 = palette[idx2];
      u16 r = (color1.r + color2.r) / 16;
      u16 g = (color1.g + color2.g) / 16;
      u16 b = (color1.b + color2.b) / 16;

      BG_PALETTE[ palette_idx_table[(idx << 4) | idx2] ] = RGB15(r, g, b);
    }
  }
  else
  {
    // Update the mainscreen protected palette.
    BG_PALETTE_SUB[16 * (idx & 0x0F) + 2] =
     RGB15(color1.r / 8, color1.g / 8, color1.b / 8);
  }
}

static void nds_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  unsigned int i;

  for(i = 0; i < count; i++)
    nds_update_palette_entry(palette, i);
}

static void nds_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  // stub
}

static void nds_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  // stub
}

static void nds_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  // stub
}

static void nds_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  if(chr < 512)
  {
    /* Each character is 64 bytes.  Advance the vram pointer. */
    u16 *vram = (u16 *)BG_TILE_RAM_SUB(1) + 32 * chr;
    u8 *charset = graphics->charset + 14 * chr;
    u8 cshift = chr >= 256 ? 1 : 0;

    /* Iterate over every line. */
    u8 *end = charset + 14;
    for(; charset < end; charset++)
    {
      u16 line = *charset;

      /* Plot 8 pixels, 4 pixels at a time. */
      *(vram++) = (((line >> 7) & 1)      |
                  ((line >> 6) & 1) <<  4 |
                  ((line >> 5) & 1) <<  8 |
                  ((line >> 4) & 1) << 12) << cshift;
      *(vram++) = (((line >> 3) & 1)      |
                  ((line >> 2) & 1) <<  4 |
                  ((line >> 1) & 1) <<  8 |
                  ((line     ) & 1) << 12) << cshift;
    }

    nds_clear_graph_cache();
  }
}

static void nds_remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  if(chr < 512)
  {
    /* Each character is 64 bytes.  Advance the vram pointer. */
    u16 *vram = (u16 *)BG_TILE_RAM_SUB(1) + 32 * chr + 2*byte;
    u8 *charset = graphics->charset + 14 * chr + byte;
    u16 line = *charset;
    u8 cshift = chr >= 256 ? 1 : 0;

    /* Plot 8 pixels, 4 pixels at a time. */
    *(vram++) = (((line >> 7) & 1)      |
                ((line >> 6) & 1) <<  4 |
                ((line >> 5) & 1) <<  8 |
                ((line >> 4) & 1) << 12) << cshift;
    *(vram++) = (((line >> 3) & 1)      |
                ((line >> 2) & 1) <<  4 |
                ((line >> 1) & 1) <<  8 |
                ((line     ) & 1) << 12) << cshift;

    nds_clear_graph_cache();
  }
}

static void nds_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  int stop = first + count;
  int chr;

  if(stop > 512)
    stop = 512;

  for(chr = first; chr < stop; chr++)
    nds_remap_char(graphics, chr);
}

static void nds_focus_pixel(struct graphics_data *graphics,
 unsigned int x, unsigned int y)
{
  switch(get_allow_focus_changes())
  {
    case FOCUS_FORBID:
      return;
    case FOCUS_ALLOW:
      if(last_focus_x != (int)x || last_focus_y != (int)y)
      {
        last_focus_x = x;
        last_focus_y = y;
        focus_x = x;
        focus_y = y;
      }
      break;
    case FOCUS_PASS:
      focus_x = x;
      focus_y = y;
      break;
  }
}

const struct renderer renderer_nds =
{
  "nds",
  NULL,
  nds_init_video,
  NULL,
  nds_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  nds_update_colors,
  nds_remap_char_range,
  nds_remap_char,
  nds_remap_charbyte,
  NULL,
  nds_render_graph,
  NULL,
  nds_render_cursor,
  NULL,
  nds_render_mouse,
  nds_sync_screen,
  nds_focus_pixel,
};

boolean nds_subscreen_preview(void)
{
  // Don't switch during a transition.
  if(transition.state != TRANSITION_NONE)
    return false;

  // Call appropriate exit function.
  if(subscreen_mode != SUBSCREEN_SCALED)
  {
    last_subscreen_mode = subscreen_mode;
    subscreen_mode = SUBSCREEN_SCALED;
    nds_subscreen_keyboard_exit();
  }
  return true;
}

boolean nds_subscreen_keyboard(void)
{
  // Don't switch during a transition.
  if(transition.state != TRANSITION_NONE)
    return false;

  if(subscreen_mode != SUBSCREEN_KEYBOARD)
  {
    last_subscreen_mode = subscreen_mode;
    subscreen_mode = SUBSCREEN_KEYBOARD;
    nds_subscreen_keyboard_init();
  }
  return true;
}

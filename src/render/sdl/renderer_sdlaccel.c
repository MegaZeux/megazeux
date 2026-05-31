/* MegaZeux
 *
 * Copyright (C) 2019, 2024-2025 Alice Rowan <petrifiedrowan@gmail.com>
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

/* SDL hardware accelerated renderer using the SDL_Renderer API.
 * This thing is too slow to have much use in practice right now, but it might
 * be useful to keep around for reference. SMZX is not implemented yet.
 *
 * The old SDL_RenderTexture implementation (<2.0.18) is just plain slow.
 * SDL_RenderGeometry is better, but not enough to justify using it currently.
 *
 * The other main performance problem here is writes to the chars texture.
 * There's no compelling way to make it faster, and even a few rows updated can
 * cause a significant FPS drop at speed 2. Having a worker thread helps.
 */

#include "../render.h"
#include "../renderers.h"
#include "render_sdl.h"

#include "../../util.h"

#include <stdlib.h>

// 6 versions of each char, 16*256 chars -> 24576 total "chars"
// -> 49152 8x8s -> sqrt ~= 221 > 32 * 6

//#define THREADED_CHARSETS

#if SDL_VERSION_ATLEAST(2,0,18)
#define RENDER_GEOMETRY
#endif

#define TEX_SCREEN      0
#define TEX_CHARS       1
#define TEX_BACKGROUND  2

// 32 * 6 * 8 = 1536 px
// 128 * 1 * 14 = 1792 px
#define CHARS_ROW_SIZE 32
#define CHARS_NUM_ROWS (FULL_CHARSET_SIZE / CHARS_ROW_SIZE)

#define NUM_CHAR_VARIANTS 6

// ARGB8888
#define CHARS_RAW_ROW_W (CHAR_W * NUM_CHAR_VARIANTS * CHARS_ROW_SIZE)
#define CHARS_RAW_ROW_H (CHAR_H)
#define CHARS_RAW_ROW_PITCH (CHARS_RAW_ROW_W * sizeof(uint32_t))
#ifdef THREADED_CHARSETS
#define CHARS_RAW_ROW_SIZE (CHARS_RAW_ROW_W * CHARS_RAW_ROW_H)
#endif

#define TEX_CHARS_PIX_W (CHAR_W * NUM_CHAR_VARIANTS * CHARS_ROW_SIZE)
#define TEX_CHARS_PIX_H (CHAR_H * CHARS_NUM_ROWS)
#define TEX_CHAR_W ((float)CHAR_W / 2048.0)
#define TEX_CHAR_H ((float)CHAR_H / 2048.0)

#define TEX_SCREEN_W 1024
#define TEX_SCREEN_H 512

#define TEX_BG_W 128
#define TEX_BG_H 32

#define WHITE (0xFFFFFFFF)

#ifdef THREADED_CHARSETS
#define NUM_WORKERS 1
#define MAX_WORKER_ROWS 4

struct entry
{
  uint8_t first_row;
  uint8_t count;
};
#endif

struct sdlaccel_render_data
{
  struct sdl_render_data sdl;
#ifdef RENDER_GEOMETRY
  SDL_Vertex vertex[6 * (SCREEN_W + 2) * (SCREEN_H + 2)];
#endif
  uint8_t palette[FULL_PAL_SIZE * 3];
  boolean chars_dirty_row[CHARS_NUM_ROWS];
  boolean chars_dirty;
#ifdef THREADED_CHARSETS
  uint32_t chars_raw[TEX_CHARS_PIX_W * TEX_CHARS_PIX_H];
  platform_thread t[NUM_WORKERS];
  platform_cond cond_main;
  platform_cond cond_worker;
  platform_mutex lock;
  boolean chars_needed_row[CHARS_NUM_ROWS];
  boolean join;
#endif
};

static void write_char_byte_mzx(uint32_t **_char_data, uint8_t byte)
{
  uint32_t *char_data = *_char_data;
  *(char_data++) = (uint32_t)((int32_t)byte << 24 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 25 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 26 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 27 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 28 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 29 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 30 >> 31);
  *(char_data++) = (uint32_t)((int32_t)byte << 31 >> 31);
  *_char_data = char_data;
}

static void write_char_byte_smzx(uint32_t **_char_data, uint8_t byte, uint8_t col)
{
  uint32_t *char_data = *_char_data;
  *(char_data++) = ((byte >> 6) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 6) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 4) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 4) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 2) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 2) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 0) & 3) == col ? WHITE : 0;
  *(char_data++) = ((byte >> 0) & 3) == col ? WHITE : 0;
  *_char_data = char_data;
}

static void sdlaccel_do_remap_row(struct graphics_data *graphics,
 struct sdlaccel_render_data *render_data, uint32_t *dest, uint32_t pitch, int row)
{
  uint8_t char_byte;
  uint32_t skip = (pitch - CHARS_RAW_ROW_PITCH) / sizeof(uint32_t);
  int x;
  int y;

  for(y = 0; y < CHAR_H; y++)
  {
    for(x = 0; x < CHARS_ROW_SIZE; x++)
    {
      char_byte = graphics->charset[y + CHAR_H * (x + row * CHARS_ROW_SIZE)];
      write_char_byte_mzx(&dest, char_byte);
      write_char_byte_mzx(&dest, char_byte ^ 0xFF);
      write_char_byte_smzx(&dest, char_byte, 0);
      write_char_byte_smzx(&dest, char_byte, 1);
      write_char_byte_smzx(&dest, char_byte, 2);
      write_char_byte_smzx(&dest, char_byte, 3);
    }
    dest += skip;
  }
}

#ifdef THREADED_CHARSETS
static void enqueue(struct sdlaccel_render_data *render_data, int first_row,
 int count)
{
  platform_mutex_lock(&(render_data->lock));
  memset(render_data->chars_needed_row + first_row, true, count);
  platform_cond_signal(&(render_data->cond_worker));
  platform_mutex_unlock(&(render_data->lock));
}

static THREAD_RES sdlaccel_remap_worker(void *data)
{
  struct graphics_data *graphics = (struct graphics_data *)data;
  struct sdlaccel_render_data *render_data = graphics->render_data;
  uint32_t *pixels;
  int rows[MAX_WORKER_ROWS];
  int i, j;

  platform_mutex_lock(&(render_data->lock));

  while(!render_data->join)
  {
    platform_cond_wait(&(render_data->cond_worker), &(render_data->lock));

    if(render_data->join)
      break;

    while(true)
    {
      for(i = 0, j = 0; i < CHARS_NUM_ROWS && j < MAX_WORKER_ROWS; i++)
      {
        if(render_data->chars_needed_row[i])
        {
          render_data->chars_needed_row[i] = false;
          rows[j++] = i;
        }
      }
      if(j == 0)
        break;

      platform_mutex_unlock(&(render_data->lock));
      for(i = 0; i < j; i++)
      {
        pixels = render_data->chars_raw + CHARS_RAW_ROW_SIZE * rows[i];
        sdlaccel_do_remap_row(graphics, render_data, pixels, CHARS_RAW_ROW_PITCH, rows[i]);
      }

      platform_mutex_lock(&(render_data->lock));
    }

    platform_cond_signal(&(render_data->cond_main));
  }

  platform_mutex_unlock(&(render_data->lock));
  THREAD_RETURN;
}
#endif

static void sdlaccel_free_video(struct graphics_data *graphics)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;

  if(render_data)
  {
#ifdef THREADED_CHARSETS
    int i;

    platform_mutex_lock(&(render_data->lock));

    render_data->join = true;
    platform_cond_broadcast(&(render_data->cond_worker));

    platform_mutex_unlock(&(render_data->lock));

    for(i = 0; i < NUM_WORKERS; i++)
      platform_thread_join(&(render_data->t[i]));

    platform_cond_destroy(&(render_data->cond_main));
    platform_cond_destroy(&(render_data->cond_worker));
    platform_mutex_destroy(&(render_data->lock));
#endif

    sdl_destruct_window(graphics);
    graphics->render_data = NULL;
    free(render_data);
  }
}

static boolean sdlaccel_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct sdlaccel_render_data *render_data =
   (struct sdlaccel_render_data *)cmalloc(sizeof(struct sdlaccel_render_data));
#ifdef THREADED_CHARSETS
  int i;
#endif

  if(!render_data)
    return false;

  memset(render_data, 0, sizeof(struct sdlaccel_render_data));
  memset(render_data->chars_dirty_row, true, CHARS_NUM_ROWS);
  render_data->chars_dirty = true;
  graphics->render_data = render_data;
  graphics->allow_resize = conf->allow_resize;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

#ifdef THREADED_CHARSETS
  memset(render_data->chars_needed_row, true, CHARS_NUM_ROWS);

  platform_mutex_init(&(render_data->lock));
  platform_cond_init(&(render_data->cond_main));
  platform_cond_init(&(render_data->cond_worker));

  for(i = 0; i < NUM_WORKERS; i++)
    platform_thread_create(&(render_data->t[i]), sdlaccel_remap_worker, graphics);
#endif

  return true;
}

static boolean sdlaccel_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct sdlaccel_render_data *render_data =
   (struct sdlaccel_render_data *)graphics->render_data;

  SDL_Texture **texture = render_data->sdl.texture;
  int tex_chars_w;
  int tex_chars_h;

  // This requires that the underlying driver supports framebuffer objects.
  if(!sdl_create_window_renderer(graphics, window, true))
    return false;

  texture[TEX_SCREEN] =
   SDL_CreateTexture(render_data->sdl.renderer, render_data->sdl.texture_format,
    SDL_TEXTUREACCESS_TARGET, TEX_SCREEN_W, TEX_SCREEN_H);

  if(!texture[TEX_SCREEN])
  {
    warn("Failed to create screen texture: %s\n", SDL_GetError());
    goto err_free;
  }

#if !SDL_VERSION_ATLEAST(2,0,12)
  // Always use nearest neighbor for the charset and background textures.
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#endif

  tex_chars_w = round_to_power_of_two(TEX_CHARS_PIX_W);
  tex_chars_h = round_to_power_of_two(TEX_CHARS_PIX_H);

  // Don't use SDL_TEXTUREACCESS_STREAMING here right now.
  //
  // The main difference at the moment is it keeps a duplicate buffer of the
  // entire texture in memory. Changing arbitrary parts of this texture
  // requires locking the whole thing, after which the whole thing will be
  // uploaded (SLOW). It's better for the typical use case to write to a
  // smaller local version and only upload recently updated parts.
  texture[TEX_CHARS] =
   SDL_CreateTexture(render_data->sdl.renderer, render_data->sdl.texture_format,
    SDL_TEXTUREACCESS_STREAMING, tex_chars_w, tex_chars_h);

  if(!texture[TEX_CHARS])
  {
    warn("Failed to create texture: %s\n", SDL_GetError());
    goto err_free;
  }

  // Initialize the background texture.
  texture[TEX_BACKGROUND] =
   SDL_CreateTexture(render_data->sdl.renderer, render_data->sdl.texture_format,
    SDL_TEXTUREACCESS_STREAMING, TEX_BG_W, TEX_BG_H);

  if(!texture[TEX_BACKGROUND])
  {
    warn("Failed to create texture2: %s\n", SDL_GetError());
    goto err_free;
  }

  SDL_SetTextureBlendMode(texture[TEX_CHARS], SDL_BLENDMODE_BLEND);
  SDL_SetTextureBlendMode(texture[TEX_BACKGROUND], SDL_BLENDMODE_BLEND);

  sdl_set_texture_scale_mode(graphics, window, TEX_SCREEN, true);
  sdl_set_texture_scale_mode(graphics, window, TEX_CHARS, false);
  sdl_set_texture_scale_mode(graphics, window, TEX_BACKGROUND, false);

  SDL_SetRenderTarget(render_data->sdl.renderer, texture[TEX_SCREEN]);
  return true;

err_free:
  sdl_destruct_window(graphics);
  return false;
}

static boolean sdlaccel_resize_callback(struct graphics_data *graphics,
 struct video_window *window)
{
  sdl_set_texture_scale_mode(graphics, window, TEX_SCREEN, true);
  sdl_set_texture_scale_mode(graphics, window, TEX_CHARS, false);
  sdl_set_texture_scale_mode(graphics, window, TEX_BACKGROUND, false);
  return true;
}

static void sdlaccel_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, uint32_t count)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  uint32_t i;

  sdl_update_colors(graphics, palette, count);

  for(i = 0; i < count; i++)
  {
    render_data->palette[i*3  ] = palette[i].r;
    render_data->palette[i*3+1] = palette[i].g;
    render_data->palette[i*3+2] = palette[i].b;
  }
}

static void sdlaccel_remap_char_range(struct graphics_data *graphics,
 uint16_t first, uint16_t count)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  int last;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  last = (first + count - 1) / CHARS_ROW_SIZE;
  first = first / CHARS_ROW_SIZE;

  memset(render_data->chars_dirty_row + first, true, last - first + 1);
  render_data->chars_dirty = true;

#ifdef THREADED_CHARSETS
  enqueue(render_data, first, last - first + 1);
#endif
}

static void sdlaccel_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  int row;

  if(chr >= FULL_CHARSET_SIZE)
    chr = FULL_CHARSET_SIZE - 1;

  row = chr / CHARS_ROW_SIZE;
  render_data->chars_dirty_row[row] = true;
  render_data->chars_dirty = true;

#ifdef THREADED_CHARSETS
  enqueue(render_data, row, 1);
#endif
}

static void sdlaccel_remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  sdlaccel_remap_char(graphics, chr);
}

static void sdlaccel_do_remap_chars(struct graphics_data *graphics,
 struct sdlaccel_render_data *render_data)
{
  SDL_Rect rect = { 0, 0, CHARS_RAW_ROW_W, CHARS_RAW_ROW_H };

  int i;

  void *pixels;
  int pitch = CHARS_RAW_ROW_PITCH;

#ifndef THREADED_CHARSETS

  for(i = 0; i < CHARS_NUM_ROWS; i++)
  {
    if(render_data->chars_dirty_row[i])
    {
      rect.y = i * CHAR_H;
      SDL_LockTexture(render_data->sdl.texture[TEX_CHARS], &rect, &pixels, &pitch);

      sdlaccel_do_remap_row(graphics, render_data, pixels, pitch, i);

      SDL_UnlockTexture(render_data->sdl.texture[TEX_CHARS]);
      render_data->chars_dirty_row[i] = false;
    }
  }

#else /* THREADED_CHARSETS */

  int start;

  platform_mutex_lock(&(render_data->lock));
  for(i = 0; i < CHARS_NUM_ROWS; i++)
  {
    if(render_data->chars_needed_row[i])
    {
      platform_cond_signal(&(render_data->cond_worker));
      platform_cond_wait(&(render_data->cond_main), &(render_data->lock));
    }
  }
  platform_mutex_unlock(&(render_data->lock));

  for(i = 0; i < CHARS_NUM_ROWS; i++)
  {
    if(render_data->chars_dirty_row[i])
    {
      start = i;
      do
      {
        render_data->chars_dirty_row[i] = false;
        i++;
      }
      while(i < CHARS_NUM_ROWS && render_data->chars_dirty_row[i]);

      pixels = render_data->chars_raw + start * CHARS_RAW_ROW_SIZE;

      rect.y = start * CHAR_H;
      rect.h = (i - start) * CHARS_RAW_ROW_H;
      SDL_UpdateTexture(render_data->sdl.texture[TEX_CHARS], &rect, pixels, pitch);
    }
  }

#endif /* THREADED_CHARSETS */
}

#ifdef RENDER_GEOMETRY

static void vertex_char(struct SDL_Vertex *vertex, float topleft_x,
 float topleft_y, float tex_x, float tex_y, SDL_Color _sdl_color)
{
#if SDL_VERSION_ATLEAST(3,0,0)
  SDL_FColor sdl_color =
  {
    _sdl_color.r / 255.0,
    _sdl_color.g / 255.0,
    _sdl_color.b / 255.0,
    _sdl_color.a / 255.0,
  };
#else
  SDL_Color sdl_color = _sdl_color;
#endif
  vertex[0].position.x = topleft_x;
  vertex[0].position.y = topleft_y;
  vertex[0].tex_coord.x = tex_x;
  vertex[0].tex_coord.y = tex_y;
  vertex[0].color = sdl_color;
  vertex[1].position.x = topleft_x + CHAR_W;
  vertex[1].position.y = topleft_y;
  vertex[1].tex_coord.x = tex_x + TEX_CHAR_W;
  vertex[1].tex_coord.y = tex_y;
  vertex[1].color = sdl_color;
  vertex[2].position.x = topleft_x + CHAR_W;
  vertex[2].position.y = topleft_y + CHAR_H;
  vertex[2].tex_coord.x = tex_x + TEX_CHAR_W;
  vertex[2].tex_coord.y = tex_y + TEX_CHAR_H;
  vertex[2].color = sdl_color;

  vertex[3] = vertex[0];
  vertex[4].position.x = topleft_x;
  vertex[4].position.y = topleft_y + CHAR_H;
  vertex[4].tex_coord.x = tex_x;
  vertex[4].tex_coord.y = tex_y + TEX_CHAR_H;
  vertex[4].color = sdl_color;
  vertex[5] = vertex[2];
}

#else

static void sdlaccel_set_color(struct graphics_data *graphics, int color, int mode)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  uint8_t *palette = render_data->palette;

  if(!mode && color >= 16)
    color = graphics->protected_pal_position + (color % 16);

  SDL_SetTextureColorMod(render_data->sdl.texture[TEX_CHARS],
   palette[color * 3], palette[color * 3 + 1], palette[color * 3 + 2]);
}

#endif

static void sdlaccel_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  struct char_element *next = layer->data;
  int offx = layer->x;
  int offy = layer->y;
  int w = layer->w;
  int h = layer->h;
  int x;
  int y;
  int transparent_color = layer->transparent_col;
  unsigned chr;

  SDL_Renderer *renderer = render_data->sdl.renderer;
  SDL_Texture *chars_tex = render_data->sdl.texture[TEX_CHARS];
  SDL_Texture *bg_tex = render_data->sdl.texture[TEX_BACKGROUND];

  SDL_Rect_mzx dest_bg = sdl_render_rect(offx, offy, w * CHAR_W, h * CHAR_H);
  SDL_Rect_mzx src_bg = sdl_render_rect(0, 0, w, h);

  void *_bg;
  int bg_pitch;
  uint32_t *bg;

#ifdef RENDER_GEOMETRY
  SDL_Vertex *vertex = render_data->vertex;
  SDL_Color sdl_color;
  int vertices = 0;
  float topleft_x;
  float topleft_y;
  float tex_x;
  float tex_y;
#endif

  if(render_data->chars_dirty)
  {
    sdlaccel_do_remap_chars(graphics, render_data);
    render_data->chars_dirty = false;
  }

  if(!layer->mode)
  {
    SDL_LockTexture(bg_tex, NULL, &_bg, &bg_pitch);
    bg = (uint32_t *)_bg;
    bg_pitch >>= 2;
    memset(bg, 0, TEX_BG_W * TEX_BG_H * sizeof(uint32_t));

    for(y = 0; y < h; y++, bg += bg_pitch)
    {
      for(x = 0; x < w; x++, next++)
      {
        uint32_t color;

        if(next->char_value == INVISIBLE_CHAR)
          continue;

        /* Render background. */
        if(next->bg_color != transparent_color && next->fg_color != transparent_color)
        {
          color = next->bg_color;
          if(color >= 16)
            color = graphics->protected_pal_position + (color & 15);

          bg[x] = graphics->flat_intensity_palette[color];
        }

#ifdef RENDER_GEOMETRY
        /* Render foreground and partially transparent chars. */
        chr = next->char_value + layer->offset;
        tex_x = (chr % CHARS_ROW_SIZE) * (TEX_CHAR_W * NUM_CHAR_VARIANTS);
        tex_y = (chr / CHARS_ROW_SIZE) * TEX_CHAR_H;

        if(next->fg_color != transparent_color)
        {
          color = next->fg_color;
        }
        else

        if(next->bg_color != transparent_color)
        {
          color = next->bg_color;
          tex_x += TEX_CHAR_W;
        }
        else
          continue;

        if(color >= 16)
          color = graphics->protected_pal_position + (color & 15);

        sdl_color.r = render_data->palette[color * 3 + 0];
        sdl_color.g = render_data->palette[color * 3 + 1];
        sdl_color.b = render_data->palette[color * 3 + 2];
        sdl_color.a = 255;

        topleft_x = offx + x * CHAR_W;
        topleft_y = offy + y * CHAR_H;

        vertex_char(vertex, topleft_x, topleft_y, tex_x, tex_y, sdl_color);
        vertex += 6;
        vertices += 6;
#endif
      }
    }
    SDL_UnlockTexture(bg_tex);
    SDL_RenderTexture(renderer, bg_tex, &src_bg, &dest_bg);

#ifdef RENDER_GEOMETRY

    SDL_RenderGeometry(renderer, chars_tex, render_data->vertex, vertices, NULL, 0);

#else /* !RENDER_GEOMETRY */
    /* Render foreground and partially transparent chars. */
    {
    SDL_Rect dest_rect = { 0, 0, CHAR_W, CHAR_H };
    SDL_Rect src_rect = { 0, 0, CHAR_W, CHAR_H };
    next = layer->data;
    for(y = 0; y < h; y++)
    {
      for(x = 0; x < w; x++, next++)
      {
        if(next->char_value == INVISIBLE_CHAR)
          continue;

        chr = next->char_value + layer->offset;
        src_rect.x = (chr % CHARS_ROW_SIZE) * CHAR_W * NUM_CHAR_VARIANTS;
        src_rect.y = (chr / CHARS_ROW_SIZE) * CHAR_H;
        dest_rect.x = offx + x * CHAR_W;
        dest_rect.y = offy + y * CHAR_H;

        if(next->fg_color != transparent_color)
        {
          sdlaccel_set_color(graphics, next->fg_color, 0);
        }
        else

        if(next->bg_color != transparent_color)
        {
          sdlaccel_set_color(graphics, next->bg_color, 0);
          src_rect.x += CHAR_W;
        }
        else
          continue;

        SDL_RenderCopy(renderer, chars_tex, &src_rect, &dest_rect);
      }
    }
    }
#endif /* !RENDER_GEOMETRY */
  }
  else
  {
    // FIXME
  }
}

static void sdlaccel_render_cursor(struct graphics_data *graphics, unsigned x,
 unsigned y, uint16_t color, unsigned lines, unsigned offset)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  uint8_t *palette = render_data->palette;

  // Input coordinates are on the character grid.
  SDL_Rect_mzx dest =
   sdl_render_rect(x * CHAR_W, y * CHAR_H + offset, CHAR_W, lines);

  SDL_SetRenderDrawBlendMode(render_data->sdl.renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(render_data->sdl.renderer,
   palette[color * 3], palette[color * 3 + 1], palette[color * 3 + 2],
   SDL_ALPHA_OPAQUE);

  SDL_RenderFillRect(render_data->sdl.renderer, &dest);
}

static void sdlaccel_render_mouse(struct graphics_data *graphics, unsigned x,
 unsigned y, unsigned w, unsigned h)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;

  // Input coordinates are pixel values.
  SDL_Rect_mzx dest = sdl_render_rect(x, y, w, h);

  /* There is no preset inversion blend mode, so make a custom blend mode.
   * Lower SDL versions should simply fall back to drawing a white rectangle.
   */
#if SDL_VERSION_ATLEAST(2,0,6)
  {
    SDL_BlendMode invert = SDL_ComposeCustomBlendMode(
      /* Color: invert destination color. */
      SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD,
      /* Alpha: keep destination alpha. SDL docs don't mention this, but the
       * blend operations must be the same to work in OpenGL. */
      SDL_BLENDFACTOR_DST_ALPHA, SDL_BLENDFACTOR_ZERO, SDL_BLENDOPERATION_ADD);

    SDL_SetRenderDrawBlendMode(render_data->sdl.renderer, invert);
  }
#endif
  SDL_SetRenderDrawColor(render_data->sdl.renderer, 255, 255, 255, 255);

  SDL_RenderFillRect(render_data->sdl.renderer, &dest);
}

static void sdlaccel_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct sdlaccel_render_data *render_data = graphics->render_data;
  SDL_Renderer *renderer = render_data->sdl.renderer;
  SDL_Texture *screen_tex = render_data->sdl.texture[TEX_SCREEN];
  SDL_Rect_mzx src;
  SDL_Rect_mzx dest;

  src = sdl_render_rect(0, 0, SCREEN_PIX_W, SCREEN_PIX_H);

  dest.x = window->viewport_x;
  dest.y = window->viewport_y;
  dest.w = window->viewport_width;
  dest.h = window->viewport_height;

  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderTexture(renderer, screen_tex, &src, &dest);

  SDL_RenderPresent(renderer);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(renderer);

  SDL_SetRenderTarget(renderer, screen_tex);
}

const struct renderer renderer_sdlaccel =
{
  "sdlaccel",
  NULL,
  sdlaccel_init_video,
  sdlaccel_free_video,
  sdlaccel_create_window,
  sdl_resize_window,
  sdlaccel_resize_callback,
  set_window_viewport_scaled,
  sdl_set_window_caption,
  sdl_set_window_icon,
  NULL,
  sdlaccel_update_colors,
  sdlaccel_remap_char_range,
  sdlaccel_remap_char,
  sdlaccel_remap_charbyte,
  NULL,
  NULL,
  sdlaccel_render_layer,
  sdlaccel_render_cursor,
  NULL,
  sdlaccel_render_mouse,
  sdlaccel_sync_screen,
  NULL,
};

/* MegaZeux
 *
 * Copyright (C) 2010 Alan Williams <mralert@gmail.com>
 * Copyright (C) 2023-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "image_file.h"
#include "smzxconv.h"
#include "y4m.h"

#define SKIP_SDL
#ifdef SKIP_SDL
// Shut Up GCC
#endif
#include "../graphics.h"
#include "../util.h"
#include "../io/memfile.h"
#include "../platform/platform_thread.h"

#ifdef _WIN32
#include <fcntl.h>
#endif

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

//#define Y4M_DEBUG

#ifndef PLATFORM_NO_THREADING
#define MAX_WORKERS 256
#endif

#define STDIO_BUFFER_SIZE (1 << 15)

#define DEFAULT_MZX_SPEED   4
#define DEFAULT_FRAMERATE_N 125
#define DEFAULT_FRAMERATE_D (2 * (DEFAULT_MZX_SPEED - 1))

static const char USAGE[] =
{
"y4m2smzx Image Conversion Utility\n\n"

"Usage: %s <in.y4m|-> <out.dat|-> [options]\n\n"

"  Converts a .y4m input file to a custom video format easily loaded by Robotic.\n\n"

"  If the input file is '-', it will be read from stdin.\n"
"  If the output file is '-', it will be written to stdout.\n\n"

"  Options:\n\n"

"    --             Do not parse any of the following arguments as options.\n"
"    -f#:#          Set the framerate ratio of the output. The default is the\n"
"                   framerate specified in the input .y4m, or 125:6 if none.\n"
"    -j#            Set the number of worker threads (0=synchronous, default).\n"
"    --skip-char=#  Set the character to skip (one max, -1=none, default).\n"
"\n"

"  The binary output format is an unpadded IFF-like:\n\n"

"    Head:  MZXV [len:u32le] [chunks...] (len may be zero)\n"
"    Chunk: [FOURCC identifier] [len:u32le] [data...]\n\n"

"  Supported header chunk FOURCCs (must preceed frame data):\n\n"

"    fwid     [display_width_in_chars:u32le] (required)\n"
"    fhei     [display_height_in_chars:u32le] (required)\n"
"    bwid     [buffer_width_in_chars:u32le] (default fwid)\n"
"    bhei     [buffer_height_in_chars:u32le] (default fhei)\n"
"    sprx     [sprite_x_pitch:u32le] (default 0)\n"
"    spry     [sprite_y_pitch:u32le] (default 0)\n"
"    sprw     [sprites_per_row:u32le] (default 1)\n"
"    sprh     [sprites_per_column:u32le] (default 1)\n"
"    spru     [sprites_unbound:u32le] (default 0)\n"
"    spro     [sprite_offset_per_sprite:u32le] (default 0)\n"
"    sprt     [sprite_tcol:s32le] (default -1, bottom sprite always -1)\n"
"\n"
"  Supported frame chunk FOURCCs (omitted data persists between frames):\n\n"

"    smzx     [smzxmode:u32le] (default 0)\n"
"    rate     [numerator:u32le] [denominator:u32le] (default 125/6)\n"
"    fpal     [palette:len bytes]\n"
"    fidx     [indices:len bytes]\n"
"    fchr     [charset:len bytes]\n"
"    f1ch     [char_plane:len bytes] (frame displays upon parsing)\n"
"    f1co     [color_plane:len bytes] (frame displays upon parsing)\n"
"    f2in     [interleaved_planes:len bytes] (frame displays upon parsing)\n"
"\n"
};

struct y4m_convert_data
{
  const struct y4m_data *y4m;
  struct y4m_frame_data yf;
  struct rgba_color *rgba_buffer;
  smzx_converter *conv;
  mzx_tile *tile;
  mzx_glyph chr[256];
  mzx_color pal[256];
  size_t tile_size;
  boolean init;
  boolean ready;
};

#ifdef MAX_WORKERS
static platform_mutex worker_lock;
static platform_sem worker_sem;
static platform_sem main_sem;
static platform_thread workers[MAX_WORKERS];
static boolean worker_exit = false;
static boolean sync_init = false;
static int num_workers_allocated = 0;
static int queue_worker_head = 0;
static int queue_main_head = 0;
#endif
static int num_workers = 0;

static struct y4m_convert_data *queue = NULL;
static int queue_size = 0;
static int mzm_size = 0;
static int framerate_n = -1;
static int framerate_d = -1;
static int skip_char = -1;
static size_t ram_usage = 0;

static void fourcc(const char *fourcc, size_t len, struct memfile *mf)
{
  mfwrite(fourcc, 4, 1, mf);
  mfputud(len, mf);
}

static boolean y4m_queue_init(const struct y4m_data *y4m, int num,
 int width_chars, int height_chars)
{
  int i;
  if(queue)
  {
    fprintf(stderr, "ERROR: conversion data already initialized!\n");
    return false;
  }

  queue = (struct y4m_convert_data *)calloc(num, sizeof(struct y4m_convert_data));
  if(!queue)
  {
    fprintf(stderr, "ERROR: failed to conversion data!\n");
    return false;
  }
  ram_usage += num * sizeof(struct y4m_convert_data);
  queue_size = num;
  mzm_size = sizeof(mzx_tile) * width_chars * height_chars;

  for(i = 0; i < queue_size; i++)
  {
    queue[i].y4m = y4m;
    queue[i].ready = true;
    if(!y4m_init_frame(y4m, &(queue[i].yf)))
    {
      fprintf(stderr, "ERROR: failed to initialize y4m frame buffer %d.\n", i);
      return false;
    }
    ram_usage += y4m->ram_per_frame;

    queue[i].conv = smzx_convert_init(width_chars, height_chars, 0, skip_char, 256, 0, 16);
    if(!queue[i].conv)
    {
      fprintf(stderr, "ERROR: failed to initialize converter %d.\n", i);
      return false;
    }
    // TODO: smzx converter RAM

    queue[i].tile = (mzx_tile *)malloc(mzm_size);
    if(!queue[i].tile)
    {
      fprintf(stderr, "ERROR: failed to allocate tile buffer %d.\n", i);
      return false;
    }
    ram_usage += mzm_size;

    queue[i].rgba_buffer = (struct rgba_color *)malloc(y4m->rgba_buffer_size);
    if(!queue[i].rgba_buffer)
    {
      fprintf(stderr, "ERROR: failed to allocate frame buffer %d.\n", i);
      return false;
    }
    ram_usage += y4m->rgba_buffer_size;
  }
  return true;
}

static void y4m_queue_destroy(void)
{
  int i;
  if(!queue)
    return;

  for(i = 0; i < queue_size; i++)
  {
    y4m_free_frame(&(queue[i].yf));

    if(queue[i].conv)
      smzx_convert_free(queue[i].conv);

    free(queue[i].tile);
    free(queue[i].rgba_buffer);
  }
  free(queue);
  queue = NULL;
}

static void y4m_smzx_convert(struct y4m_convert_data *d)
{
  y4m_convert_frame_rgba(d->y4m, &(d->yf), (struct y4m_rgba_color *)d->rgba_buffer);
  smzx_convert(d->conv, d->rgba_buffer, d->tile, d->chr, d->pal);
}

#ifdef MAX_WORKERS
static THREAD_RES y4m_worker_function(void *opaque)
{
  struct y4m_convert_data *d = NULL;
#ifdef Y4M_DEBUG
  int n;
#endif

  platform_mutex_lock(&worker_lock);
  while(!worker_exit)
  {
    platform_mutex_unlock(&worker_lock);
    platform_sem_wait(&worker_sem);
    platform_mutex_lock(&worker_lock);
    if(worker_exit)
      break;

#ifdef Y4M_DEBUG
    n = queue_worker_head;
    fprintf(stderr, "convert at %d\n", n);
    fflush(stderr);
#endif

    d = &queue[queue_worker_head];
    queue_worker_head++;
    if(queue_worker_head >= queue_size)
      queue_worker_head = 0;

    platform_mutex_unlock(&worker_lock);

    y4m_smzx_convert(d);

    platform_mutex_lock(&worker_lock);
    platform_sem_post(&main_sem);
    d->ready = true;
#ifdef Y4M_DEBUG
    fprintf(stderr, "done at %d\n", n);
    fflush(stderr);
#endif
  }
  platform_mutex_unlock(&worker_lock);
  THREAD_RETURN;
}
#endif

static boolean y4m_workers_init(int num)
{
#ifdef MAX_WORKERS
  int i;
  if(num < 1)
    return true;

  if(sync_init || num_workers_allocated > 0)
    return false;

  if(!platform_mutex_init(&worker_lock) ||
     !platform_sem_init(&worker_sem, 0) ||
     !platform_sem_init(&main_sem, queue_size))
      return false;
  sync_init = true;

  for(i = 0; i < num; i++)
  {
    if(!platform_thread_create(&workers[i], y4m_worker_function, NULL))
    {
      fprintf(stderr, "ERROR: failed to initialize worker %d.\n", i);
      return false;
    }
  }
  num_workers_allocated = num;
#endif

  return true;
}

static void y4m_workers_destroy(void)
{
#ifdef MAX_WORKERS
  if(num_workers_allocated)
  {
    int i;
    platform_mutex_lock(&worker_lock);
    worker_exit = true;
    platform_mutex_unlock(&worker_lock);

    for(i = 0; i < num_workers_allocated; i++)
      platform_sem_post(&worker_sem);

    for(i = 0; i < num_workers_allocated; i++)
      platform_thread_join(&workers[i]);

    num_workers_allocated = 0;
  }

  if(sync_init)
  {
    platform_sem_destroy(&main_sem);
    platform_sem_destroy(&worker_sem);
    platform_mutex_destroy(&worker_lock);
    sync_init = false;
  }
#endif
}

static void y4m_write_frame(struct y4m_convert_data *d, FILE *out)
{
  struct memfile mf;
  uint8_t buffer[8];

  mfopen_wr(buffer, sizeof(buffer), &mf);
  fourcc("fchr", CHAR_SIZE * CHARSET_SIZE, &mf);
  fwrite(buffer, 8, 1, out);
  fwrite(d->chr, CHAR_SIZE, CHARSET_SIZE, out);

  mfseek(&mf, 0, SEEK_SET);
  fourcc("fpal", SMZX_PAL_SIZE * 3, &mf);
  fwrite(buffer, 8, 1, out);
  fwrite(d->pal, SMZX_PAL_SIZE, 3, out);

  mfseek(&mf, 0, SEEK_SET);
  fourcc("f2in", mzm_size, &mf);
  fwrite(buffer, 8, 1, out);
  fwrite(d->tile, mzm_size, 1, out);
}

static boolean y4m_do_conversion(struct y4m_data *y4m,
 int width_chars, int height_chars, FILE *in, FILE *out)
{
  struct memfile mf;
  uint8_t buffer[256];

  struct y4m_convert_data *d;
#ifdef Y4M_DEBUG
  int n = 0;
#endif

  size_t frames_in = 0;
  size_t frames_out = 0;
  boolean input_done = false;
  int pending = 0;

  if(framerate_n <= 0 || framerate_d <= 0)
  {
    if(y4m->framerate_n && y4m->framerate_d)
    {
      framerate_n = y4m->framerate_n;
      framerate_d = y4m->framerate_d;
    }
    else
    {
      framerate_n = DEFAULT_FRAMERATE_N;
      framerate_d = DEFAULT_FRAMERATE_D;
    }
  }

  /* Headerless mode 1 MZM output. */
  mzm_size = 2 * width_chars * height_chars;

  mfopen_wr(buffer, sizeof(buffer), &mf);
  fourcc("MZXV", 0, &mf);
  fourcc("fwid", 4, &mf);
  mfputud(width_chars, &mf);
  fourcc("fhei", 4, &mf);
  mfputud(height_chars, &mf);
  fourcc("smzx", 4, &mf);
  mfputud(2, &mf);
  fourcc("rate", 8, &mf);
  mfputud(framerate_n, &mf);
  mfputud(framerate_d, &mf);

  if(!fwrite(buffer, mftell(&mf), 1, out))
    return false;

  while(!input_done || frames_out < frames_in)
  {
#ifdef MAX_WORKERS
    if(num_workers)
    {
      platform_mutex_lock(&worker_lock);
      d = &queue[queue_main_head];
      if(pending == 0 || !d->ready)
      {
        platform_mutex_unlock(&worker_lock);
        platform_sem_wait(&main_sem);
        pending++;
        continue;
      }

#ifdef Y4M_DEBUG
      n = queue_main_head;
#endif
      queue_main_head++;
      if(queue_main_head >= queue_size)
        queue_main_head = 0;

      platform_mutex_unlock(&worker_lock);
    }
    else
#endif
      d = queue;

    if(d->init)
    {
#ifdef Y4M_DEBUG
      fprintf(stderr, "write from %d\n", n);
      fflush(stderr);
#endif
      if(!(frames_out & 0xff))
      {
        fprintf(stderr, ".");
        fflush(stderr);
      }
      y4m_write_frame(d, out);
      frames_out++;
      pending--;
    }

    if(input_done)
      continue;

    if(!y4m_begin_frame(y4m, &(d->yf), in))
    {
      input_done = true;
      continue;
    }

    if(!y4m_read_frame(y4m, &(d->yf), in))
    {
      fprintf(stderr, "ERROR: failed to process frame\n");
      return false;
    }
    d->ready = false;
    d->init = true;
    frames_in++;

#ifdef Y4M_DEBUG
    fprintf(stderr, "input to %d\n", n);
    fflush(stderr);
#endif

#ifdef MAX_WORKERS
    if(num_workers)
      platform_sem_post(&worker_sem);
    else
#endif
      y4m_smzx_convert(d);
  }

  fprintf(stderr, "\n%zu frames in, %zu frames out\n", frames_in, frames_out);
  return true;
}


int main(int argc, char **argv)
{
  struct y4m_data y4m;
  int num_queue;

  const char *input_file_name = NULL;
  const char *output_file_name = NULL;
  FILE *in = NULL;
  FILE *out = NULL;
  boolean init = false;
  boolean noopt = false;
  int ret = 2;
  char *end;
  int i;

  unsigned width_chars;
  unsigned height_chars;

  for(i = 1; i < argc; i++)
  {
    if(argv[i][0] == '-' && argv[i][1] == '-' && !noopt)
    {
      if(!strcmp(argv[i], "--"))
      {
        noopt = true;
      }
      else

      if(!strncmp(argv[i], "--skip-char=", 12))
      {
        skip_char = strtol(argv[i] + 12, &end, 10);
        if(skip_char < -1 || skip_char >= 256 || *end != '\0')
          goto err_param;
      }
    }
    else

    if(argv[i][0] == '-' && argv[i][1] != '\0' && !noopt)
    {
      int j;
      for(j = 1; argv[i][j]; j++)
      {
        if(argv[i][j] == 'f')
        {
          framerate_n = strtoul(argv[i] + j + 1, &end, 10);
          if(*end != ':' || framerate_n <= 0)
            goto err_param;
          framerate_d = strtoul(end + 1, &end, 10);
          if(*end != '\0' || framerate_d <= 0)
            goto err_param;
          break;
        }
        else

        if(argv[i][j] == 'j')
        {
          num_workers = strtoul(argv[i] + j + 1, &end, 10);
          if(*end != '\0')
            goto err_param;
          break;
        }
        else
          goto err_param;
      }
    }
    else

    if(!input_file_name)
    {
      input_file_name = argv[i];
    }
    else

    if(!output_file_name)
    {
      output_file_name = argv[i];
    }
    continue;

err_param:
    fprintf(stderr, "ERROR: invalid parameter '%s'\n", argv[i]);
    return 1;
  }

  if(!input_file_name || !output_file_name)
  {
    fprintf(stderr, USAGE, argv[0]);
    return 1;
  }

#ifdef MAX_WORKERS
  num_workers = CLAMP(num_workers, 0, MAX_WORKERS);
  num_queue = num_workers * 2;
#else
  num_workers = 0;
#endif
  if(num_workers < 1)
    num_queue = 1;

  if(!strcmp(input_file_name, "-"))
  {
    input_file_name = NULL;
    in = stdin;
#ifdef _WIN32
    /* Windows forces stdin to be text mode by default, fix it. */
    _setmode(_fileno(stdin), _O_BINARY);
#endif
  }

  if(!strcmp(output_file_name, "-"))
  {
    output_file_name = NULL;
    out = stdout;
#ifdef _WIN32
    /* Windows forces stdout to be text mode by default, fix it. */
    _setmode(_fileno(stdout), _O_BINARY);
#endif
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if((input_file_name && unveil(input_file_name, "r")) ||
   (output_file_name && unveil(output_file_name, "cw")) ||
   unveil(NULL, NULL))
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

  if(input_file_name)
  {
    in = fopen_unsafe(input_file_name, "rb");
    if(!in)
    {
      fprintf(stderr, "ERROR: failed to open input file.\n");
      goto err;
    }
    setvbuf(in, NULL, _IOFBF, STDIO_BUFFER_SIZE);
    ram_usage += STDIO_BUFFER_SIZE;
  }
  if(!y4m_init(&y4m, in))
  {
    fprintf(stderr, "ERROR: input file is not y4m\n");
    goto err;
  }
  init = true;

  if(y4m.width % 8)
  {
    fprintf(stderr, "ERROR: image width not divisible by 8\n");
    goto err;
  }
  if(y4m.height % 14)
  {
    fprintf(stderr, "WARNING: image height not divisible by 14; image will be cropped\n");
  }
  width_chars = y4m.width / 8;
  height_chars = y4m.height / 14;

  if(!y4m_queue_init(&y4m, num_queue, width_chars, height_chars))
    goto err;

  if(!y4m_workers_init(num_workers))
    goto err;

  if(output_file_name)
  {
    out = fopen_unsafe(output_file_name, "wb");
    if(!out)
    {
      fprintf(stderr, "ERROR: failed to open output file.\n");
      goto err;
    }
    setvbuf(out, NULL, _IOFBF, STDIO_BUFFER_SIZE);
    ram_usage += STDIO_BUFFER_SIZE;
  }

  fprintf(stderr, "Converter ready - allocated %zu of buffers.\n", ram_usage);
  fflush(stderr);

  if(!y4m_do_conversion(&y4m, width_chars, height_chars, in, out))
    goto err;

  // Insert total output file length (if not stdout).
  if(output_file_name)
  {
    long total = ftell(out) - 8;
    if(total > 0 && fseek(out, 4, SEEK_SET) == 0)
    {
      fputc(total & 0xff, out);
      fputc((total >> 8) & 0xff, out);
      fputc((total >> 16) & 0xff, out);
      fputc((total >> 24) & 0xff, out);
    }
  }
  ret = 0;

err:
  y4m_workers_destroy();
  y4m_queue_destroy();

  if(init)
    y4m_free(&y4m);

  if(input_file_name && in)
    fclose(in);

  if(output_file_name && out)
    fclose(out);

  return ret;
}

#if 0
#include <yuv4mpeg.h>

#define MZX_SPEED 4

static const y4m_ratio_t dest_fps = { 125, 2 * (MZX_SPEED - 1) };

int main(int argc, char **argv)
{
  int sf, df, stop = 0;
  int fd, frame_count, csub_type, csub_x, csub_y, x, y, yc, ic, i, r, g, b;
  smzx_converter *c;
  struct rgba_color img[640 * 350];
  mzx_tile tile[80 * 25];
  mzx_glyph chr[256];
  mzx_color pal[256];
  int yuvlen[3];
  uint8_t *yuv[3];
  y4m_ratio_t src_fps;
  y4m_ratio_t frame_ratio;
  y4m_stream_info_t istream;
  y4m_frame_info_t iframe;

  fd = fileno(stdin);
  y4m_accept_extensions(1);
  y4m_init_stream_info(&istream);
  y4m_init_frame_info(&iframe);

  if(y4m_read_stream_header(fd, &istream) != Y4M_OK)
  {
    fprintf(stderr, "Error reading stream header.\n");
    return 1;
  }
  if(y4m_si_get_plane_count(&istream) != 3)
  {
    fprintf(stderr, "Only 3-plane formats supported.\n");
    return 1;
  }
  if((y4m_si_get_width(&istream) != 640) || (y4m_si_get_height(&istream) != 350))
  {
    fprintf(stderr, "Video not 640x350.\n");
    return 1;
  }

  src_fps = y4m_si_get_framerate(&istream);
  frame_ratio.n = src_fps.d * dest_fps.n;
  frame_ratio.d = src_fps.n * dest_fps.d;
  y4m_ratio_reduce(&frame_ratio);
  frame_count = frame_ratio.d;
  csub_type = y4m_si_get_chroma(&istream);
  csub_x = y4m_chroma_ss_x_ratio(csub_type).d;
  csub_y = y4m_chroma_ss_y_ratio(csub_type).d;
  yuvlen[0] = y4m_si_get_plane_length(&istream, 0);
  yuvlen[1] = y4m_si_get_plane_length(&istream, 1);
  yuvlen[2] = y4m_si_get_plane_length(&istream, 2);

  yuv[0] = malloc(yuvlen[0]);
  yuv[1] = malloc(yuvlen[1]);
  yuv[2] = malloc(yuvlen[2]);
  if(!yuv[0] || !yuv[1] || !yuv[2])
  {
    fprintf(stderr, "Error allocating YUV buffer.\n");
    free(yuv[0]);
    free(yuv[1]);
    free(yuv[2]);
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    return 1;
  }

  c = smzx_convert_init(80, 25, 0, -1, 256, 0, 16);
  if(!c)
  {
    fprintf(stderr, "Error initializing converter.\n");
    free(yuv[0]);
    free(yuv[1]);
    free(yuv[2]);
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    return 1;
  }

  sf = df = 0;
  while(!stop && (y4m_read_frame(fd, &istream, &iframe, yuv) == Y4M_OK))
  {
    frame_count += frame_ratio.n;
    if(frame_count > frame_ratio.d)
    {
      for(i = y = 0; y < 350; y++)
      {
        yc = y / csub_y * 640 / csub_x;
        for(x = 0; x < 640; x++, i++)
        {
          ic = yc + x / csub_x;
          r = yuv[0][i]
            + 359 * (yuv[2][ic] - 128) / 256;
          g = yuv[0][i]
            - 88 * (yuv[1][ic] - 128) / 256
            - 183 * (yuv[2][ic] - 128) / 256;
          b = yuv[0][i]
            + 453 * (yuv[1][ic] - 128) / 256;
          r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
          g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
          b = (b < 0) ? 0 : ((b > 255) ? 255 : b);
          img[i].r = r;
          img[i].g = g;
          img[i].b = b;
          img[i].a = 255;
        }
      }
      smzx_convert(c, img, tile, chr, pal);
      fprintf(stderr, "Converted src: %d dest: %d\n", sf, df);
    }

    while(frame_count > frame_ratio.d)
    {
      if(!fwrite(pal, sizeof(pal), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      if(!fwrite(chr, sizeof(chr), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      if(!fwrite(tile, sizeof(tile), 1, stdout))
      {
        fprintf(stderr, "Error writing stream.\n");
        stop = 1;
        break;
      }
      frame_count -= frame_ratio.d;
      df++;
    }
    sf++;
  }

  free(yuv[0]);
  free(yuv[1]);
  free(yuv[2]);
  y4m_fini_frame_info(&iframe);
  y4m_fini_stream_info(&istream);
  return 0;
}
#endif

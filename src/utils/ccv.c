/* Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#define CCV_VERSION "v0.075"

// With fix contributed by Lachesis

#define CORE_LIBSPEC

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include "../config.h"

#include "utils_alloc.h"
#include "../io/image_file.h"

#ifndef VERSION_DATE
#define VERSION_DATE
#endif

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

// This software requires uthash to compile. Download it from
// http://troydhanson.github.com/uthash/

#include "uthash.h"

static const char USAGE[] =
"ccv " CCV_VERSION " :: MegaZeux " VERSION VERSION_DATE "\n"
"char conversion tool for megazeux by Dr Lancer-X <drlancer@megazeux.org>\n"
"Usage: ccv <input files...> [options...]\n";

static const char USAGE_DESC[] =
"\n"
"  By default, ccv converts input file 'input.fil' to output charset 'input.chr'\n"
"  with no char reuse.\n\n"

"  If the input file '-' is provided, it will be read from stdin.\n\n"

"  Supported image file formats are:\n\n"

"  * PNG\n"
"  * GIF (multi-image GIFs will be flattened into a single image;\n"
"         non-square pixel aspect ratios are supported via upscaling)\n"
"  * BMP (1bpp, 2bpp, 4bpp, 8bpp, 16bpp, 24bpp, 32bpp, RLE8, RLE4, bitfields)\n"
"  * TGA (all)\n"
"  * Netpbm/PNM (.pbm, .pgm, .ppm, .pnm, .pam)\n"
"  * farbfeld (.ff)\n"
"  * raw (see raw format)\n"
"\n";

static const char USAGE_OPTS[] =
"Options:\n\n"

"  -blank [#]       Specifies a blank char index (default is -1 for none).\n"
"  -c [#]           Specifies the maximum number of chars to output, or 0 for\n"
"                   no maximum char limit (default).\n"
"  -dither [type]   Specifies a dithering algorithm to apply (default is none).\n"
"                   Supported values: floyd-steinberg, stucki, jarvis-judice-ninke,\n"
"                   burkes, sierra, stevenson-arce. These values can be abbreviated,\n"
"                   e.g. 'floyd' for 'floyd-steinberg'.\n"
"  -exclude [...]   Exclude a single char or range of chars from conversion.\n"
"                   The parameter can be a single number (32), a range (128-159),\n"
"                   or a comma-separated list of numbers and ranges. In the\n"
"                   output charset, these will be left blank.\n"
"  -help            Display this message.\n"
"  -mzm             Output both an MZM and a CHR.\n"
"  -offset [#]      Specifies the starting char offset for MZMs. Intended for use\n"
"                   with LOAD CHAR SET \"@###file.chr\", where ### is the offset.\n"
"  -output [name]   Specifies an output filename prefix. For example, '-output da_bomb'\n"
"                   will output 'da_bomb.chr' (and 'da_bomb.mzm' if -mzm is specified).\n"
"                   By default, this is the prefix of the input file, or 'out' if the\n"
"                   input file is stdin.\n"
"  -q               Suppress stdout messages.\n"
"  -reuse -noreuse  Specify whether or not chars should be reused.\n"
"                   On by default with -mzm or -c, off by default otherwise.\n"
"  -threshold [#]   Specify the intensity threshold for quantization. This is a value\n"
"                   from 0 to 255; if the intensity of a pixel is greater-than-or-equal\n"
"                   to this threshold, it will be considered 'on'; if less than this\n"
"                   threshold, it will be considered 'off'. Default is 127.\n"
"  -w [#]           Specify raw format width (see raw format).\n"
"  -h [#]           Specify raw format height (see raw format).\n"
"\n";

static const char USAGE_RAW[] =
"Raw format:\n\n"

"  ccv allows input of raw greyscale images if -w and -h are specified. Each\n"
"  byte of the input file represents the intensity of one pixel.\n"

"  Prior ccv versions treated these intensities as boolean values (zero for off,\n"
"  non-zero for on). For equivalent behavior to this, use -threshold 1.\n"
"\n";

static const char USAGE_EXAMPLES[] =
"Examples:\n\n"

"  ccv input.bmp\n\n"

"    converts input.bmp to input.chr. The conversion will convert the image to\n"
"    a charset or partial charset containing a quantised representation of the\n"
"    image. No char reuse will be done.\n\n"

"  ccv input.png -mzm -output output\n\n"

"    converts input.bmp to output.chr and output.mzm. If the same character would\n"
"    have been used multiple times, it will be reused. Hence while the .chr\n"
"    itself may not match the input image, the combination of the .chr and .mzm will.\n\n"

"  ccv input1.bmp input2.bmp -mzm\n\n"

"    converts input1.bmp and input2.bmp to input1.chr, input2.chr,\n"
"    input1.mzm and input2.mzm.\n\n"

"  ccv input.bmp -mzm -blank 32\n\n"

"    converts input.bmp to input.chr and input.mzm. Char 32 will be blank and blank\n"
"    parts of the image will be assigned char #32 instead of some other char.\n\n"

"  ccv -c 30 input.bmp -mzm\n\n"

"    converts input.bmp to input.chr and input.mzm, with no more than 30 characters\n"
"    used. If necessary, close characters will be reused. -c implies -reuse.\n\n"

"  ccv -offset 128 input.bmp -mzm\n\n"

"    converts input.bmp to input.chr and input.mzm with the char indices in\n"
"    input.mzm being offset by 128 places. Hence loading the charset with\n"
"    LOAD CHAR SET \"@128input.chr\" and placing the MZM should result in\n"
"    accurate graphics.\n"

"  ccv -exclude 0,32-63 input.bmp -mzm\n\n"

"    converts input.bmp to input.chr and input.mzm. If char #0 or chars #32 through #63\n"
"    would otherwise be used in the conversion, they will be replaced with blank\n"
"    chars instead and will not be referenced in the output -mzm.\n\n"

"  ccv input.raw -w 256 -h 112 -threshold 32\n\n"

"    converts input.raw to input.chr. All bytes >=32 will be treated as white and\n"
"    bytes <32 will be treated as black.\n\n"

"  ccv input.bmp -dither floyd-steinberg -threshold 160\n\n"

"    converts input.bmp to input.chr using floyd-steinberg dithering and treating\n"
"    intensities >=160 as white.\n"
"\n";

static void Usage(void)
{
  fprintf(stderr, USAGE);
  fprintf(stderr, "Type \"ccv -help\" for extended options information\n");
}

static void Help(void)
{
  fprintf(stderr, USAGE);
  fprintf(stderr, USAGE_DESC);
  fprintf(stderr, USAGE_OPTS);
  fprintf(stderr, USAGE_RAW);
  fprintf(stderr, USAGE_EXAMPLES);
}

static void Error(const char *string, ...)
{
  va_list vaList;
  va_start(vaList, string);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, string, vaList);
  fprintf(stderr, "\n");
  va_end(vaList);
  exit(1);
}

static inline int lc_strncmp(const char *A, const char *B, int n)
{
  const unsigned char *a = (const unsigned char *)A;
  const unsigned char *b = (const unsigned char *)B;
  int i;

  if(A == NULL) return -1;
  if(B == NULL) return 1;
  for(i = 0; i < n; i++)
  {
    if(tolower(*a) < tolower(*b)) return -1;
    if(tolower(*a) > tolower(*b)) return 1;
    if(*a == '\0') return 0;
    a++;
    b++;
  }
  return 0;
}

static inline void file_write32(int val, FILE *fp)
{
  fputc((val >> 0) & 0xFF, fp);
  fputc((val >> 8) & 0xFF, fp);
  fputc((val >> 16) & 0xFF, fp);
  fputc((val >> 24) & 0xFF, fp);
}

/*
static inline int file_read32(FILE *fp)
{
  unsigned int r = 0;
  r |= fgetc(fp);
  r |= fgetc(fp) << 8;
  r |= fgetc(fp) << 16;
  r |= fgetc(fp) << 24;
  return r;
}
*/

static inline void file_write16(int val, FILE *fp)
{
  fputc((val >> 0) & 0xFF, fp);
  fputc((val >> 8) & 0xFF, fp);
}

/*
static inline int file_read16(FILE *fp)
{
  unsigned int r = 0;
  r |= fgetc(fp);
  r |= fgetc(fp) << 8;
  return r;
}
*/

typedef struct
{
  char path[MAX_PATH+1];
  UT_hash_handle hh;
} InputFile;

typedef struct
{
  InputFile *files;

  int w;
  int h;

  int mzm;
  int reuse;
  int noreuse;
  int quiet;

  int c;
  int threshold;

  int offset;
  int exclude_on;
  char exclude[256];

  int blank;

  char output[MAX_PATH - 4 + 1];
  char dither[256];
} Config;

static Config *DefaultConfig(void)
{
  Config *cfg = cmalloc(sizeof(Config));
  cfg->files = NULL;

  cfg->w = 0;
  cfg->h = 0;

  cfg->mzm = 0;
  cfg->reuse = 0;
  cfg->noreuse = 0;
  cfg->quiet = 0;

  cfg->offset = 0;
  cfg->exclude_on = 0;
  for (int i = 0; i < 256; i++) {
    cfg->exclude[i] = 0;
  }
  cfg->blank = -1;

  cfg->c = 0;
  cfg->threshold = 127;

  cfg->output[0] = '\0';
  cfg->dither[0] = '\0';

  return cfg;
}

static int Option(const char *option_name, int arguments, char *argument, int args_left)
{
  if(strcmp(argument, option_name) != 0)
    return 0;

  if(args_left < arguments)
    Error("Insufficient arguments for %s", option_name);

  return 1;
}

static Config *LoadConfig(int argc, char **argv)
{
  Config *cfg = DefaultConfig();
  int i;

  if(argc >= 1 && strcmp(argv[0], "-help") == 0)
  {
    Help();
    exit(0);
  }

  for(i = 0; i < argc; i++)
  {
    if(argv[i][0] == '-' && argv[i][1] != '\0') // Argument
    {
      int args_left = argc - i - 1;
      if(Option("-q", 0, argv[i], args_left))
      {
        cfg->quiet = 1;
        continue;
      }
      if(Option("-w", 1, argv[i], args_left))
      {
        cfg->w = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-h", 1, argv[i], args_left))
      {
        cfg->h = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-mzm", 0, argv[i], args_left))
      {
        cfg->mzm = 1;
        cfg->reuse = 1;
        continue;
      }
      if(Option("-reuse", 0, argv[i], args_left))
      {
        cfg->reuse = 1;
        continue;
      }
      if(Option("-noreuse", 0, argv[i], args_left))
      {
        cfg->noreuse = 1;
        continue;
      }
      if(Option("-output", 1, argv[i], args_left))
      {
        snprintf(cfg->output, MAX_PATH - 4, "%s", argv[i+1]);
        cfg->output[MAX_PATH-4] = '\0';
        i++;
        continue;
      }
      if(Option("-c", 1, argv[i], args_left))
      {
        cfg->c = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-dither", 1, argv[i], args_left))
      {
        snprintf(cfg->dither, sizeof(cfg->dither), "%s", argv[i+1]);
        cfg->dither[255] = '\0';
        i++;
        continue;
      }
      if(Option("-threshold", 1, argv[i], args_left))
      {
        cfg->threshold = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-offset", 1, argv[i], args_left))
      {
        cfg->offset = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-blank", 1, argv[i], args_left))
      {
        cfg->blank = atoi(argv[i+1]);
        i++;
        continue;
      }
      if(Option("-exclude", 1, argv[i], args_left))
      {
        char *s = argv[i+1];
        char tok[256];
        char op;
        int curr = 0;
        int range_st = -1;
        int range_ed = -1;

        cfg->exclude_on = 1;

        while(*s)
        {
          if(sscanf(s, "%[0-9]", tok) == 0)
            break;
          s += strlen(tok);

          cfg->exclude[atoi(tok)] = 1;

          if(curr == 1)
          {
            curr = 0;
            range_ed = atoi(tok);

            for(int ch = range_st; range_st <= range_ed ? ch <= range_ed : ch >= range_ed; range_st <= range_ed ? ch++ : ch--)
              cfg->exclude[ch] = 1;
          }

          op = s[0];
          if(op)
            s++;

          if(op == '-')
          {
            range_st = atoi(tok);
            curr = 1;
          }
        }

        i++;
        continue;
      }
    }
    else // Filename
    {
      InputFile *file = cmalloc(sizeof(InputFile));
      snprintf(file->path, MAX_PATH + 1, "%s", argv[i]);
      file->path[MAX_PATH] = 0;
      HASH_ADD_STR(cfg->files, path, file);
    }
  }

  if(cfg->files == NULL)
  {
    Usage();
    exit(1);
  }

  return cfg;
}

typedef struct
{
  int w, h;
  int channels;
  unsigned char *pixels;
} Image;

static Image *CreateImage(int w, int h, int channels)
{
  Image *image = cmalloc(sizeof(Image));
  image->pixels = cmalloc(w * h * channels);
  image->w = w;
  image->h = h;
  image->channels = channels;
  return image;
}

static void LoadImage(struct image_file *dest, Config *cfg, const char *path)
{
  struct image_raw_format format;
  struct image_raw_format *use_format = NULL;
  enum image_error ret;

  if((cfg->w > 0) && (cfg->h > 0))
  {
    format.width = cfg->w;
    format.height = cfg->h;
    format.bytes_per_pixel = 1; // TODO
    format.force_raw = true;
    use_format = &format;
  }

  ret = load_image_from_file(path, dest, use_format);
  if(ret)
    Error("Failed to load file '%s': %s", path, image_error_string(ret));
}

static void FreeImage(Image *img)
{
  free(img->pixels);
  free(img);
}

static int Brightness(const struct image_file *img, uint32_t x, uint32_t y)
{
  const struct rgba_color *color = &(img->data[y * img->width + x]);
  return (color->r * 30 + color->g * 59 + color->b * 11) / 100;
}

typedef struct
{
  char method[256];
  int diffuse[1024];
  unsigned w, h, div;
} Diffuse;

static const Diffuse diffmethods[] = {
{"Floyd-Steinberg", {0, -1, 7,
                    3, 5, 1}, 3, 2, 16},

{"Jarvis-Judice-Ninke", {0, 0, -1, 7, 5,
                         3, 5,  7, 5, 3,
                         1, 3,  5, 3, 1}, 5, 3, 48},

{"Stucki", {0, 0, -1, 7, 5,
            2, 4,  8, 4, 2,
            1, 2,  4, 2, 1}, 5, 3, 42},

{"Burkes", {0, 0, -1, 8, 4,
            2, 4, 8, 4, 2}, 5, 2, 32},

{"Sierra", {0, 0, -1, 5, 3,
            2, 4,  5, 4, 2,
            0, 2,  3, 2, 0}, 5, 3, 32},

{"Stevenson-Arce", {0,  0,  0,  -1, 0,  32, 0,
                    12, 0,  26, 0,  30, 0,  16,
                    0,  12, 0,  26, 0,  12, 0,
                    5,  0,  12, 0,  12, 0,  5}, 7, 4, 200}
};

static void QuantiseDiffuse(struct image_file *img, Image *qimg, int threshold, const char *method)
{
  int *err = cmalloc(img->width * img->height * sizeof(int));
  int method_i = -1;
  unsigned int i;
  int mx, my;
  uint32_t x, y;
  Diffuse D;
  int v_noerr, v, wr;
  memset(err, 0, img->width * img->height * sizeof(int));

  for(i = 0; i < sizeof(diffmethods) / sizeof(diffmethods[0]); i++)
  {
    if(lc_strncmp(method, diffmethods[i].method, strlen(method)) == 0)
      method_i = i;
  }
  if(method_i == -1)
    Error("-dither method '%s' not found. Try another setting", method);

  D = diffmethods[method_i];
  mx = 0;
  my = 0;
  for(i = 0; i < D.w * D.h; i++)
  {
    x = i % D.w;
    y = i / D.w;
    if(D.diffuse[i] == -1)
    {
      mx = x;
      my = y;
      D.diffuse[i] = 0;
    }
  }
  for(y = 0; y < img->height; y++)
  {
    for(x = 0; x < img->width; x++)
    {
      v_noerr = Brightness(img, x, y);
      v = v_noerr + err[y * img->width + x] / D.div;
      if (v > 255) v = 255;
      if (v < 0) v = 0;
      wr = v >= threshold ? 255 : 0;
      qimg->pixels[y * img->width + x] = v >= threshold ? 1 : 0;

      if((v_noerr - wr) != 0)
      {
        for(i = 0; i < D.w * D.h; i++)
        {
          int ox = (i % D.w) - mx + x;
          int oy = (i / D.w) - my + y;
          if((D.diffuse[i] > 0) && (ox >= 0) && (oy >= 0) &&
           ((size_t)ox < img->width) && ((size_t)oy < img->height))
          {
            err[oy * img->width + ox] += D.diffuse[i] * (v - wr);
          }
        }
      }
    }
  }
  free(err);
}

static void QuantiseTheshold(struct image_file *img, Image *qimg, int threshold)
{
  unsigned int i, x, y;

  for(i = 0, y = 0; y < img->height; y++)
  {
    for(x = 0; x < img->width; x++, i++)
    {
      int v = Brightness(img, x, y);
      qimg->pixels[i] = v >= threshold ? 1 : 0;
    }
  }
}

static Image *Quantise(Config *cfg, struct image_file *img)
{
  Image *qimg = CreateImage(img->width, img->height, 1);

  if(cfg->dither[0] == '\0')
  {
    QuantiseTheshold(img, qimg, cfg->threshold);
  }
  else
    QuantiseDiffuse(img, qimg, cfg->threshold, cfg->dither);

  return qimg;
}

typedef struct
{
  int chars;
  unsigned char *data;
} Charset;

static Charset *CreateCharset(Image *img)
{
  Charset *cset = cmalloc(sizeof(Charset));
  int cset_size = (img->w / 8) * (img->h / 14);
  int chr, cy, cx, py, px;
  cset->data = cmalloc(cset_size * 14);
  memset(cset->data, 0, cset_size * 14);

  cset->chars = cset_size;

  chr = 0;
  for(cy = 0; cy < img->h / 14; cy++)
  {
    for(cx = 0; cx < img->w / 8; cx++)
    {
      for(py = 0; py < 14; py++)
      {
        for(px = 0; px < 8; px++)
        {
          cset->data[chr * 14 + py] |= img->pixels[(cy * 14 + py) * img->w + (cx * 8 + px)] ? 128 >> px : 0;
        }
      }
      chr++;
    }
  }

  return cset;
}

typedef struct
{
  int w, h;
  int *data;
} Mzm;

static Mzm *CreateMzm(int w, int h)
{
  Mzm *mzm = cmalloc(sizeof(Mzm));
  int i;
  mzm->w = w;
  mzm->h = h;
  mzm->data = cmalloc(w * h * sizeof(int));

  for(i = 0; i < w * h; i++)
    mzm->data[i] = i;

  return mzm;
}

static void FreeMzm(Mzm *mzm)
{
  free(mzm->data);
  free(mzm);
}

typedef struct
{
  unsigned char data[14];
  int i;
  UT_hash_handle hh;
} Char;

static void Reuse(Charset *cset, Mzm *mzm)
{
  Char *clist = NULL;
  Char *c, *tmp;
  int out_chars = 0;
  int i;
  unsigned char *cdata;
  for(i = 0; i < cset->chars; i++)
  {
    cdata = cset->data + i * 14;

    HASH_FIND(hh, clist, cdata, sizeof(clist->data), c);
    if(!c)
    {
      c = cmalloc(sizeof(Char));
      memcpy(c->data, cdata, 14);

      if(mzm)
        c->i = mzm->data[i] = out_chars;

      memmove(cset->data + out_chars * 14, cset->data + i * 14, 14);
      HASH_ADD(hh, clist, data, sizeof(clist->data), c);
      out_chars++;
    }
    else
    {
      if(mzm)
        mzm->data[i] = c->i;
    }
  }
  cset->chars = out_chars;
  cset->data = realloc(cset->data, cset->chars * 14);

  HASH_ITER(hh, clist, c, tmp)
  {
    HASH_DEL(clist, c);
    free(c);
  }
}

static char *OutputPath(const char *input, Config *cfg, const char *ext)
{
  char *output_path = NULL;
  char buf[MAX_PATH+1];
  char *buf_e;
  size_t output_len;
  if(cfg->output[0] == '\0')
  {
    if(!strcmp(input, "-"))
      input = "out";

    snprintf(buf, MAX_PATH, "%s", input);
    buf[MAX_PATH] = '\0';

    buf_e = strrchr(buf, '.');
    if(buf_e)
      *buf_e = '\0';

    output_len = strlen(buf) + strlen(ext) + 1;
    output_path = cmalloc(output_len);
    snprintf(output_path, output_len, "%s%s", buf, ext);
    output_path[output_len - 1] = '\0';
  }
  else
  {
    output_len = strlen(cfg->output) + strlen(ext) + 1;
    output_path = cmalloc(output_len);
    snprintf(output_path, output_len, "%s%s", cfg->output, ext);
    output_path[output_len - 1] = '\0';
  }
  return output_path;
}

static void WriteCharset(const char *path, Charset *cset)
{
  FILE *fp = fopen_unsafe(path, "wb");
  fwrite(cset->data, 14, cset->chars, fp);
  fclose(fp);
}

static void WriteMzm(const char *path, Mzm *mzm)
{
  FILE *fp = fopen_unsafe(path, "wb");
  int i;
  fwrite("MZM2", 1, 4, fp);

  file_write16(mzm->w, fp);
  file_write16(mzm->h, fp);
  file_write32(0, fp);
  fputc(0, fp); // # of robots
  fputc(1, fp); // storage mode
  fputc(0, fp); // robot storage mode
  fputc(0, fp); // reserved

  for(i = 0; i < mzm->w * mzm->h; i++)
  {
    fputc(mzm->data[i] & 0xFF, fp);
    fputc(15, fp);
  }
  fclose(fp);
}

static inline int CountBits(const unsigned char *A)
{
  int c, i;
  const unsigned int *a;
  unsigned int v;
  unsigned int count;
  unsigned char val;
  if(sizeof(unsigned int) == 4)
  {
    c = 0;

    a = (const unsigned int*)A;
    for(i = 0; i < 3; i++)
    {
      v = *a;
      v = v - ((v >> 1) & 0x55555555);
      v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
      c += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
      a++;
    }
    v = (A[13] << 8 | A[12]);
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    c += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;

    return c;
  }
  else
  {
    count = 0;
    for(int i = 0; i < 14; i++)
    {
      val = A[i];
      for(; val; count++)
        val &= val - 1; // clear the least significant bit set
    }
    return count;
  }
  return -1;
}

static inline int CharDist(const unsigned char *A, const unsigned char *B)
{
  unsigned char v[14];
  int i;
  for(i = 0; i < 14; i++)
    v[i] = A[i] ^ B[i];

  return CountBits(v);
}

// Simple quantisation- combine the two closest pairs of chars until the right number of chars are available
static void Reduce_Simple(Charset *cset, int chars)
{
  int pair_i, pair_j, pair_dist, data_dist;
  int i, j;
  unsigned char *data_i, *data_j;
  int bits;

  while(cset->chars > chars)
  {
    pair_i = -1;
    pair_j = -1;
    pair_dist = INT_MAX;

    for(i = 0; i < cset->chars; i++)
    {
      data_i = cset->data + i * 14;
      for(j = i + 1; j < cset->chars; j++)
      {
        data_j = cset->data + j * 14;

        data_dist = CharDist(data_i, data_j);
        if(data_dist < pair_dist)
        {
          pair_dist = data_dist;
          pair_i = i;
          pair_j = j;
        }
      }
    }

    data_i = cset->data + pair_i * 14;
    data_j = cset->data + pair_j * 14;
    bits = CountBits(data_i) + CountBits(data_j);
    if(bits >= 112)
    {
      for(i = 0; i < 14; i++)
      {
        //data_i[i] = data_i[i] > data_j[i] ? data_i[i] : data_j[i];
        data_i[i] = data_i[i] | data_j[i];
      }
    }
    else
    {
      for(i = 0; i < 14; i++)
        data_i[i] = ~(~data_i[i] | ~data_j[i]);
    }

    memmove(data_j, data_j + 14, (cset->chars - pair_j - 1) * 14);
    cset->chars--;
  }

  cset->data = realloc(cset->data, cset->chars * 14);
}

static int ChrImageDist(const unsigned char *cdata, Image *img, int xoff, int yoff)
{
  int tdist = 0;
  int x, y, px, py, chr_lightness, img_lightness;
  for(y = 0; y < 14; y++)
  {
    for(x = 0; x < 8; x++)
    {
      px = xoff + x;
      py = yoff + y;
      chr_lightness = cdata[y] & (128 >> x) ? 1 : 0;
      img_lightness = img->pixels[((py * img->w) + px)] ? 1 : 0;
      tdist += abs(chr_lightness - img_lightness);
    }
  }
  return tdist;
}

static void RemapMzm(Charset *cset, Mzm *mzm, Image *qimage)
{
  // Remap the MZM to the charset to create the closest approximation of the quantised image.
  int x, y, c;
  for (y = 0; y < mzm->h; y++) {
    for (x = 0; x < mzm->w; x++) {
      int closest_chr = -1;
      int closest_dist = INT_MAX;

      for (c = 0; c < cset->chars; c++) {
        int chr_dist = ChrImageDist(cset->data + c * 14, qimage, x * 8, y * 14);
        if (chr_dist < closest_dist) {
          closest_dist = chr_dist;
          closest_chr = c;
        }
      }
      mzm->data[y * mzm->w + x] = closest_chr;
    }
  }
}

static void Reduce(Charset *cset, Mzm *mzm, Image *original, int chars, Config *cfg)
{
  if(cset->chars <= chars)
    return;

  Reduce_Simple(cset, chars);
  RemapMzm(cset, mzm, original);
}

static void OffsetMzm(Mzm *mzm, int offset)
{
  int i;
  if(mzm)
  {
    for(i = 0; i < mzm->w * mzm->h; i++)
      mzm->data[i] += offset;
  }
}

static void ExcludeChars(Charset *cset, Mzm *mzm, char *exclude, int offset)
{
  int remap[256];
  int remap_offset = 0;
  int ci, i;
  for(i = 0; i < cset->chars && i < 256; i++)
  {
    remap[i] = i;
    ci = i + remap_offset + offset;
    if((ci >= 0) && (ci < 256) && (exclude[ci]))
    {
      cset->data = realloc(cset->data, (cset->chars + 1) * 14);
      remap[i] = cset->chars;
      memmove(cset->data + (remap[i])*14, cset->data + i * 14, 14);
      memset(cset->data + i * 14, 0, 14);

      cset->chars++;
    }
  }

  if(mzm)
  {
    for(i = 0; i < mzm->w * mzm->h; i++)
    {
      while((mzm->data[i] != remap[mzm->data[i]]))
        mzm->data[i] = remap[mzm->data[i]];
    }
  }
}

static void SwapBlank(Charset *cset, Mzm *mzm, int blank)
{
  int current_blank = -1;
  int i, j, isblank;
  unsigned char tmp[14];
  for(i = 0; i < cset->chars; i++)
  {
    isblank = 1;
    for(j = 0; j < 14; j++)
    {
      if(cset->data[i * 14 + j] != 0)
      {
        isblank = 0;
        break;
      }
    }

    if(isblank)
      current_blank = i;
    break;
  }
  if(current_blank != -1)
  {
    memcpy(tmp, cset->data + current_blank*14, 14);
    memcpy(cset->data + current_blank*14, cset->data + blank*14, 14);
    memcpy(cset->data + blank*14, tmp, 14);

    if(mzm)
    {
      for(i = 0; i < mzm->w * mzm->h; i++)
      {
        if(mzm->data[i] == current_blank)
        {
          mzm->data[i] = blank;
        }
        else

        if(mzm->data[i] == blank)
        {
          mzm->data[i] = current_blank;
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  Config *cfg = LoadConfig(argc - 1, argv + 1);
  InputFile *file;
  Image *qimage;
  Charset *cset;
  Mzm *mzm;

#ifdef CONFIG_PLEDGE_UTILS
  // TODO unveil
  if(pledge(PROMISES, ""))
  {
    Error("Failed pledge!");
    return 1;
  }
#endif

  for(file = cfg->files; file != NULL; file = file->hh.next)
  {
    struct image_file image;
    LoadImage(&image, cfg, file->path);

    if(((image.width % 8) != 0) || ((image.height % 14) != 0))
      Error("Image dimensions are not divisible by 8x14");

    qimage = Quantise(cfg, &image);

    cset = CreateCharset(qimage);

    mzm = NULL;
    if(cfg->mzm)
      mzm = CreateMzm(qimage->w / 8, qimage->h / 14);

    if(cfg->reuse && !cfg->noreuse)
      Reuse(cset, mzm);

    if(cfg->c)
      Reduce(cset, mzm, qimage, cfg->c, cfg);

    if(cfg->blank != -1)
      SwapBlank(cset, mzm, cfg->blank);

    if(cfg->exclude_on)
      ExcludeChars(cset, mzm, cfg->exclude, cfg->offset);

    if(cfg->offset)
      OffsetMzm(mzm, cfg->offset);

    image_free(&image);
    FreeImage(qimage);

    { // Output charset
      char *filename_chr = OutputPath(file->path, cfg, ".chr");

      WriteCharset(filename_chr, cset);

      if (!cfg->quiet) {
        const char *in_name = strcmp(file->path, "-") ? file->path : "<stdin>";
        printf("%s -> %s (%d chars)", in_name, filename_chr, cset->chars);
      }
      free(filename_chr);
    }

    if(cfg->mzm)
    { // Output MZM
      char *filename_mzm = OutputPath(file->path, cfg, ".mzm");
      WriteMzm(filename_mzm, mzm);
      FreeMzm(mzm);

      if(!cfg->quiet)
        printf(", %s", filename_mzm);

      free(filename_mzm);
    }

    if(!cfg->quiet)
      printf("\n");
  }

  return 0;
}

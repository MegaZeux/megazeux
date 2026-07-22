/* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../compat.h"

#include "smzxconv.h"
#include "utils_alloc.h"
#include "../io/image_file.h"

#ifdef CONFIG_PLEDGE_UTILS
#include <unistd.h>
#define PROMISES "stdio rpath wpath cpath"
#endif

#ifndef MAX_PATH
#define MAX_PATH 512
#endif

int main(int argc, char **argv)
{
  FILE *fp;
  unsigned char mzmhead[16] =
  {
    'M', 'Z', 'M', '2', /* Magic */
    0, 0, /* Width */
    0, 0, /* Height */
    0, 0, 0, 0, 0, 1, 0, 0 /* Stuff */
  };

  char input_file_name[MAX_PATH] = { '\0' };
  char output_mzm_name[MAX_PATH] = { '\0' };
  char output_chr_name[MAX_PATH] = { '\0' };
  char output_pal_name[MAX_PATH] = { '\0' };
  char output_base_name[MAX_PATH] = { '\0' };
  const char *ext;

  int skip_char = -1;

  struct image_file img;
  enum image_error ret;
  uint32_t i, t;
  mzx_tile *tile;
  mzx_glyph chr[256];
  mzx_color pal[256];
  smzx_converter *c;

  if(argc < 2 || (argv[1][0] == '-' && argv[1][1] != '\0'))
  {
    fprintf(stderr,
"png2smzx Image Conversion Utility\n\n"

"Usage: %s <in.png|-> [<out> | <out.mzm> "
"[<out.chr] [<out.pal>]] [options]\n\n"

"  If the input file is '-', it will be read from stdin.\n\n"

"  Supported file formats are:\n\n"

"  * PNG\n"
"  * GIF (multi-image GIFs will be flattened into a single image;\n"
"         non-square pixel aspect ratios are supported via upscaling)\n"
"  * BMP (1bpp, 2bpp, 4bpp, 8bpp, 16bpp, 24bpp, 32bpp, RLE8, RLE4, bitfields)\n"
"  * TGA (all)\n"
"  * Netpbm/PNM (.pbm, .pgm, .ppm, .pnm, .pam)\n"
"  * farbfeld (.ff)\n"
"\n"
"Options:\n\n"

"  --skip-char=[value 0-255]	Skip this char in the conversion process.\n"
"\n",

      argv[0]
    );
    return 1;
  }

  snprintf(input_file_name, MAX_PATH, "%s", argv[1]);
  input_file_name[MAX_PATH - 1] = '\0';

  // Read the input files
  for(i = 2; i < (unsigned int)argc; i++)
  {
    if(strlen(argv[i]) >= 4)
    {
      ext = argv[i] + strlen(argv[i]) - 4;

      if(!strcasecmp(ext, ".mzm"))
        snprintf(output_mzm_name, MAX_PATH, "%s", argv[i]);
      if(!strcasecmp(ext, ".chr"))
        snprintf(output_chr_name, MAX_PATH, "%s", argv[i]);
      if(!strcasecmp(ext, ".pal"))
        snprintf(output_pal_name, MAX_PATH, "%s", argv[i]);
    }

    if(!strncasecmp(argv[i], "--skip-char", 11) &&
     (argv[i][11] == '=') && argv[i][12])
    {
      skip_char = strtol(argv[i] + 12, NULL, 10) % 256;
      fprintf(stderr, "Skipping char %i.", skip_char);
    }
  }
  output_mzm_name[MAX_PATH - 1] = '\0';
  output_chr_name[MAX_PATH - 1] = '\0';
  output_pal_name[MAX_PATH - 1] = '\0';

  // Fill in the missing filenames
  if(output_mzm_name[0])
  {
    size_t base_name_len = strlen(output_mzm_name) - 4;

    snprintf(output_base_name, MAX_PATH, "%s", output_mzm_name);
    output_base_name[base_name_len] = '\0';
  }
  else
  {
    const char *src = input_file_name;
    if(!strcmp(input_file_name, "-"))
      src = "out";

    if(argc >= 3 && argv[2] && argv[2][0] != '-')
      src = argv[2];

    snprintf(output_base_name, MAX_PATH - 5, "%s", src);
    output_base_name[MAX_PATH - 6] = '\0';

    snprintf(output_mzm_name, MAX_PATH, "%s.mzm", output_base_name);
    output_pal_name[0] = '\0';
    output_chr_name[0] = '\0';
  }

  if(!output_chr_name[0])
  {
    snprintf(output_chr_name, MAX_PATH, "%s.chr", output_base_name);
  }

  if(!output_pal_name[0])
  {
    snprintf(output_pal_name, MAX_PATH, "%s.pal", output_base_name);
  }

#ifdef CONFIG_PLEDGE_UTILS
#ifdef PLEDGE_HAS_UNVEIL
  if(unveil(input_file_name, "r") || unveil(output_mzm_name, "cw") ||
   unveil(output_chr_name, "cw") || unveil(output_pal_name, "cw") ||
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

  // Do stuff
  ret = load_image_from_file(input_file_name, &img, NULL);
  if(ret)
  {
    fprintf(stderr, "Error reading image: %s\n", image_error_string(ret));
    return 1;
  }

  if((img.width % 8) || (img.height % 14))
  {
    fprintf(stderr, "Image not divisible by 8x14; cropping...\n");
    if(img.width % 8)
    {
      t = img.width / 8 * 8;
      for(i = 1; i < img.height; i++)
        memmove(img.data + i * t, img.data + i * img.width, sizeof(struct rgba_color) * t);
    }
  }
  img.width /= 8;
  img.height /= 14;

  c = smzx_convert_init(img.width, img.height, 0, skip_char, 256, 0, 16);
  if(!c)
  {
    fprintf(stderr, "Error initializing converter.\n");
    image_free(&img);
    return 1;
  }

  tile = malloc(sizeof(mzx_tile) * img.width * img.height);
  if(!tile)
  {
    fprintf(stderr, "Error allocating tile buffer.\n");
    smzx_convert_free(c);
    image_free(&img);
    return 1;
  }

  smzx_convert(c, img.data, tile, chr, pal);
  smzx_convert_free(c);
  image_free(&img);
  mzmhead[4] = img.width & 0xFF;
  mzmhead[5] = img.width >> 8;
  mzmhead[6] = img.height & 0xFF;
  mzmhead[7] = img.height >> 8;

  fp = fopen_unsafe(output_mzm_name, "wb");
  if(!fp)
  {
    fprintf(stderr, "Error opening MZM file.\n");
    free(tile);
    return 1;
  }

  if(fwrite(mzmhead, sizeof(mzmhead), 1, fp) != 1)
  {
    fprintf(stderr, "Error writing MZM header.\n");
    fclose(fp);
    free(tile);
    return 1;
  }

  if(fwrite(tile, sizeof(mzx_tile) * img.width * img.height, 1, fp) != 1)
  {
    fprintf(stderr, "Error writing MZM data.\n");
    fclose(fp);
    free(tile);
    return 1;
  }
  free(tile);
  fclose(fp);

  fp = fopen_unsafe(output_chr_name, "wb");
  if(!fp)
  {
    fprintf(stderr, "Error opening CHR file.\n");
    return 1;
  }

  if(fwrite(chr, sizeof(chr), 1, fp) != 1)
  {
    fprintf(stderr, "Error writing CHR data.\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  fp = fopen_unsafe(output_pal_name, "wb");
  if(!fp)
  {
    fprintf(stderr, "Error opening PAL file.\n");
    return 1;
  }

  if(fwrite(pal, sizeof(pal), 1, fp) != 1)
  {
    fprintf(stderr, "Error writing PAL data.\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  return 0;
}


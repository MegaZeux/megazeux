/* MegaZeux
 *
 * Copyright (C) 2022 Alice Rowan <petrifiedrowan@gmail.com>
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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image_common.h"
#include "image_gif.h"

#if 0
#define gifdbg(...) imagedbg("GIF: " __VA_ARGS__)
#else
#define gifdbg imagenodbg
#endif

#if 0
#define giflzwdbg(...) imagedbg("GIF: " __VA_ARGS__)
#else
#define giflzwdbg imagenodbg
#endif

static inline uint16_t gif_u16(const void *_buf)
{
  const uint8_t *buf = (const uint8_t *)_buf;
  return buf[0] | (buf[1] << 8);
}

static inline uint32_t gif_u32(const void *_buf)
{
  const uint8_t *buf = (const uint8_t *)_buf;
  return buf[0] | (buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

const char *gif_error_string(enum gif_error err)
{
  switch(err)
  {
    case GIF_OK:
      return "ok";
    case GIF_IO:
      return "IO error";
    case GIF_MEM:
      return "out of memory";
    case GIF_INVALID:
      return "invalid stream";
    case GIF_SIGNATURE:
      return "not a GIF";
  }
  return "unknown";
}


struct gif_lzw_node
{
#define GIF_MAX_CODES 0x1000
#define GIF_NO_CODE 0x1000
  uint16_t len;
  uint16_t prev;
  uint8_t chr;
};

struct gif_bitstream
{
  uint8_t *in;
  unsigned in_left;
  unsigned bits_left;
  size_t bits;
};

static gif_bool gif_fill_buffer(struct gif_bitstream *b)
{
  unsigned bytes_free = (sizeof(size_t) > 4 ? 64 - b->bits_left : 32 - b->bits_left) >> 3;
  unsigned to_read = IMAGE_MIN(bytes_free, b->in_left);

  if(to_read >= 4 && sizeof(size_t) > 4) /* rarely useful for 32-bit buffers */
  {
    b->bits |= (size_t)gif_u32(b->in) << b->bits_left;
    b->bits_left += 32;
    b->in_left -= 4;
    b->in += 4;
    return GIF_TRUE;
  }
  if(to_read >= 2)
  {
    b->bits |= (size_t)gif_u16(b->in) << b->bits_left;
    b->bits_left += 16;
    b->in_left -= 2;
    b->in += 2;
    return GIF_TRUE;
  }
  if(to_read >= 1)
  {
    b->bits |= (size_t)*(b->in) << b->bits_left;
    b->bits_left += 8;
    b->in_left--;
    b->in++;
    return GIF_TRUE;
  }
  return GIF_FALSE;
}

static uint16_t gif_read_code(struct gif_bitstream *b, unsigned code_len)
{
  uint16_t code;
  if(b->bits_left < code_len)
    if(!gif_fill_buffer(b) || b->bits_left < code_len)
      return GIF_NO_CODE;

  code = b->bits & ((1u << code_len) - 1);
  b->bits >>= code_len;
  b->bits_left -= code_len;
  return code;
}

/**
 * Unpack a GIF image. GIF images are stored using a LZW variant split into
 * blocks of up to 255 bytes. The minimum allowed bit width is 3 and the
 * maximum allowed bit width is 12. See GIF89a Appendix F.
 */
static enum gif_error gif_decode(uint8_t *out, struct gif_lzw_node **_tree,
 struct gif_image *layer, void *handle, const gif_read_function readfn)
{
  struct gif_lzw_node *tree = *_tree;
  struct gif_lzw_node *current;
  struct gif_bitstream b;
  uint8_t *out_end = out + layer->width * layer->height;
  uint8_t *pos;
  uint8_t buf[255];
  uint8_t size;
  uint8_t first_chr = 0;
  uint8_t code_len_init;
  unsigned prev_code = GIF_NO_CODE;
  unsigned next_code;
  unsigned clear_code;
  unsigned end_code;
  unsigned code_len;
  unsigned i;

  if(!tree)
  {
    *_tree = tree = (struct gif_lzw_node *)calloc(1<<12, sizeof(struct gif_lzw_node));
    if(!tree)
      return GIF_MEM;
  }
  else
    memset(tree, 0, (1<<12) * sizeof(struct gif_lzw_node));

  if(readfn(&code_len_init, 1, handle) < 1)
    return GIF_IO;

  code_len_init++;
  if(code_len_init < 2 || code_len_init > 12)
    return GIF_INVALID;

  code_len = code_len_init;
  for(i = 0; i < (1u << (code_len - 1)); i++)
  {
    tree[i].prev = GIF_NO_CODE;
    tree[i].len = 1;
    tree[i].chr = i;
  }
  clear_code = i;
  end_code = i + 1;
  next_code = i + 2;
  for(; i < GIF_MAX_CODES; i++)
    tree[i].prev = GIF_NO_CODE;

  b.bits_left = 0;
  b.bits = 0;

  giflzwdbg("  Depack\t\t\t code_len:%d\n", code_len);

  while(1)
  {
    if(readfn(&size, 1, handle) < 1)
      return GIF_IO;
    giflzwdbg("  BLOCK size:%d\n", size);
    if(size == 0)
      break;

    if(readfn(buf, size, handle) < size)
      return GIF_IO;

    b.in = buf;
    b.in_left = size;

    while(1)
    {
      uint16_t code = gif_read_code(&b, code_len);
      int kwkwk = 0;
      giflzwdbg("    %d\n", code);
      if(code >= GIF_MAX_CODES || code > next_code || code == end_code)
        break;

      /* NOTE: clear is supposed to reset the entire table, so no encoder
       * should rely on stale codes after a reset and the current length
       * optimization should be safe. If a file that uses these is found
       * this should be revised. */
      if(code == clear_code)
      {
        code_len = code_len_init;
        next_code = end_code + 1;
        prev_code = GIF_NO_CODE;
        first_chr = 0;
        continue;
      }

      /* Add (kwkwk) */
      if(code == next_code && next_code < GIF_MAX_CODES)
      {
        if(prev_code >= GIF_NO_CODE) /* This should never be emitted first. */
          break;

        giflzwdbg("      add:%d prev:%d prevlen:%d (kwkwk)\n",
         next_code, prev_code, tree[prev_code].len);

        current = &tree[next_code++];
        current->prev = prev_code;
        current->len = tree[prev_code].len + 1;
        current->chr = first_chr;
        kwkwk = 1;
      }

      /* Emit */
      current = &tree[code];
      out += current->len;
      pos = out;
      giflzwdbg("      len:%d\n", current->len);
      if(out > out_end)
      {
        giflzwdbg("    early end of buffer\n");
        return GIF_INVALID;
      }

      while(1)
      {
        *(--pos) = first_chr = current->chr;
        if(current->prev >= GIF_NO_CODE)
          break;
        current = &tree[current->prev];
      }

      /* Add (normal) */
      if(!kwkwk && prev_code < GIF_NO_CODE && next_code < GIF_MAX_CODES)
      {
        giflzwdbg("      add:%d prev:%d prevlen:%d\n",
         next_code, prev_code, tree[prev_code].len);

        current = &tree[next_code++];
        current->prev = prev_code;
        current->len = tree[prev_code].len + 1;
        current->chr = first_chr;
      }

      /* Expand after adding */
      if(next_code >= (1u << code_len))
      {
        if(code_len < 12)
        {
          code_len++;
          giflzwdbg("    code len is now: %d\n", code_len);
        }
        else
          giflzwdbg("    out of codes\n");
      }

      prev_code = code;
    }

    if(b.in_left)
    {
      giflzwdbg("  unexpected end code or invalid code (%u bits left in buffer,"
       "%u bytes left in buffer, %zd left in output)\n",
       b.bits_left, b.in_left, out_end - out);
      return GIF_INVALID;
    }

    giflzwdbg("  bits_left:%u\n", b.bits_left);
  }
  return GIF_OK;
}

/**
 * Deinterlace an image that has been stored interlaced. Rows are stored in the
 * following format (See GIF89a Appendix E):
 *
 * 1) Every 8th row starting from row 0.
 * 2) Every 8th row starting from row 4.
 * 3) Every 4th row starting from row 2.
 * 4) Every 2nd row starting from row 1.
 */
static void gif_deinterlace(uint8_t * RESTRICT out, const uint8_t *in,
 unsigned width, unsigned height)
{
  uint8_t *pos;
  uint8_t *end = out + (width * height);
  unsigned pitch = width * 8;

  for(pos = out; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  for(pos = out + width * 4; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  pitch = width * 4;
  for(pos = out + width * 2; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);

  pitch = width * 2;
  for(pos = out + width; pos < end; pos += pitch, in += width)
    memcpy(pos, in, width);
}

static unsigned gif_next_power_of_two(unsigned sz)
{
  sz |= sz >> 1;
  sz |= sz >> 2;
  sz |= sz >> 4;
  sz |= sz >> 8;
  sz |= sz >> 16;
  return sz + 1;
}

static unsigned gif_ptr_size(unsigned sz)
{
  return sz * sizeof(void *);
}

#define RESIZE(type, ptr, alloc, target, size_fn) \
  do { \
    unsigned tmp = IMAGE_MAX((alloc), (target)); \
    unsigned new_size = gif_next_power_of_two(tmp); \
    unsigned a_size = size_fn(new_size); \
    void *buf; \
    if(new_size < tmp || a_size < new_size) \
      return GIF_MEM; \
    buf = realloc((ptr), a_size); \
    if(!buf) \
      return GIF_MEM; \
    ptr = (type)buf; \
    alloc = new_size; \
  } while(0)

#define RESIZE_LIST(type, ptr, alloc, target) \
 RESIZE(type, ptr, alloc, target, gif_ptr_size)

static struct gif_color_table *gif_alloc_color_table(unsigned num_entries)
{
  size_t sz = num_entries * sizeof(struct gif_color);
  sz += sizeof(struct gif_color_table);

  return (struct gif_color_table *)malloc(sz);
}

static enum gif_error gif_add_block(struct gif_info *gif, unsigned size,
 struct gif_block **_block)
{
  struct gif_block *block;

  if(gif->num_blocks >= gif->num_blocks_alloc)
  {
    RESIZE_LIST(struct gif_block **, gif->blocks, gif->num_blocks_alloc,
     gif->num_blocks + 1);
  }

  block = (struct gif_block *)calloc(1, size);
  if(!block)
    return GIF_MEM;

  gif->blocks[gif->num_blocks++] = block;
  *_block = block;
  return GIF_OK;
}

static enum gif_error gif_add_control(struct gif_info *gif,
 struct gif_graphics_control **_control)
{
  struct gif_block *block;

  if(gif_add_block(gif, sizeof(struct gif_graphics_control), &block) != GIF_OK)
    return GIF_MEM;

  block->type = GIF_CONTROL;
  *_control = (struct gif_graphics_control *)block;
  return GIF_OK;
}

static unsigned gif_add_image_size(unsigned size)
{
  return IMAGE_MAX(sizeof(struct gif_image),
   offsetof(struct gif_image, pixels) + size);
}

static enum gif_error gif_add_image(struct gif_info *gif, unsigned size,
 struct gif_image **_layer)
{
  struct gif_block *block;
  unsigned a_size = gif_add_image_size(size);

  if(a_size < size || gif_add_block(gif, a_size, &block) != GIF_OK)
    return GIF_MEM;

  block->type = GIF_IMAGE;
  *_layer = (struct gif_image *)block;
  return GIF_OK;
}

static unsigned gif_add_plaintext_size(unsigned size)
{
  return IMAGE_MAX(sizeof(struct gif_plaintext),
   offsetof(struct gif_plaintext, text) + size + 1);
}

static enum gif_error gif_add_plaintext(struct gif_info *gif, unsigned size,
 struct gif_plaintext **_layer)
{
  struct gif_block *block;
  unsigned a_size = gif_add_plaintext_size(size);

  if(a_size < size || gif_add_block(gif, a_size, &block) != GIF_OK)
    return GIF_MEM;

  block->type = GIF_PLAINTEXT;
  *_layer = (struct gif_plaintext *)block;
  (*_layer)->length = 0;
  (*_layer)->length_alloc = size;
  (*_layer)->text[0] = '\0';
  return GIF_OK;
}

static enum gif_error gif_add_plaintext_append(struct gif_block **_layer,
 uint8_t *data, uint8_t length)
{
  struct gif_plaintext *layer = (struct gif_plaintext *)*_layer;
  if(layer->length_alloc - layer->length < length)
  {
    RESIZE(struct gif_plaintext *, layer, layer->length_alloc,
     layer->length + length, gif_add_plaintext_size);
    *_layer = (struct gif_block *)layer;
  }
  memcpy(layer->text + layer->length, data, length);
  layer->length += length;
  layer->text[layer->length] = '\0';
  return GIF_OK;
}

static unsigned gif_add_comment_size(unsigned size)
{
  return IMAGE_MAX(sizeof(struct gif_comment),
   offsetof(struct gif_comment, comment) + size + 1);
}

static enum gif_error gif_add_comment(struct gif_info *gif, unsigned size)
{
  struct gif_block *block;
  struct gif_comment *c;
  unsigned a_size = gif_add_comment_size(size);

  if(a_size < size || gif_add_block(gif, a_size, &block) != GIF_OK)
    return GIF_MEM;

  if(!gif->comments_end)
    gif->comments_start = gif->num_blocks - 1;
  gif->comments_end = gif->num_blocks;

  block->type = GIF_COMMENT;
  c = (struct gif_comment *)block;
  c->length = 0;
  c->length_alloc = size;
  c->comment[0] = '\0';
  return GIF_OK;
}

static enum gif_error gif_add_comment_append(struct gif_block **_comment,
 uint8_t *data, uint8_t length)
{
  struct gif_comment *c = (struct gif_comment *)*_comment;
  if(c->length_alloc - c->length < length)
  {
    RESIZE(struct gif_comment *, c, c->length_alloc, c->length + length,
     gif_add_comment_size);
    *_comment = (struct gif_block *)c;
  }
  memcpy(c->comment + c->length, data, length);
  c->length += length;
  c->comment[c->length] = '\0';
  return GIF_OK;
}

static enum gif_error gif_add_application(struct gif_info *gif)
{
  struct gif_block *block;

  if(gif_add_block(gif, sizeof(struct gif_application), &block) != GIF_OK)
    return GIF_MEM;

  if(!gif->application_end)
    gif->application_start = gif->num_blocks - 1;
  gif->application_end = gif->num_blocks;

  block->type = GIF_APPLICATION;
  return GIF_OK;
}

static enum gif_error gif_add_appdata(struct gif_block *_app,
 uint8_t *appdata, uint8_t length)
{
  struct gif_application *app = (struct gif_application *)_app;
  struct gif_appdata *a;

  if(app->length >= app->length_alloc)
  {
    RESIZE_LIST(struct gif_appdata **, app->appdata, app->length_alloc,
     app->length + 1);
  }

  a = (struct gif_appdata *)malloc(sizeof(struct gif_appdata) + length);
  if(!a)
    return GIF_MEM;

  a->length = length;
  memcpy(a->appdata, appdata, length);
  a->appdata[length] = '\0';

  app->appdata[app->length++] = a;
  return GIF_OK;
}

enum gif_error gif_read(struct gif_info *gif, void *handle,
 const gif_read_function readfn, gif_bool skip_signature)
{
  struct gif_lzw_node *unpack_tree = NULL;
  struct gif_color_table *table;
  uint8_t buffer[256];
  enum gif_error ret;
  unsigned current = 0;

  memset(gif, 0, sizeof(*gif));

  /* Header */
  if(!skip_signature)
  {
    if(readfn(gif->signature, 3, handle) < 3)
      return GIF_IO;
    if(readfn(gif->signature, 3, handle) < 3 || memcmp(gif->signature, "GIF", 3))
      return GIF_SIGNATURE;
  }

  if(readfn(gif->version, 3, handle) < 3)
    return GIF_IO;

  if(gif->version[0] < '0' || gif->version[0] > '9' ||
   gif->version[1] < '0' || gif->version[1] > '9' ||
   gif->version[2] < 'a' || gif->version[2] > 'z')
    return GIF_SIGNATURE;

  gifdbg("GIF%3.3s\n", gif->version);

  /* Logical Screen Descriptor */
  if(readfn(buffer, 7, handle) < 7)
    return GIF_IO;

  gif->width          = gif_u16(buffer + 0);
  gif->height         = gif_u16(buffer + 2);
  gif->component_res  = ((buffer[4] & 0x70) >> 4) + 1;
  gif->bg_color       = buffer[5];
  gif->pixel_ratio    = buffer[6];

  gifdbg("  Logical Screen\t\t w:%d h:%d rgbres:%d bgcolor:%d pixelratio:%d\n",
   gif->width, gif->height, gif->component_res, gif->bg_color, gif->pixel_ratio);

  /* Global Color Table */
  if(buffer[4] & 0x80)
  {
    unsigned num_entries = 2 << (buffer[4] & 7);

    table = gif_alloc_color_table(num_entries);
    if(!table)
      return GIF_MEM;

    table->is_sorted = (buffer[4] & 0x08) >> 3;
    table->num_entries = num_entries;
    gif->global_color_table = table;

    gifdbg("  Global Color Table\t sorted:%d entries:%d\n",
     table->is_sorted, table->num_entries);

    if(readfn(table->entries, num_entries * 3, handle) < num_entries * 3)
    {
      ret = GIF_IO;
      goto error;
    }
  }

  /* All other blocks */
  while(1)
  {
    if(readfn(buffer, 1, handle) < 1)
      break;

    switch(buffer[0])
    {
      case 0x21:        /* Extension Block */
      {
        unsigned type;
        unsigned sz;
        if(readfn(buffer, 2, handle) < 1)
        {
          ret = GIF_IO;
          goto error;
        }
        type = buffer[0];
        sz = buffer[1];

        gifdbg("Extension\t\t\t type:%u\n", type);

        for(; sz != 0; sz = buffer[sz])
        {
          if(readfn(buffer, sz + 1, handle) < sz + 1)
          {
            ret = GIF_IO;
            goto error;
          }

          switch(type)
          {
            default:
            {
              gifdbg("  ignoring subblock\t size:%u\n", sz);
              break;
            }

            case 0xF9:  /* Graphics Control Extension */
            {
              struct gif_graphics_control *control;
              if(sz < 4)
                continue;

              ret = gif_add_control(gif, &control);
              if(ret != GIF_OK)
                goto error;

              control->disposal_method    = (buffer[0] & 0x1C) >> 2;
              control->input_required     = (buffer[0] & 0x02) >> 1;
              control->delay_time         = gif_u16(buffer + 1);
              control->transparent_color  = (buffer[0] & 1) ? buffer[3] : -1;

              gifdbg("  Graphics Control\t\t size:%u disp:%d input:%d delay:%d tr:%d\n",
               sz, control->disposal_method, control->input_required,
               control->delay_time, control->transparent_color);

              type += 0x100; // Only read one.
              break;
            }

            case 0xFE:  /* Comment */
            {
              gifdbg("  Comment\t\t\t size:%u\n", sz);
              current = gif->num_blocks;
              if(gif_add_comment(gif, sz) != GIF_OK)
              {
                type = 0; // Skip everything else
                break;
              }
              type += 0x100;
            }
            /* fall-through */

            case 0x1FE: /* Comment (data blocks) */
            {
              if(current >= gif->num_blocks || gif->blocks[current]->type != GIF_COMMENT)
                continue;

              if(gif_add_comment_append(&(gif->blocks[current]), buffer, sz) != GIF_OK)
                type = 0; // Skip everything else

              gifdbg("    text: %*.*s\n", (int)sz, (int)sz, buffer);
              break;
            }

            case 0xFF:  /* Application */
            {
              struct gif_application *app;
              if(sz < 11)
              {
                type = 0; // Skip everything else
                break;
              }
              current = gif->num_blocks;
              if(gif_add_application(gif) != GIF_OK)
              {
                type = 0;
                break;
              }
              app = (struct gif_application *)gif->blocks[current];

              memcpy(app->application, buffer, 11);
              app->application[11] = '\0';

              gifdbg("  Application\t\t size:%u application:%s\n",
               sz, app->application);

              type += 0x100;
              break;
            }

            case 0x1FF: /* Application (data blocks) */
            {
              if(current >= gif->num_blocks || gif->blocks[current]->type != GIF_APPLICATION)
                continue;

              gif_add_appdata(gif->blocks[current], buffer, sz);
              gifdbg("  Application data\t\t size:%u\n", sz);
              break;
            }

            case 0x01:  /* Plain Text */
            {
              struct gif_plaintext *layer;
              if(sz < 12)
              {
                type = 0; // Skip everything else
                continue;
              }
              current = gif->num_blocks;

              ret = gif_add_plaintext(gif, 32, &layer);
              if(ret != GIF_OK)
                continue;

              layer->left         = gif_u16(buffer + 0);
              layer->top          = gif_u16(buffer + 2);
              layer->width        = gif_u16(buffer + 4);
              layer->height       = gif_u16(buffer + 6);
              layer->char_width   = buffer[8];
              layer->char_height  = buffer[9];
              layer->fg_color     = buffer[10];
              layer->bg_color     = buffer[11];

              gifdbg("  Plain text\t\t size:%u width:%u height:%u\n", sz,
               layer->width, layer->height);
              gifdbg("\t\t\t\t left:%d top:%d charw:%d charh:%d fg:%d bg:%d\n",
               layer->left, layer->top, layer->char_width, layer->char_height,
               layer->fg_color, layer->bg_color);
              type += 0x100;
              break;
            }

            case 0x101:  /* Plain Text (data blocks) */
            {
              if(current >= gif->num_blocks || gif->blocks[current]->type != GIF_PLAINTEXT)
                continue;

              gif_add_plaintext_append(&(gif->blocks[current]), buffer, sz);
              gifdbg("  Plain text data\t\t size:%u\n", sz);
              break;
            }
          }
        }
        break;
      }

      case 0x2C:        /* Image Descriptor */
      {
        struct gif_image *layer;
        uint8_t *dest;
        unsigned width;
        unsigned height;

        if(readfn(buffer, 9, handle) < 9)
        {
          ret = GIF_IO;
          goto error;
        }

        width = gif_u16(buffer + 4);
        height = gif_u16(buffer + 6);

        gifdbg("Image Descriptor\t\t width:%u height:%u\n", width, height);

        ret = gif_add_image(gif, width * height, &layer);
        if(ret != GIF_OK)
          goto error;

        layer->left           = gif_u16(buffer + 0);
        layer->top            = gif_u16(buffer + 2);
        layer->width          = width;
        layer->height         = height;
        layer->is_interlaced  = (buffer[8] & 0x40) >> 6;

        gifdbg("\t\t\t\t left:%d top:%d interlaced:%d\n",
         layer->left, layer->top, layer->is_interlaced);

        /* Local Color Table */
        if(buffer[8] & 0x80)
        {
          unsigned num_entries = 2 << (buffer[8] & 0x07);
          table = gif_alloc_color_table(num_entries);
          if(!table)
          {
            ret = GIF_MEM;
            goto error;
          }
          layer->color_table = table;

          table->is_sorted = (buffer[8] & 0x20) >> 5;
          table->num_entries = num_entries;

          gifdbg("  Local Color Table\t sorted:%d entries:%d\n",
           table->is_sorted, table->num_entries);

          if(readfn(table->entries, num_entries * 3, handle) < num_entries * 3)
          {
            ret = GIF_IO;
            goto error;
          }
        }

        if(layer->is_interlaced)
        {
          dest = (uint8_t *)calloc(1, width * height);
          if(!dest)
          {
            ret = GIF_MEM;
            goto error;
          }
        }
        else
          dest = layer->pixels;

        ret = gif_decode(dest, &unpack_tree, layer, handle, readfn);
        if(ret != GIF_OK)
        {
          if(layer->is_interlaced)
            free(dest);
          goto error;
        }

        if(layer->is_interlaced)
        {
          gif_deinterlace(layer->pixels, dest, width, height);
          free(dest);
        }
        break;
      }

      case 0x3B:        /* Trailer */
        gifdbg("End of GIF\n\n");
        goto end_of_stream;
    }
  }
end_of_stream:
  gifdbg("  Comments start:%u end:%u\n", gif->comments_start, gif->comments_end);
  gifdbg("  Application start:%u end:%u\n", gif->application_start, gif->application_end);
  free(unpack_tree);
  return GIF_OK;

error:
  free(unpack_tree);
  gif_close(gif);
  return ret;
}


void gif_close(struct gif_info *gif)
{
  unsigned i, j;

  free(gif->global_color_table);
  gif->global_color_table = NULL;

  if(gif->blocks)
  {
    for(i = 0; i < gif->num_blocks; i++)
    {
      struct gif_block *block = gif->blocks[i];
      if(block)
      {
        if(block->type == GIF_APPLICATION)
        {
          struct gif_application *app = (struct gif_application *)block;
          if(app->appdata)
          {
            for(j = 0; j < app->length; j++)
              free(app->appdata[j]);
            free(app->appdata);
          }
        }

        if(block->type == GIF_IMAGE)
          free(((struct gif_image *)block)->color_table);
      }
      free(block);
    }
    free(gif->blocks);
    gif->blocks = NULL;
  }
}


#ifndef GIF_NO_COMPOSITOR

/***********************************************/
/* Multi-image GIF compositor for GrafX2 GIFs. */
/***********************************************/

#define FP_SHIFT    13
#define FP_1        (1 << FP_SHIFT)
#define FP_AND      ((FP_1) - 1)
#define FP_CEIL(x)  (((x) + FP_AND) & ~FP_AND)

static const struct gif_graphics_control default_control =
{
  { GIF_CONTROL },    /* base */
  1,                  /* do not dispose */
  0,                  /* no user input */
  0,                  /* no delay */
  -1                  /* no transparent color */
};

struct gif_composite
{
  struct gif_rgba *pixels;
  unsigned width;
  unsigned height;
  unsigned scale;
  uint32_t scalex;
  uint32_t scaley;
};

static void gif_composite_scale(uint32_t *x, uint32_t *y, const struct gif_info *gif)
{
  if(gif->pixel_ratio < (64 - 15))
  {
    *x = FP_1;
    *y = FP_1 * 64 / (gif->pixel_ratio + 15);
  }
  else
  {
    *x = FP_1 * (gif->pixel_ratio + 15) / 64;
    *y = FP_1;
  }
}

void gif_composite_size(unsigned *w, unsigned *h, const struct gif_info *gif)
{
  if(gif->pixel_ratio)
  {
    uint32_t x, y;
    gif_composite_scale(&x, &y, gif);
    *w = FP_CEIL((uint32_t)gif->width * x) >> FP_SHIFT;
    *h = FP_CEIL((uint32_t)gif->height * y) >> FP_SHIFT;
  }
  else
  {
    *w = gif->width;
    *h = gif->height;
  }
}

static enum gif_error gif_composite_init(struct gif_composite * RESTRICT image,
 const struct gif_info *gif, const gif_alloc_function allocfn)
{
  uint64_t size;
  memset(image, 0, sizeof(struct gif_composite));

  if(gif->pixel_ratio)
  {
    image->scale = 1;
    gif_composite_scale(&image->scalex, &image->scaley, gif);
    gif_composite_size(&image->width, &image->height, gif);
  }
  else
  {
    image->width = gif->width;
    image->height = gif->height;
  }

  size = (uint64_t)image->width * (uint64_t)image->height;
  if(size > SIZE_MAX || size * sizeof(struct gif_rgba) > SIZE_MAX)
    return GIF_MEM;

  image->pixels = (struct gif_rgba *)allocfn(size * sizeof(struct gif_rgba));
  if(!image->pixels)
    return GIF_MEM;

  return GIF_OK;
}

static void gif_composite_palette(struct gif_rgba palette[256],
 const struct gif_color_table *table)
{
  const struct gif_color *in;
  unsigned count;
  unsigned i;

  memset(palette, 0, sizeof(struct gif_rgba) * 256);
  if(!table)
    return;

  in = table->entries;
  count = IMAGE_MIN(table->num_entries, 256);

  for(i = 0; i < count; i++)
  {
    palette->r = in->r;
    palette->g = in->g;
    palette->b = in->b;
    palette->a = 255;

    palette++;
    in++;
  }
}

static void gif_composite_line(struct gif_rgba * RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length)
{
  size_t x;
  for(x = 0; x < length; x++)
    *(dest++) = palette[line[x]];
}

static void gif_composite_line_tr(struct gif_rgba * RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 unsigned tcol)
{
  size_t x;
  for(x = 0; x < length; x++)
  {
    if(line[x] != tcol)
      *dest = palette[line[x]];
    dest++;
  }
}

static void gif_composite_line_sc(struct gif_rgba * RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 unsigned left_offset, uint32_t scalex)
{
  // Align the FP counters to the starting position to avoid overflow.
  uint32_t scale_stop = scalex * left_offset;
  uint32_t scale_pos = FP_CEIL(scale_stop);
  size_t x;

  for(x = 0; x < length; x++)
  {
    struct gif_rgba col = palette[line[x]];
    for(scale_stop += scalex; scale_pos < scale_stop; scale_pos += FP_1)
      *(dest++) = col;
  }
}

static void gif_composite_line_sc_tr(struct gif_rgba * RESTRICT dest,
 const struct gif_rgba *palette, const uint8_t *line, size_t length,
 unsigned left_offset, uint32_t scalex, unsigned tcol)
{
  // Align the FP counters to the starting position to avoid overflow.
  uint32_t scale_stop = scalex * left_offset;
  uint32_t scale_pos = FP_CEIL(scale_stop);
  size_t x;

  for(x = 0; x < length; x++)
  {
    struct gif_rgba col = palette[line[x]];
    for(scale_stop += scalex; scale_pos < scale_stop; scale_pos += FP_1)
    {
      if(line[x] != tcol)
        *dest = col;
      dest++;
    }
  }
}

static void gif_composite_image(struct gif_composite * RESTRICT image,
 const struct gif_rgba *palette, const struct gif_info *gif,
 const struct gif_image *layer, const struct gif_graphics_control *control)
{
  const unsigned left = IMAGE_MIN(gif->width, layer->left);
  const unsigned top = IMAGE_MIN(gif->height, layer->top);
  const unsigned width = IMAGE_MIN(gif->width - left, layer->width);
  const unsigned height = IMAGE_MIN(gif->height - top, layer->height);
  const unsigned layer_pitch = layer->width;

  const unsigned output_left = image->scale ? FP_CEIL(left * image->scalex) >> FP_SHIFT : left;
  const unsigned output_top = image->scale ? FP_CEIL(top * image->scaley) >> FP_SHIFT : top;

  struct gif_rgba *pixels = image->pixels + output_left + (image->width * output_top);
  const uint8_t *line = layer->pixels;

  const int scale = image->scale;
  const int tcol = control->transparent_color;

  unsigned y;

  if(!width || !height)
    return;

  if(scale)
  {
    // Align the FP counters to the starting position to avoid overflow.
    uint32_t scale_stop = image->scaley * top;
    uint32_t scale_pos = FP_CEIL(scale_stop);

    for(y = 0; y < height; y++)
    {
      // Draw each row multiple times :( it sucks, oh well.
      for(scale_stop += image->scaley; scale_pos < scale_stop; scale_pos += FP_1)
      {
        if(tcol >= 0)
          gif_composite_line_sc_tr(pixels, palette, line, width, left, image->scalex, tcol);
        else
          gif_composite_line_sc(pixels, palette, line, width, left, image->scalex);

        pixels += image->width;
      }
      line += layer_pitch;
    }
  }
  else
  {
    for(y = 0; y < height; y++)
    {
      if(tcol >= 0)
        gif_composite_line_tr(pixels, palette, line, width, tcol);
      else
        gif_composite_line(pixels, palette, line, width);

      pixels += image->width;
      line += layer_pitch;
    }
  }
}

static void gif_composite_background(struct gif_composite * RESTRICT image,
 const struct gif_rgba *palette, const struct gif_info *gif)
{
  struct gif_rgba *dest = image->pixels;
  struct gif_rgba col;
  unsigned i;
  unsigned j;

  if(!gif->global_color_table)
    return;

  col = palette[gif->bg_color];

  for(i = 0; i < image->height; i++)
    for(j = 0; j < image->width; j++)
      *(dest++) = col;
}

enum gif_error gif_composite(struct gif_rgba **_pixels,
 const struct gif_info *gif, const gif_alloc_function allocfn)
{
  struct gif_composite image;
  struct gif_rgba global_palette[256];
  struct gif_rgba layer_palette[256];
  const struct gif_graphics_control *control = &default_control;
  unsigned i;

  enum gif_error err = gif_composite_init(&image, gif, allocfn);
  if(err != GIF_OK)
    return err;

  *_pixels = image.pixels;

  gif_composite_palette(global_palette, gif->global_color_table);
  gif_composite_background(&image, global_palette, gif);

  if(!gif->blocks)
    return GIF_OK;

  for(i = 0; i < gif->num_blocks; i++)
  {
    const struct gif_block *block = gif->blocks[i];
    struct gif_rgba *palette = global_palette;

    if(!block)
      continue;

    if(block->type == GIF_CONTROL)
    {
      control = (const struct gif_graphics_control *)block;
      continue;
    }
    else

    if(block->type == GIF_IMAGE)
    {
      const struct gif_image *layer = (const struct gif_image *)block;

      if(layer->color_table)
      {
        gif_composite_palette(layer_palette, layer->color_table);
        palette = layer_palette;
      }
      gif_composite_image(&image, palette, gif, layer, control);
    }

    control = &default_control;
  }
  return GIF_OK;
}

#endif /* !GIF_NO_COMPOSITOR */

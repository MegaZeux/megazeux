/* MegaZeux
 *
 * Copyright (C) 2021-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#include "error.h"
#include "extmem.h"
#include "robot.h"
#include "util.h"
#include "world_struct.h"
#include "platform/platform_endian.h"

#include <limits.h>
#include <stdint.h>
#include <zlib.h>

#ifdef CONFIG_NDS
#include "platform/nds/extmem_nds.h"
#define USE_PLATFORM_EXTRAM_ALLOC
#define EXTRAM_BUFFER_SIZE
#define EXTRAM_BUFFER_DECL
// Use a static buffer in fast RAM instead of the default stack buffer.
// The benefits of this are questionable but at least it isn't on the stack.
DTCM_BSS static uint32_t extram_deflate_buffer[4096 / sizeof(uint32_t)];
#endif

/**
 * NOTE: platform allocation and platform store/retrieve are mutually exclusive.
 * Only implement at most one variant for a particular platform and use the
 * dummy implementations for the other.
 */
#if defined(USE_PLATFORM_EXTRAM_ALLOC) && defined(USE_PLATFORM_EXTRAM_STORE)
#error Only implement one of platform_extram_alloc and platform_extram_store!
#endif

#ifndef USE_PLATFORM_EXTRAM_ALLOC
/* Acquire permissions to read from/write to platform extra RAM. */
static void platform_extram_lock(void) {}
/* Release permissions to read from/write to platform extra RAM. */
static void platform_extram_unlock(void) {}
/* Attempt to allocate a memory mapped buffer in extra RAM. */
static uint32_t *platform_extram_alloc(size_t len) { return NULL; }
/* Attempt to reallocate a memory mapped buffer in extra RAM. */
static uint32_t *platform_extram_resize(void *buffer, size_t len) { return NULL; }
/* Free a memory mapped buffer in extra RAM. */
static void platform_extram_free(void *buffer) {}
#endif

#ifndef USE_PLATFORM_EXTRAM_STORE
/* Send a locally allocated buffer to non-mapped extra RAM. */
static void *platform_extram_store(void *buffer, size_t len) { return buffer; }
/* Retrieve a buffer from non-mapped extra RAM. */
static void *platform_extram_retrieve(void *buffer, size_t len) { return buffer; }
#endif

#define EXTRAM_ID ((uint32_t)(('E') | ('X' << 8) | ('T' << 16) | (0xfe << 24)))

#ifndef EXTRAM_COMPRESS_BOARDS_THRESHOLD
/* Minimum size (in bytes) for board planes to be deflated. Blocks less than
 * this don't save much RAM in the long run and mostly slow down loading. */
#define EXTRAM_COMPRESS_BOARDS_THRESHOLD 1024
#endif

#ifndef EXTRAM_COMPRESS_ROBOTS_THRESHOLD
/* Minimum size (in bytes) for robots to be deflated.
 * Robots don't compress as well as boards and aren't worth as much effort. */
#define EXTRAM_COMPRESS_ROBOTS_THRESHOLD 16384
#endif

#ifndef EXTRAM_BUFFER_SIZE
/* Size (in uint32_t) of extra memory deflate buffer. */
#define EXTRAM_BUFFER_SIZE (4096 / sizeof(uint32_t))
#endif

#ifndef EXTRAM_BUFFER_DECL
#define EXTRAM_BUFFER_DECL uint32_t extram_deflate_buffer[EXTRAM_BUFFER_SIZE]
#endif

enum extram_flags
{
  EXTRAM_PLATFORM_ALLOC   = (1 << 0), /* Buffer created by platform_extram. */
  EXTRAM_DEFLATE          = (1 << 1), /* Buffer uses DEFLATE compression. */
  EXTRAM_PAGED            = (1 << 2), /* Buffer was paged to disk. (TODO?) */
  EXTRAM_RLE3             = (1 << 3), /* Buffer uses MZX RLE3 compression. */
};

struct extram_block
{
  uint32_t id;
  uint32_t flags;
  uint32_t uncompressed_size;
  uint32_t compressed_size;
  uint32_t checksum;
  uint32_t data[];
};

struct extram_data
{
  z_stream z;
  size_t compression_threshold;
  int status;
  boolean initialized;
  boolean free_data;
};

#ifdef EXTRAM_STATS
struct method_stats
{
  size_t total;
  size_t total_compressed;
  size_t total_uncompressed;
  double best_ratio;
  double worst_ratio;
};

static struct method_stats deflate_stats;
static struct method_stats rle3_stats;

static void tick_method_stats(struct method_stats *stats, size_t compressed,
 size_t uncompressed)
{
  double ratio = (double)compressed / (double)uncompressed;

  if(!stats->total)
    stats->best_ratio = INFINITY;
  stats->total++;
  stats->total_compressed += compressed;
  stats->total_uncompressed += uncompressed;
  if(ratio < stats->best_ratio)
    stats->best_ratio = ratio;
  if(ratio > stats->worst_ratio)
    stats->worst_ratio = ratio;
}

static void tick_stats(struct extram_block *block)
{
  if(block->flags & EXTRAM_RLE3)
  {
    tick_method_stats(&rle3_stats, block->compressed_size, block->uncompressed_size);
  }
  else

  if(block->flags & EXTRAM_DEFLATE)
    tick_method_stats(&deflate_stats, block->compressed_size, block->uncompressed_size);
}

static void print_method_stats(struct method_stats *stats)
{
  double avg = (double)stats->total_compressed / (double)stats->total_uncompressed;
  trace("--EXTRAM--     TOTAL:%zu  IN:%zu  OUT:%zu  AVG:%.5f  BEST:%.5f  WORST:%.5f\n",
   stats->total, stats->total_uncompressed, stats->total_compressed, avg,
   stats->best_ratio, stats->worst_ratio);
}

static void print_stats(void)
{
  if(rle3_stats.total)
  {
    trace("--EXTRAM--   RLE3 stats:\n");
    print_method_stats(&rle3_stats);
  }
  if(deflate_stats.total)
  {
    trace("--EXTRAM--   DEFLATE stats:\n");
    print_method_stats(&deflate_stats);
  }
}
#endif

#if 0
static size_t RLE2_pack(uint8_t * RESTRICT data, size_t data_len,
 const uint8_t *src, size_t src_len)
{
  size_t i, j, runsize;
  uint8_t current_char;

  for(i = 0, j = 0; i < src_len; i++)
  {
    current_char = src[i];
    runsize = 1;

    while((i < (src_len - 1)) && (src[i + 1] == current_char) &&
     (runsize < 127))
    {
      i++;
      runsize++;
    }

    if((runsize > 1) || current_char & 0x80)
    {
      if(j + 1 >= data_len)
        return 0;

      data[j++] = runsize | 0x80;
      data[j++] = current_char;
    }
    else
    {
      if(j >= data_len)
        return 0;

      data[j++] = current_char;
    }
  }
  return j;
}

static boolean RLE2_unpack(uint8_t * RESTRICT dest, size_t dest_len,
 const uint8_t *data, size_t data_len)
{
  size_t i, j, runsize;
  uint8_t current_char;

  for(i = 0, j = 0; j < data_len; j++)
  {
    current_char = data[j];

    if(!(current_char & 0x80))
    {
      if(i >= dest_len)
        return false;

      dest[i++] = current_char;
    }
    else
    {
      j++;
      runsize = current_char & 0x7F;
      if(j >= data_len || (i + runsize) > dest_len)
        return false;

      current_char = data[j];

      memset(dest + i, current_char, runsize);
      i += runsize;
    }
  }
  return true;
}
#endif

#define RLE3_MAX_CHAR   (0x3F)
#define RLE3_MAX_BLOCK  (0x2000)

#define RLE3_PACK_OVERHEAD(sz) (((sz) - 1 >= 0x20) ? 2 : 1)

#define RLE3_PACK_SIZE(code, sz) do { \
  size_t tmp = (sz) - 1; \
  if(j >= data_len || !(sz)) \
    return 0; \
  if(tmp >= 0x20) \
  { \
    if(j + 1 >= data_len) \
      return 0; \
    data[j++] = code | 0x20 | (tmp >> 8); \
    data[j++] = tmp & 0xFF; \
  } \
  else \
    data[j++] = code | tmp; \
} while(0)

#define RLE3_PACK_CHAR(ch)      do{ data[j++] = (ch) & RLE3_MAX_CHAR; }while(0)
#define RLE3_PACK_BLOCK(sz)     RLE3_PACK_SIZE(0x40, sz)
#define RLE3_PACK_RUN_CHAR(sz)  RLE3_PACK_SIZE(0x80, sz)
#define RLE3_PACK_RUN_BLOCK(sz) RLE3_PACK_SIZE(0xC0, sz)

#define RLE3_UNPACK_SIZE(ch) \
 (((ch) & 0x20) && j < data_len ? ((((ch) & 0x1F) << 8) | data[j++]) + 1 : (ch & 0x1F) + 1)

/**
 * Bound the maximum RLE3 stream size for an input of a given length. This will
 * always be slightly larger than the given input (regardless of how well the
 * input compresses).
 *
 * @param  src_len  length of the source data to be compressed.
 * @return          upper bound size of the corresponding RLE3 stream.
 */
static inline size_t RLE3_bound(size_t src_len)
{
  return src_len + ((src_len + 8191u) & ~8192u) / 4096u;
}

/**
 * Pack a buffer with RLE3 compression, a run length encoding scheme designed
 * to address some of the major flaws of Alexis' RLE2 without losing much speed.
 * This is much faster than zlib DEFLATE and compresses board data fairly well.
 *
 * @param   data      buffer to output RLE3 stream to.
 * @param   data_len  size of RLE3 buffer.
 * @param   src       uncompressed source data.
 * @param   src_len   length of uncompressed source.
 * @return            final RLE3 stream length, or 0 on failure.
 */
static size_t RLE3_pack(uint8_t * RESTRICT data, size_t data_len,
 const uint8_t *src, size_t src_len)
{
  ssize_t last_index[256];
  uint8_t prev_chr = 0;
  size_t i = 0;
  size_t j = 0;

  memset(last_index, 0xFF, sizeof(last_index));

  while(i < src_len)
  {
    const uint8_t *block = src + i;
    ssize_t block_start = i;
    size_t block_len = 0;
    size_t block_repeats = 0;
    size_t block_repeat_len = 0;
    size_t prev_repeats = 0;
    boolean block_split = false;

    while(i < src_len && block_len < RLE3_MAX_BLOCK)
    {
      if(src[i] == prev_chr)
      {
        // Is this a char run big enough to justify ending a block?
        // A char run needs to be 3 or longer to break even in the worst case
        // where the following block requires a 2 byte length (uncommon).
        if(i + 2 < src_len &&
         src[i + 1] == prev_chr &&
         src[i + 2] == prev_chr)
        {
          prev_repeats = 3;
          i += 3;
          while(i < src_len && src[i] == prev_chr)
            i++, prev_repeats++;

          break;
        }
      }

      // Is this the start of a block repeat?
      if(last_index[src[i]] >= block_start && (size_t)last_index[src[i]] + 1 < i)
      {
        ssize_t block_break = last_index[src[i]];
        size_t block_break_len = block_break - block_start;
        const uint8_t *prev_pos = src + block_break;
        const uint8_t *pos = src + i;
        size_t k = i;
        size_t x;
        size_t savings;
        size_t overhead;

        block_repeat_len = i - block_break;

        while(k < src_len)
        {
          for(x = 0; k + x < src_len && x < block_repeat_len; x++)
            if(pos[x] != prev_pos[x])
              break;

          if(x < block_repeat_len)
            break;

          block_repeats++;
          pos += block_repeat_len;
          k += block_repeat_len;
        }

        savings = block_repeat_len * block_repeats;
        /* Max overhead of following block + size of repeat code + overhead of
         * splitting the current block (if applicable). */
        overhead = 2 + RLE3_PACK_OVERHEAD(block_repeats) +
         ((block_break_len) ? RLE3_PACK_OVERHEAD(block_break_len) : 0);

        // Is this worth ending the block?
        if(savings >= overhead)
        {
          if(block_break > block_start)
          {
            block_split = true;
            block_len = block_break_len;
          }
          i += block_repeat_len * block_repeats;
          break;
        }
        block_repeats = 0;
      }

      if(last_index[src[i]] < block_start)
        last_index[src[i]] = i;

      // Continue the block...
      prev_chr = src[i++];
      block_len++;
    }

    // Emit block(s) (never allow an individual block to go over RLE3_MAX_BLOCK).
    while(block_len)
    {
      size_t pos_start = 0;
      size_t pos_end = block_len;
      boolean trim = false;

      if(!block_repeats || block_split)
      {
        // If this block isn't required to be encoded as a single block it
        // may be possible to replace part (or all) of it with literal chars,
        // reducing the block length encoding overhead.
        size_t old_overhead = RLE3_PACK_OVERHEAD(block_len);
        size_t new_overhead;

        while(pos_start < block_len && block[pos_start] <= RLE3_MAX_CHAR)
          pos_start++;

        if(pos_start < block_len)
          while(pos_end > pos_start && block[pos_end - 1] <= RLE3_MAX_CHAR)
            pos_end--;

        new_overhead = pos_start < block_len ? RLE3_PACK_OVERHEAD(pos_end - pos_start) : 0;
        if(new_overhead < old_overhead)
          trim = true;
      }

      if(trim)
      {
        // Emit the head and tail of the block as literal chars.
        size_t k = 0;
        if(j + block_len > data_len)
          return 0;

        while(block[k] <= RLE3_MAX_CHAR && k < block_len)
        {
          RLE3_PACK_CHAR(block[k]);
          k++;
        }
        if(pos_start < pos_end)
        {
          size_t middle_len = pos_end - pos_start;
          RLE3_PACK_BLOCK(middle_len);
          if(j + middle_len > data_len)
            return 0;

          memcpy(data + j, block + k, middle_len);
          j += middle_len;
          k += middle_len;
        }

        if(j + block_len > data_len)
          return 0;

        while(k < block_len)
        {
          RLE3_PACK_CHAR(block[k]);
          k++;
        }
        //trace("--RLE3--     %s %zu\n", (pos_start < pos_end) ? "mixed" : "chars", block_len);
      }
      else
      {
        RLE3_PACK_BLOCK(block_len);
        if(j + block_len > data_len)
          return 0;

        memcpy(data + j, block, block_len);
        j += block_len;
        //trace("--RLE3--     block %zu\n", block_len);
      }

      if(block_split)
      {
        block += block_len;
        block_len = block_repeat_len;
        block_split = false;
      }
      else
        break;
    }

    // Emit block run.
    while(block_repeats)
    {
      size_t pack_count = MIN(RLE3_MAX_BLOCK, block_repeats);
      block_repeats -= pack_count;
      RLE3_PACK_RUN_BLOCK(pack_count);
      //trace("--RLE3--     repeat block x %zu\n", pack_count);
    }

    // Emit char run.
    while(prev_repeats)
    {
      size_t pack_count = MIN(RLE3_MAX_BLOCK, prev_repeats);
      prev_repeats -= pack_count;
      RLE3_PACK_RUN_CHAR(pack_count);
      //trace("--RLE3--     repeat %02Xh x %zu\n", (int)prev_chr, pack_count);
    }
  }
  return j;
}

/**
 * Unpack an RLE3 stream.
 *
 * @param   dest      destination buffer for the unpacked stream.
 * @param   dest_len  size of destination buffer.
 * @param   data      source RLE3 stream to unpack.
 * @param   data_len  length of source RLE3 stream.
 * @return            `true` on success, otherwise `false`. This function will
 *                    fail if the unpacked stream size doesn't match `dest_len`.
 */
static boolean RLE3_unpack(uint8_t * RESTRICT dest, size_t dest_len,
 const uint8_t *data, size_t data_len)
{
  const uint8_t *prev_block = NULL;
  size_t prev_len = 0;
  uint8_t prev_chr = 0;
  size_t count;
  size_t i = 0;
  size_t j = 0;
  uint8_t chr;

  while(j < data_len)
  {
    if(i >= dest_len)
      break;

    chr = data[j++];
    if(chr >> 6)
      count = RLE3_UNPACK_SIZE(chr);

    switch(chr >> 6)
    {
      case 0:
      {
        // Char. Usually there are several of these in a row.
        dest[i++] = chr;
        while(i < dest_len && j < data_len && data[j] <= RLE3_MAX_CHAR)
          dest[i++] = data[j++];

        prev_chr = data[j - 1];
        prev_block = data + j - 1;
        prev_len = 1;
        break;
      }

      case 1:
      {
        // Block.
        if(i + count > dest_len || j + count > data_len)
          return false;

        prev_block = data + j;
        prev_len = count;

        memcpy(dest + i, data + j, count);
        i += count;
        j += count;

        prev_chr = data[j - 1];
        break;
      }

      case 2:
      {
        // Repeat last char.
        if(i + count > dest_len)
          return false;

        memset(dest + i, prev_chr, count);
        i += count;
        break;
      }

      case 3:
      {
        // Repeat last block.
        if(!prev_block || i + count * prev_len > dest_len)
          return false;

        while(count--)
        {
          memcpy(dest + i, prev_block, prev_len);
          i += prev_len;
        }
        break;
      }
    }
  }
  return (i == dest_len);
}

/**
 * Initialize a deflate stream.
 */
static boolean extram_deflate_init(struct extram_data *data)
{
  int window_bits = -MAX_WBITS;
  int mem_level = 8;

  if(data->status && data->status != Z_OK)
    return false;

  if(data->initialized)
  {
    data->status = deflateReset(&data->z);
    return (data->status == Z_OK);
  }

  while(mem_level > 4 || window_bits < -8)
  {
    /* Z_DEFAULT_COMPRESSION doesn't compress board data much better than
     * Z_BEST_SPEED and it takes over twice as long. */
    int res = deflateInit2(&data->z, Z_BEST_SPEED, Z_DEFLATED,
     window_bits, mem_level, Z_DEFAULT_STRATEGY);

    data->status = res;
    if(res == Z_OK)
    {
      data->initialized = true;
      return true;
    }

    /* Try progressively worse settings to see if one will stick... */
    /* This works out to (1 << (mem_level + 9)) = 8192 minimum. */
    if(mem_level > 4)
      mem_level--;
    /* This works out to (1 << (window_bits + 2)) = 2048 minimum. */
    if(window_bits < -8)
      window_bits++;
  }
  return false;
}

/**
 * Destroy a deflate stream.
 */
static void extram_deflate_destroy(struct extram_data *data)
{
  if(data->initialized)
    deflateEnd(&data->z);
}

/**
 * Initialize an inflate stream.
 */
static boolean extram_inflate_init(struct extram_data *data)
{
  if(data->status && data->status != Z_OK)
    return false;

  if(data->initialized)
  {
    data->status = inflateReset(&data->z);
    return (data->status == Z_OK);
  }

  /* This needs to be -MAX_WBITS (32k buffer). */
  data->status = inflateInit2(&data->z, -MAX_WBITS);
  if(data->status == Z_OK)
  {
    data->initialized = true;
    return true;
  }
  return false;
}

/**
 * Destroy an inflate stream.
 */
static void extram_inflate_destroy(struct extram_data *data)
{
  if(data->initialized)
    inflateEnd(&data->z);
}

/**
 * Calculate a checksum for a block of memory.
 */
static uint32_t extram_checksum(const void *src, size_t len)
{
  uint32_t checksum = adler32(0L, NULL, 0);
  while(len > 0)
  {
    unsigned int blocklen = MIN(len, UINT_MAX);
    checksum = adler32(checksum, src, blocklen);
    len -= blocklen;
  }
  return checksum;
}

#if defined(__GNUC__) && defined(__arm__)

/* Fast 32-bit aligned ARM copy. */
static void extram_copy32(uint32_t * RESTRICT d32, const uint32_t *s32, size_t len32)
{
  /* Code for handling the tail was inspired by musl.
   * a) bit 2 is shifted off (carry flag means >=4 words);
   * b) bit 1 is the sign bit (sign flag means >=2 words). */
  asm(
    "MOVS    r12, %2, lsr #3  \n\t"
    "BEQ     .Ltail           \n\t"
    ".Lbody:                  \n\t"
    "LDMIA   %1!, {r3-r10}    \n\t"
    "STMIA   %0!, {r3-r10}    \n\t"
    "SUBS    r12, r12, #1     \n\t"
    "BNE     .Lbody           \n\t"
    ".Ltail:                  \n\t"
    "MOVS    r12, %2, lsl #30 \n\t"
    "LDMCSIA %1!, {r3-r6}     \n\t"
    "LDMMIIA %1!, {r7-r8}     \n\t"
    "STMCSIA %0!, {r3-r6}     \n\t"
    "STMMIIA %0!, {r7-r8}     \n\t"
    "TST     %2, #1           \n\t"
    "LDMNEIA %1!, {r3}        \n\t"
    "STMNEIA %0!, {r3}        \n\t"
    : "+r" (d32), "+r" (s32), "+r" (len32), "=m" (*d32)
    : "m" (*s32)
    : "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r12", "cc"
  );
}

#else

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
/* Prevent GCC from replacing the copy loop with memcpy. */
static void extram_copy32(uint32_t * RESTRICT, const uint32_t *, size_t)
__attribute__((optimize("-fno-tree-loop-distribute-patterns")));
#endif

static void extram_copy32(uint32_t * RESTRICT d32, const uint32_t *s32, size_t len32)
{
  size_t i;
  for(i = 0; i < len32; i++)
    d32[i] = s32[i];
}

#endif /* extram_copy32 */

/**
 * Perform a 32-bit aligned copy. Both the source and the dest buffers must be
 * 32-bit aligned. The destination buffer additionally should be large enough
 * to fit `len` rounded up to a multiple of 4 bytes (use `extram_alloc_size`).
 */
static boolean extram_copy(void * RESTRICT dest, const void *src, size_t len)
{
  const uint32_t *s32 = (const uint32_t *)src;
  uint32_t *d32 = (uint32_t *)dest;
  size_t len32;

  if(((size_t)src & 3) || ((size_t)dest & 3))
  {
    debug("--EXTRAM-- bad copy parameters: %p %p %zu!\n", dest, src, len);
    return false;
  }

  len32 = len >> 2;
  extram_copy32(d32, s32, len32);

  if(len & 3)
  {
    const uint8_t *src8 = (const uint8_t *)src;
    uint32_t buffer = 0;
    uint32_t pos;
    size_t i = len & ~3;

#if PLATFORM_BYTE_ORDER == PLATFORM_LIL_ENDIAN
    for(pos = 0; i < len; i++, pos += 8)
      buffer |= src8[i] << pos;
#else
    for(pos = 24; i < len; i++, pos -= 8)
      buffer |= src8[i] << pos;
#endif
    d32[len32] = buffer;
  }
  return true;
}

/**
 * Get the rounded allocation size for a buffer.
 */
static size_t extram_alloc_size(size_t len)
{
  return (len + 3) & ~3;
}

/**
 * Get the extra memory block size for an input buffer.
 */
static size_t extram_block_size(size_t len)
{
  return sizeof(struct extram_block) + extram_alloc_size(len);
}

/**
 * Send a buffer to extra memory.
 *
 * @param  data structure including z_stream information.
 * @param  _src pointer to buffer to store to extra RAM.
 * @param  len  size of buffer to store to extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean store_buffer_to_extram(struct extram_data *data,
 char **_src, size_t len)
{
  struct extram_block *block;
  uint8_t *src = (uint8_t *)*_src;
  void *ptr;
  uint32_t flags = EXTRAM_PLATFORM_ALLOC;
  size_t projected_size = len;
  size_t alloc_size;
  EXTRAM_BUFFER_DECL;

  trace("--EXTRAM-- store_buffer_to_extram %p %zu\n", src, len);

  if(len >= data->compression_threshold)
  {
    // Before trying zlib (slow), how well does this RLE?
    size_t rle3_size = RLE3_pack((void *)extram_deflate_buffer, sizeof(extram_deflate_buffer), src, len);
    if(rle3_size > 0)
    {
      projected_size = rle3_size;
      flags |= EXTRAM_RLE3;
    }
    else
    {
      // Project compressed size...
      data->z.next_in = (Bytef *)src;
      data->z.avail_in = len;

      if(extram_deflate_init(data))
      {
        projected_size = deflateBound(&data->z, len);
        flags |= EXTRAM_DEFLATE;
      }
    }
  }

  platform_extram_lock();

  alloc_size = extram_block_size(projected_size);
  block = (struct extram_block *)platform_extram_alloc(alloc_size);
  if(!block)
  {
    flags &= ~EXTRAM_PLATFORM_ALLOC;
    block = cmalloc(alloc_size);
  }

  block->id = EXTRAM_ID;
  block->flags = flags;
  block->uncompressed_size = len;
  block->checksum = extram_checksum(src, len);

  if(flags & EXTRAM_DEFLATE)
  {
    // Compress.
    uint32_t *pos = block->data;
    size_t sz;
    size_t new_alloc_size;
    int res = Z_OK;

    if(~flags & EXTRAM_PLATFORM_ALLOC)
    {
      /* DEFLATE directly into the extram block. */
      data->z.next_out = (Bytef *)block->data;
      data->z.avail_out = extram_alloc_size(projected_size);

      res = deflate(&data->z, Z_FINISH);
    }
    else

    while(res != Z_STREAM_END)
    {
      data->z.next_out = (Bytef *)extram_deflate_buffer;
      data->z.avail_out = sizeof(extram_deflate_buffer);

      res = deflate(&data->z, Z_FINISH);

      if(res != Z_OK && res != Z_BUF_ERROR && res != Z_STREAM_END)
        break;

      sz = sizeof(extram_deflate_buffer) - data->z.avail_out;
      if(!extram_copy(pos, extram_deflate_buffer, sz))
        goto err;

      pos += sz >> 2;
    }

    if(res != Z_STREAM_END)
    {
      debug("--EXTRAM-- deflate failed with code %d\n", res);
      goto err;
    }
    trace("--EXTRAM--   DEFLATE size=%zu\n", (size_t)data->z.total_out);

    block->compressed_size = data->z.total_out;

    /* Shrink the allocation to the real compressed size. */
    new_alloc_size = extram_block_size(block->compressed_size);
    if(new_alloc_size < alloc_size)
    {
      alloc_size = new_alloc_size;
      if(flags & EXTRAM_PLATFORM_ALLOC)
      {
        ptr = platform_extram_resize(block, new_alloc_size);
      }
      else
        ptr = crealloc(block, new_alloc_size);

      if(!ptr)
        goto err;
      block = (struct extram_block *)ptr;
    }
  }
  else

  if(flags & EXTRAM_RLE3)
  {
    // RLE3.
    block->compressed_size = projected_size;

    if(!extram_copy(block->data, extram_deflate_buffer, projected_size))
      goto err;

    trace("--EXTRAM--   RLE3 size=%zu\n", projected_size);
  }
  else
  {
    // Store.
    block->compressed_size = len;

    if(!extram_copy(block->data, src, len))
      goto err;
  }

#ifdef EXTRAM_STATS
  tick_stats(block);
#endif

  platform_extram_unlock();

  ptr = platform_extram_store(block, alloc_size);
  if(!ptr)
  {
    debug("--EXTRAM-- failed to send buffer to non-mapped platform RAM.\n");
    goto err;
  }

  free(src);
  *_src = ptr;
  return true;

err:
  if(flags & EXTRAM_PLATFORM_ALLOC)
  {
    platform_extram_free(block);
  }
  else
    free(block);

  platform_extram_unlock();
  return false;
}

/**
 * Retrieve a buffer from extra memory.
 *
 * @param  data structure including z_stream information.
 * @param  src  pointer to buffer to retrieve from extra RAM.
 * @param  len  length of buffer to retreieve from extra RAM.
 * @return      `true` on success, otherwise `false`.
 */
static boolean retrieve_buffer_from_extram(struct extram_data *data,
 char **src, size_t len)
{
  struct extram_block *block = (struct extram_block *)(*src);
  void *ptr;
  uint32_t checksum;
  size_t alloc_size;
  uint8_t *buffer = NULL;
  EXTRAM_BUFFER_DECL;

  trace("--EXTRAM-- retrieve_buffer_from_extram %p %zu\n", *src, len);

  ptr = platform_extram_retrieve(block, len);
  if(!ptr)
  {
    debug("--EXTRAM-- failed to retrieve buffer %p from non-mapped platform RAM.\n",
     (void *)*src);
    goto err;
  }
  block = ptr;

  platform_extram_lock();

  /* Try to eliminate false positives arriving here... */
  if(block->id != EXTRAM_ID)
  {
    debug("--EXTRAM-- ID mismatch @ %p (expected %08zx, got %08zx)\n",
     (void *)block, (size_t)EXTRAM_ID, (size_t)block->id);
    goto err;
  }

  if(len != block->uncompressed_size)
  {
    debug("--EXTRAM-- size mismatch @ %p (expected %zu, got %zu)\n",
      (void *)block, len, (size_t)block->uncompressed_size
    );
    goto err;
  }

  /* Destroy the block and clear *src. */
  if(data->free_data)
    goto clear;

  alloc_size = extram_alloc_size(len);
  buffer = cmalloc(alloc_size);

  if(block->flags & EXTRAM_DEFLATE)
  {
    // Compressed.
    uint32_t *pos = block->data;
    size_t left = block->compressed_size;
    int res = Z_OK;

    data->z.next_in = (Bytef *)extram_deflate_buffer;
    data->z.avail_in = sizeof(extram_deflate_buffer);

    if(!extram_inflate_init(data))
    {
      debug("--EXTRAM-- inflateInit failed @ %p\n", (void *)block);
      goto err;
    }

    data->z.next_out = buffer;
    data->z.avail_out = len;

    if(~block->flags & EXTRAM_PLATFORM_ALLOC)
    {
      /* Inflate directly from the extram block. */
      data->z.next_in = (Bytef *)block->data;
      data->z.avail_in = block->compressed_size;

      res = inflate(&data->z, Z_FINISH);
    }
    else

    while((res == Z_OK || res == Z_BUF_ERROR) && left)
    {
      size_t sz = MIN(left, sizeof(extram_deflate_buffer));
      size_t sz_ext = extram_alloc_size(sz);
      left -= sz;
      if(!extram_copy(extram_deflate_buffer, pos, sz_ext))
        goto err;

      pos += sz_ext >> 2;

      data->z.next_in = (Bytef *)extram_deflate_buffer;
      data->z.avail_in = sz;
      res = inflate(&data->z, 0);
    }

    if(res != Z_STREAM_END || data->z.total_out != len)
    {
      debug("--EXTRAM-- inflate failed @ %p (total_out=%zu, len=%zu) with code %d (%s)\n",
        (void *)block, (size_t)data->z.total_out, len, res,
        (data->z.msg ? data->z.msg : "no message")
      );
      goto err;
    }
    trace("--EXTRAM--   inflated block of size %zu to %zu\n",
     (size_t)data->z.total_in, (size_t)data->z.total_out);
  }
  else

  if(block->flags & EXTRAM_RLE3)
  {
    // RLE3.
    size_t sz = extram_alloc_size(block->compressed_size);
    if(sz > sizeof(extram_deflate_buffer) ||
     !extram_copy(extram_deflate_buffer, block->data, sz))
    {
      debug("--EXTRAM-- failed to copy RLE3 @ %p: uncompressed=%zu, compressed=%zu\n",
        (void *)block, (size_t)block->uncompressed_size, (size_t)block->compressed_size
      );
      goto err;
    }

    if(!RLE3_unpack(buffer, len, (void *)extram_deflate_buffer,
     block->compressed_size))
    {
      debug("--EXTRAM-- failed to unpack RLE3 @ %p\n", (void *)block);
      goto err;
    }
    trace("--EXTRAM--   RLE3 unpacked block of size %zu to %zu\n",
     (size_t)block->compressed_size, (size_t)block->uncompressed_size);
  }
  else
  {
    // Stored.
    if(block->compressed_size != block->uncompressed_size)
    {
      debug("--EXTRAM-- stored size inconsistent @ %p: uncompressed=%zu, compressed=%zu\n",
        (void *)block, (size_t)block->uncompressed_size, (size_t)block->compressed_size
      );
      goto err;
    }
    if(!extram_copy(buffer, block->data, alloc_size))
      goto err;
  }

  checksum = extram_checksum(buffer, len);
  if(checksum != block->checksum)
  {
    debug("--EXTRAM-- checksum fail @ %p (expected %08zx, got %08zx)\n",
      (void *)block, (size_t)block->checksum, (size_t)checksum
    );
    goto err;
  }

clear:
  if(block->flags & EXTRAM_PLATFORM_ALLOC)
  {
    platform_extram_free(block);
  }
  else
    free(block);

  platform_extram_unlock();
  *src = (void *)buffer;
  return true;

err:
  platform_extram_unlock();
  free(buffer);
  return false;
}

/**
 * Move the board's memory from normal RAM to extra RAM.
 *
 * @param  board  board to send to extra RAM.
 * @param  file   __FILE__ of invoking call (via macro).
 * @param  line   __LINE__ of invoking call (via macro).
 */
void real_store_board_to_extram(struct board *board, const char *file, int line)
{
  size_t board_size = board->board_width * board->board_height;
  struct robot **robot_list = board->robot_list;
  struct extram_data data;
  int i;

  if(board->is_extram)
  {
    warn("--EXTRAM-- board %p is already in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- storing board %p (%s:%d)\n", (void *)board, file, line);
  board->is_extram = true;

  memset(&data, 0, sizeof(struct extram_data));

  // Layer data.
  data.compression_threshold = EXTRAM_COMPRESS_BOARDS_THRESHOLD;
  if(!store_buffer_to_extram(&data, &board->level_id, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_param, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_color, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_id, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_param, board_size))
    goto err;
  if(!store_buffer_to_extram(&data, &board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!store_buffer_to_extram(&data, &board->overlay, board_size))
      goto err;
    if(!store_buffer_to_extram(&data, &board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  data.compression_threshold = EXTRAM_COMPRESS_ROBOTS_THRESHOLD;
  for(i = 1; robot_list && i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!store_buffer_to_extram(&data, &cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!store_buffer_to_extram(&data, &cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }
#endif

#ifdef CONFIG_EDITOR
      if(cur_robot->command_map)
      {
        if(!store_buffer_to_extram(&data, (char **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      // TODO: it would eventually be better to pack and send these to extra
      // memory instead of doing this.
      clear_label_cache(cur_robot);
    }
  }

#ifdef EXTRAM_STATS
  print_stats();
#endif

  extram_deflate_destroy(&data);
  return;

err:
  {
    char msg[81];
    snprintf(msg, ARRAY_SIZE(msg), "Failed to store board to extram at %s:%d", file, line);
    error(msg, ERROR_T_FATAL, ERROR_OPT_EXIT | ERROR_OPT_NO_HELP, 0);
  }
}

/**
 * Move the board's memory from extra RAM to normal RAM.
 *
 * @param  board      board to retrieve from extra RAM.
 * @param  free_data  free all buffers and NULL their respective pointers. This
 *                    is faster than retrieving and freeing from normal RAM.
 * @param  file       __FILE__ of invoking call (via macro).
 * @param  line       __LINE__ of invoking call (via macro).
 */
void real_retrieve_board_from_extram(struct board *board, boolean free_data,
 const char *file, int line)
{
  size_t board_size = board->board_width * board->board_height;
  struct robot **robot_list = board->robot_list;
  struct extram_data data;
  int i;

  if(!board->is_extram)
  {
    warn("--EXTRAM-- board %p isn't in extram! (%s:%d)\n", (void *)board, file, line);
    return;
  }

  trace("--EXTRAM-- %s board %p (%s:%d)\n",
   (free_data ? "freeing" : "retrieving"), (void *)board, file, line);
  board->is_extram = false;

  memset(&data, 0, sizeof(struct extram_data));
  data.free_data = free_data;

  // Layer data.
  if(!retrieve_buffer_from_extram(&data, &board->level_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_color, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_id, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_param, board_size))
    goto err;
  if(!retrieve_buffer_from_extram(&data, &board->level_under_color, board_size))
    goto err;

  // Overlay.
  if(board->overlay_mode)
  {
    if(!retrieve_buffer_from_extram(&data, &board->overlay, board_size))
      goto err;
    if(!retrieve_buffer_from_extram(&data, &board->overlay_color, board_size))
      goto err;
  }

  // Robot programs and source.
  for(i = 1; robot_list && i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
      {
        if(!retrieve_buffer_from_extram(&data, &cur_robot->program_bytecode,
         cur_robot->program_bytecode_length))
          goto err;
      }

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
      {
        if(!retrieve_buffer_from_extram(&data, &cur_robot->program_source,
         cur_robot->program_source_length))
          goto err;
      }
#endif

#ifdef CONFIG_EDITOR
      if(cur_robot->command_map)
      {
        if(!retrieve_buffer_from_extram(&data, (char **)&cur_robot->command_map,
         cur_robot->command_map_length * sizeof(struct command_mapping)))
          goto err;
      }
#endif
      // TODO: it would eventually be better to pack and send these to extra
      // memory instead of doing this.
      clear_label_cache(cur_robot);
      cache_robot_labels(cur_robot);
    }
  }
  extram_inflate_destroy(&data);
  return;

err:
  {
    char msg[81];
    snprintf(msg, ARRAY_SIZE(msg), "Failed to load board from extram at %s:%d", file, line);
    error(msg, ERROR_T_FATAL, ERROR_OPT_EXIT | ERROR_OPT_NO_HELP, 0);
  }
}

#ifdef CONFIG_EDITOR

static boolean get_extram_buffer_usage(const void *buffer, size_t *compressed,
 size_t *uncompressed)
{
  const struct extram_block *block = (const struct extram_block *)buffer;
  if(block->id != EXTRAM_ID)
    return false;

  *compressed += extram_block_size(block->compressed_size);
  *uncompressed += block->uncompressed_size;
  return true;
}

/**
 * Get total actual extra RAM usage (compressed) and total uncompressed
 * extra RAM size data for a board.
 *
 * @param  board          board to get extra RAM statistics for.
 * @param  compressed     pointer to write total actual RAM usage to.
 * @param  uncompressed   pointer to write total stored data size to.
 * @return                `true` on success, otherwise `false`.
 */
boolean board_extram_usage(struct board *board, size_t *compressed,
 size_t *uncompressed)
{
  struct robot **robot_list;
  int i;

#ifdef USE_PLATFORM_EXTRAM_STORE
  /* The headers are stored in non-mapped RAM and not really accessible. */
  return false;
#endif

  if(!board || !compressed | !uncompressed)
    return false;

  *compressed = 0;
  *uncompressed = 0;

  if(!board->is_extram)
    return true;

  platform_extram_lock();

  if(!get_extram_buffer_usage(board->level_id, compressed, uncompressed))
    goto err;
  if(!get_extram_buffer_usage(board->level_param, compressed, uncompressed))
    goto err;
  if(!get_extram_buffer_usage(board->level_color, compressed, uncompressed))
    goto err;
  if(!get_extram_buffer_usage(board->level_under_id, compressed, uncompressed))
    goto err;
  if(!get_extram_buffer_usage(board->level_under_param, compressed, uncompressed))
    goto err;
  if(!get_extram_buffer_usage(board->level_under_color, compressed, uncompressed))
    goto err;

  if(board->overlay_mode)
  {
    if(!get_extram_buffer_usage(board->overlay, compressed, uncompressed))
      goto err;
    if(!get_extram_buffer_usage(board->overlay_color, compressed, uncompressed))
      goto err;
  }

  robot_list = board->robot_list;
  for(i = 1; i <= board->num_robots; i++)
  {
    struct robot *cur_robot = robot_list[i];
    if(cur_robot)
    {
      if(cur_robot->program_bytecode)
        if(!get_extram_buffer_usage(cur_robot->program_bytecode,
         compressed, uncompressed))
          goto err;

#if defined(CONFIG_DEBYTECODE) || defined(CONFIG_EDITOR)
      if(cur_robot->program_source)
        if(!get_extram_buffer_usage(cur_robot->program_source,
         compressed, uncompressed))
          goto err;

      if(cur_robot->command_map)
        if(!get_extram_buffer_usage(cur_robot->command_map,
         compressed, uncompressed))
          goto err;
#endif
    }
  }
  platform_extram_unlock();
  return true;

err:
  platform_extram_unlock();
  return false;
}

#endif /* CONFIG_EDITOR */

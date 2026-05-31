/* MegaZeux
 *
 * Copyright (C) 2017-2024 Alice Rowan <petrifiedrowan@gmail.com>
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

#ifndef MEGAZEUX_IO_MEMFILE_H
#define MEGAZEUX_IO_MEMFILE_H

#include "../compat.h"

MEGAZEUX_BEGIN_DECLS

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memfile
{
  unsigned char *current;
  unsigned char *start;
  unsigned char *end;
  boolean alloc;
  /* Generally an error on seeking past the end of memfiles is desired but for
   * wrappers with safety checks (vfile.c), it is useful to seek past the end.
   */
  boolean seek_past_end;
  boolean is_write;
};

/**
 * Open a memory buffer for reading.
 */
static inline void mfopen(const void *src, size_t len, struct memfile *mf)
{
  mf->start = (unsigned char *)src;
  mf->current = (unsigned char *)src;
  mf->end = (unsigned char *)src + len;
  mf->alloc = false;
  mf->seek_past_end = false;
  mf->is_write = false;
}

/**
 * Open a memory buffer for writing.
 */
static inline void mfopen_wr(void *dest, size_t len, struct memfile *mf)
{
  mf->start = (unsigned char *)dest;
  mf->current = mf->start;
  mf->end = mf->start + len;
  mf->alloc = false;
  mf->seek_past_end = false;
  mf->is_write = true;
}

/**
 * This version exists to be a drop-in alternative for fopen().
 * For general purposes, use mfopen() instead.
 */
static inline struct memfile *mfopen_alloc(const void *src, size_t len)
{
  struct memfile *mf = (struct memfile *)cmalloc(sizeof(struct memfile));

  mf->start = (unsigned char *)src;
  mf->current = (unsigned char *)src;
  mf->end = (unsigned char *)src + len;
  mf->alloc = true;
  mf->seek_past_end = false;
  mf->is_write = false;
  return mf;
}

/**
 * This function is intended to be a drop-in fclose() alternative for
 * use with mfopen_alloc().
 */
static inline int mf_alloc_free(struct memfile *mf)
{
  if(mf && mf->alloc)
  {
    free(mf);
    return 0;
  }
  return -1;
}

/**
 * Copy the buffer position and length to external variables.
 */
static inline void mfsync(void **buf, size_t *len, struct memfile *mf)
{
  if(buf) *buf = mf->start;
  if(len) *len = mf->end - mf->start;
}

/**
 * Determine if the memfile has at least len space remaining.
 */
static inline boolean mfhasspace(size_t len, struct memfile *mf)
{
  return mf->current && mf->current <= mf->end ? (size_t)(mf->end - mf->current) >= len : false;
}

/**
 * Move the buffer of a memfile while preserving its current position.
 */
static inline void mfmove(void *new_buf, size_t new_len, struct memfile *mf)
{
  size_t pos = mf->current - mf->start;

  mf->start = (unsigned char *)new_buf;
  mf->current = mf->start + pos;
  mf->end = mf->start + new_len;

  if(mf->current > mf->end)
    mf->current = mf->end;
}

/**
 * Resize the memfile buffer and preserve the current position.
 * Do not use this function unless the memfile buffer is on the heap.
 */
static inline void mfresize(size_t new_len, struct memfile *mf)
{
  void *new_buf = realloc(mf->start, new_len);
  if(new_buf)
    mfmove(new_buf, new_len, mf);
}

/**
 * The mfget/mfput functions assume bounding has already been performed by
 * the caller to reduce the amount of code inlined and to improve performance.
 * If full bounds checks on arbitrary data are needed, use vio instead.
 * Bounds check asserts are provided to help debug for debug builds.
 */
static inline int mfgetc(struct memfile *mf)
{
  int v;
  assert(mf->end - mf->current >= 1);
  v =  mf->current[0];
  mf->current += 1;
  return v;
}

static inline int mfgetw(struct memfile *mf)
{
  int v;
  assert(mf->end - mf->current >= 2);
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  mf->current += 2;
  return v;
}

static inline int mfgetd(struct memfile *mf)
{
  int v;
  assert(mf->end - mf->current >= 4);
  v =  mf->current[0];
  v |= mf->current[1] << 8;
  v |= mf->current[2] << 16;
  v |= (unsigned int)mf->current[3] << 24;
  mf->current += 4;
  return v;
}

static inline unsigned int mfgetud(struct memfile *mf)
{
  return (unsigned int)mfgetd(mf);
}

static inline int64_t mfgetq(struct memfile *mf)
{
  int64_t v;
  assert(mf->end - mf->current >= 8);
  v =  (int64_t)mf->current[0];
  v |= (int64_t)mf->current[1] << 8;
  v |= (int64_t)mf->current[2] << 16;
  v |= (int64_t)mf->current[3] << 24;
  v |= (int64_t)mf->current[4] << 32;
  v |= (int64_t)mf->current[5] << 40;
  v |= (int64_t)mf->current[6] << 48;
  v |= (int64_t)mf->current[7] << 56;
  mf->current += 8;
  return v;
}

static inline uint64_t mfgetuq(struct memfile *mf)
{
  return (uint64_t)mfgetq(mf);
}

static inline int mfputc(int ch, struct memfile *mf)
{
  assert(mf->is_write && mf->end - mf->current >= 1);
  mf->current[0] = ch & 0xFF;
  mf->current += 1;
  return ch & 0xFF;
}

static inline void mfputw(int ch, struct memfile *mf)
{
  assert(mf->is_write && mf->end - mf->current >= 2);
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current += 2;
}

static inline void mfputd(int ch, struct memfile *mf)
{
  assert(mf->is_write && mf->end - mf->current >= 4);
  mf->current[0] = ch & 0xFF;
  mf->current[1] = (ch >> 8) & 0xFF;
  mf->current[2] = (ch >> 16) & 0xFF;
  mf->current[3] = (ch >> 24) & 0xFF;
  mf->current += 4;
}

static inline void mfputud(size_t ch, struct memfile *mf)
{
  mfputd((unsigned int)ch, mf);
}

static inline void mfputq(int64_t v, struct memfile *mf)
{
  assert(mf->is_write && mf->end - mf->current >= 8);
  mf->current[0] = v & 0xFF;
  mf->current[1] = (v >> 8) & 0xFF;
  mf->current[2] = (v >> 16) & 0xFF;
  mf->current[3] = (v >> 24) & 0xFF;
  mf->current[4] = (v >> 32) & 0xFF;
  mf->current[5] = (v >> 40) & 0xFF;
  mf->current[6] = (v >> 48) & 0xFF;
  mf->current[7] = (v >> 56) & 0xFF;
  mf->current += 8;
}

static inline void mfputuq(uint64_t v, struct memfile *mf)
{
  mfputq((int64_t)v, mf);
}

static inline size_t mfread(void *dest, size_t len, size_t count,
 struct memfile *mf)
{
  unsigned char *pos = (unsigned char *)dest;
  size_t total = len * count;

  if(!mf->current || len == 0 || count == 0)
    return 0;

  if(!mfhasspace(total, mf))
  {
    if(mf->current >= mf->end)
      return 0;

    count = (mf->end - mf->current) / len;
    total = len * count;
  }

  memcpy(pos, mf->current, total);
  mf->current += total;
  return count;
}

static inline size_t mfwrite(const void *src, size_t len, size_t count,
 struct memfile *mf)
{
  unsigned char *pos = (unsigned char *)src;
  size_t total = len * count;

  if(!mf->current || len == 0 || count == 0 || !mf->is_write)
    return 0;

  if(!mfhasspace(total, mf))
  {
    if(mf->current >= mf->end)
      return 0;

    count = (mf->end - mf->current) / len;
    total = len * count;
  }

  memcpy(mf->current, pos, total);
  mf->current += total;
  return count;
}

/**
 * Read a line from memory, safely trimming platform-specific EOL chars
 * as-needed.
 *
 * NOTE: this only works with Unix and Windows line ends right now. If support
 * for Mac OS <=9 is ever added the loop should terminate at \r instead of \n.
 */
static inline char *mfsafegets(char *dest, int len, struct memfile *mf)
{
  unsigned char *stop = mf->current + len - 1;
  unsigned char *in = mf->current;
  unsigned char ch;
  char *out = dest;

  if(mf->end < stop)
    stop = mf->end;

  // Return NULL if this is the end of the file.
  if(in >= stop)
    return NULL;

  // Copy until the end/bound or a newline.
  while(in < stop && (ch = *(in++)) != '\n')
    *(out++) = ch;

  *out = 0;

  // Seek to the next line (or the end of the file).
  mf->current = in;

  // Length at least 1 -- get rid of \r and \n
  if(out > dest)
    if(out[-1] == '\r' || out[-1] == '\n')
      out[-1] = 0;

  // Length at least 2 -- get rid of \r and \n
  if(out - 1 > dest)
    if(out[-2] == '\r' || out[-2] == '\n')
      out[-2] = 0;

  return dest;
}

static inline int mfseek(struct memfile *mf, ptrdiff_t offs, int code)
{
  unsigned char *ptr;
  ptrdiff_t pos;

  switch(code)
  {
    case SEEK_SET:
      pos = offs;
      break;

    case SEEK_CUR:
      pos = (mf->current - mf->start) + offs;
      break;

    case SEEK_END:
      pos = (mf->end - mf->start) + offs;
      break;

    default:
      pos = -1;
      break;
  }

  // pos >= 0 doesn't necessarily imply ptr >= start due to overflow.
  ptr = mf->start + pos;
  if(pos >= 0 && ptr >= mf->start && (mf->seek_past_end || ptr <= mf->end))
  {
    mf->current = mf->start + pos;
    return 0;
  }

  return -1;
}

static inline ptrdiff_t mftell(struct memfile *mf)
{
  assert(mf->current >= mf->start);
  return mf->current - mf->start;
}

MEGAZEUX_END_DECLS

#endif /* MEGAZEUX_IO_MEMFILE_H */

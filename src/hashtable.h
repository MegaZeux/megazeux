/* The MIT License

   Copyright (c) 2008, 2009, 2011 by Attractive Chaos <attractor@live.co.uk>
   Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

/**
 * NOTE: this is a significantly altered khash specifically for MegaZeux.
 *
 * Changes from the original khash:
 * - Tab removal and style update to more closely match MZX style.
 * - The variable scopes have been replaced with C99 "static inline".
 * - The custom int types have been replaced by C99 stdint.h types and size_t.
 * - The allocator macros have been replaced with MZX's check alloc functions.
 * - The default key types have been replaced with a single struct-based key
 *   type expected to contain a key field and a key length field. The names of
 *   these fields can be specified. A uint32_t field named "hash" is also
 *   expected to exist in this struct for the full hash value to be stored.
 * - The kh_get function has also been altered to optionally take a separate
 *   key pointer and key length parameters instead of a key struct.
 * - Alternate macros have been added to provide a uthash-like interface.
 *
 * All changes to this file for MegaZeux are also licensed under the MIT License.
 * However, certain functions used here (check alloc, memcasecmp) are not.
 *
 * This header is not currently compatible with the standard khash header.
 * See contrib/khash.h for the original example and changelog from this file.
 */

#ifndef __HASHTABLE_H
#define __HASHTABLE_H

#include "compat.h"

__M_BEGIN_DECLS

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "memcasecmp.h"
#include "platform/platform_endian.h"

#ifndef klib_unused
#if (defined __clang__ && __clang_major__ >= 3) || (defined __GNUC__ && __GNUC__ >= 3)
#define klib_unused __attribute__ ((__unused__))
#else
#define klib_unused
#endif
#endif /* klib_unused */

#define __ac_isempty(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&2)
#define __ac_isdel(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&1)
#define __ac_iseither(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&3)
#define __ac_set_isdel_false(flag, i) (flag[i>>4]&=~(1ul<<((i&0xfU)<<1)))
#define __ac_set_isempty_false(flag, i) (flag[i>>4]&=~(2ul<<((i&0xfU)<<1)))
#define __ac_set_isboth_false(flag, i) (flag[i>>4]&=~(3ul<<((i&0xfU)<<1)))
#define __ac_set_isdel_true(flag, i) (flag[i>>4]|=1ul<<((i&0xfU)<<1))

#define __ac_fsize(m) ((m) < 16? 1 : (m)>>4)

#ifndef kroundup32
#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif

static const double __ac_HASH_UPPER = 0.77;

/**
 * Keep table sizes in the uint32_t range since the hashes are uint32_t.
 * In practice, tables over this size will probably never be used, since the
 * counters and strings lists are bounded to 2^31.
 */
#if ARCHITECTURE_BITS >= 64
static const uint64_t __ac_HASH_MAXIMUM = ((uint64_t)UINT32_MAX) + 1;
#else
static const size_t __ac_HASH_MAXIMUM = ((size_t)INT32_MAX) + 1;
#endif

#define __KHASH_TYPE(name, khkey_t, khval_t) \
  typedef struct kh_##name##_s \
  { \
    size_t n_buckets;   \
    size_t size;        \
    size_t n_occupied;  \
    size_t upper_bound; \
    uint32_t *flags;    \
    khkey_t *keys;      \
    khval_t *vals;      \
  } kh_##name##_t;

#define __KHASH_IMPL(name, khkey_t, khval_t, kh_is_map, ptr_field, len_field, \
 __hash_func, __hash_equal) \
  \
  static inline klib_unused kh_##name##_t *kh_init_##name(void) \
  { \
    return (kh_##name##_t*)ccalloc(1, sizeof(kh_##name##_t));             \
  } \
  static inline klib_unused void kh_destroy_##name(kh_##name##_t *h) \
  { \
    if(h)                                                                 \
    {                                                                     \
      free(h->flags);                                                     \
      free((void *)h->keys);                                              \
      free((void *)h->vals);                                              \
      free(h);                                                            \
    }                                                                     \
  } \
  \
  static inline klib_unused void kh_clear_##name(kh_##name##_t *h) \
  { \
    if(h && h->flags)                                                     \
    {                                                                     \
      size_t flags_size = __ac_fsize(h->n_buckets);                       \
      memset(h->flags, 0xaa, flags_size * sizeof(uint32_t));              \
      h->size = h->n_occupied = 0;                                        \
    }                                                                     \
  } \
  \
  static inline klib_unused size_t kh_get_no_obj_##name(const kh_##name##_t *h, \
   const void *key_ptr, uint32_t key_len, uint32_t hash, int has_hash) \
  { \
    if(h->n_buckets)                                                      \
    {                                                                     \
      size_t i, last, mask, step = 0;                                     \
      mask = h->n_buckets - 1;                                            \
      if(!has_hash)                                                       \
        hash = __hash_func(key_ptr, key_len);                             \
      i = hash & mask;                                                    \
      last = i;                                                           \
      while(!__ac_isempty(h->flags, i) &&                                 \
       (__ac_isdel(h->flags, i) ||                                        \
        hash != h->keys[i]->hash || \
        !__hash_equal(h->keys[i]->ptr_field, key_ptr,                     \
         h->keys[i]->len_field, key_len)))                                \
      {                                                                   \
        i = (i + (++step)) & mask;                                        \
        if(i == last)                                                     \
          return h->n_buckets;                                            \
      }                                                                   \
      if(!__ac_iseither(h->flags, i))                                     \
        return i;                                                         \
    }                                                                     \
    return h->n_buckets;                                                  \
  } \
  \
  static inline klib_unused size_t kh_get_##name(const kh_##name##_t *h, khkey_t key) \
  { \
    return kh_get_no_obj_##name(h, key->ptr_field, key->len_field,        \
     key->hash, 1);                                                       \
  } \
  \
  static inline klib_unused int kh_resize_##name(kh_##name##_t *h, size_t new_n_buckets) \
  { \
    /* This function uses 0.25*n_buckets bytes of working space instead of \
     * [sizeof(key_t+val_t)+.25]*n_buckets. \
     */ \
    uint32_t *new_flags = 0;                                              \
    size_t j = 1;                                                         \
    size_t new_upper_bound;                                               \
  \
    kroundup32(new_n_buckets);                                            \
    if(new_n_buckets > __ac_HASH_MAXIMUM)                                 \
      return -1;                                                          \
    if(new_n_buckets < 4)                                                 \
      new_n_buckets = 4;                                                  \
  \
    new_upper_bound = (size_t)(new_n_buckets * __ac_HASH_UPPER + 0.5);    \
    if(h->size >= new_upper_bound)                                        \
    {                                                                     \
      j = 0;  /* requested size is too small */                           \
    }                                                                     \
    else                                                                  \
    {                                                                     \
      /* hash table size to be changed (shrink or expand); rehash */      \
      size_t flags_size = __ac_fsize(new_n_buckets) * sizeof(uint32_t);   \
      new_flags = (uint32_t *)cmalloc(flags_size);                        \
      if(!new_flags)                                                      \
        return -1;                                                        \
      memset(new_flags, 0xaa, flags_size);                                \
      if(h->n_buckets < new_n_buckets)                                    \
      {                                                                   \
        /* expand */                                                      \
        size_t size = new_n_buckets * sizeof(khkey_t);                    \
        khkey_t *new_keys = (khkey_t *)crealloc((void *)h->keys, size);   \
        if(!new_keys)                                                     \
        {                                                                 \
          free(new_flags);                                                \
          return -1;                                                      \
        }                                                                 \
        h->keys = new_keys;                                               \
        if(kh_is_map)                                                     \
        {                                                                 \
          size_t size = new_n_buckets * sizeof(khval_t);                  \
          khval_t *new_vals = (khval_t *)crealloc((void *)h->vals, size); \
          if(!new_vals)                                                   \
          {                                                               \
            free(new_flags);                                              \
            return -1;                                                    \
          }                                                               \
          h->vals = new_vals;                                             \
        }                                                                 \
      } /* otherwise shrink */                                            \
    } \
  \
    if(j) \
    { \
      /* rehashing is needed */                                           \
      size_t new_mask = new_n_buckets - 1;                                \
      for(j = 0; j != h->n_buckets; ++j)                                  \
      {                                                                   \
        if(__ac_iseither(h->flags, j) == 0)                               \
        {                                                                 \
          khkey_t key = h->keys[j];                                       \
          khval_t val;                                                    \
          h->keys[j] = 0;                                                 \
          if(kh_is_map)                                                   \
          {                                                               \
            val = h->vals[j];                                             \
            h->vals[j] = 0;                                               \
          }                                                               \
          __ac_set_isdel_true(h->flags, j);                               \
          while(1)                                                        \
          {                                                               \
            /* kick-out process; sort of like in Cuckoo hashing */        \
            size_t k = key->hash;                                         \
            size_t i, step = 0;                                           \
            i = k & new_mask;                                             \
            while(!__ac_isempty(new_flags, i))                            \
              i = (i + (++step)) & new_mask;                              \
            __ac_set_isempty_false(new_flags, i);                         \
            if(i < h->n_buckets && __ac_iseither(h->flags, i) == 0)       \
            {                                                             \
              /* kick out the existing element */                         \
              { khkey_t tmp = h->keys[i]; h->keys[i] = key; key = tmp; }  \
              if(kh_is_map)                                               \
              { khval_t tmp = h->vals[i]; h->vals[i] = val; val = tmp; }  \
              /* mark it as deleted in the old hash table */              \
              __ac_set_isdel_true(h->flags, i);                           \
            }                                                             \
            else                                                          \
            {                                                             \
              /* write the element and jump out of the loop */            \
              h->keys[i] = key;                                           \
              if(kh_is_map)                                               \
                h->vals[i] = val;                                         \
              break;                                                      \
            }                                                             \
          }                                                               \
        }                                                                 \
      }                                                                   \
      if(h->n_buckets > new_n_buckets)                                    \
      {                                                                   \
        /* shrink the hash table */                                       \
        size_t size = new_n_buckets * sizeof(khkey_t);                    \
        h->keys = (khkey_t*)crealloc((void *)h->keys, size);              \
        if(kh_is_map)                                                     \
        {                                                                 \
          size = new_n_buckets * sizeof(khval_t);                         \
          h->vals = (khval_t*)crealloc((void *)h->vals, size);            \
        }                                                                 \
      }                                                                   \
      free(h->flags); /* free the working space */                        \
      h->flags = new_flags;                                               \
      h->n_buckets = new_n_buckets;                                       \
      h->n_occupied = h->size;                                            \
      h->upper_bound = new_upper_bound;                                   \
    }                                                                     \
    return 0;                                                             \
  } \
  \
  static inline klib_unused size_t kh_put_##name(kh_##name##_t *h, khkey_t key, int *ret) \
  { \
    size_t x;                                                             \
    if(h->n_occupied >= h->upper_bound)                                   \
    {                                                                     \
      /* update the hash table */                                         \
      if(h->n_buckets > (h->size << 1))                                   \
      {                                                                   \
        /* clear "deleted" elements */                                    \
        if(kh_resize_##name(h, h->n_buckets - 1) < 0)                     \
        {                                                                 \
          *ret = -1;                                                      \
          return h->n_buckets;                                            \
        }                                                                 \
      }                                                                   \
      else                                                                \
  \
      if(kh_resize_##name(h, h->n_buckets + 1) < 0)                       \
      {                                                                   \
        /* expand the hash table */                                       \
        *ret = -1;                                                        \
        return h->n_buckets;                                              \
      }                                                                   \
    }                                                                     \
    /* TODO: to implement automatically shrinking;                        \
     * resize() already support shrinking */                              \
    {                                                                     \
      size_t i, site, last, mask = h->n_buckets - 1, step = 0;            \
      uint32_t hash = __hash_func(key->ptr_field, key->len_field);        \
      key->hash = hash;                                                   \
      x = site = h->n_buckets;                                            \
      i = hash & mask;                                                    \
      if(__ac_isempty(h->flags, i))                                       \
      {                                                                   \
        /* for speed up */                                                \
        x = i;                                                            \
      }                                                                   \
      else                                                                \
      {                                                                   \
        last = i;                                                         \
        while(!__ac_isempty(h->flags, i) &&                               \
         (__ac_isdel(h->flags, i) ||                                      \
          hash != h->keys[i]->hash || \
          !__hash_equal(h->keys[i]->ptr_field, key->ptr_field,            \
           h->keys[i]->len_field, key->len_field)))                       \
        {                                                                 \
          if(__ac_isdel(h->flags, i))                                     \
            site = i;                                                     \
          i = (i + (++step)) & mask;                                      \
          if(i == last)                                                   \
          {                                                               \
            x = site;                                                     \
            break;                                                        \
          }                                                               \
        }                                                                 \
        if(x == h->n_buckets)                                             \
        {                                                                 \
          if(__ac_isempty(h->flags, i) && site != h->n_buckets)           \
            x = site;                                                     \
          else                                                            \
            x = i;                                                        \
        }                                                                 \
      }                                                                   \
    }                                                                     \
    if(__ac_isempty(h->flags, x))                                         \
    {                                                                     \
      /* not present at all */                                            \
      h->keys[x] = key;                                                   \
      __ac_set_isboth_false(h->flags, x);                                 \
      ++h->size;                                                          \
      ++h->n_occupied;                                                    \
      *ret = 1;                                                           \
    }                                                                     \
    else                                                                  \
  \
    if(__ac_isdel(h->flags, x))                                           \
    {                                                                     \
      /* deleted */                                                       \
      h->keys[x] = key;                                                   \
      __ac_set_isboth_false(h->flags, x);                                 \
      ++h->size;                                                          \
      *ret = 2;                                                           \
    }                                                                     \
    /* Don't touch h->keys[x] if present and not deleted */               \
    else                                                                  \
      *ret = 0;                                                           \
    return x;                                                             \
  }                                                                       \
  \
  static inline klib_unused void kh_del_##name(kh_##name##_t *h, size_t x) \
  { \
    if(x != h->n_buckets && !__ac_iseither(h->flags, x))                  \
    {                                                                     \
      __ac_set_isdel_true(h->flags, x);                                   \
      --h->size;                                                          \
      h->keys[x] = 0;                                                     \
      if(kh_is_map)                                                       \
        h->vals[x] = 0;                                                   \
    }                                                                     \
  }

#define KHASH_INIT2(name, khkey_t, khval_t, kh_is_map,    \
 ptr_field, len_field, __hash_func, __hash_equal)         \
  __KHASH_TYPE(name, khkey_t, khval_t)                    \
  __KHASH_IMPL(name, khkey_t, khval_t, kh_is_map,         \
   ptr_field, len_field, __hash_func, __hash_equal)

#define KHASH_INIT(name, khkey_t, khval_t, kh_is_map,     \
 ptr_field, len_field, __hash_func, __hash_equal)         \
  KHASH_INIT2(name, khkey_t, khval_t, kh_is_map,          \
   ptr_field, len_field, __hash_func, __hash_equal)

/* Other convenient macros... */

/*!
  @abstract Type of the hash table.
  @param  name  Name of the hash table [symbol]
 */
#define khash_t(name) kh_##name##_t

/*! @function
  @abstract     Initiate a hash table.
  @param  name  Name of the hash table [symbol]
  @return       Pointer to the hash table [khash_t(name)*]
 */
#define kh_init(name) kh_init_##name()

/*! @function
  @abstract     Destroy a hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
 */
#define kh_destroy(name, h) kh_destroy_##name(h)

/*! @function
  @abstract     Reset a hash table without deallocating memory.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
 */
#define kh_clear(name, h) kh_clear_##name(h)

/*! @function
  @abstract     Resize a hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  s     New size [size_t]
 */
#define kh_resize(name, h, s) kh_resize_##name(h, s)

/*! @function
  @abstract     Insert a key to the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Key [type of keys]
  @param  r     Extra return code: -1 if the operation failed;
                0 if the key is present in the hash table;
                1 if the bucket is empty (never used); 2 if the element in
        the bucket has been deleted [int*]
  @return       Iterator to the inserted element [size_t]
 */
#define kh_put(name, h, k, r) kh_put_##name(h, k, r)

/*! @function
  @abstract     Retrieve a key from the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Key [type of keys]
  @return       Iterator to the found element, or kh_end(h) if the element is
                absent [size_t]
 */
#define kh_get(name, h, k) kh_get_##name(h, k)
#define kh_get_no_obj(name, h, kp, kl) kh_get_no_obj_##name(h, kp, kl, 0, 0)

/*! @function
  @abstract     Remove a key from the hash table.
  @param  name  Name of the hash table [symbol]
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  k     Iterator to the element to be deleted [size_t]
 */
#define kh_del(name, h, k) kh_del_##name(h, k)

/*! @function
  @abstract     Test whether a bucket contains data.
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [size_t]
  @return       1 if containing data; 0 otherwise [int]
 */
#define kh_exist(h, x) (!__ac_iseither((h)->flags, (x)))

/*! @function
  @abstract     Get key given an iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [size_t]
  @return       Key [type of keys]
 */
#define kh_key(h, x) ((h)->keys[x])

/*! @function
  @abstract     Get value given an iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  x     Iterator to the bucket [size_t]
  @return       Value [type of values]
  @discussion   For hash sets, calling this results in segfault.
 */
#define kh_val(h, x) ((h)->vals[x])

/*! @function
  @abstract     Alias of kh_val()
 */
#define kh_value(h, x) ((h)->vals[x])

/*! @function
  @abstract     Get the start iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       The start iterator [size_t]
 */
#define kh_begin(h) (size_t)(0)

/*! @function
  @abstract     Get the end iterator
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       The end iterator [size_t]
 */
#define kh_end(h) ((h)->n_buckets)

/*! @function
  @abstract     Get the number of elements in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       Number of elements in the hash table [size_t]
 */
#define kh_size(h) ((h)->size)

/*! @function
  @abstract     Get the number of buckets in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @return       Number of buckets in the hash table [size_t]
 */
#define kh_n_buckets(h) ((h)->n_buckets)

/*! @function
  @abstract     Iterate over the entries in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  kvar  Variable to which key will be assigned
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define kh_foreach(h, kvar, vvar, code) { size_t __i;    \
  for (__i = kh_begin(h); __i != kh_end(h); ++__i) {    \
    if (!kh_exist(h,__i)) continue;            \
    (kvar) = kh_key(h,__i);                \
    (vvar) = kh_val(h,__i);                \
    code;                        \
  } }

/*! @function
  @abstract     Iterate over the values in the hash table
  @param  h     Pointer to the hash table [khash_t(name)*]
  @param  vvar  Variable to which value will be assigned
  @param  code  Block of code to execute
 */
#define kh_foreach_value(h, vvar, code) { size_t __i;    \
  for (__i = kh_begin(h); __i != kh_end(h); ++__i) {    \
    if (!kh_exist(h,__i)) continue;            \
    (vvar) = kh_val(h,__i);                \
    code;                        \
  } }

/* --- BEGIN OF HASH FUNCTIONS --- */

// TODO make case sensitive versions?

/**
 * This hash function results in far better distribution than the default X31
 * function and isn't significantly more expensive.
 *
 * http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
 */
static inline uint32_t fnv_1a_hash_string_len(const void *_str, uint32_t len)
{
  const char *str = _str;
  uint32_t h = 0;
  for(; len; len--)
    h = (h ^ (uint32_t)memtolower((int)*(str++))) * 16777619;
  return h;
}

#define kh_mem_hash_func(keyptr, keylen) \
  fnv_1a_hash_string_len(keyptr, keylen)

#define kh_mem_hash_equal(aptr, bptr, alen, blen) \
  (((uint32_t)alen == (uint32_t)blen) && !memcasecmp32(aptr, bptr, blen))

/* --- END OF HASH FUNCTIONS --- */

#define HASH_SET_INIT(name, khkey_t, key_ptr_field, key_len_field) \
  KHASH_INIT(name, khkey_t, char, 0, \
   key_ptr_field, key_len_field, kh_mem_hash_func, kh_mem_hash_equal)

#define HASH_MAP_INIT(name, khkey_t, khval_t, key_ptr_field, key_len_field) \
  KHASH_INIT(name, khkey_t, khval_t, 1, \
   key_ptr_field, key_len_field, kh_mem_hash_func, kh_mem_hash_equal)

// khash_t(n) abstraction for the unlikely event this library gets replaced.
#define hash_t(n) khash_t(n)

// uthash-like macros.

/**
 * Add an object to the hash table.
 * If the hash table hasn't been initialized, this function will initialize it.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER).
 * @param h       Variable containing hash table pointer. If this variable is
 *                set to null, the hash table is considered uninitialized.
 * @param keyobj  A new key struct to add to the table. Its key and key length
 *                fields must be initialized. Its hash field will be
 *                initialized during this operation.
 */
#define HASH_ADD(n, h, keyobj) do                                 \
{                                                                 \
  int _res;                                                       \
  if(!h) h = kh_init(n);                                          \
  kh_put(n, (khash_t(n) *)h, keyobj, &_res);                      \
} while(0)

/**
 * Find an object in the hash table.
 * If the hash table hasn't been initialized, this function will do nothing.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER).
 * @param h       Variable containing the hash table pointer.
 * @param keyptr  Pointer to the key variable (usually a string).
 * @param keylen  Length of the key variable in bytes.
 * @param destobj Pointer variable that will be set to the object if found
 *                or NULL if not found.
 */
#define HASH_FIND(n, _h, keyptr, keylen, destobj) do              \
{                                                                 \
  destobj = NULL;                                                 \
  if(_h)                                                          \
  {                                                               \
    khash_t(n) *h = _h;                                           \
    size_t iter = kh_get_no_obj(n, h, keyptr, (uint32_t)keylen);  \
                                                                  \
    if(iter < kh_end(h)) destobj = h->keys[iter];                 \
  }                                                               \
} while(0)

/**
 * Delete an object from the hash table.
 * If the hash table hasn't been initialized, this function will do nothing.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER).
 * @param h       Variable containing the hash table pointer.
 * @param keyobj  Object to remove from the hash table. Its key, key length,
 *                and hash fields should be initialized (which will be the
 *                case if this object was passed to HASH_ADD and/or was
 *                returned from HASH_FIND).
 */
#define HASH_DELETE(n, _h, keyobj) do                             \
{                                                                 \
  if(_h)                                                          \
  {                                                               \
    khash_t(n) *h = _h;                                           \
    size_t iter = kh_get(n, h, keyobj);                           \
                                                                  \
    if(iter < kh_end(h)) kh_del(n, h, iter);                      \
  }                                                               \
} while(0)

/**
 * Destroy the hash table.
 * If the hash table hasn't been initialized, this function will do nothing.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER)
 * @param h       Variable containing the hash table pointer. This function
 *                will set it to NULL after destroying the hash table.
 */
#define HASH_CLEAR(n, _h) do                                      \
{                                                                 \
  if(_h)                                                          \
  {                                                               \
    khash_t(n) *h = _h;                                           \
    kh_destroy(n, h);                                             \
    _h = NULL;                                                    \
  }                                                               \
} while(0)

/**
 * Iterate the hash table.
 * If the hash table hasn't been initialized, this function will do nothing.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER)
 * @param h       Variable containing the hash table pointer.
 * @param element The variable the current element will be stored to.
 * @param code    A block of code to execute for every element.
 */
#define HASH_ITER(n, _h, element, code) do                        \
{                                                                 \
  khash_t(n) *__h = _h;                                           \
  size_t __i;                                                     \
  if(_h) for(__i = kh_begin(__h); __i != kh_end(__h); __i++)      \
  {                                                               \
    if(!kh_exist(__h, __i)) continue;                             \
    (element) = kh_key(__h, __i);                                 \
    code;                                                         \
  }                                                               \
} while(0)

/**
 * Get the total memory usage of a hash table.
 *
 * @param name    The unique identifier of the hash table type (e.g. COUNTER)
 * @param h       Variable containing the hash table pointer.
 * @param size    Variable to store total size (should be size_t).
 */
#define HASH_MEMORY_USAGE(n, _h, size) do                         \
{                                                                 \
  khash_t(n) *__h = _h;                                           \
  if(__h && __h->keys)                                            \
  {                                                               \
    (size) = sizeof(*__h);                                        \
    (size) += __h->n_buckets * sizeof(__h->keys[0]);              \
    (size) += __ac_fsize(__h->n_buckets);                         \
    if(__h->vals)                                                 \
      (size) += __h->n_buckets * sizeof(__h->vals[0]);            \
  }                                                               \
  else                                                            \
    (size) = 0;                                                   \
} while(0)

__M_END_DECLS

#endif /* __HASHTABLE_H */

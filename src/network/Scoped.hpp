/* MegaZeux
 *
 * Copyright (C) 2020-2021 Alice Rowan <petrifiedrowan@gmail.com>
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

/**
 * RAII wrappers for misc. data types used in MZX.
 */

#ifndef MEGAZEUX_SCOPED_HPP
#define MEGAZEUX_SCOPED_HPP

#include "../compat.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef IS_CXX_11
#include <type_traits>
#endif

/**
 * RAII wrapper for <FILE, fclose> and <vfile, vfclose>.
 */
template<class T, int (*RELEASE_FN)(T *)>
class ScopedFile
{
private:
  T *ptr;

  ScopedFile &operator=(const ScopedFile &) { return *this; }

public:
  ScopedFile(T *p = nullptr): ptr(p) {}
  ~ScopedFile()
  {
    if(ptr)
      RELEASE_FN(ptr);
  }

  void reset(T *p = nullptr)
  {
    if(p != ptr)
    {
      if(ptr)
        RELEASE_FN(ptr);
      ptr = p;
    }
  }

  operator T *() const
  {
    return ptr;
  }

  maybe_explicit operator bool() const
  {
    return ptr != nullptr;
  }
};

/**
 * RAII wrapper for a scoped resizable char buffer (as vector requires
 * libstdc++ and linking that causes apk bloat and MinGW dependency creep).
 * Also, vector<bool> and no .data() in C++98 :(
 */
template<class T>
class ScopedBuffer
{
private:
  T *ptr;
  size_t alloc;
  ScopedBuffer(const ScopedBuffer &) {}
  ScopedBuffer &operator=(const ScopedBuffer &) { return *this; }

public:
  explicit ScopedBuffer(size_t length = 0): ptr(nullptr), alloc(length)
  {
#ifdef IS_CXX_11
    static_assert(std::is_trivial<T>::value,
     "Please use ScopedBuffer for trivial data types only ;-(");
#endif
    if(length)
      ptr = (T *)cmalloc(length);
  }
  ~ScopedBuffer()
  {
    if(ptr)
      free(ptr);
  }

  // NOTE: this function invalidates external copies of this pointer!
  boolean resize(size_t new_length)
  {
    if(new_length == alloc)
      return true;

    if(new_length == 0)
    {
      free(ptr);
      ptr = nullptr;
      alloc = 0;
      return true;
    }

    T *newptr = (T *)crealloc(ptr, new_length);
    if(newptr)
    {
      ptr = newptr;
      alloc = new_length;
      return true;
    }
    return false;
  }

  size_t size() const
  {
    return alloc;
  }

  T *data() const
  {
    return ptr;
  }

  operator T *() const
  {
    return ptr;
  }

  maybe_explicit operator bool() const
  {
    return ptr != nullptr;
  }
};

/**
 * RAII wrapper to delete a pointer allocated by new.
 * The pointer will be deleted when this instance exits scope or when reset()
 * is explicitly called (optionally, with a new pointer allocated by new).
 * Once this wrapper has been given control of a pointer, the pointer cannot
 * be released from this wrapper without delete being used on it.
 */
template<class T>
class ScopedPtr
{
private:
  T *ptr;
  ScopedPtr &operator=(const ScopedPtr &) { return *this; }

public:
  ScopedPtr(T *p = nullptr): ptr(p) {}
  ~ScopedPtr()
  {
    if(ptr)
      delete ptr;
  }

  void reset(T *p = nullptr)
  {
    if(p != ptr)
    {
      if(ptr)
        delete ptr;
      ptr = p;
    }
  }

#if IS_CXX_11
  ScopedPtr &operator=(T *&&p)
  {
    reset(p);
    return *this;
  }
#endif

  operator T *() const
  {
    return ptr;
  }

  T *operator->() const
  {
    return ptr;
  }

  T *get() const
  {
    return ptr;
  }

  maybe_explicit operator bool() const
  {
    return ptr != nullptr;
  }
};

/**
 * RAII wrapper to delete[] a pointer allocated by new[].
 * The pointer will be delete[]ed when this instance exits scope or when reset()
 * is explicitly called (optionally, with a new pointer allocated by new[]).
 * Once this wrapper has been given control of a pointer, the pointer cannot
 * be released from this wrapper without delete[] being used on it.
 */
template<class T>
class ScopedPtr<T[]>
{
private:
  T *ptr;
  ScopedPtr &operator=(const ScopedPtr &) { return *this; }

public:
  ScopedPtr(T *p = nullptr): ptr(p) {}
  ~ScopedPtr()
  {
    if(ptr)
      delete[] ptr;
  }

  void reset(T *p = nullptr)
  {
    if(p != ptr)
    {
      if(ptr)
        delete[] ptr;
      ptr = p;
    }
  }

#if IS_CXX_11
  ScopedPtr &operator=(T *&&p)
  {
    reset(p);
    return *this;
  }
#endif

  operator T *() const
  {
    return ptr;
  }

  T *operator->() const
  {
    return ptr;
  }

  T *get() const
  {
    return ptr;
  }

  maybe_explicit operator bool() const
  {
    return ptr != nullptr;
  }
};

#endif /* MEGAZEUX_SCOPED_HPP */

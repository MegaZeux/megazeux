/* MegaZeux
 *
 * Copyright (C) 2019-2026 Alice Rowan <petrifiedrowan@gmail.com>
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
 * Unit test class macros and template. Requires C++11 or higher. Includes a
 * main() implementation, a SIGABRT handler, and custom check_alloc function
 * implementations. To implement a test, simply include this header and write
 * tests as follows:
 *
 * UNITTEST(TestName)
 * {
 *   // test here
 *   // run this once standalone + once for each section
 *
 *   SECTION(SectionName)
 *   {
 *     // only run this once
 *   }
 *
 *   SECTION(SectionName2)
 *   {
 *     // only run this once, but separately from SectionName...
 *   }
 * }
 *
 * The file can contain multiple UNITTEST() {...}. Each must have a different
 * name, including different UNITTEST() invocations between compilation units.
 * The following macros can be used in tests:
 *
 * [mfmt,...] = optional message to display on failure, passed to vsnprintf.
 *              mfmt must be a string literal.
 *
 * ASSERT(t[,mfmt,...])         t must be non-zero.
 * ASSERTEQ(a,b[,mfmt,...])     a must equal b.
 * ASSERTCMP(a,b[,mfmt,...])    a and b must be null-terminated C strings and must
 *                              be exactly equal (strcmp).
 * ASSERTNCMP(a,b,n[,mfmt,...]) a and b must be null-terminated C strings and must
 *                              be exactly equal for the first n chars (strncmp).
 * ASSERTMEM(a,b,l,[mfmt,...])  the memory pointed to by a and b must be identical
 *                              for l bytes (memcmp).
 * FAIL([mfmt,...])             unconditionally fail the test.
 * SKIP()                       unconditionally skip the test.
 *
 * Additionally, failed assert() assertions will be detected and print error
 * messages (generally, these should only be used in the code being tested).
 *
 * This is not a replacement for the testworlds tests, and likely will be
 * useful only for the subset of MegaZeux algorithm and IO code that can be
 * tested completely standalone from the rest of MegaZeux.
 */

#ifndef UNIT_HPP
#define UNIT_HPP

#ifdef __M_BEGIN_DECLS
#error "Include Unit.hpp first!"
#endif

#define CORE_LIBSPEC
#define EDITOR_LIBSPEC
#define SKIP_SDL
#include "../src/compat.h"
#include "../src/platform/platform_attribute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS
#include <inttypes.h>

#ifndef UNIT_DIR_SEPARATOR
#if defined(_WIN32) || defined(CONFIG_DJGPP)
#define UNIT_DIR_SEPARATOR "\\"
#else
#define UNIT_DIR_SEPARATOR "/"
#endif
#endif

#ifndef UNIT_BUILD_BASE
#define UNIT_BUILD_BASE ".build"
#endif

#ifndef UNIT_TMP_DIR
#define UNIT_TMP_DIR UNIT_BUILD_BASE UNIT_DIR_SEPARATOR "tmp"
#endif

/**
 * Utility templates.
 */

template<class T, class S, int A, int B>
static inline constexpr int samesize(T (&a)[A], S (&b)[B])
{
  static_assert(A == B, "array size mismatch");
  return 0;
}

template<class T, int A>
static inline constexpr int arraysize(T (&a)[A])
{
  return A;
}

template<class T>
static inline const T coalesce(const T var)
{
  return var;
}

static inline unsigned int coalesce(boolean var)
{
  return var;
}

static inline const char *coalesce(const char *var)
{
  return (var ? var : "NULL");
}

static inline constexpr const char *coalesce(decltype(nullptr) ignore)
{
  return "NULL";
}

template<class aligntype, size_t A=128>
class alignstr
{
protected:
  union
  {
    char arr[A];
    aligntype ignore;
  } u;

public:
  template<int B>
  alignstr(const char (&str)[B])
  {
    static_assert(A >= B, "alignstr buffer is too small!");
    memcpy(u.arr, str, B);
  }

  alignstr(const char * const str)
  {
    size_t slen = strlen(str);
    if(slen + 1 > A)
      abort();
    memcpy(u.arr, str, slen);
  }

  constexpr const char *c_str() const
  {
    return u.arr;
  }
};

/**
 * Unit test macros.
 */

#define UNIMPLEMENTED() \
  do\
  {\
    throw Unit::exception(__LINE__, "Test is not yet implemented"); \
  } while(0)

#define ASSERT(test, ...) \
  do\
  {\
    if(!(test)) \
    { \
      throw Unit::exception(__LINE__, #test, "~" __VA_ARGS__); \
    } \
  } while(0)

#define ASSERTEQ(a, b, ...) \
  do\
  {\
    if(!((a) == (b)))\
    { \
      throw Unit::exception(__LINE__, #a " == " #b, \
       Unit::arg(a), Unit::arg(b), "~" __VA_ARGS__); \
    } \
  } while(0)

#define ASSERTCMP(a, b, ...) \
  do\
  {\
    if(strcmp(a,b)) \
    {\
      throw Unit::exception(__LINE__, "strcmp(" #a ", " #b ")", \
       Unit::arg(a), Unit::arg(b), "~" __VA_ARGS__); \
    }\
  } while(0)

#define ASSERTNCMP(a, b, n, ...) \
  do\
  {\
    if(strncmp(a,b,n)) \
    {\
      throw Unit::exception(__LINE__, "strncmp(" #a ", " #b ", " #n ")", \
       Unit::arg(a), Unit::arg(b), "~" __VA_ARGS__); \
    }\
  } while(0)

#define ASSERTMEM(a, b, l, ...) \
  do\
  {\
    if(memcmp(a,b,l)) \
    {\
      throw Unit::exception(__LINE__, "memcmp(" #a ", " #b ", " #l ")", \
       Unit::arg(a, l), Unit::arg(b, l), "~" __VA_ARGS__); \
    }\
  } while(0)

#define FAIL(...) \
  do\
  {\
    throw Unit::exception(__LINE__, nullptr, "" __VA_ARGS__); \
  } while(0)

#define SKIP() \
  do\
  {\
    throw Unit::skip(); \
  } while(0)

#define SECTION(sectionname) \
  this->section_name = #sectionname; \
  if((++this->count_sections) == this->expected_section)

#define UNITTEST(testname) \
class testname ## _unittest_impl final : public Unit::unittest \
{\
  void run_impl() override;\
public:\
  testname ## _unittest_impl(): Unit::unittest(__FILE__, #testname) {}\
}\
static testname ## _unittest_inst;\
\
inline void testname ## _unittest_impl::run_impl(void)

#define Uerr(...) do{ fprintf(stderr, "" __VA_ARGS__); }while(0)
#define UerrFlush() do{ fflush(stderr); }while(0)

namespace Unit
{
  class unittest;

  template<class T>
  constexpr const T &min(const T &a, const T &b)
  {
    return (a < b) ? a : b;
  }

  template<class T>
  constexpr const T &max(const T &a, const T &b)
  {
    return (a > b) ? a : b;
  }

  template<class T>
  constexpr const T &clamp(const T &a, const T &_min, const T &_max)
  {
    return Unit::max(_min, Unit::min(_max, a));
  }

  /**
   * Input value to a failed assertion. These are generated for Unit::exception
   * instances thrown from assertions with multiple operands. This provides a
   * printable text representation of the operand (i.e. std::string but worse)
   * and also disambiguates the correct Unit::exception constructor to use.
   */
  class arg final
  {
    static constexpr int BUF_SIZE = 23;
    char *allocbuf = nullptr;
    char tmpbuf[BUF_SIZE] = { '\0' };

  public:
    boolean has_value = false;
    const char *op = nullptr;

    arg();
    arg(arg &&a);
    arg(const arg &a);
    ~arg();

    explicit arg(decltype(nullptr) _op);
    explicit arg(const char *_op);
    explicit arg(const void *_op);
    explicit arg(unsigned char _op);
    explicit arg(unsigned short _op);
    explicit arg(unsigned int _op);
    explicit arg(unsigned long _op);
    explicit arg(unsigned long long _op);
    explicit arg(signed char _op);
    explicit arg(short _op);
    explicit arg(int _op);
    explicit arg(long _op);
    explicit arg(long long _op);

    explicit arg(const void *_ptr, size_t length_bytes);
    explicit arg(const unsigned short *_ptr, size_t length_bytes);
    explicit arg(const unsigned int *_ptr, size_t length_bytes);
    explicit arg(const unsigned long *_ptr, size_t length_bytes);

  private:
    const char *fix_op(const arg &src);
  };

  /**
   * Thrown to signal the current test (or test section) has been skipped.
   */
  class skip final {};

  /**
   * Thrown on test failure (see ASSERT macros above).
   */
  class exception final
  {
  private:
    char reasonbuf[1024];
    Unit::arg leftarg;
    Unit::arg rightarg;

    void diagnostics();

  public:
    int line;
    const char *test      = coalesce(nullptr);
    const char *reason    = coalesce(nullptr);
    bool has_reason       = false;
    const char *left      = coalesce(nullptr);
    const char *right     = coalesce(nullptr);
    bool has_values       = false;

    /* A copy constructor is required to be throwable. */
    exception(const exception &e);

    exception(int _line, const char *_test);
    ATTRIBUTE_PRINTF(4, 5)
    exception(int _line, const char *_test, const char *_reason_fmt, ...);
    ATTRIBUTE_PRINTF(6, 7)
    exception(int _line, const char *_test, Unit::arg &&_left, Unit::arg &&_right,
     const char *_reason_fmt, ...);
  };

  /**
   * Class for running all unit tests in the current set of unit tests.
   */
  class unittestrunner final
  {
  private:

    unittest *current_test;
    unsigned int count;
    unsigned int passed;
    unsigned int failed;
    unsigned int skipped;
    unsigned int total;

    void print_status();

  public:
    static unittestrunner &get();
    bool run();
    void signal_fail();
    void addtest(unittest *t);
    void set_filter(const char *filter);
  };

  /**
   * Class for an individual unit test. Don't use directly; create individual
   * test subclasses using the UNITTEST() macro.
   */
  class unittest
  {
    bool has_run = false;
    bool has_failed_main = false;
    unsigned int last_failed_section = 0;
    bool printed_failed = false;

  protected:

    const char *file_name;
    const char *test_name;
    const char *section_name = nullptr;

    unsigned int num_sections;
    unsigned int count_sections;
    unsigned int expected_section;
    unsigned int failed_sections;
    unsigned int skipped_sections;

  public:

    bool entire_test_skipped;

    unittest(const char *_file, const char *_test_name);

    bool run();
    void signal_fail();
    bool match_filter(const char *filter);

  private:
    void run_section(void);
    int passed_sections();
    void print_test_success(void);
    void print_test_failed(void);
    void print_test_skipped(void);
    void print_exception(const Unit::exception &e);
    void skip();

    virtual void run_impl(void) = 0;
  };

  /**
   * Check the unit test system before running any tests.
   */
  bool self_check();
}

#endif /* UNIT_HPP */

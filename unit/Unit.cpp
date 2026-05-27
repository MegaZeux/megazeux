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

#include "Unit.hpp"
#include "../src/platform.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include <algorithm>
#include <csignal>
#include <type_traits>
#include <utility>
#include <vector>

/* Enable exception diagnostics if -fno-exceptions or platform lacks
 * a stack unwinder. */
#if defined(__GNUC__) && !defined(__clang__) && \
    (!defined(__EXCEPTIONS) || __EXCEPTIONS == 0)
#define PRINT_EXCEPTION_DIAGNOSTICS
#endif
#if defined(CONFIG_AMIGA) && defined(__mc68000__)
#define PRINT_EXCEPTION_DIAGNOSTICS
#endif

/* Utility functions that inlined MegaZeux source expects to exist. */
#include "../src/utils/utils_alloc.h"

static const clock_t get_ticks_base = clock();
uint64_t get_ticks()
{
  return clock() - get_ticks_base;
}

/* Main functions. */

static void sigabrt_handler(int signal)
{
  if(signal == SIGABRT)
  {
    Uerr("Received SIGABRT: ");
  }
  else
    Uerr("Unexpected signal %d received: ", signal);

  Unit::unittestrunner::get().signal_fail();
}

int main(int argc, char *argv[])
{
  struct stat st;
  if(stat(UNIT_TMP_DIR, &st) != 0)
  {
    Uerr("Make directory '" UNIT_TMP_DIR "' and try running again.");
    return 1;
  }

  if(!Unit::self_check())
    return 1;

  if(argc > 1)
    for(int i = 1; i < argc; i++)
      Unit::unittestrunner::get().set_filter(argv[i]);

  std::signal(SIGABRT, sigabrt_handler);
  return !(Unit::unittestrunner::get().run());
}


namespace Unit
{
  /************************************************************************
   * Unit::arg functions.
   */

  arg::~arg()
  {
    delete[] allocbuf;
  }

  arg::arg() {}

  const char *arg::fix_op(const arg &src)
  {
    if(src.op == src.tmpbuf)
      return tmpbuf;

    if(src.op == src.allocbuf)
      return allocbuf;

    return src.op;
  }

  arg::arg(arg &&a)
  {
    memcpy(tmpbuf, a.tmpbuf, sizeof(tmpbuf));
    has_value = a.has_value;
    allocbuf = a.allocbuf;
    op = fix_op(a);

    a.has_value = false;
    a.op = nullptr;
    a.allocbuf = nullptr;
  }

  arg::arg(const arg &a)
  {
    memcpy(tmpbuf, a.tmpbuf, sizeof(tmpbuf));
    has_value = a.has_value;

    if(a.allocbuf)
    {
      size_t len = strlen(a.allocbuf);
      allocbuf = new char[len + 1];
      memcpy(allocbuf, a.allocbuf, len + 1);
    }
    op = fix_op(a);
  }

  arg::arg(std::nullptr_t _op)
  {
    op = coalesce(_op);
  }

  arg::arg(const char *_op)
  {
    has_value = !!_op;
    _op = coalesce(_op);

    size_t len = strlen(_op);
    allocbuf = new char[len + 1];
    memcpy(allocbuf, _op, len + 1);
    op = allocbuf;
  }

  arg::arg(const void *_op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "0x%08zx", reinterpret_cast<size_t>(_op));
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(unsigned char _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%u", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(unsigned short _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%u", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(unsigned int _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%d", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(unsigned long _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%lu", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(unsigned long long _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%llu", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(signed char _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%d", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(short _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%d", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(int _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%d", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(long _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%ld", _op);
    op = tmpbuf;
    has_value = true;
  }

  arg::arg(long long _op)
  {
    snprintf(tmpbuf, sizeof(tmpbuf), "%lld", _op);
    op = tmpbuf;
    has_value = true;
  }

  template<class T, class E = std::enable_if<std::is_integral<T>::value>>
  char *_set_operand_integral_ptr(const T *_op, size_t length)
  {
    if(_op)
    {
      char *buf;
      char *pos;
      size_t allocleft = length * (sizeof(T) * 2 + 1);

      buf = new char[allocleft];
      pos = buf;

      for(size_t i = 0; i < length; i++)
      {
        typename std::make_unsigned<T>::type tmp = _op[i];
        size_t n = snprintf(pos, allocleft, "%0*lx ", (int)sizeof(T) * 2, (unsigned long)tmp);
        if(n >= allocleft)
          break;

        pos += n;
        allocleft -= n;
      }

      return buf;
    }
    return nullptr;
  }

  arg::arg(const void *_ptr, size_t length_bytes)
  {
    // Print most memcmp types (including  non-integral types) bytewise.
    const unsigned char *_ptr8 = reinterpret_cast<const unsigned char *>(_ptr);
    allocbuf = _set_operand_integral_ptr(_ptr8, length_bytes / sizeof(char));
    op = allocbuf;
    has_value = !!allocbuf;
  }

  arg::arg(const unsigned short *_ptr, size_t length_bytes)
  {
    allocbuf = _set_operand_integral_ptr(_ptr, length_bytes / sizeof(short));
    op = allocbuf;
    has_value = !!allocbuf;
  }

  arg::arg(const unsigned int *_ptr, size_t length_bytes)
  {
    allocbuf = _set_operand_integral_ptr(_ptr, length_bytes / sizeof(int));
    op = allocbuf;
    has_value = !!allocbuf;
  }

  arg::arg(const unsigned long *_ptr, size_t length_bytes)
  {
    allocbuf = _set_operand_integral_ptr(_ptr, length_bytes / sizeof(long));
    op = allocbuf;
    has_value = !!allocbuf;
  }


  /************************************************************************
   * Unit::exception functions.
   */

  template<size_t N>
  static void set_reason_fmt(Unit::exception *e, char (&reasonbuf)[N], const char *_reason_fmt, va_list vl)
  {
    /**
     * The reason format may be prefixed with a ~. This is so the format string
     * provided to Unit::exception constructors can be checked by the compiler
     * and not be flagged as invalid due to an empty input string.
     */
    if(_reason_fmt[0] == '~')
      _reason_fmt++;

    if(_reason_fmt[0] == '\0')
      return;

    vsnprintf(reasonbuf, sizeof(reasonbuf), _reason_fmt, vl);
    e->reason = reasonbuf;
    e->has_reason = true;
  }

  void exception::diagnostics()
  {
#ifdef PRINT_EXCEPTION_DIAGNOSTICS
    Uerr("  ***** unit::exception *****\n");
    Uerr("    at line %d: %s", line, test);

    if(has_reason)
      Uerr(" (%s)\n", reason);
    else
      Uerr("\n");

    if(has_values)
    {
      Uerr("    Left:  %s\n", left);
      Uerr("    Right: %s\n", right);
    }
    UerrFlush();
#endif
  }

  exception::exception(const exception &e):
   leftarg(e.leftarg), rightarg(e.rightarg)
  {
    memcpy(reasonbuf, e.reasonbuf, sizeof(reasonbuf));
    line = e.line;
    test = e.test;
    reason = e.reason;
    has_reason = e.has_reason;
    left = e.left;
    right = e.right;
    has_values = e.has_values;
  }

  exception::exception(int _line, const char *_test):
   line(_line), test(coalesce(_test)), reason("")
  {
    diagnostics();
  }

  exception::exception(int _line, const char *_test, const char *_reason_fmt, ...):
   line(_line), test(coalesce(_test))
  {
    if(_reason_fmt)
    {
      va_list vl;
      va_start(vl, _reason_fmt);
      set_reason_fmt(this, reasonbuf, _reason_fmt, vl);
      va_end(vl);
    }

    diagnostics();
  }

  exception::exception(int _line, const char *_test, Unit::arg &&_left, Unit::arg &&_right,
   const char *_reason_fmt, ...):
   leftarg(std::move(_left)), rightarg(std::move(_right)), line(_line), test(coalesce(_test))
  {
    if(_reason_fmt)
    {
      va_list vl;
      va_start(vl, _reason_fmt);
      set_reason_fmt(this, reasonbuf, _reason_fmt, vl);
      va_end(vl);
    }

    left = leftarg.op;
    right = rightarg.op;
    has_values = leftarg.has_value || rightarg.has_value;

    diagnostics();
  }


  /************************************************************************
   * Unit::unittest functions.
   */

  unittest::unittest(const char *_file, const char *_test_name):
   file_name(_file), test_name(_test_name)
  {
    unittestrunner::get().addtest(this);
  }

  void unittest::run_section(void)
  {
    try
    {
      run_impl();
    }
    catch(const Unit::skip &e)
    {
      this->skip();
    }
    catch(const Unit::exception &e)
    {
      this->print_exception(e);
    }
  }

  bool unittest::run()
  {
    assert(!has_run);
    has_run = true;

    // NOTE-- first time doesn't run any section if sections are present.
    this->failed_sections = 0;
    this->count_sections = 0;
    this->expected_section = 0;
    this->skipped_sections = 0;
    this->entire_test_skipped = false;
    run_section();

    if(has_failed_main)
      return false;

    if(this->entire_test_skipped)
    {
      print_test_skipped();
      return true;
    }

    this->num_sections = this->count_sections;

    while(this->expected_section < this->num_sections)
    {
      this->count_sections = 0;
      this->expected_section++;
      run_section();
    }

    if(this->entire_test_skipped ||
      (this->num_sections && this->skipped_sections == this->num_sections))
    {
      print_test_skipped();
      this->entire_test_skipped = true;
      return true;
    }

    if(this->last_failed_section)
    {
      unsigned int passed = this->passed_sections();
      Uerr("  Failed %u section(s)", this->failed_sections);

      if(this->skipped_sections)
      {
        Uerr(" (passed %u, skipped %u).\n", passed, this->skipped_sections);
      }
      else
        Uerr(" (passed %u).\n", passed);

      UerrFlush();
      return false;
    }

    print_test_success();
    return true;
  }

  int unittest::passed_sections()
  {
    return this->count_sections - this->failed_sections - this->skipped_sections;
  }

  void unittest::print_test_success(void)
  {
    const char *_test_name = coalesce(test_name);
    unsigned int passed = this->passed_sections();

    Uerr("Passed test '%s::%s'", file_name, _test_name);

    if(num_sections > 0)
    {
      Uerr(" (%u section%s", passed, (passed > 1) ? "s" : "");

      if(this->skipped_sections)
        Uerr(", %u skipped)\n", this->skipped_sections);
      else
        Uerr(")\n");
    }
    else
      Uerr("\n");

    UerrFlush();
  }

  void unittest::print_test_failed(void)
  {
    if(!printed_failed)
    {
      const char *_test_name = coalesce(test_name);

      Uerr("Failed test '%s::%s'\n", file_name, _test_name);
      printed_failed = true;
    }
  }

  void unittest::print_test_skipped(void)
  {
    const char *_test_name = coalesce(test_name);
    Uerr("Skipping test '%s::%s'", file_name, _test_name);

    if(this->skipped_sections)
    {
      Uerr(" (%u section%s)\n", this->skipped_sections,
        (this->skipped_sections > 0) ? "s" : "");
    }
    else
      Uerr("\n");

    UerrFlush();
  }

  void unittest::signal_fail()
  {
    const char *_test_name = coalesce(this->test_name);
    const char *_section_name = coalesce(this->section_name);

    if(this->expected_section)
    {
      Uerr("Test '%s::%s' aborted in section '%s' (#%u out of %u)\n",
        file_name, _test_name, _section_name, this->expected_section, this->num_sections);
    }
    else
      Uerr("Test '%s::%s' aborted\n", file_name, _test_name);
  }

  void unittest::print_exception(const Unit::exception &e)
  {
    const char *_section_name = coalesce(this->section_name);

    print_test_failed();

    if(this->expected_section)
    {
      if(this->last_failed_section < this->expected_section)
      {
        this->failed_sections++;
        Uerr("  In section '%s': \n", _section_name);
        this->last_failed_section = this->expected_section;
      }
      Uerr("    Assert failed at line %d: %s", e.line, e.test);
    }
    else
    {
      Uerr("  Assert failed at line %d: %s", e.line, e.test);
      this->has_failed_main = true;
    }

    if(e.has_reason)
      Uerr(" (%s)\n", e.reason);
    else
      Uerr("\n");

    if(e.has_values)
    {
      Uerr("    Left:  %s\n", e.left);
      Uerr("    Right: %s\n", e.right);
    }
    UerrFlush();
  }

  void unittest::skip()
  {
    if(!this->expected_section)
    {
      this->entire_test_skipped = true;
    }
    else
      this->skipped_sections++;
  }

  bool unittest::match_filter(const char *filter)
  {
    if(strstr(this->file_name, filter))
      return true;
    if(strstr(this->test_name, filter))
      return true;

    return false;
  }


  /************************************************************************
   * Unit::unittestrunner_cls and support functions.
   * Use singletons for globals to force initialization order.
   */

  static std::vector<unittest *> &gettests()
  {
    static std::vector<unittest *> tests;
    return tests;
  }

  static std::vector<const char *> &getfilters()
  {
    static std::vector<const char *> filters;
    return filters;
  }

  unittestrunner &unittestrunner::get()
  {
    static unittestrunner inst;
    return inst;
  }

  void unittestrunner::print_status()
  {
    if(!total)
    {
      Uerr("ERROR: no tests defined!\n\n");
      failed = 1;
      return;
    }

    if(total == count && total == passed && !failed && !skipped)
    {
      // Print a shorter summary for the general case...
      Uerr("Passed %u test%s.\n\n", total, (total > 1) ? "s" : "");
      UerrFlush();
      return;
    }

    Uerr("\nSummary:\n  Tests total: %u\n", total);

    if(passed)  Uerr("  Tests passed: %u\n", passed);
    if(failed)  Uerr("  Tests failed: %u\n", failed);
    if(skipped) Uerr("  Tests skipped: %u\n", skipped);

    Uerr("\n");
    UerrFlush();
  }

  bool unittestrunner::run()
  {
    const std::vector<const char *> &filters = getfilters();
    char execdir[1024];
    count = 0;
    passed = 0;
    failed = 0;
    skipped = 0;
    total = gettests().size();

    /* Fix static initialization ignoring link order. For now, this compares
     * pointers, which should preserve link order and function order. */
    std::stable_sort(gettests().begin(), gettests().end());

    getcwd(execdir, sizeof(execdir));

    for(unittest *t : gettests())
    {
      if(filters.size())
      {
        bool should_run = false;
        for(const char *filter : filters)
        {
          if(t->match_filter(filter))
          {
            should_run = true;
            break;
          }
        }
        if(!should_run)
        {
          skipped++;
          continue;
        }
      }
      chdir(execdir);

      count++;
      current_test = t;
      bool result = t->run();

      if(result)
      {
        if(!t->entire_test_skipped)
          passed++;
        else
          skipped++;
      }
      else
        failed++;
    }

    print_status();
    return !failed;
  }

  void unittestrunner::signal_fail()
  {
    if(current_test)
      current_test->signal_fail();
    else
      Uerr("ERROR: NULL test!\n");

    failed++;
    skipped += total - count;
    print_status();
  }

  void unittestrunner::addtest(unittest *t)
  {
    gettests().push_back(t);
  }

  void unittestrunner::set_filter(const char *filter)
  {
    getfilters().push_back(filter);
  }


  /************************************************************************
   * Unit::self_check
   *
   * Precheck Unit::arg and Unit::exception to make sure they will behave
   * as expected if one of the actual tests fails.
   */

  static bool match(const char *name, const Unit::arg &value, const char *expected, boolean has_value=true)
  {
    if(value.has_value != has_value)
    {
      Uerr("ERROR: self check '%s' failed: has_value %u != %u\n", name, value.has_value, has_value);
      return false;
    }
    else

    if(value.op == nullptr || expected == nullptr)
    {
      if(value.op != expected)
      {
        Uerr("ERROR: self check '%s' failed: %p != %p\n", name, (void *)value.op, (void *)expected);
        return false;
      }
    }
    else

    if(strcmp(value.op, expected))
    {
      Uerr("ERROR: self check '%s' failed: '%s' != '%s'\n", name, value.op, expected);
      return false;
    }
    return true;
  }

  static bool match(const char *name, const Unit::exception &e, int line,
   const char *test, const char *left, const char *right, const char *reason)
  {
    if(!e.test)
    {
      Uerr("ERROR: self check '%s' failed: NULL test\n", name);
      return false;
    }
    if(!e.left)
    {
      Uerr("ERROR: self check '%s' failed: NULL left\n", name);
      return false;
    }
    if(!e.right)
    {
      Uerr("ERROR: self check '%s' failed: NULL right\n", name);
      return false;
    }
    if(!e.reason)
    {
      Uerr("ERROR: self check '%s' failed: NULL reason\n", name);
      return false;
    }
    if(e.line != line)
    {
      Uerr("ERROR: self check '%s' failed: line %d != %d\n", name, e.line, line);
      return false;
    }
    if(strcmp(e.test, test))
    {
      Uerr("ERROR: self check '%s' failed: test '%s' != '%s'\n", name, e.test, test);
      return false;
    }
    if(strcmp(e.left, left))
    {
      Uerr("ERROR: self check '%s' failed: left '%s' != '%s'\n", name, e.left, left);
      return false;
    }
    if(strcmp(e.right, right))
    {
      Uerr("ERROR: self check '%s' failed: right '%s' != '%s'\n", name, e.right, right);
      return false;
    }
    if(strcmp(e.reason, reason))
    {
      Uerr("ERROR: self check '%s' failed: reason '%s' != '%s'\n", name, e.reason, reason);
      return false;
    }
    return true;
  }

  bool self_check()
  {
    /* Test Unit::arg. */
    if(!match("arg::default", Unit::arg(), nullptr, false))
      return false;

    if(!match("arg::nullptr", Unit::arg(nullptr), "NULL", false))
      return false;

    if(!match("arg::string", Unit::arg("abfjksdsfl"), "abfjksdsfl"))
      return false;

    if(!match("arg::void *", Unit::arg((const void *)0x12345678), "0x12345678"))
      return false;

    if(!match("arg::unsigned char", Unit::arg((unsigned char)255), "255"))
      return false;

    if(!match("arg::unsigned short", Unit::arg((unsigned short)65535), "65535"))
      return false;

    if(!match("arg::unsigned int", Unit::arg(65535U), "65535"))
      return false;

    if(!match("arg::unsigned long", Unit::arg(2147483647UL), "2147483647"))
      return false;

    Unit::arg ulln(18446744073709551615ULL);
    if(!match("arg::unsigned long long", ulln, "18446744073709551615"))
      return false;

    if(!match("arg::signed char", Unit::arg((signed char)-1), "-1"))
      return false;

    if(!match("arg::short", Unit::arg((signed short)-32767), "-32767"))
      return false;

    if(!match("arg::int", Unit::arg(-32767), "-32767"))
      return false;

    if(!match("arg::long", Unit::arg(-2147483647L), "-2147483647"))
      return false;

    Unit::arg lln(-9223372036854775807LL);
    if(!match("arg::long long", lln, "-9223372036854775807"))
      return false;

    int8_t values8s[] = { -1, -2, -3, -4, -5 };
    uint8_t values8[] = { 1, 2, 3, 4, 5 };
    uint16_t values16[] = { 0x1234, 0x5678, 0x9abc, 0xdef0 };
    uint32_t values32[] = { 0xfedc, 0xab98, 0x7654, 0x3210 };

    Unit::arg alloc8s(values8s, sizeof(values8s));
    if(!match("arg::int8_t array", alloc8s, "ff fe fd fc fb"))
      return false;

    Unit::arg alloc8(values8, sizeof(values8));
    if(!match("arg::uint8_t array", alloc8, "01 02 03 04 05"))
      return false;

    Unit::arg alloc16(values16, sizeof(values16));
    if(!match("arg::uint16_t array", alloc16, "1234 5678 9abc def0"))
      return false;

    Unit::arg alloc32(values32, sizeof(values32));
    if(!match("arg::uint32_t array", alloc32, "0000fedc 0000ab98 00007654 00003210"))
      return false;

    Unit::arg src(12345U);
    if(!match("arg::copy int dest", Unit::arg(src), "12345"))
      return false;
    if(!match("arg::copy int src", src, "12345"))
      return false;

    Unit::arg str("abcdef");
    if(!match("arg::copy str dest", Unit::arg(str), "abcdef"))
      return false;
    if(!match("arg::copy str src", str, "abcdef"))
      return false;

    if(!match("arg::copy array dest", Unit::arg(alloc8s), "ff fe fd fc fb"))
      return false;
    if(!match("arg::copy array src", alloc8s, "ff fe fd fc fb"))
      return false;

    if(!match("arg::move int dest", Unit::arg(std::move(src)), "12345"))
      return false;
    if(!match("arg::move int src", src, nullptr, false))
      return false;

    if(!match("arg::move str dest", Unit::arg(std::move(str)), "abcdef"))
      return false;
    if(!match("arg::move str src", str, nullptr, false))
      return false;

    if(!match("arg::move array dest", Unit::arg(std::move(alloc8)), "01 02 03 04 05"))
      return false;
    if(!match("arg::move array src", alloc8, nullptr, false))
      return false;

    /* Test Unit::exception. */

    if(!match("exception::fixed", Unit::exception(1, "Test name only."),
     1, "Test name only.", coalesce(nullptr), coalesce(nullptr), ""))
      return false;

    if(!match("exception::fail", Unit::exception(2, "Test name+reason", "%s%d", "abc", 123),
     2, "Test name+reason", coalesce(nullptr), coalesce(nullptr), "abc123"))
      return false;

    Unit::exception e_equal(3, "Test equal", Unit::arg("abc"), Unit::arg(456),
     "~Should remove the ~.");
    if(!match("exception::equal", e_equal, 3, "Test equal", "abc", "456", "Should remove the ~."))
      return false;

    Unit::exception e_memcmp(4, "Test memcmp",
     Unit::arg("0123", 4), Unit::arg("4567", 4), "mesg 0x%08x", 0x1234);
    if(!match("exception::memcmp", e_memcmp, 4, "Test memcmp",
     "30 31 32 33", "34 35 36 37", "mesg 0x00001234"))
      return false;

    Unit::exception e_noreason(5, "No reason!", Unit::arg(1), Unit::arg(2), "~");
    if(!match("exception::noreason", e_noreason, 5, "No reason!", "1", "2", coalesce(nullptr)))
      return false;

    return true;
  }
}

/***************************************************************************

 codesets.library - Amiga shared library for handling different codesets
 Copyright (C) 2001-2005 by Alfonso [alfie] Ranieri <alforan@tin.it>.
 Copyright (C) 2005-2021 codesets.library Open Source Team

 Extended Module Player modifications:
 Copyright (C) 2026 Lachesis <petrifiedrowan@gmail.com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 codesets.library project: http://sourceforge.net/projects/codesetslib/

***************************************************************************/

/* Workbench, AmigaOS, AROS, MorphOS require manually setting the
 * size of the stack. Partially based on libinit.c code from codesets.library.
 *
 * Define MZX_NO_STACKSWAP to disable StackSwap/NewStackSwap/NewPPCStackSwap.
 */
#include "../platform_main.h"
#undef main

#include <proto/exec.h>

#if defined(__GNUC__) && (__GNUC__ >= 4 || (__GNUC__ >= 3 && __GNUC_MINOR__ >= 1))
#define MZX_USED __attribute__((used))
#else
#define MZX_USED
#endif

#define MIN_STACK_SIZE 65536

/* AmigaOS 3.2, 4.x */
extern const char stack_cookie[];
const char MZX_USED stack_cookie[] = "$STACK: 65536";

/* libnix, when swapstack.o is included (must be located manually...). */
extern int __stack;
int MZX_USED __stack = MIN_STACK_SIZE;

/* Workbench 2.04 through 3.1: manually set stack size via StackSwap.
 * This needs to be performed through inline ASM to be safe.
 *
 * VBCC: no m68k asm for vbcc, but it responds to global var `__stack`
 * AROS: use NewStackSwap instead.
 * MorphOS: use NewPPCStackSwap instead.
 */
#if !defined(__amigaos4__) && !defined(__VBCC__) && !defined(MZX_NO_STACKSWAP)

int real_main(int argc, char **argv);

/* The inline ASM from codesets.library expects one function argument, so
 * it's easier to just use its semantics for now instead of rewriting it. */
struct main_args
{
  int argc;
  char **argv;
};

/* Work around -pedantic warnings */
union main_ptr_conv
{
  ULONG (*v_fn_ptr)(struct main_args *);
  APTR *v_ptr;
};

static ULONG main_wrapper(struct main_args *args)
{
  return real_main(args->argc, args->argv);
}

#if defined(__mc68000__) && !defined(__AROS__)

ULONG mzx_stackswap_call(struct StackSwapStruct *stack,
 ULONG (*main_fn)(struct main_args *args), struct main_args *args);

asm("\n\
  .text                       \n\
  .even                       \n\
  .globl _mzx_stackswap_call  \n\
_mzx_stackswap_call:          \n\
  moveml #0x3022,sp@-         \n\
  movel sp@(20),d3            \n\
  movel sp@(24),a2            \n\
  movel sp@(28),d2            \n\
  movel _SysBase,a6           \n\
  movel d3,a0                 \n\
  jsr a6@(-732:W)             \n\
  movel d2,sp@-               \n\
  jbsr a2@                    \n\
  movel d0,d2                 \n\
  addql #4,sp                 \n\
  movel _SysBase,a6           \n\
  movel d3,a0                 \n\
  jsr a6@(-732:W)             \n\
  movel d2,d0                 \n\
  moveml sp@+,#0x440c         \n\
  rts"
);

#elif defined(__AROS__)

static ULONG mzx_stackswap_call(struct StackSwapStruct *stack,
 ULONG (*main_fn)(struct main_args *args), struct main_args *args)
{
  struct StackSwapArgs sa;
  union main_ptr_conv cv;

  sa.Args[0] = (IPTR)args;
  cv.v_fn_ptr = main_fn;

  return NewStackSwap(stack, cv.v_ptr, &sa);
}

#elif defined(__MORPHOS__)

static ULONG mzx_stackswap_call(struct StackSwapStruct *stack,
 ULONG (*main_fn)(struct main_args *args), struct main_args *args)
{
  struct PPCStackSwapArgs sa;
  union main_ptr_conv cv;

  sa.Args[0] = (IPTR)args;
  cv.v_fn_ptr = main_fn;

  return NewPPCStackSwap(stack, cv.v_ptr, &sa);
}

#else
#error Unknown Amiga OS variant
#endif

int main(int argc, char **argv)
{
  struct StackSwapStruct sw;
  struct Task *tc;
  ULONG sz;

  tc = FindTask(NULL);

#ifdef __MORPHOS__
  NewGetTaskAttrsA(tc, &sz, sizeof(ULONG), TASKINFOTYPE_STACKSIZE, NULL);
#else
  sz = (UBYTE *)tc->tc_SPUpper - (UBYTE *)tc->tc_SPLower;
#endif

  if(sz < MIN_STACK_SIZE)
  {
    sw.stk_Lower = AllocVec(MIN_STACK_SIZE, MEMF_PUBLIC);
    if(sw.stk_Lower != NULL)
    {
      struct main_args args;
      int ret;

#ifdef __AROS__
      sw.stk_Upper = (APTR)((IPTR)sw.stk_Lower + MIN_STACK_SIZE);
#else
      sw.stk_Upper = (ULONG)sw.stk_Lower + MIN_STACK_SIZE;
#endif
      sw.stk_Pointer = (APTR)sw.stk_Upper;

#if 0
      fprintf(stderr, "Swapping stack from %u to %d...\n",
       (unsigned)sz, MIN_STACK_SIZE);
      fflush(stderr);
#endif

      args.argc = argc;
      args.argv = argv;
      ret = (int)mzx_stackswap_call(&sw, main_wrapper, &args);

      FreeVec(sw.stk_Lower);
      return ret;
    }
  }

  return real_main(argc, argv);
}

#endif /* !AmigaOS4 && !defined(MZX_NO_STACKSWAP) */

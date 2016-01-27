#ifndef MY_ATOMIC_INCLUDED
#define MY_ATOMIC_INCLUDED

/* Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  This header defines five atomic operations:

  my_atomic_add#(&var, what)
    'Fetch and Add'
    add 'what' to *var, and return the old value of *var

  my_atomic_fas#(&var, what)
    'Fetch And Store'
    store 'what' in *var, and return the old value of *var

  my_atomic_cas#(&var, &old, new)
    An odd variation of 'Compare And Set/Swap'
    if *var is equal to *old, then store 'new' in *var, and return TRUE
    otherwise store *var in *old, and return FALSE
    Usually, &old should not be accessed if the operation is successful.

  my_atomic_load#(&var)
    return *var

  my_atomic_store#(&var, what)
    store 'what' in *var

  '#' is substituted by a size suffix - 8, 16, 32, 64, or ptr
  (e.g. my_atomic_add8, my_atomic_fas32, my_atomic_casptr).

  NOTE This operations are not always atomic, so they always must be
  enclosed in my_atomic_rwlock_rdlock(lock)/my_atomic_rwlock_rdunlock(lock)
  or my_atomic_rwlock_wrlock(lock)/my_atomic_rwlock_wrunlock(lock).
  Hint: if a code block makes intensive use of atomic ops, it make sense
  to take/release rwlock once for the whole block, not for every statement.

  On architectures where these operations are really atomic, rwlocks will
  be optimized away.
  8- and 16-bit atomics aren't implemented for windows (see generic-msvc.h),
  but can be added, if necessary. 
*/

#ifndef my_atomic_rwlock_init

#define intptr         void *
/**
  Currently we don't support 8-bit and 16-bit operations.
  It can be added later if needed.
*/
#undef MY_ATOMIC_HAS_8_16

#ifndef MY_ATOMIC_MODE_RWLOCKS
/*
 * Attempt to do atomic ops without locks
 */
#include "atomic/nolock.h"
#endif

#ifndef make_atomic_cas_body
/* nolock.h was not able to generate even a CAS function, fall back */
#include "atomic/rwlock.h"
#endif

/* define missing functions by using the already generated ones */
#ifndef make_atomic_add_body
#define make_atomic_add_body(S)                                 \
  int ## S tmp=*a;                                              \
  while (!my_atomic_cas ## S(a, &tmp, tmp+v)) ;                 \
  v=tmp;
#endif
#ifndef make_atomic_fas_body
#define make_atomic_fas_body(S)                                 \
  int ## S tmp=*a;                                              \
  while (!my_atomic_cas ## S(a, &tmp, v)) ;                     \
  v=tmp;
#endif
#ifndef make_atomic_load_body
#define make_atomic_load_body(S)                                \
  ret= 0; /* avoid compiler warning */                          \
  (void)(my_atomic_cas ## S(a, &ret, ret));
#endif
#ifndef make_atomic_store_body
#define make_atomic_store_body(S)                               \
  (void)(my_atomic_fas ## S (a, v));
#endif

/*
  transparent_union doesn't work in g++
  Bug ?

  Darwin's gcc doesn't want to put pointers in a transparent_union
  when built with -arch ppc64. Complains:
  warning: 'transparent_union' attribute ignored
*/
#if defined(__GNUC__) && !defined(__cplusplus) && \
      ! (defined(__APPLE__) && (defined(_ARCH_PPC64) ||defined (_ARCH_PPC)))
/*
  we want to be able to use my_atomic_xxx functions with
  both signed and unsigned integers. But gcc will issue a warning
  "passing arg N of `my_atomic_XXX' as [un]signed due to prototype"
  if the signedness of the argument doesn't match the prototype, or
  "pointer targets in passing argument N of my_atomic_XXX differ in signedness"
  if int* is used where uint* is expected (or vice versa).
  Let's shut these warnings up
*/
#define make_transparent_unions(S)                              \
        typedef union {                                         \
          int  ## S  i;                                         \
          uint ## S  u;                                         \
        } U_ ## S   __attribute__ ((transparent_union));        \
        typedef union {                                         \
          int  ## S volatile *i;                                \
          uint ## S volatile *u;                                \
        } Uv_ ## S   __attribute__ ((transparent_union));
#define uintptr intptr
make_transparent_unions(8)
make_transparent_unions(16)
make_transparent_unions(32)
make_transparent_unions(64)
make_transparent_unions(ptr)
#undef uintptr
#undef make_transparent_unions
#define a       U_a.i
#define cmp     U_cmp.i
#define v       U_v.i
#define set     U_set.i
#else
#define U_8    int8
#define U_16   int16
#define U_32   int32
#define U_64   int64
#define U_ptr  intptr
#define Uv_8   int8
#define Uv_16  int16
#define Uv_32  int32
#define Uv_64  int64
#define Uv_ptr intptr
#define U_a    volatile *a
#define U_cmp  *cmp
#define U_v    v
#define U_set  set
#endif /* __GCC__ transparent_union magic */

#define make_atomic_cas(S)                                      \
static inline int my_atomic_cas ## S(Uv_ ## S U_a,              \
                            Uv_ ## S U_cmp, U_ ## S U_set)      \
{                                                               \
  int8 ret;                                                     \
  make_atomic_cas_body(S);                                      \
  return ret;                                                   \
}

#define make_atomic_add(S)                                      \
static inline int ## S my_atomic_add ## S(                      \
                        Uv_ ## S U_a, U_ ## S U_v)              \
{                                                               \
  make_atomic_add_body(S);                                      \
  return v;                                                     \
}

#define make_atomic_fas(S)                                      \
static inline int ## S my_atomic_fas ## S(                      \
                         Uv_ ## S U_a, U_ ## S U_v)             \
{                                                               \
  make_atomic_fas_body(S);                                      \
  return v;                                                     \
}

#define make_atomic_load(S)                                     \
static inline int ## S my_atomic_load ## S(Uv_ ## S U_a)        \
{                                                               \
  int ## S ret;                                                 \
  make_atomic_load_body(S);                                     \
  return ret;                                                   \
}

#define make_atomic_store(S)                                    \
static inline void my_atomic_store ## S(                        \
                     Uv_ ## S U_a, U_ ## S U_v)                 \
{                                                               \
  make_atomic_store_body(S);                                    \
}

#ifdef MY_ATOMIC_HAS_8_16
make_atomic_cas(8)
make_atomic_cas(16)
#endif
make_atomic_cas(32)
make_atomic_cas(64)
make_atomic_cas(ptr)

#ifdef MY_ATOMIC_HAS_8_16
make_atomic_add(8)
make_atomic_add(16)
#endif
make_atomic_add(32)
make_atomic_add(64)

#ifdef MY_ATOMIC_HAS_8_16
make_atomic_load(8)
make_atomic_load(16)
#endif
make_atomic_load(32)
make_atomic_load(64)
make_atomic_load(ptr)

#ifdef MY_ATOMIC_HAS_8_16
make_atomic_fas(8)
make_atomic_fas(16)
#endif
make_atomic_fas(32)
make_atomic_fas(64)
make_atomic_fas(ptr)

#ifdef MY_ATOMIC_HAS_8_16
make_atomic_store(8)
make_atomic_store(16)
#endif
make_atomic_store(32)
make_atomic_store(64)
make_atomic_store(ptr)

#ifdef _atomic_h_cleanup_
#include _atomic_h_cleanup_
#undef _atomic_h_cleanup_
#endif

#undef U_8
#undef U_16
#undef U_32
#undef U_64
#undef U_ptr
#undef Uv_8
#undef Uv_16
#undef Uv_32
#undef Uv_64
#undef Uv_ptr
#undef a
#undef cmp
#undef v
#undef set
#undef U_a
#undef U_cmp
#undef U_v
#undef U_set
#undef make_atomic_add
#undef make_atomic_cas
#undef make_atomic_load
#undef make_atomic_store
#undef make_atomic_fas
#undef make_atomic_add_body
#undef make_atomic_cas_body
#undef make_atomic_load_body
#undef make_atomic_store_body
#undef make_atomic_fas_body
#undef intptr

#define MY_ATOMIC_OK       0
#define MY_ATOMIC_NOT_1CPU 1
C_MODE_START
extern int my_atomic_initialize();
C_MODE_END

#endif

/*
  the macro below defines (as an expression) the code that
  will be run in spin-loops. Intel manuals recummend to have PAUSE there.
*/
#ifdef HAVE_PAUSE_INSTRUCTION
   /*
      According to the gcc info page, asm volatile means that the instruction
      has important side-effects and must not be removed.  Also asm volatile may
      trigger a memory barrier (spilling all registers to memory).
   */
#  define MY_PAUSE() __asm__ __volatile__ ("pause")
# elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#  define UT_PAUSE() __asm__ __volatile__ ("rep; nop")
# elif defined(_MSC_VER)
   /*
      In the Win32 API, the x86 PAUSE instruction is executed by calling the
      YieldProcessor macro defined in WinNT.h. It is a CPU architecture-
      independent way by using YieldProcessor.
   */
#  define MY_PAUSE() YieldProcessor()
# else
#  define MY_PAUSE() ((void) 0)
#endif

/*
  POWER-specific macros to relax CPU threads to give more core resources to
  other threads executing in the core.
*/
#if defined(HAVE_HMT_PRIORITY_INSTRUCTION)
#  define MY_LOW_PRIORITY_CPU() __asm__ __volatile__ ("or 1,1,1")
#  define MY_RESUME_PRIORITY_CPU() __asm__ __volatile__ ("or 2,2,2")
#else
#  define MY_LOW_PRIORITY_CPU() ((void)0)
#  define MY_RESUME_PRIORITY_CPU() ((void)0)
#endif

/*
  my_yield_processor (equivalent of x86 PAUSE instruction) should be used to
  improve performance on hyperthreaded CPUs. Intel recommends to use it in spin
  loops also on non-HT machines to reduce power consumption (see e.g
  http://softwarecommunity.intel.com/articles/eng/2004.htm)

  Running benchmarks for spinlocks implemented with InterlockedCompareExchange
  and YieldProcessor shows that much better performance is achieved by calling
  YieldProcessor in a loop - that is, yielding longer. On Intel boxes setting
  loop count in the range 200-300 brought best results.
 */
#define MY_YIELD_LOOPS 200

static inline int my_yield_processor()
{
  int i;

  MY_LOW_PRIORITY_CPU();

  for (i= 0; i < MY_YIELD_LOOPS; i++)
  {
    MY_COMPILER_BARRIER();
    MY_PAUSE();
  }

  MY_RESUME_PRIORITY_CPU();

  return 1;
}
#endif /* MY_ATOMIC_INCLUDED */

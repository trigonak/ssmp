/*   
 *   File: ssmp_tile.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: definitions specific to tilera architectures
 *   ssmp_tile.h is part of ssmp
 *
 * The MIT License (MIT)
 *
 * Copyright (C) 2013  Vasileios Trigonakis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _SSMP_TILE_H_
#define _SSMP_TILE_H_

#include <tmc/alloc.h>
#include <tmc/cpus.h>
#include <tmc/task.h>
#include <tmc/udn.h>
#include <tmc/spin.h>
#include <tmc/sync.h>
#include <tmc/cmem.h>
#include <arch/cycle.h> 

#define SSMP_CHUNK_SIZE 8192

#define TILE_SMALL_MSG
#define TILE_1WORD_MSG__

#if !defined(PREFETCH) 
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x)
#  define PREFETCHT0(x)
#  define PREFETCHT1(x)
#  define PREFETCHT2(x)
#endif	/* PREFETCH */

#if defined(__tilepro__)
#  if defined(TILE_SMALL_MSG)
#    define SSMP_CACHE_LINE_W    4
#  else
#    define SSMP_CACHE_LINE_W    16
#  endif
#elif defined(__tilegx__)
#  if defined(TILE_SMALL_MSG)
#    define SSMP_CACHE_LINE_W   2
#  elif defined(TILE_1WORD_MSG)
#    define SSMP_CACHE_LINE_W   1
#  else
#    define SSMP_CACHE_LINE_W   8
#  endif
#endif

#if defined(TILE_SMALL_MSG)
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg 
{
  int32_t w0;
  int32_t w1;
  int32_t w2;
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint32_t sender;
  };
} ssmp_msg_t;

typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_buf
{
  char data[15];
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint8_t sender;
  };
} ssmp_buf_t;

#elif defined(TILE_1WORD_MSG)
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg 
{
  int32_t w0;
  union 
  {
    int32_t w1;
    SSMP_FLAG_TYPE state;
    volatile uint8_t sender;
  };
} ssmp_msg_t;

typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_buf
{
  char data[7];
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint8_t sender;
  };
} ssmp_buf_t;

#else
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg 
{
  int32_t w0;
  int32_t w1;
  int32_t w2;
  int32_t w3;
  int32_t w4;
  int32_t w5;
  int32_t w6;
  int32_t w7;
  int32_t f[6];
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint32_t sender;
    volatile uint64_t pad;
  };
} ssmp_msg_t;

typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_buf
{
  char data[SSMP_CACHE_LINE_SIZE - 1];
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint8_t sender;
  };
} ssmp_buf_t;
#endif

typedef tmc_sync_barrier_t ssmp_barrier_t;

extern cpu_set_t cpus;
#if defined(__tilepro__)
#  define SSMP_MSG_NUM_WORDS 16
#else
#  define SSMP_MSG_NUM_WORDS 8
#endif


/*********************************************************************************
  memory stuff
*********************************************************************************/

#include <arch/atomic.h>
#include <arch/cycle.h>

#define _mm_lfence() arch_atomic_read_barrier()
#define _mm_sfence() arch_atomic_write_barrier()
#define _mm_mfence() arch_atomic_full_barrier()
#define _mm_clflush(x)   tmc_mem_finv_no_fence((const void*) x, 64);
#define PAUSE cycle_relax()
#define _mm_pause() PAUSE

#endif	/* _SSMP_TILE_H_ */

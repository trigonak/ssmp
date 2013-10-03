/*   
 *   File: ssmp_sparc.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: definitions specific to sparc architectures
 *   ssmp_sparc.h is part of ssmp
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

#ifndef _SSMP_SPARC_H_
#define _SSMP_SPARC_H_

#define SPARC_SMALL_MSG
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>

#if defined(__sparcv8)
#  define SSMP_CHUNK_SIZE 1024
#else
#  define SSMP_CHUNK_SIZE 8192
#endif

#if !defined(PREFETCH) 
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x)
#  define PREFETCHT0(x)
#  define PREFETCHT1(x)
#  define PREFETCHT2(x)
#endif	/* PREFETCH */

#if defined(SPARC_SMALL_MSG)
#  define SSMP_CACHE_LINE_DW   2
#else
#  define SSMP_CACHE_LINE_DW   7
#endif

#define _mm_pause() PAUSE

#if defined(SPARC_SMALL_MSG)
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_msg 
{
  int32_t w0;
  int32_t w1;
  int32_t w2;
  int32_t w3;
  uint8_t pad[3];
  union 
  {
    SSMP_FLAG_TYPE state;
    volatile uint8_t sender;
  };
} ssmp_msg_t;

typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE) ssmp_buf
{
  char data[19];
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

/* barrier type */
typedef struct ALIGNED(SSMP_CACHE_LINE_SIZE)
{
  volatile uint64_t participants; /* the participants of a barrier can be given
				     either by this, as bits (0 -> no, 1 ->participate */
  int (*color)(int); /* or as a color function: if the function return 0 -> no participant, 
			1 -> participant. The color function has priority over the lluint participants*/
  volatile uint32_t ticket;
  volatile uint32_t cleared;
} ssmp_barrier_t;

static inline void
memcpy64(volatile uint64_t* dst, const uint64_t* src, const size_t dwords)
{
#if defined(SPARC_SMALL_MSG)
  dst[0] = src[0];
  dst[1] = src[1];
#else
  uint32_t w;
  for (w = 0; w < dwords; w++)
    {
      *dst++ = *src++;
    }
  _mm_mfence();
#endif
}

/*********************************************************************************
  memory stuff
*********************************************************************************/
#include <atomic.h>

#define _mm_lfence() asm volatile("membar #LoadLoad | #LoadStore");
#define _mm_sfence() asm volatile("membar #StoreLoad | #StoreStore"); 
#define _mm_mfence() asm volatile("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore"); 
#define _mm_clflush(x) asm volatile("nop"); 
#define PAUSE    asm volatile("rd    %%ccr, %%g0\n\t" \
                    ::: "memory")

#endif	/* _SSMP_SPARC_H_ */

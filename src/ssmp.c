/*   
 *   File: ssmp.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: initialization of ssmp and helper functions
 *   ssmp.c is part of ssmp
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

#include "ssmp.h"

//#define SSMP_DEBUG

/* ------------------------------------------------------------------------------- */
/* library variables */
/* ------------------------------------------------------------------------------- */

int ssmp_num_ues_;
int ssmp_id_;
ssmp_barrier_t* ssmp_barrier;
volatile int* ues_initialized;
static uint32_t ssmp_my_core;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void
ssmp_init(int num_procs)
{
  ssmp_init_platf(num_procs);
}

void
ssmp_mem_init(int id, int num_ues) 
{
  ssmp_mem_init_platf(id, num_ues);
}


void
ssmp_term() 
{
  ssmp_term_platf();
}

/* ------------------------------------------------------------------------------- */
/* color-based initialization fucntions */
/* ------------------------------------------------------------------------------- */

void
ssmp_color_buf_init(ssmp_color_buf_t* cbuf, int (*color)(int))
{
  ssmp_color_buf_init_platf(cbuf, color);
}


void
ssmp_color_buf_free(ssmp_color_buf_t* cbuf)
{
  ssmp_color_buf_free_platf(cbuf);
}



/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */

int
ssmp_color_app(int id)
{
  return ((id % 3) ? 1 : 0);
}

inline ssmp_barrier_t*
ssmp_get_barrier(int barrier_num)
{
  if (barrier_num < SSMP_NUM_BARRIERS)
    {
      return (ssmp_barrier + barrier_num);
    }
  else
    {
      return NULL;
    }
}

int
color(int id)
{
  return (10*id);
}

void
ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int))
{
  ssmp_barrier_init_platf(barrier_num, participants, color);
}

void 
ssmp_barrier_wait(int barrier_num) 
{
  ssmp_barrier_wait_platf(barrier_num);
}

/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

inline double
wtime(void)
{
  struct timeval t;
  gettimeofday(&t,NULL);
  return (double)t.tv_sec + ((double)t.tv_usec)/1000000.0;
}

inline void 
wait_cycles(uint64_t cycles)	/* FIX: need to make platform specific */
{
  ticks _start_ticks = getticks();
  ticks _end_ticks = 0;
  if (cycles > 2 * getticks_correction)
    {
      _end_ticks = _start_ticks + cycles - 2 * getticks_correction;
    }
    while (getticks() < _end_ticks);
}

inline void
_mm_pause_rep(uint32_t num_reps)
{
  while (num_reps--)
    {
      _mm_pause();
    }
}

void
set_cpu(int cpu) 
{
  set_cpu_platf(cpu);
  set_numa_platf(cpu);
}

inline uint32_t
get_cpu()
{
  return ssmp_my_core;
}

inline ticks
getticks(void)
{
  return getticks_platf();
}

ticks getticks_correction;

ticks 
getticks_correction_calc() 
{
#define GETTICKS_CALC_REPS 2000000
  ticks t_dur = 0;
  uint32_t i;
  for (i = 0; i < GETTICKS_CALC_REPS; i++) {
    ticks t_start = getticks();
    ticks t_end = getticks();
    t_dur += t_end - t_start;
  }
  getticks_correction = (ticks)(t_dur / (double) GETTICKS_CALC_REPS);
  return getticks_correction;
}

/* Round up to next higher power of 2 (return x if it's already a power */
/* of 2) for 32-bit numbers */
uint32_t 
pow2roundup(uint32_t x)
{
  if (x==0) return 1;
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x+1;
}

inline int
ssmp_id() 
{
  return ssmp_id_;
}

inline int
ssmp_num_ues() 
{
  return ssmp_num_ues_;
}


/*   
 *   File: tile/ssmp_arch.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: initialization of ssmp and helper functions on Tilera platforms
 *   tile/ssmp_arch.c is part of ssmp
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

uint8_t id_to_core[] =
  {
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47,
  };

/* ------------------------------------------------------------------------------- */
/* library variables */
/* ------------------------------------------------------------------------------- */

int ssmp_num_ues_;
int ssmp_id_;
int last_recv_from;
ssmp_barrier_t* ssmp_barrier;
volatile int* ues_initialized;
static uint32_t ssmp_my_core;

DynamicHeader* udn_header; //headers for messaging
cpu_set_t cpus;

/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void
ssmp_init_platf(int num_procs)
{
  ssmp_num_ues_ = num_procs;

  //initialize shared memory
  tmc_cmem_init(0);

  // Reserve the UDN rectangle that surrounds our cpus.
  if (tmc_udn_init(&cpus) < 0)
    tmc_task_die("Failure in 'tmc_udn_init(0)'.");

  ssmp_barrier = (tmc_sync_barrier_t* ) tmc_cmem_calloc(SSMP_NUM_BARRIERS, sizeof (tmc_sync_barrier_t));
  if (ssmp_barrier == NULL)
    {
      tmc_task_die("Failure in allocating mem for barriers");
    }

  uint32_t b;
  for (b = 0; b < SSMP_NUM_BARRIERS; b++)
    {
      tmc_sync_barrier_init(ssmp_barrier + b, num_procs);
    }

  if (tmc_cpus_count(&cpus) < num_procs)
    {
      tmc_task_die("Insufficient cpus (%d < %d).", tmc_cpus_count(&cpus), num_procs);
    }

  tmc_task_watch_forked_children(1);
}

void
ssmp_mem_init_platf(int id, int num_ues) 
{  
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;

  // Now that we're bound to a core, attach to our UDN rectangle.
  if (tmc_udn_activate() < 0)
    tmc_task_die("Failure in 'tmc_udn_activate()'.");

  udn_header = (DynamicHeader* ) memalign(SSMP_CACHE_LINE_SIZE, num_ues * sizeof (DynamicHeader));
  if (udn_header == NULL)
    {
      tmc_task_die("Failure in allocating dynamic headers");
    }

  int r;
  for (r = 0; r < num_ues; r++)
    {
      int _cpu = tmc_cpus_find_nth_cpu(&cpus, id_to_core[r]);
      DynamicHeader header = tmc_udn_header_from_cpu(_cpu);
      udn_header[r] = header;
    }
}


void
ssmp_term_platf() 
{
  if (tmc_udn_close() != 0)
    {
      tmc_task_die("Failure in 'tmc_udn_close()'.");
    }

  free(udn_header);
}




/* ------------------------------------------------------------------------------- */
/* color-based initialization fucntions */
/* ------------------------------------------------------------------------------- */

void
ssmp_color_buf_init_platf(ssmp_color_buf_t* cbuf, int (*color)(int))
{
}


void
ssmp_color_buf_free_platf(ssmp_color_buf_t* cbuf)
{
}

void
ssmp_barrier_init_platf(int barrier_num, long long int participants, int (*color)(int))
{
  if (barrier_num >= SSMP_NUM_BARRIERS)
    {
      return;
    }

  uint32_t n, num_part = 0;
  for (n = 0; n < ssmp_num_ues_; n++)
    {
      if (color(n))
	{
	  num_part++;
	}
    }

  tmc_sync_barrier_init(ssmp_barrier + barrier_num, num_part);
}

void 
ssmp_barrier_wait_platf(int barrier_num) 
{
  if (barrier_num >= SSMP_NUM_BARRIERS)
    {
      return;
    }

  tmc_sync_barrier_wait(ssmp_barrier + barrier_num);
}

/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

void
set_cpu_platf(int cpu) 
{
  ssmp_my_core = cpu;
    if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }
}

#include <arch/cycle.h>
inline ticks
getticks_platf()
{
  return get_cycle_count();
}

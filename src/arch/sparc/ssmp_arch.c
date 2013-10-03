/*   
 *   File: sparc/ssmp_arch.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: initialization of ssmp and helper functions on SPARC platforms
 *   sparc/ssmp_arch.c is part of ssmp
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

#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/fcntl.h>
 
uint8_t id_to_core[] =
  {
    0, 8, 16, 24, 32, 40, 48, 56,
    9, 17, 25, 33, 41, 49, 57,
    10, 18, 26, 34, 42, 50, 58,
    11, 19, 27, 35, 43, 51, 59,
    12, 20, 28, 36, 44, 52, 60,
    13, 21, 29, 37, 45, 53, 61,
    14, 22, 30, 38, 46, 54, 62,
    15, 23, 31, 39, 47, 55, 63,
    1, 2, 3, 4, 5, 6, 7
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

static ssmp_msg_t* ssmp_mem;
volatile ssmp_msg_t** ssmp_recv_buf;
volatile ssmp_msg_t** ssmp_send_buf;
static ssmp_chunk_t* ssmp_chunk_mem;
volatile ssmp_chunk_t** ssmp_chunk_buf;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void
ssmp_init_platf(int num_procs)
{

  //create the shared space which will be managed by the allocator
  uint32_t sizem, sizeb, sizeui, sizecnk, size;;

  sizem = (num_procs * num_procs) * sizeof(ssmp_msg_t);
  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  SSMP_INC_ALIGN(sizeb);
  sizeui = num_procs * sizeof(int);
  SSMP_INC_ALIGN(sizeui);
  sizecnk = num_procs * sizeof(ssmp_chunk_t);
  SSMP_INC_ALIGN(sizecnk);
  size = sizem + sizeb + sizeui + sizecnk;

  char keyF[100];
  sprintf(keyF, SSMP_MEM_NAME);

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0)
    {
      if (errno != EEXIST)
	{
	  perror("In shm_open");
	  exit(1);
	}

      //this time it is ok if it already exists
      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0)
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else
    {
      if (ftruncate(ssmpfd, size) < 0) 
	{
	  perror("ftruncate failed\n");
	  exit(1);
	}
    }

  ssmp_mem = (ssmp_msg_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if ((unsigned long) ssmp_mem % SSMP_CACHE_LINE_SIZE)
    {
      printf("** the messaging buffers are not cache aligned.\n");
    }
  if (ssmp_mem == NULL)
    {
      perror("ssmp_mem = NULL\n");
      exit(134);
    }

  char* mem_just_int = (char*) ssmp_mem;
  ssmp_barrier = (ssmp_barrier_t*) (mem_just_int + sizem);
  ues_initialized = (int*) (mem_just_int + sizem + sizeb);
  ssmp_chunk_mem = (ssmp_chunk_t*) (mem_just_int + sizem + sizeb + sizeui);

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) 
    {
      ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
    }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, ssmp_color_app);

  _mm_mfence();
}

void
ssmp_mem_init_platf(int id, int num_ues) 
{  
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;
  last_recv_from = (id + 1) % num_ues;

  ssmp_recv_buf = (volatile ssmp_msg_t**) memalign(SSMP_CACHE_LINE_SIZE, num_ues * sizeof(ssmp_msg_t*));
  ssmp_send_buf = (volatile ssmp_msg_t**) memalign(SSMP_CACHE_LINE_SIZE, num_ues * sizeof(ssmp_msg_t*));
  ssmp_chunk_buf = (volatile ssmp_chunk_t**) malloc(num_ues * sizeof(ssmp_chunk_t*));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL || ssmp_chunk_buf == NULL)
    {
      perror("malloc@ ssmp_mem_init\n");
      exit(-1);
    }

  int core;
  for (core = 0; core < num_ues; core++) 
    {
      ssmp_chunk_buf[core] = ssmp_chunk_mem + core;

      ssmp_recv_buf[core] = ssmp_mem + (id * num_ues) + core;
      ssmp_recv_buf[core]->state = 0;

      ssmp_send_buf[core] = ssmp_mem + (core * num_ues) + id;
    }

  ssmp_chunk_buf[id]->state = 0;

  ues_initialized[id] = 1;

  /* SP("waiting for all to be initialized!"); */
  int ue;
  for (ue = 0; ue < num_ues; ue++) 
    {
      while(!ues_initialized[ue]) 
	{
	  _mm_pause();
	  _mm_mfence();
	};
    }
  /* SP("\t\t\tall initialized!"); */
}

void
ssmp_term_platf() 
{
  if (ssmp_id_ == 0 && shm_unlink(SSMP_MEM_NAME) < 0)
    {
      printf("Could not unlink ssmp_mem\n");
      fflush(stdout);
    }

  free(ssmp_recv_buf);
  free(ssmp_send_buf);
  free(ssmp_chunk_buf);
}


/* ------------------------------------------------------------------------------- */
/* color-based initialization fucntions */
/* ------------------------------------------------------------------------------- */

void
ssmp_color_buf_init_platf(ssmp_color_buf_t* cbuf, int (*color)(int))
{
  uint32_t* participants = (uint32_t*) malloc(ssmp_num_ues_ * sizeof(uint32_t));
  if (participants == NULL)
    {
      perror("malloc @ ssmp_color_buf_init\n");
      exit(-1);
    }

  uint32_t ue, num_ues = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++)
    {
      if (ue == ssmp_id_)
	{
	  participants[ue] = 0;
	  continue;
	}

      participants[ue] = color(ue);
      if (participants[ue])
	{
	  num_ues++;
	}
    }

  cbuf->num_ues = num_ues;

  uint32_t size_buf = num_ues * sizeof(ssmp_msg_t*);
  uint32_t size_pad = 0;

  if (size_buf % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_buf % SSMP_CACHE_LINE_SIZE)) / sizeof(ssmp_msg_t*);
      size_buf += size_pad * sizeof(ssmp_msg_t*);
    }

  cbuf->buf = (volatile ssmp_msg_t**) malloc(size_buf);
  if (cbuf->buf == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }

  cbuf->buf_state = (volatile uint8_t**) malloc(size_buf);
  if (cbuf->buf_state == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
  
  uint32_t size_from = num_ues * sizeof(uint8_t);
  if (size_from % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_from % SSMP_CACHE_LINE_SIZE)) / sizeof(uint8_t);
      size_from += size_pad * sizeof(uint8_t);
    }


  cbuf->from = (uint8_t*) memalign(SSMP_CACHE_LINE_SIZE, size_from);
  if (cbuf->from == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
    
  uint32_t buf_num = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++)
    {
      if (participants[ue])
	{
	  cbuf->buf[buf_num] = ssmp_recv_buf[ue];
	  cbuf->buf_state[buf_num] = &ssmp_recv_buf[ue]->state;
	  cbuf->from[buf_num] = ue;
	  buf_num++;
	}
    }
  free(participants);
}


void
ssmp_color_buf_free_platf(ssmp_color_buf_t* cbuf)
{
  free(cbuf->buf);
  free(cbuf->buf_state);
  free(cbuf->from);
}

void
ssmp_barrier_init_platf(int barrier_num, long long int participants, int (*color)(int))
{
  if (barrier_num >= SSMP_NUM_BARRIERS)
    {
      return;
    }
  ssmp_barrier[barrier_num].participants = 0xFFFFFFFFFFFFFFFF;
  ssmp_barrier[barrier_num].color = color;
  ssmp_barrier[barrier_num].ticket = 0;
  ssmp_barrier[barrier_num].cleared = 0;
}

void 
ssmp_barrier_wait_platf(int barrier_num) 
{
  if (barrier_num >= SSMP_NUM_BARRIERS)
    {
      return;
    }

  _mm_mfence();
  ssmp_barrier_t* b = &ssmp_barrier[barrier_num];
  PD(">>Waiting barrier %d\t(v: %d)", barrier_num, version);

  int (*col)(int);
  col = b->color;

  uint64_t bpar = (uint64_t) b->participants;
  uint32_t num_part = 0;

  int from;
  for (from = 0; from < ssmp_num_ues_; from++) 
    {
      /* if there is a color function it has priority */
      if (col != NULL) 
	{
	  num_part += col(from);
	  if (from == ssmp_id_ && !col(from))
	    {
	      return;
	    }
	}
      else 
	{
	  uint32_t is_part = (uint32_t) (bpar & 0x0000000000000001);
	  num_part += is_part;
	  if (ssmp_id_ == from && !is_part)
	    {
	      return;
	    }
	  bpar >>= 1;
	}
    }
  
  _mm_lfence();
  uint32_t reps = 1;
  while (b->cleared == 1)
    {
      _mm_pause_rep(reps++);
      reps &= 255;
      _mm_lfence();
    }

  uint32_t my_ticket = atomic_inc_32_nv(&b->ticket);
  if (my_ticket == num_part)
    {
      b->cleared = 1;
    }

  _mm_mfence();

  reps = 1;
  while (b->cleared == 0)
    {
      _mm_pause_rep(reps++);
      reps &= 255;
      _mm_lfence();
    }
  
  my_ticket = atomic_dec_32_nv(&b->ticket);
  if (my_ticket == 0)
    {
      b->cleared = 0;
    }

  _mm_mfence();
  PD("<<Cleared barrier %d (v: %d)", barrier_num, version);
}

/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

void
set_cpu_platf(int cpu) 
{
  ssmp_my_core = cpu;
  processor_bind(P_LWPID,P_MYID, cpu, NULL);
}

inline ticks getticks_platf(void)
{
  ticks ret = 0;
  __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
  return ret;
}

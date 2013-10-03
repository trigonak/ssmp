/*   
 *   File: x86/ssmp_arch.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: initialization of ssmp and helper functions on x86 platforms
 *   x86/ssmp_arch.c is part of ssmp
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
ssmp_chunk_t** ssmp_chunk_buf;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void
ssmp_init_platf(int num_procs)
{
  //create the shared space which will be managed by the allocator
  unsigned int sizeb, sizeui, sizecnk, size;;

  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  SSMP_INC_ALIGN(sizeb);
  sizeui = num_procs * sizeof(int);
  SSMP_INC_ALIGN(sizeui);
  sizecnk = num_procs * sizeof(ssmp_chunk_t);
  SSMP_INC_ALIGN(sizecnk);  
  size = sizeb + sizeui + sizecnk;

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
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

  ssmp_mem = (ssmp_msg_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (ssmp_mem == NULL)
    {
      perror("ssmp_mem = NULL\n");
      exit(134);
    }

  char* mem_just_int = (char*) ssmp_mem;
  ssmp_barrier = (ssmp_barrier_t*) (mem_just_int);
  ues_initialized = (volatile int*) (mem_just_int + sizeb);
  ssmp_chunk_mem = (ssmp_chunk_t*) (mem_just_int + sizeb + sizeui);

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
  ssmp_chunk_buf = (ssmp_chunk_t**) malloc(num_ues * sizeof(ssmp_chunk_t*));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL || ssmp_chunk_buf == NULL)
    {
      perror("malloc@ ssmp_mem_init\n");
      exit(-1);
    }

  char keyF[100];
  unsigned int size = (num_ues - 1) * sizeof(ssmp_msg_t);
  unsigned int core;
  sprintf(keyF, "/ssmp_core%03d", id);
  
  if (num_ues == 1) return;

  
  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd < 0)
    {
      if (errno != EEXIST)
	{
	  perror("In shm_open");
	  exit(1);
	}

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

  ssmp_msg_t* tmp = (ssmp_msg_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if ((unsigned long) tmp % SSMP_CACHE_LINE_SIZE)
    {
      printf("** the messaging buffers are not cache aligned.\n");
    }
  if (tmp == NULL)
    {
      perror("tmp = NULL\n");
      exit(134);
    }

  for (core = 0; core < num_ues; core++)
    {
      ssmp_chunk_buf[core] = ssmp_chunk_mem + core;
      ssmp_chunk_buf[core]->state = 0;

      if (id == core)
	{
	  continue;
	}
      ssmp_recv_buf[core] = tmp + ((core > id) ? (core - 1) : core);
      ssmp_recv_buf[core]->state = 0;
    }

  /*********************************************************************************
    initialized own buffer
    ********************************************************************************
    */

  ssmp_barrier_wait(0);
  
  for (core = 0; core < num_ues; core++)
    {
      if (core == id)
	{
	  continue;
	}

      sprintf(keyF, "/ssmp_core%03d", core);
  
      int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd < 0)
	{
	  if (errno != EEXIST)
	    {
	      perror("In shm_open");
	      exit(1);
	    }

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

      ssmp_msg_t* tmp = (ssmp_msg_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
      if (tmp == NULL)
	{
	  perror("tmp = NULL\n");
	  exit(134);
	}

      ssmp_send_buf[core] = tmp + ((core < id) ? (id - 1) : id);
    }

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
      shm_unlink("/ssmp_mem");
    }
  char keyF[100];
  sprintf(keyF, "/ssmp_core%03d", ssmp_id_);
  shm_unlink(keyF);

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
  if (cbuf == NULL) 
    {
      return;
    }

  uint32_t* participants = (uint32_t*) malloc(ssmp_num_ues_ * sizeof(uint32_t));
  if (participants == NULL) 
    {
      perror("malloc @ ssmp_color_buf_init");
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
      size_pad = SSMP_CACHE_LINE_SIZE - (size_buf % SSMP_CACHE_LINE_SIZE);
      size_buf += size_pad;
    }

  cbuf->buf = (volatile ssmp_msg_t**) malloc(size_buf);
  if (cbuf->buf == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }

  cbuf->buf_state = (SSMP_FLAG_TYPE**) malloc(size_buf);
  if (cbuf->buf_state == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
  
  uint32_t size_from = num_ues * sizeof(uint8_t);

  if (size_from % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = SSMP_CACHE_LINE_SIZE - (size_from % SSMP_CACHE_LINE_SIZE);
      size_from += size_pad;
    }


  cbuf->from = (uint8_t*) malloc(size_from);
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

  uint32_t my_ticket = __sync_add_and_fetch(&b->ticket, 1);
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
  
  my_ticket = __sync_sub_and_fetch(&b->ticket, 1);
  if (my_ticket == 0)
    {
      b->cleared = 0;
    }

  _mm_mfence();
  PD("<<Cleared barrier %d (v: %d)", barrier_num, version);
}

/* ------------------------------------------------------------------------------- */
/* help functions */
/* ------------------------------------------------------------------------------- */

void
set_cpu_platf(int cpu) 
{
  ssmp_my_core = cpu;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
    {
      printf("Problem with setting processor affinity: %s\n",
	     strerror(errno));
      exit(3);
    }
}

#if defined(__i386__)
inline ticks getticks_platf(void)
{
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}
#elif defined(__x86_64__)
inline ticks getticks_platf(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#endif

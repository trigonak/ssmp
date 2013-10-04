/*   
 *   File: niagara/ssmp_platf.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: implementation of send, recv, etc. functions for UltraSPARC-T2
 *   niagara/ssmp_platf.c is part of ssmp
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

extern volatile ssmp_msg_t** ssmp_recv_buf;
extern volatile ssmp_msg_t** ssmp_send_buf;
extern ssmp_chunk_t** ssmp_chunk_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t* ssmp_barrier;

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_recv_from_platf(uint32_t from, volatile ssmp_msg_t* msg) 
{
  volatile ssmp_msg_t* tmpm = ssmp_recv_buf[from];
  while(tmpm->state != SSMP_BUF_MESSG) 
    {
      PAUSE;
    }
  
  memcpy64((volatile uint64_t*) msg, (const uint64_t*) tmpm, SSMP_CACHE_LINE_DW);
  tmpm->state = SSMP_BUF_EMPTY;
}

inline void 
ssmp_recv_platf(ssmp_msg_t* msg)
{
  uint32_t from;
  uint32_t num_ues = ssmp_num_ues_;
 
  while(1)
    {
      for (from = 0; from < num_ues; from++) 
	{
	  if (from != ssmp_id_ && ssmp_recv_buf[from]->state == SSMP_BUF_MESSG)
	    {
	      volatile ssmp_msg_t* tmpm = ssmp_recv_buf[from];
	      memcpy64((volatile uint64_t*) msg, (const uint64_t*) tmpm, SSMP_CACHE_LINE_DW);
	      msg->sender = from;

	      tmpm->state = SSMP_BUF_EMPTY;
	      return;
	    }
	}
    }
}



inline void 
ssmp_recv_color_platf(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  uint32_t num_ues = cbuf->num_ues;
  SSMP_FLAG_TYPE** cbuf_state = cbuf->buf_state;
  uint32_t start_recv_from;
  while(1) 
    {
      for (start_recv_from = 0; start_recv_from < num_ues; start_recv_from++)
	{
	  if (*cbuf_state[start_recv_from] == SSMP_BUF_MESSG)
	    {
	      volatile ssmp_msg_t* tmpm = cbuf->buf[start_recv_from];
	      memcpy64((volatile uint64_t*) msg, (const uint64_t*) tmpm, SSMP_CACHE_LINE_DW);
	      tmpm->state = SSMP_BUF_EMPTY;
	      msg->sender = cbuf->from[start_recv_from];

	      if (++start_recv_from == num_ues)
		{
		  start_recv_from = 0;
		}
	      return;
	    }
	}
    }
}


static uint32_t start_recv_from = 0; /* keeping from which core to start the recv from next */

inline void
ssmp_recv_color_start_platf(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  uint32_t num_ues = cbuf->num_ues;
  SSMP_FLAG_TYPE** cbuf_state = cbuf->buf_state;
  while(1) 
    {
      for (; start_recv_from < num_ues; start_recv_from++)
	{

	  if (*cbuf_state[start_recv_from] == SSMP_BUF_MESSG)
	    {
	      volatile ssmp_msg_t* tmpm = cbuf->buf[start_recv_from];

	      memcpy64((volatile uint64_t*) msg, (const uint64_t*) tmpm, SSMP_CACHE_LINE_DW);
	      tmpm->state = SSMP_BUF_EMPTY;
	      msg->sender = cbuf->from[start_recv_from];

	      /* _mm_sfence(); */
	      if (++start_recv_from == num_ues)
		{
		  start_recv_from = 0;
		}
	      return;
	    }
	}
      start_recv_from = 0;
    }
}
      

void
ssmp_recv_from_big_platf(int from, void* data, size_t length) 
{
  int last_chunk = length % SSMP_CHUNK_SIZE;
  int num_chunks = length / SSMP_CHUNK_SIZE;

  while(num_chunks--)
    {
      while(!ssmp_chunk_buf[from]->state)
	{
	  PAUSE;
	}

      memcpy(data, ssmp_chunk_buf[from], SSMP_CHUNK_SIZE);
      data = ((char*) data) + SSMP_CHUNK_SIZE;

      ssmp_chunk_buf[from]->state = 0;
    }

  if (!last_chunk)
    {
      return;
    }

  while(!ssmp_chunk_buf[from]->state)
    {
      PAUSE;
    }

  memcpy(data, ssmp_chunk_buf[from], last_chunk);
  ssmp_chunk_buf[from]->state = 0;

  PD("recved from %d\n", from);
}


/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_send_platf(uint32_t to, volatile ssmp_msg_t* msg) 
{
  volatile ssmp_msg_t* tmpm = ssmp_send_buf[to];
  while (tmpm->state != SSMP_BUF_EMPTY)
    {
      PAUSE;
    }

  memcpy64((volatile uint64_t*) tmpm, (const uint64_t*) msg, SSMP_CACHE_LINE_DW);
  tmpm->state = SSMP_BUF_MESSG;
}

inline int
ssmp_send_is_free_platf(uint32_t to) 
{
  volatile ssmp_msg_t* tmpm = ssmp_send_buf[to];
  return (tmpm->state == SSMP_BUF_EMPTY);
}


inline void
ssmp_send_no_sync_platf(uint32_t to, volatile ssmp_msg_t* msg) 
{
  volatile ssmp_msg_t* tmpm = ssmp_send_buf[to];
  memcpy64((volatile uint64_t*) tmpm, (const uint64_t*) msg, SSMP_CACHE_LINE_DW);
  tmpm->state = SSMP_BUF_MESSG;
}


void
ssmp_send_big_platf(int to, void* data, size_t length) 
{
  int last_chunk = length % SSMP_CHUNK_SIZE;
  int num_chunks = length / SSMP_CHUNK_SIZE;

  while(num_chunks--)
    {
      while(ssmp_chunk_buf[ssmp_id_]->state)
	{
	  PAUSE;
	}

      memcpy(ssmp_chunk_buf[ssmp_id_], data, SSMP_CHUNK_SIZE);
      data = ((char*) data) + SSMP_CHUNK_SIZE;

      ssmp_chunk_buf[ssmp_id_]->state = 1;
    }

  if (!last_chunk)
    {
      return;
    }

  while(ssmp_chunk_buf[ssmp_id_]->state)
    {
      PAUSE;
    }

  memcpy(ssmp_chunk_buf[ssmp_id_], data, last_chunk);

  ssmp_chunk_buf[ssmp_id_]->state = 1;

  PD("sent to %d", to);
}

void
set_numa_platf(int cpu)
{

}

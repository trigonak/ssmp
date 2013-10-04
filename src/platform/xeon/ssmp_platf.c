/*   
 *   File: xeon/ssmp_platf.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: implementation of send, recv, etc. functions for Intel Xeon
 *   xeon/ssmp_platf.c is part of ssmp
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

uint8_t id_to_core[] =
  {
    01, 02, 03, 04, 05, 06, 07,  8,  9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    00, 41, 42, 43, 44, 45, 46, 47, 48, 49, 
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255

  };

uint8_t id_to_node[] =
  {
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  };


/* ------------------------------------------------------------------------------- */
/* helper functions                                                                */
/* ------------------------------------------------------------------------------- */

static inline uint32_t
ssmp_cores_on_same_socket_platf(uint32_t core1, uint32_t core2)
{
  return (id_to_node[id_to_core[core1]] == id_to_node[id_to_core[core2]]);
}

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking                                       */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_recv_from_platf(uint32_t from, volatile ssmp_msg_t* msg) 
{
  volatile ssmp_msg_t* tmpm = ssmp_recv_buf[from];
  if (!ssmp_cores_on_same_socket_platf(ssmp_id_, from))
    {
      while (!__sync_bool_compare_and_swap(&tmpm->state, SSMP_BUF_MESSG, SSMP_BUF_LOCKD)) 
	{
	  wait_cycles(SSMP_WAIT_TIME);
	}
    }
  else			/* same socket */
    {
      int32_t wted = 0;
      _mm_lfence();
      while(tmpm->state != SSMP_BUF_MESSG) 
	{
	  _mm_pause_rep(wted++ & 63);
	  _mm_lfence();
	}
    }
  
  memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
  tmpm->state = SSMP_BUF_EMPTY;
  _mm_mfence();
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
	  if (from != ssmp_id_ &&
#  if USE_ATOMIC == 1
	      __sync_bool_compare_and_swap(&ssmp_recv_buf[from]->state, SSMP_BUF_MESSG, SSMP_BUF_LOCKD)
#  else
	      ssmp_recv_buf[from]->state == SSMP_BUF_MESSG
#  endif
	      )
	    {
	      volatile ssmp_msg_t* tmpm = ssmp_recv_buf[from];
	      memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
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
  uint32_t have_msg = 0;
  SSMP_FLAG_TYPE** cbuf_state = cbuf->buf_state;
  uint32_t num_ues = cbuf->num_ues;
  uint32_t start_recv_from;  
  while(1) 
    {
      for (start_recv_from = 0; start_recv_from < num_ues; start_recv_from++)
	{
	  if (!ssmp_cores_on_same_socket_platf(ssmp_id_, start_recv_from))
	    {
	      if(__sync_bool_compare_and_swap(cbuf_state[start_recv_from], SSMP_BUF_MESSG, SSMP_BUF_LOCKD))
		{
		  have_msg = 1;
		}
	    }
	  else
	    {
	      if (*cbuf_state[start_recv_from] == SSMP_BUF_MESSG)
		{
		  have_msg = 1;
		}
	    }

	  if (have_msg)
	    {
	      volatile ssmp_msg_t* tmpm = cbuf->buf[start_recv_from];
	      memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
	      msg->sender = cbuf->from[start_recv_from];

	      tmpm->state = SSMP_BUF_EMPTY;

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
  uint32_t have_msg = 0;
  SSMP_FLAG_TYPE** cbuf_state = cbuf->buf_state;
  uint32_t num_ues = cbuf->num_ues;
  while(1) 
    {
      for (; start_recv_from < num_ues; start_recv_from++)
	{
	  if (!ssmp_cores_on_same_socket_platf(ssmp_id_, start_recv_from))
	    {
	      if(__sync_bool_compare_and_swap(cbuf_state[start_recv_from], SSMP_BUF_MESSG, SSMP_BUF_LOCKD))
		{
		  have_msg = 1;
		}
	    }
	  else
	    {
	      if (*cbuf_state[start_recv_from] == SSMP_BUF_MESSG)
		{
		  have_msg = 1;
		}
	    }

	  if (have_msg)
	    {
	      volatile ssmp_msg_t* tmpm = cbuf->buf[start_recv_from];
	      memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
	      msg->sender = cbuf->from[start_recv_from];

	      tmpm->state = SSMP_BUF_EMPTY;

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
      

inline void
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

  while(!ssmp_chunk_buf[from]->state);

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
  if (!ssmp_cores_on_same_socket_platf(ssmp_id_, to))
    {
      while (!__sync_bool_compare_and_swap(&tmpm->state, SSMP_BUF_EMPTY, SSMP_BUF_LOCKD)) 
	{
	  wait_cycles(SSMP_WAIT_TIME);
	}
    }
  else
    {
      _mm_mfence();
      while (tmpm->state != SSMP_BUF_EMPTY)
	{
	  _mm_pause();
	  _mm_mfence();
	}
    }
  msg->state = SSMP_BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);
  _mm_mfence();
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
  msg->state = SSMP_BUF_MESSG;
  memcpy((void*) tmpm, (const void*) msg, SSMP_CACHE_LINE_SIZE);
}


inline void
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

  while(ssmp_chunk_buf[ssmp_id_]->state);

  memcpy(ssmp_chunk_buf[ssmp_id_], data, last_chunk);

  ssmp_chunk_buf[ssmp_id_]->state = 1;

  PD("sent to %d", to);
}

void
set_numa_platf(int cpu)
{
}

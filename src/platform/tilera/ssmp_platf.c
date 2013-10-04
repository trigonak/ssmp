/*   
 *   File: tilera/ssmp_platf.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: interfacing send, recv, etc. with the hw mp of the Tilera
 *   tilera/ssmp_platf.c is part of ssmp
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

extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t* ssmp_barrier;
extern DynamicHeader* udn_header; //headers for messaging

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_recv_from_platf(uint32_t from, volatile ssmp_msg_t* msg) 
{
  tmc_udn0_receive_buffer((void*) msg, SSMP_CACHE_LINE_W);
}


inline void
ssmp_recv_platf(ssmp_msg_t* msg) 
{
  tmc_udn0_receive_buffer((void*) msg, SSMP_CACHE_LINE_W);
}


inline void 
ssmp_recv_color_platf(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  tmc_udn0_receive_buffer((void*) msg, SSMP_CACHE_LINE_W);
}

inline void
ssmp_recv_color_start_platf(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  tmc_udn0_receive_buffer((void*) msg, SSMP_CACHE_LINE_W);
}
      

inline void
ssmp_recv_from_big_platf(int from, void* data, size_t length)
{
  printf("** ssmp_recv_from_big_platf currently not supported on the Tilera\n");
}


/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_send_platf(uint32_t to, volatile ssmp_msg_t* msg) 
{
  msg->sender = ssmp_id_;
  tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, (void*) msg, SSMP_CACHE_LINE_W);
}

inline int
ssmp_send_is_free_platf(uint32_t to) 
{
  return 1;			/* cannot really implement this on the Tilera */
}


inline void
ssmp_send_no_sync_platf(uint32_t to, volatile ssmp_msg_t* msg) 
{
  msg->sender = ssmp_id_;
  tmc_udn_send_buffer(udn_header[to], UDN0_DEMUX_TAG, (void*) msg, SSMP_CACHE_LINE_W);
}


inline void
ssmp_send_big_platf(int to, void* data, size_t length) 
{
  printf("** ssmp_send_from_big_platf currently not supported on the Tilera\n");
}

void
set_numa_platf(int cpu)
{
}

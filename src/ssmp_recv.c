/*   
 *   File: ssmp_recv.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: skeleton of receive functions (calling the platform specific
 *                implementations)
 *   ssmp_recv.c is part of ssmp
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
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline void
ssmp_recv_from(uint32_t from, volatile ssmp_msg_t* msg) 
{
  ssmp_recv_from_platf(from, msg);
}


inline void 
ssmp_recv(ssmp_msg_t* msg)
{
  ssmp_recv_platf(msg);
}


inline void 
ssmp_recv_color(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  ssmp_recv_color_platf(cbuf, msg);
}


inline void
ssmp_recv_color_start(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg)
{
  ssmp_recv_color_start_platf(cbuf, msg);
}

inline 
void ssmp_recv_from_big(int from, void* data, size_t length) 
{
  ssmp_recv_from_big_platf(from, data, length);
}
      

/*   
 *   File: measurements.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: interface of the simple profiler
 *   measurements.h is part of ssmp
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


#ifndef _MEASUREMENTS_H_
#define _MEASUREMENTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "ssmp.h"

extern ticks getticks_correction_calc();

#ifndef REF_SPEED_GHZ
#  if defined(OPTERON)
#    define REF_SPEED_GHZ           2.1
#  elif defined(XEON)
#    define REF_SPEED_GHZ           2.1
#  elif defined(TILERA)
#    define REF_SPEED_GHZ           1.2
#  elif defined(NIAGARA)
#    define REF_SPEED_GHZ           1.17
#  elif defined(COREi7)
#    define REF_SPEED_GHZ           2.8
#  else
#    warning "Need to set REF_SPEED_GHZ for the platform, using default of 2.1 GHz"
#    define REF_SPEED_GHZ           2.1
#  endif
#endif  /* REF_SPEED_GHZ */

#ifndef DO_TIMINGS
#  undef DO_TIMINGS_TICKS
#  undef DO_TIMINGS_STD
#  define PF_MSG(pos, msg)	/* set the message for position pos for results printing */
#  define PF_START(pos)		/* start measuring for position pos */
#  define PF_STOP(pos)		/* stop measuring for position pos */
#  define PF_PRINT_TICKS	/* report results */
#  define PF_PRINT		/* report results */
#  define PF_EXCLUDE(pos)    	/* exclude entry from the report */
#  define PF_CORRECTION 	/* calculate the necessary correction due to getticks */
#else
#define DO_TIMINGS_TICKS
#  define PF_MSG(pos, msg)        SET_PROF_MSG_POS(pos, msg)
#  define PF_START(pos)           ENTRY_TIME_POS(pos)
#  define PF_STOP(pos)            EXIT_TIME_POS(pos)
#  define PF_PRINT_TICKS          REPORT_TIMINGS
#  define PF_PRINT                REPORT_TIMINGS_SECS
#  define PF_EXCLUDE(pos)         EXCLUDE_ENTRY(pos)
#  define PF_CORRECTION           MEASUREREMENT_CORRECTION
#endif

#define PFIN                    PF_START
#define PFOUT                   PF_STOP

#include <stdio.h>
#include <math.h>

#if defined DO_TIMINGS_TICKS
  
#  include <stdint.h>

#  define ENTRY_TIMES_SIZE 16

    enum timings_bool_t {
        M_FALSE, M_TRUE
    };

  extern uint64_t entry_time[ENTRY_TIMES_SIZE];
  extern enum timings_bool_t entry_time_valid[ENTRY_TIMES_SIZE];
  extern uint64_t total_sum_ticks[ENTRY_TIMES_SIZE];
  extern long long total_samples[ENTRY_TIMES_SIZE];
  extern const char *measurement_msgs[ENTRY_TIMES_SIZE];
#  define MEASUREREMENT_CORRECTION getticks_correction_calc();
#  define SET_PROF_MSG(msg) SET_PROF_MSG_POS(0, msg) 
#  define ENTRY_TIME ENTRY_TIME_POS(0)
#  define EXIT_TIME EXIT_TIME_POS(0)

#  define SET_PROF_MSG_POS(pos, msg)		\
  measurement_msgs[pos] = msg;

#  define ENTRY_TIME_POS(position)					\
do {									\
  entry_time[position] = getticks();					\
  entry_time_valid[position] = M_TRUE;					\
} while (0);

#  define EXIT_TIME_POS(position)					\
  do {									\
    ticks exit_time = getticks();					\
    if (entry_time_valid[position]) {					\
      entry_time_valid[position] = M_FALSE;				\
      total_sum_ticks[position] += (exit_time - entry_time[position] - getticks_correction); \
      total_samples[position]++;					\
}} while (0);

#  define EXCLUDE_ENTRY(position)                                         \
        do {                                                            \
        total_samples[position] = 0;                                    \
        } while(0);

#  ifndef _MEASUREMENTS_ID_
#    define _MEASUREMENTS_ID_ -1
#  endif

#  define REPORT_TIMINGS REPORT_TIMINGS_RANGE(0,ENTRY_TIMES_SIZE)

#  define REPORT_TIMINGS_RANGE(start,end)				\
  do {                                                                  \
    int i;                                                              \
    for (i = start; i < end; i++) {					\
      if (total_samples[i]) {						\
	printf("[%02d]%s:\n", i, measurement_msgs[i]);			\
	printf("  samples: %-16llu| ticks: %-16llu| avg ticks: %-16llu\n", \
		total_samples[i], total_sum_ticks[i], total_sum_ticks[i] / total_samples[i]);\
        }                                                               \
    }                                                                   \
}                                                                       \
while (0);

#  define REPORT_TIMINGS_SECS REPORT_TIMINGS_SECS_RANGE(0, ENTRY_TIMES_SIZE)

#  define REPORT_TIMINGS_SECS_RANGE_(start,end)				\
  do {                                                                  \
    int i;                                                              \
    for (i = start; i < end; i++) {					\
      if (total_samples[i]) {						\
	printf("[%02d]%s:\n", i, measurement_msgs[i]);			\
	printf("  samples: %-16llu | secs: %-4.10f | avg ticks: %-4.10f\n", \
	       total_samples[i], total_sum_ticks[i] / (REF_SPEED_GHZ * 1.e9), \
	       (total_sum_ticks[i] / total_samples[i])/ (REF_SPEED_GHZ * 1.e9)); \
      }									\
    }                                                                   \
  }									\
  while (0);

#  define trunc

  extern void prints_ticks_stats(int start, int end);
#  define REPORT_TIMINGS_SECS_RANGE(start,end)	\
        prints_ticks_stats(start, end);


#else  /* !DO_TIMINGS_TICKS */
#  define ENTRY_TIME
#  define EXIT_TIME
#  define REPORT_TIMINGS
#  define REPORT_TIMINGS_RANGE(x,y)
#  define ENTRY_TIME_POS(X)
#  define EXIT_TIME_POS(X)
#endif

#ifdef	__cplusplus
}
#endif

#endif

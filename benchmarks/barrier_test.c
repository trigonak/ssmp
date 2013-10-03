#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <getopt.h>

#include "common.h"
#include "ssmp.h"

int num_procs = 2;
long long int num_reps = 100000;
uint8_t ID;

static inline unsigned long* 
seed_rand() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(3 * sizeof(unsigned long));
  seeds[0] = getticks() % 123456789;
  seeds[1] = getticks() % 362436069;
  seeds[2] = getticks() % 521288629;
  return seeds;
}

int
main(int argc, char **argv) 
{

  struct option long_options[] =
    {
      // These options don't set a flag
      {"help", no_argument, NULL, 'h'},
      {"num-reps", required_argument, NULL, 'r'},
      {"num-procs", required_argument, NULL, 'n'},
      {NULL, 0, NULL, 0}
    };

 int i, c;
 while (1)
   {
     i = 0;
     c = getopt_long(argc, argv, "hn:r:", long_options, &i);

     if (c == -1)
       break;

     if (c == 0 && long_options[i].flag == 0)
       c = long_options[i].val;

     switch (c)
       {
       case 0:
	 /* Flag is automatically set */
	 break;
       case 'h':
	 PRINT("barrier_test -- Testing the barriers\n"
	       "\n"
	       "Usage:\n"
	       "  ./barrier_test [options...]\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help\n"
	       "        Print this message\n"
	       "  -r, --num-reps <int>\n"
	       "        Number of repetitions\n"
	       "  -n, --num-procs <int>\n"
	       "        Number of processes\n"
	       );
	 exit(0);
       case 'r':
	 num_reps = atoi(optarg);
	 break;
       case 'n':
	 num_procs = atoi(optarg);
	 break;
       case '?':
	 PRINT("Use -h or --help for help\n");

	 exit(0);
       default:
	 exit(1);
       }
   }

  ID = 0;
  printf("NUM of processes      : %d\n", num_procs);
  printf("NUM of barrier crosses: %lld\n", num_reps);

  ssmp_init(num_procs);

  int rank;
  for (rank = 1; rank < num_procs; rank++) 
    {
      pid_t child = fork();
      if (child < 0)
	{
	  P("Failure in fork():\n%s", strerror(errno));
	}
      else if (child == 0)
	{
	  goto fork_done;
	}
    }
  rank = 0;

 fork_done:
  ID = rank;
  set_cpu(ID);
  ssmp_mem_init(ID, num_procs);

  unsigned long* seeds = seed_rand();
  
  ssmp_barrier_wait(0);

  ticks _start_ticks = getticks();
  double _start = wtime();

  int num = 0;
  while (num++ < num_reps) 
    {

      int barr;
      if (ID == 0)
      	{
	  barr = my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) % SSMP_NUM_BARRIERS;
	  ssmp_msg_t m;
	  m.w0 = barr;
	  ssmp_broadcast(&m);
	  ssmp_barrier_wait(barr);
      	}
      else
      	{
      	  ssmp_msg_t msg;
      	  ssmp_recv_from(0, &msg);
      	  barr = msg.w0;
      	  ssmp_barrier_wait(barr);
	}
    }

  ticks _end_ticks = getticks();
  double _end = wtime();

  ssmp_barrier_wait(10);

  free(seeds);

  if (ID == 0)
    {
      double _time = _end - _start;
      ticks _ticks = _end_ticks - _start_ticks;
      ticks _ticksm =(ticks) ((double)_ticks / num_reps);
      double lat = (double) (1000*1000*1000*_time) / num_reps;
      printf("crossed %lld barriers\n\t"
	     "in %f secs\n\t"
	     "%f ns latency\n\t"
	     "%llu ticks/crossing\n", num_reps, _time, lat, (long long unsigned) _ticksm);
    }
  ssmp_term();
  return 0;
}

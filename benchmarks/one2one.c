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

#include "measurements.h"

#define ROUNDTRIP_

int num_procs = 2;
long long int num_msgs = 10000000;
ticks getticks_correction;
uint8_t ID;
uint32_t wait_cycles_after = 0;
int core1 = 0;
int core2 = 1;
int core_offs = 0;

int
main(int argc, char **argv) 
{
  /* before doing any allocations */
#if defined(__tile__)
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
    }
#endif

  struct option long_options[] =
    {
      // These options don't set a flag
      {"help",        no_argument, NULL, 'h'},
      {"num-msgs",    required_argument, NULL, 'm'},
      {"num-procs",   required_argument, NULL, 'n'},
      {"delay-after", required_argument, NULL, 'd'},
      {"core1", required_argument, NULL, 'x'},
      {"core2", required_argument, NULL, 'y'},
      {"core-offset", required_argument, NULL, 'o'},
      {NULL, 0, NULL, 0}
    };

  int i, c;
  while (1)
    {
      i = 0;
      c = getopt_long(argc, argv, "hn:m:d:x:y:o:", long_options, &i);

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
	  PRINT("one2one -- Testing one-to-one communication \n"
		"\n"
		"Usage:\n"
		"  ./one2one[_rt] [options...]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"        Print this message\n"
		"  -n, --num-procs <int>\n"
		"        Number of processes\n"
		"  -m, --num-msgs <int>\n"
		"        Number of messages to send\n"
		"  -d, --delay-after <int>\n"
		"        How many cycles to pause after completing a request.\n"
		"  -x, --core1 <int>\n"
		"        On which core to put the receiver process\n"
		"  -y, --core2 <int>\n"
		"        On which core to put the sender process\n"
		"  -o, --core-offset <int>\n"
		"        If the app is executed with more than 2 processes, put\n"
		"        proc 0 on core1 (-x), proc 1 on core2 (-y) and the rest\n"
		"        on the consecutive cores starting from the given offset.\n"
		"        For example, if the offset is 2, proc 3 with be placed on\n"
		"        core 2, proc 4 on core 3, etc.\n"
		);
	  exit(0);
	case 'n':
	  num_procs = atoi(optarg);
	  break;
	case 'm':
	  num_msgs = atoi(optarg);
	  break;
	case 'd':
	  wait_cycles_after = atoi(optarg);
	  break;
	case 'x':
	  core1 = atoi(optarg);
	  break;
	case 'y':
	  core2 = atoi(optarg);
	  break;
	case 'o':
	  core_offs = atoi(optarg);
	  break;
	case '?':
	  PRINT("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }

  assert(num_procs % 2 == 0);

  ID = 0;
  printf("processes: %-10d / msgs: %10lld / delay after: %u\n", num_procs, num_msgs, wait_cycles_after);
  printf("core1: %3d / core2: %3d", core1, core2);
  if (num_procs > 2)
    {
      printf(" / core3: %3d / core4: %3d / etc.", core_offs, core_offs + 1);
    }
  printf("\n");
#if defined(ROUNDTRIP)
  PRINT("ROUNDTRIP");
#else
  PRINT("ONEWAY");
#endif  /* ROUNDTRIP */

  getticks_correction = getticks_correction_calc();

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

  uint32_t on = 0;
  if (ID == 0)
    {
      on = core1;
    }
  else if (ID == 1)
    {
      on = core2;
    }
  else
    {
      on = ID - 2 + core_offs;
    }

#if defined(TILERA)
  tmc_cmem_init(0);		/*   initialize shared memory */
#endif  /* TILERA */

  set_cpu(on);

  ssmp_mem_init(ID, num_procs);

  ssmp_barrier_wait(0);

  volatile ssmp_msg_t* msgp;
  msgp = (volatile ssmp_msg_t*) memalign(SSMP_CACHE_LINE_SIZE, sizeof(ssmp_msg_t));
  assert(msgp != NULL);
  
  PF_MSG(0, "receiving");
  PF_MSG(1, "sending");

  /* PFDINIT(num_msgs); */

  ssmp_barrier_wait(0);

  /********************************************************************************* 
   * main part
   *********************************************************************************/
  if (ID % 2 == 0)		/* recv -> send */
    {
      uint32_t from = ID + 1;
      uint32_t out = num_msgs - 1;
      uint32_t expected = 0;

      PF_START(0);
      while(1) 
	{
	  ssmp_recv_from(from, msgp);

#if defined(ROUNDTRIP)
	  ssmp_send(from, msgp);
#endif

	  if (msgp->w0 == out) 
	    {
	      break;
	    }

	  if (msgp->w0 != expected++)
	    {
	      PRINT(" *** expected %5u, got %5d", expected, msgp->w0);
	    }
	}
      PF_STOP(0);
	
      total_samples[0] = msgp->w0 + 1;
    }
  else 				/* send -> recv */
    {
      uint32_t to = ID - 1;
      uint32_t num_msgs1 = num_msgs;

      for (num_msgs1 = 0; num_msgs1 < num_msgs; num_msgs1++)
	{
	  msgp->w0 = num_msgs1;

	  ssmp_send(to, msgp);

#if defined(ROUNDTRIP)
	  ssmp_recv_from(to, msgp);

	  if (msgp->w0 != num_msgs1)
	    {
	      PRINT(" *** expected %5u, got %5d", num_msgs1, msgp->w0);
	    }
#endif

	  if (wait_cycles_after)
	    {
	      wait_cycles(wait_cycles_after);
	    }
	}
    }


  uint32_t co;
  for (co = 0; co < ssmp_num_ues(); co++)
    {
      if (co == ssmp_id())
	{
	  PF_PRINT;
	  if (total_sum_ticks[0] > 0)
	    {
	      double secs = total_sum_ticks[0] / (REF_SPEED_GHZ * 1.e9);
	      printf("[%02d] Throughput (core): %.1f", ID, num_msgs/secs);
#if defined(ROUNDTRIP)
	      printf(" Roundtrips/s\n");
#else
	      printf(" Messages/s\n");
#endif
	    }
	}
      ssmp_barrier_wait(0);
    }

  free((void*) msgp);
  ssmp_term();
  return 0;
}


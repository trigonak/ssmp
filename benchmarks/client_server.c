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
#define DEBUG_
#define NO_SYNC_SRV_

uint32_t num_msgs = 5000000;
uint8_t dsl_seq[256];
uint8_t num_procs = 2;
uint8_t ID;
uint8_t num_dsl = 0;
uint8_t num_app = 0;
uint8_t dsl_per_core = 2;
uint32_t delay_after = 0;
uint32_t delay_cs = 0;

int 
color_all(int id)
{
  return 1;
}

int 
color_dsl(int id)
{
  return (id % dsl_per_core == 0);
}

uint8_t 
nth_dsl(uint8_t id)
{
  uint8_t i, id_seq = 0;
  for (i = 0; i < ssmp_num_ues(); i++)
    {
      if (i == id)
	{
	  return id_seq;
	}
      id_seq++;
    }
  return id_seq;
}

int 
color_app1(int id)
{
  return !(color_dsl(id));
}


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
      {"server",      required_argument, NULL, 's'},
      {"delay-after", required_argument, NULL, 'd'},
      {"delay-cs",    required_argument, NULL, 'c'},
      {NULL, 0, NULL, 0}
    };

  int i, c;
  while (1)
    {
      i = 0;
      c = getopt_long(argc, argv, "hn:s:m:d:c:", long_options, &i);

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
	  PRINT("client_server -- Testing client-server communication \n"
		"\n"
		"Usage:\n"
		"  ./client_server[_rt] [options...]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"        Print this message\n"
		"  -n, --num-procs <int>\n"
		"        Number of processes\n"
		"  -m, --num-msgs <int>\n"
		"        Number of messages to send\n"
		"  -s, --server <int>\n"
		"        1 server core per app cores\n"
		"  -d, --delay-after <int>\n"
		"        How many cycles to pause after completing a request.\n"
		"  -c, --delay-cs <int>\n"
		"        How long to wait (cycles) before sending a response.\n"
		);
	  exit(0);
	case 'n':
	  num_procs = atoi(optarg);
	  break;
	case 'm':
	  num_msgs = atoi(optarg);
	  break;
	case 's':
	  dsl_per_core = atoi(optarg);
	  break;
	case 'd':
	  delay_after = atoi(optarg);
	  break;
	case 'c':
	  delay_cs = atoi(optarg);
	  break;
	case '?':
	  PRINT("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }

  ID = 0;

  printf("processes: %-10d / msgs: %10u\n", num_procs, num_msgs);
  printf("Delay after each message : %u\n", delay_after);
  printf("Delay after in the cs    : %u\n", delay_cs);
#if defined(ROUNDTRIP)
  PRINT("ROUNDTRIP");
#else
  PRINT("ONEWAY");
#endif  /* ROUNDTRIP */

  uint8_t j, dsl_seq_idx = 0;;
  for (j = 0; j < num_procs; j++)
    {
      if (color_dsl(j))
	{
	  num_dsl++;
	  dsl_seq[dsl_seq_idx++] = j;
	}
      else
	{
	  num_app++;
	}
    }

  PRINT("dsl: %2d | app: %d", num_dsl, num_app);

  ssmp_init(num_procs);

  ssmp_barrier_init(2, 0, color_dsl);
  ssmp_barrier_init(1, 0, color_app1);
#if defined(XEON)
  ssmp_barrier_init(0, 0, color_all);
  ssmp_barrier_init(5, 0, color_all);
#endif

  uint8_t rank;
  for (rank = 1; rank < num_procs; rank++) 
    {
      pid_t child = fork();
      if (child < 0) {
	P("Failure in fork():\n%s", strerror(errno));
      } else if (child == 0) 
	{
	  goto fork_done;
	}
    }
  rank = 0;

 fork_done:
  ID = rank;

  set_cpu(id_to_core[ID]);
  ssmp_mem_init(ID, num_procs);

  ssmp_color_buf_t *cbuf = NULL;
  if (color_dsl(ID)) 
    {
      cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
      assert(cbuf != NULL);
      ssmp_color_buf_init(cbuf, color_app1);
    }

  ssmp_msg_t *msg;
  msg = (ssmp_msg_t *) memalign(SSMP_CACHE_LINE_SIZE, sizeof(ssmp_msg_t));
  assert(msg != NULL);

  PF_MSG(0, "recv");
  PF_MSG(1, "send");
  PF_MSG(2, "roundtrip");


  uint32_t num_zeros = num_app;
  uint32_t lim_zeros = num_dsl;

  ticks t_start = 0, t_end = 0;

  ssmp_barrier_wait(0);
  /* ********************************************************************************
     main functionality
  *********************************************************************************/

  if (color_dsl(ID)) 
    {
      ssmp_barrier_wait(0);

      PF_START(2);
      while(1) 
	{
	  ssmp_recv_color_start(cbuf, msg);

	  if (delay_cs)
	    {
	      wait_cycles(delay_cs);
	    }

#if defined(ROUNDTRIP)
#  if defined(NO_SYNC_SRV)
	  ssmp_send_no_sync(msg->sender, msg);
#  else
	  ssmp_send(msg->sender, msg);
#  endif
#endif  /* ROUNDTRIP */
	  
	  if (msg->w0 < lim_zeros)
	    {
	      if (--num_zeros == 0)
		{
		  break;
		}
	    }
	}
      PF_STOP(2);
      total_samples[2] = num_msgs / num_dsl;
    }
  else 				/* client */
    {
      unsigned int to = 0, to_idx = 0;
      long long int num_msgs1 = num_msgs;

      ssmp_barrier_wait(0);

      t_start = getticks();

      while (num_msgs1--)
	{
	  msg->w0 = num_msgs1;

	  ssmp_send(to, msg);

#if defined(ROUNDTRIP)
	  ssmp_recv_from(to, msg);
#endif  /* ROUNDTRIP */

	  to = dsl_seq[to_idx++];
	  if (to_idx == num_dsl)
	    {
	      to_idx = 0;
	    }

	  if (msg->w0 != num_msgs1) 
	    {
	      P("Ping-pong failed: sent %lld, recved %d", num_msgs1, msg->w0);
	    }

	  if (delay_after > 0)
	    {
	      wait_cycles(delay_after);
	    }
	}
      t_end = getticks();
    }
  
  double total_throughput = 0;
  uint32_t co;
  for (co = 0; co < ssmp_num_ues(); co++)
    {
      if (co == ssmp_id())
  	{
	  if (co <= 1)
	    {
	      PF_PRINT;
	    }
	  if (!color_dsl(co))
	    {
 	      ticks t_dur = t_end - t_start - getticks_correction;
	      double ticks_per_sec = REF_SPEED_GHZ * 1e9;
	      double dur = t_dur / ticks_per_sec;
	      double througput = num_msgs / dur;

#if defined(DEBUG)
	      PRINT("Completed in %10f secs | Througput: %f", dur, througput);
#endif

	      memcpy(msg, &througput, sizeof(double));
	      ssmp_send(0, msg);
	    }
  	}
      else if (ssmp_id() == 0 && color_app1(co))
	{
	  ssmp_recv_from(co, msg);
	  double througput = *(double*) msg;
	  /* PRINT("recved th from %02d : %f", c, througput); */
	  total_throughput += througput;
	}
      ssmp_barrier_wait(0);
    }

  ssmp_barrier_wait(0);
  if (ssmp_id() == 0)
    {
      PRINT("Total throughput: %.1f Msgs/s", total_throughput);
    }

  if (color_dsl(ID))
    {
      free(cbuf);
    }

  free(msg);
  ssmp_term();
  return 0;
}


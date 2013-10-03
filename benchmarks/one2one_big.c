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
long long int num_msgs = 100000;
size_t siz_data = 16 * 1024;
uint8_t ID;
int core1 = 0;
int core2 = 1;

int
main(int argc, char** argv)
{
  struct option long_options[] =
    {
      // These options don't set a flag
      {"help", no_argument, NULL, 'h'},
      {"num-msgs", required_argument, NULL, 'n'},
      {"size-msg", required_argument, NULL, 's'},
      {"core1", required_argument, NULL, 'x'},
      {"core2", required_argument, NULL, 'y'},
      {NULL, 0, NULL, 0}
    };

 int i, c;
 while (1)
   {
     i = 0;
     c = getopt_long(argc, argv, "hn:s:x:y:", long_options, &i);

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
	 PRINT("one2one_big -- Testing sending big messages\n"
	       "\n"
	       "Usage:\n"
	       "  ./one2one_big [options...]\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help\n"
	       "        Print this message\n"
	       "  -n, --num-msgs <int>\n"
	       "        Number of messages to send\n"
	       "  -s, --size-msg <int>\n"
	       "        Size of a message in bytes\n"
	       "  -x, --core1 <int>\n"
	       "        On which core to put the receiver process\n"
	       "  -y, --core2 <int>\n"
	       "        On which core to put the sender process\n"
	       );
	 exit(0);
       case 'n':
	 num_msgs = atoi(optarg);
	 break;
       case 's':
	 siz_data = atoi(optarg);
	 break;
       case 'x':
	 core1 = atoi(optarg);
	 break;
       case 'y':
	 core2 = atoi(optarg);
	 break;
       case '?':
	 PRINT("Use -h or --help for help\n");

	 exit(0);
       default:
	 exit(1);
       }
   }

 ID = 0;
 printf("NUM of msgs      : %lld\n", num_msgs);
 printf("Size of msg      : %lu\n", (long unsigned) siz_data);
 printf("Core receiver    : %d\n", core1);
 printf("Core sender      : %d\n", core2);


 ssmp_init(num_procs);

 int rank;
 for (rank = 1; rank < num_procs; rank++)
   {
     pid_t child = fork();
     if (child < 0)
       {
	 P("Failure in fork():\n%s", strerror(errno));
       } else if (child == 0)
       {
	 goto fork_done;
       }
   }
 rank = 0;

 fork_done:
 ID = rank;

 if (ID == 0)
   {
     set_cpu(core1);
   }
 else
   {
     set_cpu(core2);
   }

 ssmp_mem_init(ID, num_procs);

 char* data = (char*) malloc(siz_data);
 assert(data != NULL);

 for (i = 0; i < siz_data; i++)
   {
     data[i] = 1;
   }

 ssmp_barrier_wait(0);

 double _start = wtime();
 ticks _start_ticks = getticks();

 ssmp_msg_t msg;
 long long int num_msgs1 = num_msgs;


 if (ID % 2 == 0)
   {
     while (num_msgs1--)
       {
	 ssmp_recv(&msg);
	 ssmp_recv_from_big(msg.sender, data, msg.w0);
	 if (num_msgs1 == num_msgs-1)
	   {
	     size_t l, sum = 0;
	     for(l = 0; l < siz_data; l++)
	       {
		 sum += data[l];
	       }
	     if (sum != msg.w1)
	       { 
		 P("** warning: got sum: %lu, instead of %d", (long unsigned) sum, msg.w1); 
	       }
	   }
       }
   }
 else
   {
     int to = ID-1;

     while (num_msgs1--)
       {
	 msg.w0 = siz_data; msg.w1 = siz_data;
	 ssmp_send(to, &msg);
	 ssmp_send_big(to, data, siz_data);
       }
   }

  
 ticks _end_ticks = getticks();
 double _end = wtime();

 if (ID == 1)
   {
     double _time = _end - _start;
     ticks _ticks = _end_ticks - _start_ticks;
     ticks _ticksm =(ticks) ((double)_ticks / num_msgs);
     double lat = (double) (1000*1000*1000*_time) / num_msgs;
     double throughput_M = ((num_msgs * siz_data) / _time) / (1024 * 1024);

     printf("sent %lld msgs\n\t"
	    "in %f secs\n\t"
	    "%.1f ns latency\n\t"
	    "%llu ticks/msg\n"
	    "throughput: %10.2f MB/s = %.2f GB/s\n",
	    num_msgs, _time, lat, (long long unsigned) _ticksm, throughput_M, throughput_M / 1024);
   }

 ssmp_barrier_wait(1);
 ssmp_term();
 return 0;
}

ssmp is a highly optimized message passing library built on top of the cache-coherence protocols of shared memory processors. It exports functions for sending and receiving cache-line-sized (or bigger) messages. 

* Website             : http://lpd.epfl.ch/site/ssmp
* Author              : Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
* Related Publications: ssmp was implemented for TM2C (http://lpd.epfl.ch/site/ssync) and is now a part of the SSYNC synchronization suite (http://lpd.epfl.ch/site/ssync)
  1. TM2C: a Software Transactional Memory for Many-Cores, 
     Vincent Gramoli, Rachid Guerraoui, Vasileios Trigonakis (alphabetical order), 
     EuroSys '12 - Proceedings of the 7th ACM European conference on Computer Systems
  2. Everything You Always Wanted to Know about Synchronization but Were Afraid to Ask, 
     Tudor David, Rachid Guerraoui, Vasileios Trigonakis (alphabetical order), 
     SOSP '13 - Proceeding of the 24th ACM Symposium on Operating Systems Principles


Installation:
-------------

Please refer to the INSTALL file.


Interface of ssmp:
------------------

ssmp exports the following functions:
     - extern void ssmp_init(int num_procs);
     - extern void ssmp_mem_init(int id, int num_ues);
     - extern void ssmp_term(void);
     - extern inline void ssmp_send(uint32_t to, volatile ssmp_msg_t* msg);
     - extern inline void ssmp_send_no_sync(uint32_t to, volatile ssmp_msg_t* msg);
     - extern inline void ssmp_send_big(int to, void* data, size_t length);
     - extern inline void ssmp_broadcast(ssmp_msg_t* msg);
     - extern inline void ssmp_recv_from(uint32_t from, volatile ssmp_msg_t* msg);
     - extern inline void ssmp_recv_from_big(int from, void* data, size_t length);
     - extern inline void ssmp_recv(ssmp_msg_t* msg);
     - extern void ssmp_color_buf_init(ssmp_color_buf_t* cbuf, int (*color)(int));
     - extern void ssmp_color_buf_free(ssmp_color_buf_t* cbuf);
     - extern inline void ssmp_recv_color(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg);
     - extern inline void ssmp_recv_color_start(ssmp_color_buf_t* cbuf, ssmp_msg_t* msg);
     - extern int ssmp_color_app(int id);
     - extern inline ssmp_barrier_t*  ssmp_get_barrier(int barrier_num);
     - extern inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int));
     - extern inline void ssmp_barrier_wait(int barrier_num);
and a number of helper functions.

Check ssmp.h for the details of the available functions.

Additionally, you can use the simple profiler functions in measurements.h.


Using ssmp (libssmp):
---------------------

ssmp includes the following applications:
     - one2one : test one-to-one one-way messaging
     - one2one_rt : test one-to-one roundtrip messaging 
     - one2one_big : test one-to-one messaging with big messages
     - client_server : test client-server one-way messaging
     - client_server_rt : test client-server roundtrip messaging 
     - bank : a simple bank application based on servers
     - barrier_test : test the barriers in ssmp
     - cs : try to measure the cost of a context switch

Execute:
   ./app -h
for the parameters that each application accepts
	 
To use libssmp in your application, execute:
   make libssmp.a

This will generate the libssmp.a library.

In your application you need to include the ssmp.h header.
Additionally, you also need to copy the ssmp_ARC.h (ARCH = x86, sparc, or tile) file that corresponds to your architecture, because it is included by ssmp.h.

Finally, you need to link your application with -lssmp and point the linker to the folder of libssmp.a (with -L/folder/to/libssmp.a).


Limitations:
------------

1. ssmp mostly aims at cache-line-sized messages and, for simplicity, it does not implement messaging queues. In other words, every process is allowed to send only a single pending message to each other process.
2. ssmp works with process, not threads.
3. the ssmp_[send/recv_from]_big functions are currently not implemented for the Tilera platforms.

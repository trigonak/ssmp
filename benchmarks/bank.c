/*
 * File:
 *   bank.c
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Bank stress test.
 *
 * Copyright (c) 2007-2011.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>

#include "common.h"
#include "measurements.h"
#include "ssmp.h"


/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */
#define RO                              1
#define RW                              0

#define DEFAULT_NUM_OPS                 1000000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NUM_PROCS               2
#define DEFAULT_BALANCE_PERC            60
#define DEFAULT_WITHDRAW_PERC           20
#define DEFAULT_DEPOSIT_PERC            20
#define DEFAULT_SEED                    0
#define DEFAULT_USE_LOCKS               1
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0
#define DEFAULT_DSL_PER_CORE            2

uint8_t dsl_seq[256];
uint8_t ID, num_dsl, num_app;
uint8_t num_procs = DEFAULT_NUM_PROCS;

int num_ops = DEFAULT_NUM_OPS;
int nb_accounts = DEFAULT_NB_ACCOUNTS;
int balance_perc = DEFAULT_BALANCE_PERC;
int deposit_perc = DEFAULT_DEPOSIT_PERC;
int seed = DEFAULT_SEED;
int withdraw_perc = DEFAULT_WITHDRAW_PERC;
int write_threads = DEFAULT_WRITE_THREADS;
int disjoint = DEFAULT_DISJOINT;
int dsl_per_core = DEFAULT_DSL_PER_CORE;

ssmp_msg_t* msg;


#define XSTR(s)                         STR(s)
#define STR(s)                          #s

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

unsigned long* seeds;


/* ################################################################### *
 * BANK ACCOUNTS
 * ################################################################### */

typedef struct account 
{
  uint32_t number;
  int32_t balance;
} account_t;

typedef struct bank 
{
  account_t *accounts;
  uint32_t size;
} bank_t;


enum 
  {
    DEPOSIT,
    WITHDRAW,
    CHECK,
    EXIT
  } bank_op;

/* help functions */
int color_dsl(int id);
uint8_t nth_dsl(uint8_t id);
int color_app1(int id);
int color_all(int id);

/* bank funcitonality */
int
deposit(uint32_t acc, uint32_t dsl, uint32_t amount)
{
  msg->w0 = DEPOSIT;
  msg->w1 = acc;
  msg->w2 = amount;
  ssmp_send(dsl, msg);
 
  return amount;
}

int
check(uint32_t acc, uint8_t dsl)
{
  msg->w0 = CHECK;
  msg->w1 = acc;
  ssmp_send(dsl, msg);
  _mm_pause();
  ssmp_recv_from(dsl, msg);
 
  return msg->w2;
}

int
withdraw(uint32_t acc, uint32_t dsl, uint32_t amount)
{
  msg->w0 = WITHDRAW;
  msg->w1 = acc;
  msg->w2 = amount;
  ssmp_send(dsl, msg);
  _mm_pause();
  ssmp_recv_from(dsl, msg);
 
  return msg->w2;
}

int 
total(bank_t *bank, int transactional)
{
  return 0;
}

void
reset(bank_t *bank)
{
}

/* ################################################################### *
 * STRESS TEST
 * ################################################################### */

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



static inline uint8_t
get_dsl(uint32_t acc_num, uint32_t nb_accs_per_dsl)
{
  return dsl_seq[acc_num / nb_accs_per_dsl];
}

void 
bank_app()
{
  /* PRINT("APP"); */
  uint32_t rand_max;
  unsigned int seedi = getticks() % 13532224;
  srand(seedi);
  wait_cycles(rand() % 33445);
  seedi = getticks() % 13534;

  seeds = seed_rand();

  uint32_t nb_accounts_per_dsl = nb_accounts / num_dsl;
  rand_max = nb_accounts - 1;

  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);

  ssmp_barrier_wait(4);

  ticks t_start = getticks();
  uint32_t ops;
  for (ops = 0; ops < num_ops; ops++)
    {
      uint32_t nb = (uint32_t) (my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) % 100);
      uint32_t acc = (uint32_t) my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & rand_max;
      uint8_t dsl = get_dsl(acc, nb_accounts_per_dsl);
      /* PRINT("* %6d *to %-3d for %3d (nb = %3d)", ops, dsl, acc, nb); */

      acc %= nb_accounts_per_dsl;
      if (nb < deposit_perc) 
	{
	  deposit(acc, dsl, 1);
	} 
      else if (nb < withdraw_perc) 
	{
	  withdraw(acc, dsl, 1);
	} 
      else	     /* nb < balance_perc */
	{
	  check(acc, dsl);
	}
    }
  ticks t_end = getticks();

  /* PRINT("********************************* done"); */
  ssmp_barrier_wait(1);
  if (ssmp_id() == 1)
    {
      uint32_t i;
      for (i = 0; i < num_dsl; i++)
	{
	  msg->w0 = EXIT;
	  ssmp_send(dsl_seq[i], msg);
	}
    }

  ssmp_barrier_wait(1);

  ticks t_dur = t_end - t_start - getticks_correction;
  double ticks_per_sec = REF_SPEED_GHZ * 1e9;
  double dur = t_dur / ticks_per_sec;
  double througput = num_ops / dur;

  /* PRINT("Completed in %10f secs | Througput: %f", dur, througput); */
  memcpy(msg, &througput, sizeof(double));
  ssmp_send(0, msg);

  free(seeds);
}

void 
bank_dsl()
{
  uint32_t nb_accounts_mine = nb_accounts / num_dsl;
  
  bank_t* bank = (bank_t*) malloc(sizeof(bank));
  assert(bank != NULL);

  bank->size = nb_accounts_mine;
  bank->accounts = (account_t*) calloc(nb_accounts_mine, sizeof(account_t));
  account_t* accs = bank->accounts;
  assert(accs != NULL);


  uint32_t i;
  for (i = 0; i < nb_accounts_mine; i++)
    {
      accs[i].number = i;
      accs[i].balance = 0;
    }

  ssmp_color_buf_t *cbuf = NULL;
  cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
  assert(cbuf != NULL);
  ssmp_color_buf_init(cbuf, color_app1);

  msg = (ssmp_msg_t *) memalign(SSMP_CACHE_LINE_SIZE, sizeof(ssmp_msg_t));
  assert(msg != NULL);

  uint8_t done = 0;

  ssmp_barrier_wait(4);

  while(done == 0)
    {
      ssmp_recv_color_start(cbuf, msg);
      switch (msg->w0)
	{
	case DEPOSIT:
	  /* PRINT("from %-3d : DEPOSIT for %-5d", msg->sender, msg->w1); */
	  accs[msg->w1].balance += msg->w2;
	  break;
	case WITHDRAW:
	  {
	    /* PRINT("from %-3d : WITHDRW for %-5d", msg->sender, msg->w1); */
	    uint8_t succesfull = 0;
	    if (accs[msg->w1].balance >= msg->w2)
	      {
		accs[msg->w1].balance -= msg->w2;
		succesfull = 1;
	      }
	    msg->w2 = succesfull;
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	case CHECK:
	  /* PRINT("from %-3d : CHECK   for %-5d", msg->sender, msg->w1); */
	  msg->w2 = accs[msg->w1].balance;
	  ssmp_send(msg->sender, msg);
	  break;
	default:
	  /* PRINT("exiting"); */
	  free(bank->accounts);
	  free(bank);
	  free(msg);
	  free(cbuf);
	  done = 1;
	  break;
	}
    }
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
      {"help",                      no_argument,       NULL, 'h'},
      {"accounts",                  required_argument, NULL, 'a'},
      {"num_ops",                   required_argument, NULL, 'o'},
      {"num-threads",               required_argument, NULL, 'n'},
      {"check",                     required_argument, NULL, 'c'},
      {"deposit_perc",              required_argument, NULL, 'e'},
      {"servers",                   required_argument, NULL, 's'},
      {"withdraws",            required_argument, NULL, 'w'},
      {NULL, 0, NULL, 0}
    };

  int i, c;

  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "ha:c:o:n:r:e:s:w:W:j", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) {
      case 0:
	/* Flag is automatically set */
	break;
      case 'h':
	printf("bank -- lock stress test\n"
	       "\n"
	       "Usage:\n"
	       "  bank [options...]\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help\n"
	       "        Print this message\n"
	       "  -a, --accounts <int>\n"
	       "        Number of accounts in the bank (default=" XSTR(DEFAULT_NB_ACCOUNTS) ")\n"
	       "  -o, --num_ops <int>\n"
	       "        Test for number of operations (0=infinite, default=" XSTR(DEFAULT_NUM_OPS) ")\n"
	       "  -n, --num-threads <int>\n"
	       "        Number of threads (default=" XSTR(DEFAULT_NUM_PROCS) ")\n"
	       "  -c, --check <int>\n"
	       "        Percentage of check balance transactions (default=" XSTR(DEFAULT_BALANCE_PERC) ")\n"
	       "  -e, --deposit_perc <int>\n"
	       "        Percentage of deposit transactions (default=" XSTR(DEFAULT_DEPOSIT_PERC) ")\n"
	       "  -s, --server <int>\n"
	       "        1 server core per arg cores (default=" XSTR(DEFAULT_DSL_PER_CORE) ")\n"
	       "  -w, --withdraws <int>\n"
	       "        Percentage of withdraw_perc transactions (default=" XSTR(DEFAULT_WITHDRAW_PERC) ")\n"
	       );
	exit(0);
      case 'a':
	nb_accounts = atoi(optarg);
	break;
      case 'o':
	num_ops = atoi(optarg);
	break;
      case 'n':
	num_procs = atoi(optarg);
	break;
      case 'c':
	balance_perc = atoi(optarg);
	break;
      case 'e':
	deposit_perc = atoi(optarg);
	break;
      case 's':
	dsl_per_core = atoi(optarg);
	break;
      case 'w':
	withdraw_perc = atoi(optarg);
	break;
      case 'j':
	disjoint = 1;
	break;
      case '?':
	printf("Use -h or --help for help\n");
	exit(0);
      default:
	exit(1);
      }
    }

  assert(num_ops >= 0);
  assert(nb_accounts >= 2);
  assert(num_procs > 0);
  assert(balance_perc >= 0 && withdraw_perc >= 0
	 && deposit_perc >= 0
	 && deposit_perc + balance_perc + withdraw_perc <= 100);

  nb_accounts = pow2roundup(nb_accounts);

  uint32_t missing = 100 - (deposit_perc + balance_perc + withdraw_perc);
  if (missing > 0)
    {
      balance_perc += missing;
    }

  printf("Nb accounts    : %d\n", nb_accounts);
  printf("Num ops        : %d\n", num_ops);
  printf("Nb threads     : %d\n", num_procs);
  printf("Check balance  : %d\n", balance_perc);
  printf("Deposit        : %d\n", deposit_perc);
  printf("Withdraws      : %d\n", withdraw_perc);
  printf("Server/cores   : %d\n", dsl_per_core);

  withdraw_perc += deposit_perc;
  balance_perc += withdraw_perc;

  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);


  uint8_t dsl_seq_idx = 0;;
  for (i = 0; i < num_procs; i++)
    {
      if (color_dsl(i))
	{
	  num_dsl++;
	  dsl_seq[dsl_seq_idx++] = i;
	}
      else
	{
	  num_app++;
	}
    }

  PRINT("Cores: dsl: %2d | app: %d", num_dsl, num_app);

  if (nb_accounts % num_dsl > 0)
    {
      PRINT("*** NB accounts (%d) is not dividable by number of DSL (%d)", nb_accounts, num_dsl);
    }


  ssmp_init(num_procs);

  ssmp_barrier_init(2, 0, color_dsl);
  ssmp_barrier_init(1, 0, color_app1);
  ssmp_barrier_init(4, 0, color_all);
  ssmp_barrier_init(3, 0, color_all);

  uint8_t rank;
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

  set_cpu(id_to_core[ID]);
  ssmp_mem_init(ID, num_procs);
  getticks_correction_calc();

  if (color_dsl(ID))
    {
      bank_dsl();
    }
  else
    {
      bank_app();
    }

  ssmp_barrier_wait(0);

  double total_throughput = 0;
  if (ssmp_id() == 0)
    {
      uint32_t c;
      for (c = 0; c < ssmp_num_ues(); c++)
	{
	  if (color_app1(c))
	    {
	      ssmp_recv_from(c, msg);
	      double througput = *(double*) msg;
	      total_throughput += througput;
	    };
	}

      PRINT("Total throughput: %.1f Ops/s", total_throughput);
    }

  /* stats */

  ssmp_barrier_wait(3);
  ssmp_term();
  return 0;
}


/* help functions */

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
color_all(int id)
{
  return 1;
}

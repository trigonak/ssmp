SRC = src
INCLUDE = include
BENCH = benchmarks
PROF = prof

CFLAGS = -O3 -Wall
LDFLAGS = -lssmp -lm -lrt
VER_FLAGS = -D_GNU_SOURCE

MEASUREMENTS = 1
TARGET_ARCH = i386
TARGET_PLAT = generic

ifeq ($(VERSION),DEBUG) 
CFLAGS = -O0 -ggdb -Wall -g -fno-inline
VER_FLAGS += -DDEBUG
endif

UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM = OPTERON
CC = gcc
PLATFORM_NUMA=1
TARGET_PLAT = opteron
endif

ifeq ($(UNAME), trigonak-laptop)
PLATFORM = COREi7
TARGET_ARCH = x86_64
TARGET_PLAT = generic
CC = gcc
endif

ifeq ($(UNAME), diassrv8)
PLATFORM = XEON
CC = gcc
PLATFORM_NUMA=1
TARGET_ARCH = x86_64
TARGET_PLAT = xeon
endif

ifeq ($(UNAME), maglite)
PLATFORM = NIAGARA
CC = /opt/csw/bin/gcc
CFLAGS += -m64 -mcpu=v9 -mtune=v9
TARGET_ARCH = sparc
TARGET_PLAT = niagara
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM = TILERA
CC = tile-gcc
LDFLAGS += -ltmc
TARGET_ARCH = tile
TARGET_PLAT = tilera
endif

ifeq ($(UNAME), smal1.sics.se)
PLATFORM = TILERA
CC = tile-gcc
LDFLAGS += -ltmc
TARGET_ARCH = tile
endif

ifeq ($(PLATFORM), )
PLATFORM = DEFAULT
CC = gcc
endif

ifeq ($(PLATFORM_NUMA),1) #give PLATFORM_NUMA=1 for NUMA
LDFLAGS += -lnuma
endif 

VER_FLAGS += -D$(PLATFORM)


ifeq ($(TARGET_ARCH), i386)
ARCH_C = $(SRC)/arch/x86
else ifeq ($(TARGET_ARCH), x86_64)
ARCH_C = $(SRC)/arch/x86
else ifeq ($(TARGET_ARCH), sparc)
ARCH_C = $(SRC)/arch/sparc
else ifeq ($(TARGET_ARCH), tile)
ARCH_C = $(SRC)/arch/tile
endif

PLAT_C = $(SRC)/platform/$(TARGET_PLAT)

all: one2one one2one_rt client_server client_server_rt bank one2one_big barrier_test cs

default: one2one

ssmp.o: $(SRC)/ssmp.c 
	$(CC) $(VER_FLAGS) -c $(SRC)/ssmp.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ssmp_arch.o: $(ARCH_C)/ssmp_arch.c 
	$(CC) $(VER_FLAGS) -c $(ARCH_C)/ssmp_arch.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ssmp_platf.o: $(PLAT_C)/ssmp_platf.c 
	$(CC) $(VER_FLAGS) -c $(PLAT_C)/ssmp_platf.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ssmp_send.o: $(SRC)/ssmp_send.c
	$(CC) $(VER_FLAGS) -c $(SRC)/ssmp_send.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ssmp_recv.o: $(SRC)/ssmp_send.c
	$(CC) $(VER_FLAGS) -c $(SRC)/ssmp_recv.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ssmp_broadcast.o: $(SRC)/ssmp_broadcast.c
	$(CC) $(VER_FLAGS) -c $(SRC)/ssmp_broadcast.c $(CFLAGS) -I./$(INCLUDE) -L./ 

ifeq ($(MEASUREMENTS),1)
VER_FLAGS += -DDO_TIMINGS
MEASUREMENTS_FILES += measurements.o
endif

measurements.o: $(PROF)/measurements.c
	$(CC) $(VER_FLAGS) -c $(PROF)/measurements.c $(CFLAGS) -I./$(INCLUDE) -L./ 

libssmp.a: ssmp.o ssmp_arch.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o ssmp_platf.o $(INCLUDE)/ssmp.h $(MEASUREMENTS_FILES)
	@echo Archive name = libssmp.a
	ar -r libssmp.a ssmp.o ssmp_arch.o ssmp_send.o ssmp_recv.o ssmp_broadcast.o ssmp_platf.o $(MEASUREMENTS_FILES)
	rm -f *.o	

client_server: libssmp.a client_server.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o client_server client_server.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

client_server.o:	$(BENCH)/client_server.c
	$(CC) $(VER_FLAGS) -c $(BENCH)/client_server.c $(CFLAGS) -I./$(INCLUDE) -L./ 

client_server_rt: libssmp.a client_server_rt.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o client_server_rt client_server_rt.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

client_server_rt.o: $(BENCH)/client_server.c
	$(CC) $(VER_FLAGS) -DROUNDTRIP -o client_server_rt.o -c $(BENCH)/client_server.c $(CFLAGS) -I./$(INCLUDE) -L./ 
bank: libssmp.a bank.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o bank bank.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

bank.o:	$(BENCH)/bank.c
	$(CC) $(VER_FLAGS) -c $(BENCH)/bank.c $(CFLAGS) -I./$(INCLUDE) -L./ 

one2one: libssmp.a one2one.o $(INCLUDE)/common.h measurements.o
	$(CC) $(VER_FLAGS) -o one2one one2one.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

one2one.o: $(BENCH)/one2one.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -c $(BENCH)/one2one.c $(CFLAGS) -I./$(INCLUDE) -L./ 

one2one_rt: libssmp.a one2one_rt.o $(INCLUDE)/common.h measurements.o
	$(CC) $(VER_FLAGS) -o one2one_rt one2one_rt.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

one2one_rt.o:	$(BENCH)/one2one.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -DROUNDTRIP -o one2one_rt.o -c $(BENCH)/one2one.c $(CFLAGS) -I./$(INCLUDE) -L./ 

one2one_big: libssmp.a one2one_big.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o one2one_big one2one_big.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 	

one2one_big.o:	$(BENCH)/one2one_big.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -c $(BENCH)/one2one_big.c $(CFLAGS) -I./$(INCLUDE) -L./ 

barrier_test: libssmp.a barrier_test.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o barrier_test barrier_test.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 	

barrier_test.o: $(BENCH)/barrier_test.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -c $(BENCH)/barrier_test.c $(CFLAGS) -I./$(INCLUDE) -L./ 

l1_spil: libssmp.a l1_spil.o $(INCLUDE)/common.h
	$(CC) $(VER_FLAGS) -o l1_spil l1_spil.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

l1_spil.o: $(PROF)/l1_spil.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -c $(PROF)/l1_spil.c $(CFLAGS) -I./$(INCLUDE) -L./ 

cs: libssmp.a cs.o $(INCLUDE)/common.h measurements.o
	$(CC) $(VER_FLAGS) -o cs cs.o $(CFLAGS) $(LDFLAGS) -I./$(INCLUDE) -L./ 

cs.o: $(BENCH)/cs.c $(SRC)/ssmp.c
		$(CC) $(VER_FLAGS) -c $(BENCH)/cs.c $(CFLAGS) -I./$(INCLUDE) -L./ 

clean:
	rm -f *.o *.a client_server client_server_rt one2one one2one_rt bank barrier_test one2one_big l1_spil cs

CC=gcc
CFLAGS=-O2 -Wall
API_LOCATION=../..
COMMON_LOCATION=../common

SRCS=forced_sweep_2.c \
     $(API_LOCATION)/bme69x.c \
     $(COMMON_LOCATION)/common.c

INCLUDES=-I$(API_LOCATION) -I$(COMMON_LOCATION)

LDFLAGS=-lm

all: forced_sweep_2

forced_sweep_2: $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o forced_sweep_2 $(LDFLAGS)

clean:
	rm -f forced_sweep_2
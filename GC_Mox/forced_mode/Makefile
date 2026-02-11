CC=gcc
CFLAGS=-O2 -Wall

API_LOCATION=../..
COMMON_LOCATION=../common

SRCS=forced_2_gas.c \
     $(API_LOCATION)/bme69x.c \
     $(COMMON_LOCATION)/common.c

INCLUDES=-I$(API_LOCATION) -I$(COMMON_LOCATION)

LDFLAGS=-lm

all: forced_2_gas

forced_2_gas: $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o forced_2_gas $(LDFLAGS)

clean:
	rm -f forced_2_gas 

#C=gcc
#CFLAGS=-O2 -Wall
#API=../..
#COMMON=../common

#SRC=forced_2_gas_ratio.c $(API)/bme69x.c $(COMMON)/common.c
#INC=-I$(API) -I$(COMMON)

#all:
#	$(CC) $(CFLAGS) $(INC) $(SRC) -o forced_2_gas_ratio -lm

#clean:
#	rm -f forced_2_gas_ratio

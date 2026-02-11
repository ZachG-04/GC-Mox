CC = gcc

CFLAGS = -O2 -Wall

API_LOCATION = ../..
COMMON_LOCATION = ../common

INCLUDES = -I$(API_LOCATION) -I$(COMMON_LOCATION)

SRCS = forced_2_fft.c \
       $(API_LOCATION)/bme69x.c \
       $(COMMON_LOCATION)/common.c

TARGET = forced_2_fft

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) -lm

clean:
	rm -f $(TARGET)

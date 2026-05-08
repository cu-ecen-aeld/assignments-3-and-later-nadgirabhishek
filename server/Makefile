CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc

CFLAGS ?= -Wall -Werror -g
TARGET = aesdsocket
SRC = aesdsocket.c

.PHONY: all default clean

all: default

default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
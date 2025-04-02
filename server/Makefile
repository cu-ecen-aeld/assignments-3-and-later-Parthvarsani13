# Makefile for aesdsocket

# Allow CROSS_COMPILE override
CROSS_COMPILE ?= 
CC := $(CROSS_COMPILE)gcc
#CC ?= gcc

# Compiler and linker flags
CFLAGS ?= -g -Wall -pthread -DUSE_AESD_CHAR_DEVICE
LDFLAGS ?=

# Binary name
EXECUTABLE = aesdsocket

# Source and object files
SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXECUTABLE)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

.PHONY: all clean


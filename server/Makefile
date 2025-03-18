# Reference: https://gist.github.com/Miouyouyou/6ee23eec681b21782b17ec8a45258b87

# If Yocto or Buildroot sets CROSS_COMPILE, we won't override it.
CROSS_COMPILE ?= aarch64-linux-gnu-
CC ?= $(CROSS_COMPILE)gcc

# Compiler flags
CFLAGS ?= -g -Wall -pthread -DUSE_AESD_CHAR_DEVICE
LDFLAGS ?=

# Target binary
EXECUTABLE = aesdsocket
SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	# Include both $(CFLAGS) and $(LDFLAGS) on the link line
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXECUTABLE)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

.PHONY: all clean


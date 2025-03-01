# Reference Note:- took ref from the below Website for Line no - 5
# https://gist.github.com/Miouyouyou/6ee23eec681b21782b17ec8a45258b87

# Instead, let CC default to whatever the environment sets:
# If CC is not defined in the environment, default to CROSS_COMPILE + 'gcc' or just 'gcc'
CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -pthread
EXECUTABLE = aesdsocket

SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

# .PHONY ensures these targets are always executed when called
.PHONY: all clean

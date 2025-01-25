# Reference Note:- took ref from the below Website for Line no - 5
# https://gist.github.com/Miouyouyou/6ee23eec681b21782b17ec8a45258b87

# Set the cross-compiler prefix. If not set, defaults to an empty string.
CROSS_COMPILE ?= 

CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall
EXECUTABLE = writer

SRC = writer.c
OBJ = writer.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

# .PHONY ensures these targets are always executed when called
.PHONY: all clean

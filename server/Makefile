# Reference Note:- took ref from the below Website for Line no - 5
# https://gist.github.com/Miouyouyou/6ee23eec681b21782b17ec8a45258b87

# If Yocto sets CROSS_COMPILE or CC in the environment, we won't override it.
CROSS_COMPILE ?=
#CC ?= $(CROSS_COMPILE)gcc

CC ?= gcc

# We also let LDFLAGS be provided by the environment if set
CFLAGS ?= -g -Wall -pthread -DUSE_AESD_CHAR_DEVICE
LDFLAGS ?=


EXECUTABLE = aesdsocket
SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	# IMPORTANT: Include both $(CFLAGS) and $(LDFLAGS) on the link line
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXECUTABLE)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

.PHONY: all clean
# Reference Note:- took ref from the below Website for Line no - 5
# https://gist.github.com/Miouyouyou/6ee23eec681b21782b17ec8a45258b87

# If Yocto sets CROSS_COMPILE or CC in the environment, we won't override it.
CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

# We also let LDFLAGS be provided by the environment if set
CFLAGS ?= -g -Wall -pthread
LDFLAGS ?=

EXECUTABLE = aesdsocket
SRC = aesdsocket.c
OBJ = aesdsocket.o

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	# IMPORTANT: Include both $(CFLAGS) and $(LDFLAGS) on the link line
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $(EXECUTABLE)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $(OBJ) $(SRC)

clean:
	rm -f $(EXECUTABLE) $(OBJ)

.PHONY: all clean

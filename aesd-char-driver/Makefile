CROSS_COMPILE ?=
CC      = $(CROSS_COMPILE)gcc
CFLAGS  = -Wall -Werror -g
TARGET  = aesd-circular-buffer.o

all: $(TARGET)

$(TARGET): aesd-circular-buffer.c
	$(CC) $(CFLAGS) -c aesd-circular-buffer.c -o $(TARGET)

clean:
	rm -f $(TARGET)

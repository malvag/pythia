
CC=gcc

LIBS  =
CFLAGS = -ansi #-fsanitize=address -g -Wall

SRC=$(wildcard *.c)
# OBJ=$(SRC:.c=.o)
OBJ=pythia_table.o
all: clean test

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

test: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -rf test *.o

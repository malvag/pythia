
CC=gcc

LIBS  =
CFLAGS = -Wall -fsanitize=address -g #-ansi

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
# OBJ=pythia_table.o
all: clean driver

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# lib: $(OBJ)
# 	$(CC) -o $@ -c $^ $(CFLAGS) $(LIBS)

driver: $(OBJ)
	$(CC) -o test $^ $(CFLAGS) -lpthread $(LIBS)

clean:
	rm -rf test *.o

CC = gcc

INCLUDE = -I ./include
#LIB=-lpthread

OBJ += $(patsubst %.c, %.o, $(wildcard ./*.c))
OBJ += $(patsubst %.c, %.o, $(wildcard ./src/*.c))

CFLAGS = -O0 -g -Wall
target = demo

all:$(OBJ)
	$(CC) out/*.o -o $(target) $(LIB)
	mv $(target) out
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE)
	mv $@ out
clean:
	rm -rf out/*

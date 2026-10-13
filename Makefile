# Makefile for OS Programming Assignment 02 (ls utility)
#
#   make        -> compiles src/*.c into obj/*.o, then links them into bin/ls
#   make clean  -> deletes the object files and the executable
#
# It uses a wildcard, so it keeps working when you rename the source
# file for each version (ls-v1.0.0.c, ls-v1.1.0.c, ... ls-v1.6.0.c).

CC      = gcc
CFLAGS  = -Wall -g

SRC     = $(wildcard src/*.c)
OBJ     = $(patsubst src/%.c, obj/%.o, $(SRC))
TARGET  = bin/ls

all: $(TARGET)

# Link: object files -> executable
$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

# Compile: each src/X.c -> obj/X.o
obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o $(TARGET)

.PHONY: all clean

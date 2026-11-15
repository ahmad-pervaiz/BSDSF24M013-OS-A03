CC = gcc
CFLAGS = -Wall -g -Iinclude
SRC = src/main.c src/shell.c src/execute.c
OBJ = obj/main.o obj/shell.o obj/execute.o
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

obj/%.o: src/%.c include/shell.h
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin

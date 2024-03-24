# Makefile for CIS*3110 Assignment 2 - Threaded Spell-Checker

# Compiler settings - Can change cc to gcc if required
CC = gcc
# Compiler flags
CFLAGS = -Wall -g -std=c11
# Linker flags
LDFLAGS = -lpthread

# Target executable name
TARGET = A2checker

# Source file
SRC = main.c

# Object file
OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# To start over from scratch, type 'make clean'. Removes the executable and object files.
clean:
	rm -f $(TARGET) $(OBJ)

# Dependency on header files, if you have them.
# You can manually add dependencies or use gcc to auto-generate them

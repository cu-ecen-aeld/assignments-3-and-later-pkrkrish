# Makefile for writer application

# Compiler prefix for cross-compilation (empty by default)
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror

# Target application
TARGET := writer

# Source files (since this Makefile is inside finder-app/, just use writer.c)
SRCS := writer.c
OBJS := $(SRCS:.c=.o)

# Default target: build writer
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target: remove binary and object files
clean:
	rm -f $(TARGET) $(OBJS)

# Variable for the compiler, defaults to local gcc if not set by environment
CC ?= $(CROSS_COMPILE)gcc

# Compiler flags, allows environment overrides
# -g: includes debug symbols
# -Wall: enables all warnings
# -Werror: treats warnings as errors (best practice for this course)
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -pthread

# The final executable name
TARGET = aesdsocket

# Default target
all: $(TARGET)

default: $(TARGET)

# Link the object files into the final executable
$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile the .c file into an .o object file
aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c -o aesdsocket.o

# Clean target to remove build artifacts
clean:
	rm -f $(TARGET) *.o

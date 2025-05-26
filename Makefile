CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g # -O2 for release
LDFLAGS = -lm -lpng 

# Find all .c files in the current directory
SRCS = $(wildcard *.c)
# Generate object file names from source file names
OBJS = $(SRCS:.c=.o)
# Executable name
EXEC = mypaint

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LDFLAGS)

# Generic rule for .o files from .c files
%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

# If some .c files don't have a direct .h counterpart but depend on others:
# For example, main.c depends on all .h files
main.o: main.c image.h png_handler.h operations.h utils.h
	$(CC) $(CFLAGS) -c main.c -o main.o

image.o: image.c image.h
	$(CC) $(CFLAGS) -c image.c -o image.o
	
png_handler.o: png_handler.c png_handler.h image.h
	$(CC) $(CFLAGS) -c png_handler.c -o png_handler.o

operations.o: operations.c operations.h image.h utils.h
	$(CC) $(CFLAGS) -c operations.c -o operations.o

utils.o: utils.c utils.h image.h
	$(CC) $(CFLAGS) -c utils.c -o utils.o


clean:
	rm -f $(OBJS) $(EXEC)
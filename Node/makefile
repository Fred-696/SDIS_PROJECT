CC = gcc
CFLAGS = -Wall -Werror -fpic
LDLIBS = -lmosquitto -lpigpio -lpthread
SOURCES = $(wildcard node*.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLES = $(SOURCES:.c=)

all: $(EXECUTABLES)

# Rule to create each executable from its respective .o file
$(EXECUTABLES): %: %.o
	$(CC) -o $@ $< $(LDLIBS)

# Rule to create object files from .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) $(EXECUTABLES)

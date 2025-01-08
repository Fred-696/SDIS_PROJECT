CC = gcc
CFLAGS = -Wall -Werror -fpic
LDLIBS = -lmosquitto -lpigpio -lpthread
SOURCES = $(wildcard node*.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLES = $(SOURCES:.c=.o)  # Temporary use .o list and rename correctly in rule

# Define the 'all' target that will build all mqtt_nodeX executables
all: $(EXECUTABLES)

# Rule to create each mqtt_nodeX executable from its respective .o file
$(EXECUTABLES): %.o:
	$(CC) -o mqtt_$* $< $(LDLIBS)

# Rule to create object files from .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJECTS) mqtt_node*

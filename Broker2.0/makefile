# Targets
all: mqtt_broker

# Clean up build artifacts
clean:
	rm -f *.o mqtt_broker

# Pattern rule for compiling .c files into .o files
%.o: %.c
	gcc -c -Wall -fpic $<

# Final executable target
mqtt_broker: main.o broker.o
	gcc -o mqtt_broker main.o broker.o


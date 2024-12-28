all: mqtt_node

clean:
		rm -f *.o mqtt_node

mqtt_node: main.o
		gcc -o $@ main.o -lmosquitto -lpigpio -lpthread

%.o: %.c
		gcc -c -Wall -fpic $^

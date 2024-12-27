all: mqtt_node

clean:
		rm -f *.o *.so mqtt_sub_pub

mqtt_node: main.o
		gcc -o $@ main.o -lpaho-mqtt3c -lpigpio

%.o: %.c
		gcc -c -Wall -fpic $^

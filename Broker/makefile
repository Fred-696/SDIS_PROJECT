all: mqtt_broker

clean:
		rm -f *.o mqtt_broker

mqtt_broker: main.o
		gcc -o mqtt_broker main.o

main.o: main.c
		gcc -c -Wall -fpic main.c

Required to run Node:

```
sudo apt-get update
sudo apt install gcc
sudo apt install make
sudo apt install libmosquitto-dev

sudo apt install pigpio
```

Alternative quick connect-conack-subscribe-suback
```
mosquitto_sub -h 192.168.1.227 -p 1883 -t "test/topic" -q 1 -i "Node0" -d
```

Alternative quick connect-conack-publish-puback
```
mosquitto_pub -h 192.168.1.227 -p 1883 -t "test/topic" -m "Hello, broker!" -q 1 -i "Node2" -d
```
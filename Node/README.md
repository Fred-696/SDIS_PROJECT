Veio daqui

https://www.hivemq.com/blog/implementing-mqtt-in-c/

do

```
sudo apt-get update
sudo apt install gcc
sudo apt install make
sudo apt-get install libssl-dev

git clone https://github.com/eclipse-paho/paho.mqtt.c.git
cd paho.mqtt.c
make
sudo make install
```

```
sudo apt install pigpio
```

my broker ip fowards 192.168.5.126 to WSL

```
netsh interface portproxy add v4tov4 listenaddress=192.168.5.126 listenport=1883 connectaddress=172.17.120.119 connectport=1883
netsh interface portproxy show all
```

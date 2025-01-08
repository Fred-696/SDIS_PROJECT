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

Our broker is designed to handle all messages as QoS 1 for simplified testing. This means that even if a PUBLISH message with QoS 0 is received, the broker will still respond with a PUBACK, even though it is not required by the MQTT protocol. Additionally, when forwarding the message to a subscribed client, the broker will assume the forwarded message is also QoS 1, causing it to wait indefinitely for a PUBACK. This behavior can be intentionally used to test the broker's retransmission function, treating this as a simulated delivery failure.
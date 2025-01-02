#!/bin/bash

PORT=1883  # Replace with the port your broker is trying to bind to

# Find the process using the port
PID=$(lsof -t -i:$PORT)

if [ -z "$PID" ]; then
    echo "No process found using port $PORT."
else
    echo "Killing process $PID using port $PORT..."
    kill -9 $PID
    echo "Process $PID has been terminated. You can now restart your application."
fi

# Client

Daemon that sends monitoring data every X seconds using UDP.
Reconnect after 5 seconds if connection failed
The data can be: cpu usage, ...

# Server

Program that receives monitoring data. For each client, make a table row in ncurses that displays monitoring information and connection status


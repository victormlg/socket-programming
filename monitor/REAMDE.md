# Client

Daemon that sends monitoring data every X seconds using UDP.
Reconnect after 5 seconds if connection failed
The data can be: cpu usage, ...

# Server

Program that receives monitoring data. For each client, make a table row in ncurses that displays monitoring information and connection status


# Future:

In the future, add a way to map clients to server client table entries. To do so, at the first connection, the client asks for a "handl" with TCP, then ater, inside all UDP packets, it includes this handle, so the server can correctly place the packet in the right table.

# Basic Linux Server

TCP server example which uses fork().

```
$ ./server/server 
Set up a connection
Waiting for connections...
Got connection from 127.0.0.1
'Hello, world' sent!

$ ./client/client 
Connecting to 127.0.0.1
Received 'Hello, world!'

```

## Building 
You need CMake and GCC to build this example.

```
mkdir build && cd build
cmake ../
cd build
make
```

Client and server are built separately.

## Reference

[Beej's Guide to Network Programming](http://beej.us/guide/bgnet/)

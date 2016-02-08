# Cubbyhole server

A multi-threaded server implemented in C serving clients speaking the [cubbyhole protocol](#protocol).

## Background

For one of my university courses ([Network Protocols and Architectures](http://www.inet.tu-berlin.de/menue/teaching0/ws201516/npa_1516/parameter/en/)) in the last assignment of the semester we were asked to implement a server capable of serving clients speaking a protocol designed by the department. The protocol provides the functionality of a [pigeon-hole messagebox](https://en.wikipedia.org/wiki/Pigeon-hole_messagebox), thus the name **cubbyhole protocol**. Multiple clients were expected to be able to connect to such a cubbyhole server and either store (PUT), retrieve (GET), see (LOOK) or delete (DROP) a message. Other than that it was required to make the server somewhat fault tolerant, so that e.g. incomplete requests or sudden disconnects would not impair the server.

This implementation is my take on a C version.

## Dependencies

In order to compile the server, the ```pthread``` library is needed. Please make sure to have it available on your system.

## Installation

Clone this repository into a location of your liking, switch into the created folder and ```make``` the project:

```bash
$ git clone https://github.com/numbleroot/cubbyhole-server.git
$ cd cubbyhole-server
$ make
```

After this, the executable ```cubbyhole-server``` should have been created. By supplying a port to listen on after the executable you can start the server:

```bash
$ ./cubbyhole-server 12345
Cubbyhole server listening on port 12345.
```

Now start up a networking utility capable of sending and receiving TCP connections (such as ```netcat``` and ```telnet``` are capable of) and connect to the cubbyhole server on the just specified port, e.g.:

```bash
$ nc localhost 12345
```

Experiment with the commands in [Protocol](#protocol).

## Protocol

The goal of the cubbyhole protocol is to share (PUT) one message on the server so that potentially multiple clients can retrieve (GET), see (LOOK) or delete (DROP) it. The following commands are supported for clients:

| Command   | Argument    | Description                                                                       |
|:--------: |:----------: |---------------------------------------------------------------------------------- |
| **PUT**   | `<message>` | Store a message on the server, eventually overwriting the old message.            |
| **GET**   | -           | Retrieve and delete the message from server and display it to requesting client.  |
| **LOOK**  | -           | Retrieve but do not delete message and display it to requesting client.           |
| **DROP**  | -           | Delete currently stored message on server.                                        |
| **HELP**  | -           | Display the list of possible commands.                                            |
| **QUIT**  | -           | Gracefully terminate the connection.                                              |

This leads to a state diagram of the server cubbyhole that might look like this one:

![State diagram of cubbyhole on server](https://github.com/numbleroot/cubbyhole-server/blob/master/state-diagram.png)

## Other cubbyhole implementations

Of course I am not the only student in that class, so here is a probably incomplete list of implementations of my fellow students:

* [cubbyhole](https://github.com/MetalMatze/cubbyhole) by [MetalMatze](https://github.com/MetalMatze) - [Go](https://golang.org/)
* [cubbyhole](https://github.com/KjellPirschel/cubbyhole) by [KjellPirschel](https://github.com/KjellPirschel) - C

## License

This project is [GPLv3 licensed](https://raw.githubusercontent.com/numbleroot/cubbyhole-server/master/LICENSE).
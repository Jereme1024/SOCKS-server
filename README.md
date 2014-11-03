Project #1: Remote Access System (ras).

Implement a shell conosle with server-client architecture and new pipe subsystem.

Login by "csh> telnet <server_addr> <server_port>" # the server port number

Allow user to enter the following command:

% removetag test.html |3 removetag test.html | number |1 number ### |3 skips two processes.

% ls |1 ctt | cat ### cat will not execute and numbered-pipe will not be counted.
Unknown command: [ctt].
% cat
bin/
test.html

@mainpage
This project conatins three main components: Console, Parser, and Server.

Conosle:
- The console class is able to handle shell commands and perfom basic and extened pipe(|n) and filedump(>) operations.
- It needs a parser-policy to complete this class.
- It uses a pipe lookup table to store the pipe needed information of shell commands.
- The parent of this conosle is roled a pipe fd resource manager including garbage collection of pipe.

Parser:
- The parser class is able to handle some basic operations to extract wanted context of string.

Server:
- The server class is able to handle socket operation to build a server. It accepts a Service policy with "run" interface.

@see console.hpp
@see parser.hpp
@see server.hpp


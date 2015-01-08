CC=g++
CPPFLAGS=-std=c++11 -O2
EXEC=httpd *.cgi

all: hw4 sserver

hw4: hw4.cpp socks.hpp
	$(CC) $(CPPFLAGS) hw4.cpp -o hw4.cgi

sserver: socks_server.cpp socks.hpp
	$(CC) $(CPPFLAGS) socks_server.cpp -o sserver

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

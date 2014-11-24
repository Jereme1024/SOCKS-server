CC=g++
CPPFLAGS=-std=c++11 -O2
EXEC=singleServer multipleServer

all: part1/singleServer.cpp part2/multipleServer.cpp
	$(CC) $(CPPFLAGS) part1/singleServer.cpp -o singleServer
	$(CC) $(CPPFLAGS) part2/multipleServer.cpp -o multipleServer

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

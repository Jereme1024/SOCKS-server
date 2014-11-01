CC=g++
CPPFLAGS=-std=c++11
EXEC=proj1


all: console.hpp parser.hpp server.hpp proj1.cpp
	$(CC) $(CPPFLAGS) proj1.cpp -o $(EXEC)

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

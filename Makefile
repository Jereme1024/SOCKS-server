CC=g++
CPPFLAGS=-std=c++11 -O2 -lpthread
EXEC=singleServer multipleServer

all: 
	#$(CC) $(CPPFLAGS) part1/singleServer.cpp -o singleServer
	#$(CC) $(CPPFLAGS) part2/multipleServer.cpp -o multipleServer
	$(CC) $(CPPFLAGS) proj2_single.cpp -o singleServer
	$(CC) $(CPPFLAGS) proj2_multiple.cpp -o multipleServer
	

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

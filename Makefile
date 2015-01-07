CC=g++
CPPFLAGS=-std=c++11 -O2
EXEC=httpd *.cgi

all: hw4 

hw4: hw4.cpp
	$(CC) $(CPPFLAGS) hw4.cpp -o hw4.cgi

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

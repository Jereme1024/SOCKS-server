CC=g++
CPPFLAGS=-std=c++11 -O2
EXEC=httpd *.cgi

all: hello welcome printenv hw3 httpd.cpp
	#$(CC) $(CPPFLAGS) proj2_single.cpp -o singleServer
	#$(CC) $(CPPFLAGS) proj2_multiple.cpp -o multipleServer
	$(CC) $(CPPFLAGS) httpd.cpp -o httpd

hello: hello.cpp
	$(CC) $(CPPFLAGS) hello.cpp -o hello.cgi

welcome: welcome.cpp
	$(CC) $(CPPFLAGS) welcome.cpp -o welcome.cgi

printenv: printenv.cpp
	$(CC) $(CPPFLAGS) printenv.cpp -o printenv.cgi

hw3: hw3.cpp
	$(CC) $(CPPFLAGS) hw3.cpp -o hw3.cgi

install:
	cp -f /bin/ls /bin/cat ras/bin/

clean: rm $(EXEC)

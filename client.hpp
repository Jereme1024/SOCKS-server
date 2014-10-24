#ifndef __CLIENT_HPP__
#define __CLIENT_HPP__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <string>

class Client
{
private:
	int sockfd;
	int portno;
	int n;
	const std::string serv_ip;
	struct sockaddr_in server_addr;
	char buffer[1024];
	
public:
	Client(const std::string ip, int port = 5487)
		: serv_ip(ip)
		, portno(port)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
		{
			std::cerr << "Client socket open failed.\n";
			exit(-1);
		}

		memset(&server_addr, '0', sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(portno);
		inet_aton(serv_ip.c_str(), &server_addr.sin_addr);

		if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		{
			std::cerr << "Client bind failed.\n";
			exit(-1);
		}
	}

	void run()
	{
		std::cout << "Client up!\n";

		strcpy(buffer, "Hello, i'm client\n");
		std::cout << "Send to server\n";
		write(sockfd, buffer, strlen(buffer));

		read(sockfd, buffer, 1023);
		std::cout << "Get from server: " << buffer << "\n";
	}


};

#endif

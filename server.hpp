#ifndef _SERVER_HPP_
#define _SERVER_HPP_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <functional>

const int MAX_CONNECTION = 5;

template <class Service>
class Server : public Service
{
private:
	int sockfd;
	int portno;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len;
	char buffer[1024];
	
public:
	Server(int port = 5487)
		: portno(port)
	{
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
		{
			std::cerr << "Server socket open failed.\n";
			exit(EXIT_FAILURE);
		}

		memset(&server_addr, '0', sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = INADDR_ANY;
		server_addr.sin_port = htons(portno);

		while (bind(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) < 0)
		{
			std::cerr << "Server bind failed.\n";
			std::cerr << "Try to bind port " << ++portno << "...\n";
			server_addr.sin_port = htons(portno);
		}

		listen(sockfd, MAX_CONNECTION);

		client_len = sizeof(client_addr);
	}

	int accept_one()
	{
		int clientfd;

		if ((clientfd = accept(sockfd, (sockaddr *) &client_addr, &client_len)) < 0)
		{
			std::cerr << "Server accept failed.\n";
			exit(EXIT_FAILURE);
		}

		return clientfd;
	}

	void run()
	{
		signal(SIGCHLD, SIG_IGN);
		auto do_nothing = [](int n){};

		while (true)
		{
			int clientfd = accept_one();
			
			int pid = fork();
			if (pid < 0)
			{
				std::cerr << "Server fork failed.\n";
				exit(-1);
			}

			if (pid == 0)
			{
				signal(SIGCHLD, do_nothing);

				close(sockfd);

				Service::replace_fd(clientfd);
				Service::run();

				exit(EXIT_SUCCESS);;
			}
			else
			{
				close(clientfd);
			}
		}
	}

};

#endif

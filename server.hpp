#ifndef _SERVER_HPP_
#define _SERVER_HPP_

/// @file
/// @author Jeremy
/// @version 1.0
/// @section DESCRIPTION
///
/// The server class is able to handle socket operation to build a server. It accepts a Service policy with "run" interface.

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <functional>

#include <map>
#include <vector>
#include <arpa/inet.h>

const int MAX_CONNECTION = 5;
const int MAX_CLIENT = 31;
const int MAX_NAME = 32;


/// @brief
/// The server class is able to handle socket operation to build a server. It accepts a Service policy with "run" interface.
template <class ServerPoly>
class ServerBase
{
protected:
	int sockfd; // listen fd
	int portno;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len;

public:
	/// @brief This method is used to initialize a server by following socket -> bind -> listen(...).
	/// @param port A port number with a default value 5487.
	ServerBase(int port = 5487)
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


	/// @brief This method is used to aceept a client once.
	/// @return A number of client fd.
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


	/// @brief This method is used to accept clients and perform a Service run operation. It will control the fork/wait(...) operations at the same time.
	void run()
	{
		static_cast<ServerPoly *>(this)->run_impl();
	}

};


template <class Service>
class ServerMultiple : public ServerBase<ServerMultiple<Service>>, Service
{
public:
	typedef ServerBase<ServerMultiple<Service>> Super;

	void run_impl()
	{
		while (true)
		{
			int clientfd = Super::accept_one();

			int pid = fork();
			if (pid < 0)
			{
				std::cerr << "Server fork failed.\n";
				exit(EXIT_FAILURE);
			}

			if (pid == 0)
			{
				close(Super::sockfd);

				Service::enter(clientfd, Super::client_addr);

				Service::routine(clientfd);

				exit(EXIT_SUCCESS);
			}
			else
			{
				close(clientfd);
			}
		}
	}
};

template <class Service>
class ServerSingle : public ServerBase<ServerSingle<Service>>, Service
{
public:
	typedef ServerBase<ServerSingle<Service>> Super;

	void run_impl()
	{
		fd_set fds_act, fds_rd;
		FD_ZERO(&fds_act);
		FD_SET(Super::sockfd, &fds_act);
		int nfds = getdtablesize();

		int f0 = dup(0);
		int f1 = dup(1);
		int f2 = dup(2);

		while (true)
		{
			memcpy(&fds_rd, &fds_act, sizeof(fds_rd));

			if (select(nfds, &fds_rd, NULL, NULL, NULL) < 0)
			{
				std::cerr << "Select failed: " << strerror(errno) << "\n";
				exit(EXIT_FAILURE);
			}

			if (FD_ISSET(Super::sockfd, &fds_rd))
			{
				int clientfd = Super::accept_one();	
				FD_SET(clientfd, &fds_act);
				
				Service::enter(clientfd, Super::client_addr);
			}

			for (int fd_i = 0; fd_i < nfds; fd_i++)
			{
				if (fd_i != Super::sockfd && FD_ISSET(fd_i, &fds_rd))
				{
					Service::routine(fd_i);

					if (Service::is_leave(fd_i))
					{
						Service::leave(fd_i);

						FD_CLR(fd_i, &fds_act);
						close(fd_i);
					}

				}
			}
		}
	}
};


#endif

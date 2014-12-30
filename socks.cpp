#ifndef __SOCKS_HPP__
#define __SOCKS_HPP__

#include <cstring>
#include <cstdio>
#include <sys/time.h>

#include "server.hpp"
#include "client.hpp"

struct Socks4Request
{
	unsigned char vn;
	unsigned char cd;
	unsigned short dst_port;
	unsigned int dst_ip;
	unsigned char *user_id;

	Socks4Request() : user_id(NULL)
	{}

	ssize_t size()
	{
		ssize_t total = sizeof(Socks4Request) - sizeof(unsigned char *);
		std::cerr << "size..." << total << "\n";

		if (user_id != NULL)
		{
			total += strlen((char *)user_id);
		}

		return total;
	}

	void set(unsigned char *buffer, ssize_t buffer_len)
	{
		vn = buffer[0];
		cd = buffer[1];
		dst_port = buffer[2] << 8 | buffer[3];
		dst_ip = buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7];

		user_id = new unsigned char[buffer_len - 8 + 1];
		strcpy((char *)user_id, (char *)(buffer + 8));
	}
};

struct Socks4Reply
{
	enum CD { GRANTED = 90, FAILED = 91};
	unsigned char vn;
	unsigned char cd;
	unsigned short dst_port;
	unsigned int dst_ip;

	Socks4Reply()
		: vn(0)
		, cd(FAILED)
	{}

	void success(unsigned int ip, unsigned short port)
	{
		dst_ip = ip;
		dst_port = port;
		cd = GRANTED;
	}
};


class SocksServerService
{
public:
	sockaddr_in source_addr, destination_addr;
	int source_fd, destination_fd;

	timeval default_timeout;

	SocksServerService()
	{ 
		default_timeout.tv_sec = 3;
		default_timeout.tv_usec = 0;

		signal(SIGCHLD, SIG_IGN);
	}

	void print_ip(in_addr &addr)
	{
		char *some_addr;
		some_addr = inet_ntoa(addr);
		printf("%s\n", some_addr); // prints "10.0.0.1"
	}


	void enter(int clientfd, sockaddr_in client_addr)
	{
		std::cerr << "New connection!\n";

		source_fd = clientfd;
		source_addr = client_addr;

		const int buffer_max = 1024;
		unsigned char buffer[buffer_max];
		ssize_t buffer_len = recv(clientfd, buffer, buffer_max, 0);

		Socks4Request socks4request;
		socks4request.set(buffer, buffer_len);

		memset(&destination_addr, '0', sizeof(destination_addr));
		destination_addr.sin_family = AF_INET;
		destination_addr.sin_addr.s_addr = htonl(socks4request.dst_ip);
		destination_addr.sin_port = htons(socks4request.dst_port);

		print_ip(destination_addr.sin_addr);

		destination_fd = socket(AF_INET, SOCK_STREAM, 0);

		if (connect(destination_fd, (sockaddr *) &destination_addr, sizeof(destination_addr)) < 0)
		{
			perror("Connect failed.");
			std::cerr << "Connect failed.\n";
			exit(EXIT_FAILURE);
		}

		Socks4Reply socks4reply;
		socks4reply.success(socks4request.dst_ip, socks4request.dst_port);
		send(source_fd, &socks4reply, sizeof(socks4reply), 0);
	}

	void routine(int clientfd)
	{
		std::cerr << "Transfering!\n";

		fd_set fds_act, fds_rd;
		FD_ZERO(&fds_act);
		FD_SET(source_fd, &fds_act);
		FD_SET(destination_fd, &fds_act);

		const int nfds = getdtablesize();
		timeval timeout;

		std::string message;

		bool is_talked;

		while (true)
		{
			memcpy(&fds_rd, &fds_act, sizeof(fds_rd));
			memcpy(&timeout, &default_timeout, sizeof(timeval));

			is_talked = false;

			if (select(nfds, &fds_rd, NULL, NULL, &timeout) < 0)
			{
				perror("Select");
			}

			if (FD_ISSET(source_fd, &fds_rd))
			{
				is_talked = true;
				if (!proxy(source_fd, destination_fd)) break;
			}

			if (FD_ISSET(destination_fd, &fds_rd))
			{
				is_talked = true;
				if (!proxy(destination_fd, source_fd)) break;
			}

			if (!is_talked)
			{
				std::cout << "Timeout!\n";
				break;
			}
		}
	}

	bool proxy(int from, int to)
	{
		const int buffer_max = 1024;
		unsigned char buffer[buffer_max];
		ssize_t recv_len, send_len;

		do
		{
			recv_len = recv(from, buffer, buffer_max, 0);
			send_len = send(to, buffer, recv_len, 0);
		} while (recv_len >= buffer_max);

		if (recv_len <= 0) return false;

		return true;
	}
};


int main()
{
	ServerMultiple<SocksServerService> socks_server;
	socks_server.run();
}

#endif

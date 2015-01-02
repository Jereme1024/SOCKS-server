#ifndef __SOCKS_HPP__
#define __SOCKS_HPP__

#include <cstring>
#include <cstdio>
#include <ctime>
#include <sys/time.h>

#include "server.hpp"
#include "client.hpp"

enum CD { CONNECT = 1, BIND = 2 };

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

	inline void success()
	{
		cd = GRANTED;
	}

	inline void set_ip(unsigned int ip)
	{
		dst_ip = ip;
	}

	inline void set_port(unsigned short port)
	{
		dst_port = port;
	}
};


template <class Service>
class SocksClient : public Client<Service>
{
public:
	typedef Client<Service> Super;

	char *server_ip;
	int server_port;
	char *remote_ip;
	int remote_port;

	SocksClient(char *s_ip, int s_port, char *r_ip, int r_port)
		: server_ip(s_ip)
		, server_port(s_port)
		, remote_ip(r_ip)
		, remote_port(r_port)
	{
		
		std::cerr << remote_port << std::endl;
	}

	void connect()
	{
		sockaddr_in remote_addr;
		hostent *remote;
		remote = gethostbyname(remote_ip);
		remote_addr.sin_addr = *((struct in_addr *)remote->h_addr);

		Socks4Request socks4request;
		socks4request.vn = 4;
		socks4request.cd = 1;
		socks4request.dst_port = (unsigned short)htons(remote_port);
		socks4request.dst_ip = (unsigned int)remote_addr.sin_addr.s_addr;

		Super::setup(server_ip, server_port);

		Super::connect_once();

		Socks4Reply socks4reply;

		ssize_t send_len = send(Super::sockfd, &socks4request, sizeof(socks4request), 0);
		ssize_t recv_len = recv(Super::sockfd, &socks4reply, sizeof(socks4reply), 0);

		// TODO: Check if the reply is ok?
		Super::enter(Super::sockfd, Super::server_addr);
	}

};


class SocksServerService
{
public:
	sockaddr_in source_addr, destination_addr;
	int source_fd, destination_fd;
	bool is_bind_flag;

	timeval default_timeout;

	SocksServerService()
		: is_bind_flag(false)
	{ 
		default_timeout.tv_sec = 120;
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

		if (socks4request.cd == CONNECT)
		{
			std::cerr << "Do connect!\n";
			do_connect(socks4request);
		}
		else if (socks4request.cd == BIND)
		{
			std::cerr << "Do bind!\n";
			do_bind(socks4request);
		}

		print_ip(destination_addr.sin_addr);
	}

	inline void do_connect(Socks4Request &socks4request)
	{
		Socks4Reply socks4reply;
		socks4reply.set_ip(socks4request.dst_ip);
		socks4reply.set_port(socks4request.dst_port);

		destination_fd = socket(AF_INET, SOCK_STREAM, 0);

		memset(&destination_addr, '0', sizeof(destination_addr));
		destination_addr.sin_family = AF_INET;
		destination_addr.sin_addr.s_addr = htonl(socks4request.dst_ip);
		destination_addr.sin_port = htons(socks4request.dst_port);

		if (connect(destination_fd, (sockaddr *) &destination_addr, sizeof(destination_addr)) < 0)
		{
			perror("Connect failed.");
			std::cerr << "Connect failed.\n";
			exit(EXIT_FAILURE);
		}
		else
		{
			socks4reply.success();
		}

		send(source_fd, &socks4reply, sizeof(socks4reply), 0);
	}

	inline void do_bind(Socks4Request &socks4request)
	{
		std::srand(std::time(0));

		is_bind_flag = true;

		Socks4Reply socks4reply;
		socks4reply.set_ip(socks4request.dst_ip);
		socks4reply.set_port(socks4request.dst_port);

		destination_fd = source_fd;
		destination_addr = source_addr;

		source_fd = socket(AF_INET, SOCK_STREAM, 0);

		int rand_port = std::rand() % 20480 + 1024;

		memset(&source_addr, '0', sizeof(source_addr));
		source_addr.sin_family = AF_INET;
		source_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		source_addr.sin_port = htons(rand_port);

		while (bind(source_fd, (sockaddr *) &source_addr, sizeof(source_addr)) < 0)
		{
			std::cerr << "Server bind failed.\n";
			rand_port = std::rand() % 20480 + 1024;
			std::cerr << "Try to bind port " << rand_port << "...\n";
			source_addr.sin_port = htons(rand_port);
		}


		listen(source_fd, MAX_CONNECTION);

		socks4reply.set_port(source_addr.sin_port);
		socks4reply.set_ip(source_addr.sin_addr.s_addr);
		socks4reply.success();
		send(destination_fd, &socks4reply, sizeof(socks4reply), 0);
	}

	inline bool is_bind_mode()
	{
		return is_bind_flag;
	}

	void routine(int clientfd)
	{
		std::cerr << "Transfering!\n";
		
		Socks4Reply socks4reply;

		if (is_bind_mode())
		{
			socks4reply.set_port(source_addr.sin_port);
			socks4reply.set_ip(source_addr.sin_addr.s_addr);
			socks4reply.success();

			int clientfd;
			sockaddr_in client_addr;
			socklen_t client_len = sizeof(client_addr);

			std::cerr << "Time to accept!\n";

			if ((clientfd = accept(source_fd, (sockaddr *) &client_addr, &client_len)) < 0)
			{
				perror("Bind mode accept");
				exit(EXIT_FAILURE);
				return;
			}

			close(source_fd);
			source_fd = clientfd;
			source_addr = client_addr;

			std::cerr << "Accept done!\n";

			send(destination_fd, &socks4reply, sizeof(socks4reply), 0);

			std::cerr << "Accept done!\n";
		}

		fd_set fds_act, fds_rd;
		FD_ZERO(&fds_act);
		FD_SET(source_fd, &fds_act);
		FD_SET(destination_fd, &fds_act);

		const int nfds = getdtablesize();
		timeval timeout;

		bool is_talked;

		std::cerr << "source " << source_fd << " destination " << destination_fd << std::endl;

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
		const int buffer_max = 4096;
		unsigned char buffer[buffer_max];
		ssize_t recv_len, send_len;

		recv_len = recv(from, buffer, buffer_max, 0);
		send_len = send(to, buffer, recv_len, 0);

		if (recv_len != 0)
			std::cerr << from << "-" << to << " " << "recv: " << recv_len << "\n";

		if (recv_len <= 0) return false;

		return true;
	}
};


//int main()
//{
//	ServerMultiple<SocksServerService> socks_server;
//	socks_server.run();
//}

#endif

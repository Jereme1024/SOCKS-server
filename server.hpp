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
#include <netinet/in.h>
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

#include "fdsetfamily.hpp"

const int MAX_CONNECTION = 5;
const int MAX_CLIENT = 31;
const int MAX_NAME = 32;

struct FifoStatus
{
	short rwstatus[31][31];
	int writefd[31][31];

	FifoStatus()
	{
		for (int i = 0; i < 31; i++)
		{
			for (int j = 0; j < 31; j++)
			{
				rwstatus[i][j] = 0;
			}
		}
	}
};

struct User
{
	std::string name;
	char ip[INET_ADDRSTRLEN];
	unsigned short port;
	int clientfd; // Id
	std::map<std::string, std::string> env;

	std::string get_address()
	{
		std::string address(ip);
		address += ("/" + std::to_string(port));
		return std::string(address);
	}
};

struct UserMan
{
	User userz[MAX_CLIENT];

	UserMan()
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			userz[i].clientfd = -1;
		}
	}

	int get_smallest_id()
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (userz[i].clientfd == -1)
			{
				return i;
			}
		}

		return -1;
	}

	int add(std::string name, char ip[], unsigned short port, int clientfd)
	{
		int uid = get_smallest_id();
		if (uid < 0)
		{
			std::cerr << "User size is reached full!\n";
			return -1;
		}

		//strcpy(userz[uid].name, name);
		userz[uid].name =  name;
		strcpy(userz[uid].ip, ip);
		userz[uid].port = port;
		userz[uid].clientfd = clientfd;

		userz[uid].env["PATH"] = "bin:.";

		// init env
		
		// init chat buffer
		
		return uid;
	}

	void remove(int uid)
	{
		if (userz[uid].clientfd == -1) return;

		close(userz[uid].clientfd);
		userz[uid].clientfd = -1;
	}

};


FifoStatus global_fifo_status;

void collect_fifo_garbage(int sig)
{
	for (int i = 0; i < 31; i++)
	{
		for (int j = 0; j < 31; j++)
		{
			if (global_fifo_status.rwstatus[i][j] == 2)
			{
				close(global_fifo_status.writefd[i][j]);
				global_fifo_status.rwstatus[i][j] = 0;
			}
		}
	}
}


/// @brief
/// The server class is able to handle socket operation to build a server. It accepts a Service policy with "run" interface.
template <class Service>
class Server : public Service
{
private:
	int sockfd; // listen fd
	int portno;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len;
	char buffer[1024];

	std::map<int, User> users;
	int now;

	UserMan uabc;
	
public:
	/// @brief This method is used to initialize a server by following socket -> bind -> listen(...).
	/// @param port A port number with a default value 5487.
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

		User user;
		inet_ntop( AF_INET, &(client_addr.sin_addr.s_addr), user.ip, INET_ADDRSTRLEN );
		user.port = client_addr.sin_port;
		user.env["PATH"] = "bin:.";
		user.clientfd = clientfd;
		user.name = "(no name)";

		users[clientfd] = std::move(user);

		int uid = uabc.add("(no name)", user.ip, user.port, clientfd);

		return uid;
		//return clientfd;
	}


	/// @brief This method is used to accept clients and perform a Service run operation. It will control the fork/wait(...) operations at the same time.
	/// @return Nothing.
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
				exit(EXIT_FAILURE);
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

	void run_single_proc_mode()
	{
		fd_set fds_act, fds_rd;
		FD_ZERO(&fds_act);
		FD_SET(sockfd, &fds_act);
		int nfds = getdtablesize();

		char string[1024] = "Hello world!\n";

		const int MAX_BUFFER = 1024;
		char buffer[MAX_BUFFER];

		User user;
		user.clientfd = 0;
		users[0] = std::move(user);

		Service::set_fifo_status(&global_fifo_status);

		signal(SIGUSR1, collect_fifo_garbage);

		while (true)
		{
			memcpy(&fds_rd, &fds_act, sizeof(fds_rd));

			if (select(nfds, &fds_rd, NULL, NULL, NULL) < 0)
			{
				std::cerr << "Select failed: " << strerror(errno) << "\n";
				exit(EXIT_FAILURE);
			}

			if (FD_ISSET(sockfd, &fds_rd))
			{
				//int clientfd = accept_one();	
				int uid = accept_one();	
				int clientfd = uabc.userz[uid].clientfd;
				FD_SET(clientfd, &fds_act);
				
				std::string motd = Service::get_MOTD();
				send_to(clientfd, motd + "% ");

				std::string new_user_tip = "*** User '(no name)' entered from ";
				new_user_tip.append(uabc.userz[uid].ip);
				new_user_tip += "/";
				new_user_tip += std::to_string(uabc.userz[uid].port);
				new_user_tip += ". ***\n";
				//send_to(clientfd, motd + "% ");

				//std::string new_user_tip = "*** User '(no name)' entered from ";
				//new_user_tip.append(users[clientfd].ip);
				//new_user_tip += "/";
				//new_user_tip += std::to_string(users[clientfd].port);
				//new_user_tip += ". ***\n";
				
				broadcast(new_user_tip);
			}

			for (int i = 1; i < MAX_CLIENT; i++)
			{
				if (uabc.userz[i].clientfd != -1)
				{
					int fd = uabc.userz[i].clientfd;

					if (fd != sockfd && FD_ISSET(fd, &fds_rd))
					{
						now = i;

						Service::replace_fd(fd);
						Service::system_id = fd;
						Service::issue("setenv PATH " + uabc.userz[now].env["PATH"]);

						std::string cmd_line;					

						cmd_line = receive_from(fd);

						auto is_builtin = true;
						auto parsed_result = Service::parse_cmd(cmd_line);
						auto commands = Service::setup_cmd(parsed_result, is_builtin);

						if (!execute_serv_builtin_cmd(commands))
						{
							Service::replace_fd(fd);
							Service::system_id = now;
							Service::issue(cmd_line);
						}

						std::cout.flush();

						if (Service::is_exit())
						{
							std::string user_left_tip = "*** User '" + uabc.userz[now].name + "' left. ***\n";
							uabc.remove(now);

							FD_CLR(fd, &fds_act);
							close(fd);

							broadcast(user_left_tip);
							Service::unexit();
						}
						else
						{
							write(fd, "% ", strlen("% ") + 1);
						}

						Service::undo_fd();
					}
				}
			}

			//for (auto &user : users)
			//{
			//	int fd = user.first;

			//	if (fd != sockfd && FD_ISSET(fd, &fds_rd))
			//	{
			//		now = fd;

			//		Service::replace_fd(fd);
			//		Service::system_id = fd;
			//		Service::issue("setenv PATH " + users[now].env["PATH"]);

			//		std::string cmd_line;					

			//		cmd_line = receive_from(fd);

			//		auto parsed_result = Service::parse_cmd(cmd_line);
			//		auto commands = Service::setup_cmd(parsed_result);

			//		if (!execute_serv_builtin_cmd(commands))
			//		{
			//			Service::replace_fd(fd);
			//			Service::system_id = fd;
			//			Service::issue(cmd_line);
			//		}

			//		std::cout.flush();

			//		if (Service::is_exit())
			//		{
			//			std::string user_left_tip = "*** User '" + users[fd].name + "' left. ***\n";
			//			users.erase(fd);

			//			FD_CLR(fd, &fds_act);
			//			close(fd);

			//			broadcast(user_left_tip);
			//			Service::unexit();
			//		}
			//		else
			//		{
			//			write(fd, "% ", strlen("% ") + 1);
			//		}

			//		Service::undo_fd();
			//	}
			//	
			//}
		}
	}

	std::string receive_from(int fd)
	{
		std::string message;

		Service::replace_fd(fd);
		std::getline(std::cin, message);

		return message;
	}

	void send_to(int fd, std::string message)
	{
		Service::replace_fd(fd);
		std::cout << message;
		std::cout.flush();
	}

	void broadcast(std::string message)
	{
		//for (auto &user : users)
		//{
		//	if (user.first == 0) continue;
		//	send_to(user.first, message);
		//}

		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (uabc.userz[i].clientfd != -1)
			{
				send_to(uabc.userz[i].clientfd, message);
			}
		}
	}

	std::string query_who(int me)
	{
		std::string info = "<ID>	<nickname>	<IP/port>	<indicate me>\n";
		//for (auto &user : users)
		//{
		//	if (user.first == 0) continue;

		//	info += std::to_string(user.first) + "\t";
		//	info += user.second.name + "\t";
		//	info += user.second.ip;
		//	info += "/" + std::to_string(user.second.port);
		//	
		//	if (user.first == me)
		//		info += "\t<-me\n";
		//	else
		//		info += "\n";
		//}
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (uabc.userz[i].clientfd != -1)
			{
				info += std::to_string(i) + "\t";
				info += uabc.userz[i].name + "\t";
				info += uabc.userz[i].ip;
				info += "/" + std::to_string(uabc.userz[i].port);
				
				if (i == me)
					info += "\t<-me\n";
				else
					info += "\n";
			}
		}

		return info;
	}

	bool execute_serv_builtin_cmd(typename Service::command_vec &commands)
	{
		for (auto it = commands.begin(); it != commands.end(); ++it)
		{
			auto &cmd  = it->argv;
			
			//// XXX Testing region XXX
			//Service::undo_fd();
			//std::cout << "command: ";
			//for (auto &s : cmd)
			//{	
			//	std::cout << s << " ";
			//}
			//std::cout << std::endl;
			//// XXX Testing region XXX
			
			if (cmd[0] == "who")
			{
				//send_to(now, query_who(now));
				send_to(uabc.userz[now].clientfd, query_who(now));

				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "tell")
			{
				int target = std::atoi(cmd[1].c_str());
				//bool is_user_exist = (target != 0 && users.find(target) != users.end());
				bool is_user_exist = (target != 0 && uabc.userz[target].clientfd != -1);

				std::string message;
				if (is_user_exist)
				{
					//message = "*** " + users[now].name + " told you ***: ";
					message = "*** " + uabc.userz[now].name + " told you ***: ";
					message += cmd[2];
					for (int i = 3; i < cmd.size(); i++)
					{
						message += (" " + cmd[i]);
					}
					message += "\n";
					//send_to(target, message);
					send_to(uabc.userz[target].clientfd, message);
				}
				else
				{
					message = "*** Error: user #" + cmd[1] + " does not exist yet. ***\n";
					//send_to(now, message);
					send_to(uabc.userz[now].clientfd, message);
				}

				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "yell")
			{
				std::string message = cmd[1];
				for (int i = 2; i < cmd.size(); i++)
				{
					message += (" " + cmd[i]);
				}

				//message = "*** " + users[now].name + " yelled ***: " + message + "\n";
				message = "*** " + uabc.userz[now].name + " yelled ***: " + message + "\n";
				broadcast(message);

				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "name")
			{
				bool is_name_duplicated = false;
				//for (auto &user : users)
				//{
				//	if (cmd[1] == user.second.name)
				//	{
				//		is_name_duplicated = true;
				//		break;
				//	}
				//}
				for (int i = 1; i < MAX_CLIENT; i++)
				{
					if (uabc.userz[i].clientfd != -1 && cmd[1] == uabc.userz[i].name)
					{
						is_name_duplicated = true;
						break;
					}
				}

				if (is_name_duplicated)
				{
					std::string message = "*** User '" + cmd[1] + "' already exists. ***\n";
					send_to(uabc.userz[now].clientfd, message);
				}
				else
				{
					//users[now].name = cmd[1];
					//std::string message = "*** User from ";
					//message.append(users[now].ip);
					//message.append("/");
					//message.append(std::to_string(users[now].port));
					//message.append(" is named '" + users[now].name + "'. ***\n");
					uabc.userz[now].name = cmd[1];
					std::string message = "*** User from ";
					message.append(uabc.userz[now].ip);
					message.append("/");
					message.append(std::to_string(uabc.userz[now].port));
					message.append(" is named '" + uabc.userz[now].name + "'. ***\n");

					broadcast(message);
				}

				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "printenv")
			{
				if (cmd.size() == 2)
				{
					//std::string message = cmd[1] + "=" + users[now].env[cmd[1]] + "\n";
					std::string message = cmd[1] + "=" + uabc.userz[now].env[cmd[1]] + "\n";
					send_to(uabc.userz[now].clientfd, message);
				}

				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "setenv")
			{
				//Service::undo_fd();

				//std::cerr << "cmdsize: " << cmd.size() << "\n";
				if (cmd.size() == 3)
				{
					setenv(cmd[1].c_str(), cmd[2].c_str(), true);
					//users[now].env[cmd[1]] = cmd[2];
					uabc.userz[now].env[cmd[1]] = cmd[2];
				}

				commands.erase(it);

				return true;
			}

			if (commands.size() < 1) break;
		}
		return false;
	}
};

#endif

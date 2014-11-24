#ifndef _SERVICE_HPP_
#define _SERVICE_HPP_

/// @file
///// @author Jeremy
///// @version 1.0
///// @section DESCRIPTION
/////
///// The ServiceWrapper is a adapter for Server having common interface e.g. enter(), routine(), is_leave(), leave().
///// There are some data structures and ServiceWrappers of RAS for Project 2.

#include "shm.hpp"

struct FifoStatus
{
	short rwstatus[31][31];
	int writefd[31][31];
	int readfd[31][31];

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

FifoStatus *global_fifo_status;

void collect_fifo_garbage(int sig)
{
	for (int i = 0; i < 31; i++)
	{
		for (int j = 0; j < 31; j++)
		{
			if (global_fifo_status->rwstatus[i][j] == 2)
			{
				close(global_fifo_status->writefd[i][j]);
				close(global_fifo_status->readfd[i][j]);
				global_fifo_status->rwstatus[i][j] = 0;
			}
		}
	}
}

struct User
{
	char name[128];
	char ip[INET_ADDRSTRLEN];
	unsigned short port;
	int clientfd; 
};

struct UserStatus
{
	User users[MAX_CLIENT];

	UserStatus()
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			users[i].clientfd = -1;
		}
	}

	char *get_name(int uid)
	{
		return users[uid].name;
	}

	void set_name(int uid, std::string name)
	{
		strcpy(users[uid].name, name.c_str());
	}

	int get_smallest_id()
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (!is_available(i))
			{
				return i;
			}
		}

		return -1;
	}

	inline bool is_available(int id)
	{
		return users[id].clientfd != -1;
	}

	int add(std::string name, char ip[], unsigned short port, int clientfd)
	{
		int uid = get_smallest_id();
		if (uid < 0)
		{
			std::cerr << "User size is reached full!\n";
			return -1;
		}

		strcpy(users[uid].name, name.c_str());
		strcpy(users[uid].ip, ip);
		users[uid].port = port;
		users[uid].clientfd = clientfd;

		return uid;
	}

	void remove(int uid)
	{
		if (users[uid].clientfd == -1) return;

		close(users[uid].clientfd);
		users[uid].clientfd = -1;
	}

	int get_uid_by_fd(int fd)
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (is_available(i) && fd == users[i].clientfd)
				return i;
		}
		return -1;
	}
};


struct ChatField
{
	char content[1024];
	int from;
};

struct ChatBuffer
{
	ChatField chat_buffer[10];
};

struct ChatStatus
{
	ChatBuffer who[MAX_CLIENT];

	ChatStatus()
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			for (int j = 0; j < 10; j++)
			{
				who[i].chat_buffer[j].from = -1;
			}
		}
	}
};


template <class Host, class Console>
class ServerCommand
{
public:
	Host *host;

	ServerCommand(Host *ihost) : host(ihost)
	{}

	std::string query_who(int me)
	{
		std::string info = "<ID>	<nickname>	<IP/port>	<indicate me>\n";
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (host->user_status->users[i].clientfd != -1)
			{
				info += std::to_string(i) + "\t";
				info.append(host->user_status->get_name(i));
				info.append("\t");
				info += host->user_status->users[i].ip;
				info += "/" + std::to_string(host->user_status->users[i].port);
				if (i == me)
					info += "\t<-me\n";
				else
					info += "\n";
			}
		}

		return info;
	}


	bool execute_serv_builtin_cmd(typename Console::command_vec &commands)
	{
		UserStatus *user_status = host->user_status;
		int now = host->get_uid();

		for (auto it = commands.begin(); it != commands.end(); ++it)
		{
			auto &cmd  = it->argv;
			
			if (cmd[0] == "who")
			{
				host->send_to(now, query_who(now));
				commands.erase(it);

				return true;
			}
			else if (cmd[0] == "tell")
			{
				int target = std::atoi(cmd[1].c_str());
				bool is_user_exist = (target != 0 && user_status->is_available(target));

				std::string message;
				if (is_user_exist)
				{
					message = "*** ";
					message.append(user_status->get_name(now));
					message += " told you ***: ";
					message += cmd[2];
					for (int i = 3; i < cmd.size(); i++)
					{
						message += (" " + cmd[i]);
					}
					message += "\n";
					host->send_to(target, message);
				}
				else
				{
					message = "*** Error: user #" + cmd[1] + " does not exist yet. ***\n";
					host->send_to(now, message);
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

				std::string prefix = "*** ";
				prefix.append(user_status->users[now].name);
				prefix.append(" yelled ***: ");
				message = prefix + message + "\n";

				host->broadcast(message);

				commands.erase(it);
				return true;
			}
			else if (cmd[0] == "name")
			{
				bool is_name_duplicated = false;
				for (int i = 1; i < MAX_CLIENT; i++)
				{
					if (user_status->users[i].clientfd != -1 && cmd[1] == user_status->users[i].name)
					{
						is_name_duplicated = true;
						break;
					}
				}

				if (is_name_duplicated)
				{
					std::string message = "*** User '" + cmd[1] + "' already exists. ***\n";
					host->send_to(now, message);
				}
				else
				{
					user_status->set_name(now, cmd[1]);
					std::string message = "*** User from ";
					message.append(user_status->users[now].ip);
					message.append("/");
					message.append(std::to_string(user_status->users[now].port));
					message.append(" is named '");
					message.append(user_status->get_name(now));
					message.append("'. ***\n");

					host->broadcast(message);
				}

				commands.erase(it);
				return true;
			}
			else if (cmd[0] == "printenv")
			{
				if (cmd.size() == 2)
				{
					std::string env_val = host->get_env(now, cmd[1]);
					std::string message = cmd[1] + "=" + env_val + "\n";
					host->send_to(now, message);
				}

				commands.erase(it);
				return true;
			}
			else if (cmd[0] == "setenv")
			{
				if (cmd.size() == 3)
				{
					setenv(cmd[1].c_str(), cmd[2].c_str(), true);
					host->set_env(now, cmd[1], cmd[2]);
				}

				commands.erase(it);
				return true;
			}

			if (commands.size() < 1) break;
		}
		return false;
	}
};


template <class Console>
class ServiceWrapperSingle : public Console
{
public:
	int stdin_backup;
	int stdout_backup;
	int stderr_backup;

	std::vector<Console *> console_list;

	UserStatus *user_status;

	ServerCommand<ServiceWrapperSingle<Console>, Console> ServerCmd;

public:

	ServiceWrapperSingle<Console>() : console_list(MAX_CLIENT), ServerCmd(this)
	{
		stdin_backup = dup(0);
		stdout_backup = dup(1);
		stderr_backup = dup(2);

		user_status = new UserStatus();
		global_fifo_status = new FifoStatus();

		signal(SIGUSR1, collect_fifo_garbage);
	}

	~ServiceWrapperSingle<Console>()
	{
		delete user_status;
	}

	int get_uid()
	{
		return Console::system_id;
	}

	void set_uid(int uid)
	{
		Console::system_id = uid;
	}

	void set_env(int uid, std::string key, std::string val)
	{
		console_list[uid]->env[key] = val;
	}

	std::string get_env(int uid, std::string key)
	{
		return console_list[uid]->env[key];
	}

	void enter(int clientfd, sockaddr_in client_addr)
	{
		std::string motd = Console::get_MOTD();

		User user;
		inet_ntop( AF_INET, &(client_addr.sin_addr.s_addr), user.ip, INET_ADDRSTRLEN );
		user.port = client_addr.sin_port;
		user.clientfd = clientfd;
		strcpy(user.name, "(no name)");

		const int uid = user_status->add("(no name)", user.ip, user.port, clientfd);
		send_to(uid, motd);
		
		console_list[uid] = new Console();
		console_list[uid]->set_fifo_status(global_fifo_status);
		console_list[uid]->set_user_status(user_status);
		console_list[uid]->set_system_id(uid);
		console_list[uid]->env["PATH"] = "bin:.";

		std::string welcome_msg = "*** User '(no name)' entered from ";
		welcome_msg.append(user_status->users[uid].ip);
		welcome_msg += "/";
		welcome_msg += std::to_string(user_status->users[uid].port);
		welcome_msg += ". ***\n";

		broadcast(welcome_msg);

		send_to(uid, "% ");
	}

	void routine(int my_fd)
	{
		const int me = user_status->get_uid_by_fd(my_fd); 

		set_uid(me);
		
		auto commands = console_list[me]->issue("setenv PATH " + console_list[me]->env["PATH"]);
		console_list[me]->execute_cmd(commands);

		std::string cmd_line;					

		cmd_line = receive_from(my_fd);

		auto parsed_result = console_list[me]->parse_cmd(cmd_line);
		commands = console_list[me]->setup_builtin_cmd(parsed_result);

		if (!ServerCmd.execute_serv_builtin_cmd(commands))
		{
			console_list[me]->set_system_id(me);

			auto commands = console_list[me]->issue(cmd_line);
			for (auto &cmd : commands)
			{
				console_list[me]->log = "";
				if (cmd.proc_id != -1)
				{
					console_list[me]->execute(cmd);
				}
				else
				{
					std::cerr << "Unknown command: [" << cmd.argv[0] << "].\n";
					break;
				}

				if (console_list[me]->log != "")
				{
					broadcast(console_list[me]->log);
				}
			}
		}

		std::cout.flush();

		if (!is_leave(my_fd))
			send_to(me, "% ");
	}

	inline bool is_leave(int my_fd)
	{
		const int me = user_status->get_uid_by_fd(my_fd);
		
		if (console_list[me]->is_exit())
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	void leave(int my_fd)
	{
		const int me = user_status->get_uid_by_fd(my_fd);

		std::string user_left_tip = "*** User '";
		user_left_tip.append(user_status->get_name(me));
		user_left_tip += "' left. ***\n";

		broadcast(user_left_tip);

		user_status->remove(me);

		delete console_list[me];

		dup2(stdin_backup, 0);
		dup2(stdout_backup, 1);
		dup2(stderr_backup, 2);
	}

	void broadcast(std::string message)
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (user_status->is_available(i))
			{
				send_to(i, message);
			}
		}

		int fd = user_status->users[get_uid()].clientfd;

		Console::replace_fd(fd);
	}

	void send_to(int uid, std::string message)
	{
		const int fd = user_status->users[uid].clientfd;
		Console::replace_fd(fd);

		std::cout << message;
		std::cout.flush();
	}

	std::string receive_from(int fd)
	{
		std::string message;
		Console::replace_fd(fd);
		std::getline(std::cin, message);

		return message;
	}
};


template <class Console>
class ServiceWrapperMultiple : public Console
{
public:
	int stdin_backup;
	int stdout_backup;
	int stderr_backup;

	UserStatus *user_status;
	ChatStatus *chat_status;
	FifoStatus *fifo_status;

	ServerCommand<ServiceWrapperMultiple<Console>, Console> ServerCmd;

	int shm_id;
	int shm_id_chat;
	int shm_id_fifo;

public:
	static ServiceWrapperMultiple<Console> *instance;

	static ServiceWrapperMultiple<Console> *get_instance()
	{
		return instance;
	}

	static void receive_message(int sig)
	{
		ServiceWrapperMultiple<Console> *inst =  ServiceWrapperMultiple<Console>::get_instance();

		for (int i = 0; i < 10; i++)
		{
			if (inst->chat_status->who[inst->system_id].chat_buffer[i].from != -1)
			{
				std::cout << inst->chat_status->who[inst->system_id].chat_buffer[i].content;
				inst->chat_status->who[inst->system_id].chat_buffer[i].from = -1;
			}
			else
			{
				break;
			}
		}
		std::cout.flush();
	}

	static void collect_fifo_garbage(int sig)
	{
		ServiceWrapperMultiple<Console> *inst =  ServiceWrapperMultiple<Console>::get_instance();
		const int uid = inst->system_id;

		for (int j = 0; j < MAX_CLIENT; j++)
		{
			if (inst->fifo_status->rwstatus[uid][j] == 2)
			{
				close(inst->fifo_status->writefd[uid][j]);
				close(inst->fifo_status->readfd[uid][j]);
				inst->fifo_status->rwstatus[uid][j] = 0;
			}
		}
	}

	static void delete_shm(int sig)
	{
		get_instance()->ServiceWrapperMultiple<Console>::~ServiceWrapperMultiple<Console>();
		exit(EXIT_SUCCESS);
	}

public:
	ServiceWrapperMultiple<Console>() : ServerCmd(this)
	{
		signal(SIGCHLD, SIG_IGN);
		auto do_nothing = [](int n){};

		signal(SIGUSR1, do_nothing);
		signal(SIGUSR2, do_nothing);

		signal(SIGINT, ServiceWrapperMultiple<Console>::delete_shm);

		const int SHMKEY = 5487;
		const int SHMKEY_CHAT = 5488;
		const int SHMKEY_FIFO = 5489;

		user_status = new_shm<UserStatus>(shm_id, SHMKEY);
		chat_status = new_shm<ChatStatus>(shm_id_chat, SHMKEY_CHAT);
		fifo_status = new_shm<FifoStatus>(shm_id_fifo, SHMKEY_FIFO);

		Console::user_status = user_status;
		Console::fifo_status = fifo_status;

		set_instance();
	}

	~ServiceWrapperMultiple<Console>()
	{
		shmdt(user_status);
		shmctl(shm_id, IPC_RMID, NULL);
		shmdt(chat_status);
		shmctl(shm_id_chat, IPC_RMID, NULL);
		shmdt(fifo_status);
		shmctl(shm_id_fifo, IPC_RMID, NULL);
	}

	int get_uid()
	{
		return Console::system_id;
	}

	void set_uid(int uid)
	{
		get_instance()->system_id = uid;
		Console::system_id = uid;
	}

	void set_env(int uid, std::string key, std::string val)
	{
		Console::env[key] = val;
	}

	std::string get_env(int uid, std::string key)
	{
		return Console::env[key];
	}


	void enter(int clientfd, sockaddr_in client_addr)
	{
		auto do_nothing = [](int n){};

		signal(SIGCHLD, do_nothing);
		signal(SIGUSR1, ServiceWrapperMultiple<Console>::collect_fifo_garbage);
		signal(SIGUSR2, ServiceWrapperMultiple<Console>::receive_message);

		Console::replace_fd(clientfd);

		User user;
		inet_ntop( AF_INET, &(client_addr.sin_addr.s_addr), user.ip, INET_ADDRSTRLEN );
		user.port = client_addr.sin_port;
		user.clientfd = clientfd;
		strcpy(user.name,"(no name)");

		int uid = user_status->add("(no name)", user.ip, user.port, clientfd);

		set_uid(uid);

		std::string welcome_msg = "*** User '(no name)' entered from ";
		welcome_msg.append(user_status->users[uid].ip);
		welcome_msg += "/";
		welcome_msg += std::to_string(user_status->users[uid].port);
		welcome_msg += ". ***\n";

		std::cout << Console::get_MOTD();
		broadcast(welcome_msg);
	}

	void routine(int my_fd)
	{
		std::string cmd_line;					

		while (Console::get_command())
		{
			cmd_line = Console::cmd_line;

			auto parsed_result = Console::parse_cmd(cmd_line);
			auto commands = Console::setup_builtin_cmd(parsed_result);

			if (!ServerCmd.execute_serv_builtin_cmd(commands))
			{
				commands = Console::issue(cmd_line);
				for (auto &cmd : commands)
				{
					Console::log = "";
					if (cmd.proc_id != -1)
					{
						Console::execute(cmd);
					}
					else
					{
						std::cerr << "Unknown command: [" << cmd.argv[0] << "].\n";
						break;
					}

					if (Console::log != "")
					{
						broadcast(Console::log);
					}
				}
			}

			if (is_leave())
			{
				leave(my_fd);
				break;
			}
		}
	}

	inline bool is_leave()
	{
		return Console::is_exit();	
	}

	void leave(int my_fd)
	{
		int me = get_uid();
		std::string user_left_tip = "*** User '";
		user_left_tip.append(user_status->users[me].name);
		user_left_tip.append("' left. ***\n");

		broadcast(user_left_tip);

		user_status->remove(me);

		close(my_fd);
	}

	void broadcast(std::string message)
	{
		for (int i = 1; i < MAX_CLIENT; i++)
		{
			if (user_status->is_available(i))
			{
				send_to(i, message);
			}
		}
	}

	void send_to(int id, std::string message)
	{
		if (id == get_uid())
		{
			std::cout << message;
			return;
		}

		const int from = get_uid();

		for (int i = 0; i < 10; i++)
		{
			if (chat_status->who[id].chat_buffer[i].from == -1)
			{
				chat_status->who[id].chat_buffer[i].from = from;
				strcpy(chat_status->who[id].chat_buffer[i].content, message.c_str());
				kill(0, SIGUSR2); 

				break;
			}
		}
	}


private:
	inline void set_instance()
	{
		instance = this;
	}

};

template <class Console>
ServiceWrapperMultiple<Console> *ServiceWrapperMultiple<Console>::instance = NULL;

#endif

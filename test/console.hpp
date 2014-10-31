#ifndef __CONSOLE_HPP__
#define __CONSOLE_HPP__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <tuple>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <regex>
#include "server.hpp"

enum pid_dic { CHILD = 0 };
enum cmd_dic { CMD = 0, PIPETO = 1};
enum pip_dic { PIPEIN = 1, PIPEOUT = 0};

class PipeManager
{
private:
	enum std_dic { STDIN = 0, STDOUT, STDERR};
	enum pip_dic { PIPEIN = 1, PIPEOUT = 0, PIPEIN_USED = 3, PIPEOUT_USED = 2};
	int default_in;
	int default_out;

public:
	std::map<int, std::tuple<int, int, bool, bool>> pipe_lookup;

	PipeManager() : default_in(STDIN), default_out(STDOUT)
	{}

	void set_default_in(int fd)
	{
		default_in = fd;
	}

	void set_default_out(int fd)
	{
		default_out = fd;
	}

	std::tuple<int, int> get_fd(int proc_id, int pipe_to) // It's implemented to get the IO of the process with proc_id.
	{
		int fdin = default_in;
		int fdout = default_out;

		auto prev = pipe_lookup.find(proc_id - 1);
		auto next = pipe_lookup.find(proc_id);
		if (pipe_to > 1)
			next = pipe_lookup.find(proc_id + pipe_to - 1);

		if (prev != pipe_lookup.end())
		{
			fdin = std::get<PIPEOUT>(prev->second);
			std::get<PIPEOUT_USED>(prev->second) = true;
		}

		if (next != pipe_lookup.end())
		{
			fdout = std::get<PIPEIN>(next->second);
			std::get<PIPEIN_USED>(prev->second) = true;
		}

		return std::make_tuple(fdin, fdout);
	}

	void register_pipe(int pipe_id)
	{
		auto it = pipe_lookup.find(pipe_id);
		if (it != pipe_lookup.end())
		{
			// This pipe is exist, so need not to register again!
			return;
		}

		int fd[2];
		if (pipe(fd) < 0)
		{
			std::cerr << "Pipe open failed!\n";
			exit(EXIT_FAILURE);
		}

		pipe_lookup[pipe_id] = std::make_tuple(fd[PIPEOUT], fd[PIPEIN], false, false); // fd[0], fd[1], false, false
	}

	void unregister_pipe(int pipe_id)
	{
		auto it = pipe_lookup.find(pipe_id);
		if (it == pipe_lookup.end())
		{
			// This pipe is no found, so need not to unregister!
			return;
		}

		bool is_need_to_close = (std::get<PIPEIN_USED>(it->second) && std::get<PIPEOUT_USED>(it->second));

		if (is_need_to_close)
		{
			close(std::get<PIPEIN>(it->second));
			close(std::get<PIPEOUT>(it->second));
			
			pipe_lookup.erase(it);
		}
	}

};



template <class Parser, class PipeManager>
class Console : public Parser, public PipeManager // Policy-based design class
{
private:
	std::string cmd_line;
	std::vector<std::tuple<std::vector<std::string>, int>> cmd_result;
	std::vector<std::tuple<int, int>> pipe_vector; // DEPRECATED
	std::vector<int> exectest;
	int proc_counter;
	int client_fd;

	typedef std::vector<std::tuple<std::vector<std::string>, int>> command_t;

public:
	Console() : proc_counter(0)
	{
		setenv("PATH", "bin:.", true);
	}

	void replace_fd(int new_fd)
	{
		dup2(new_fd, 0);
		dup2(new_fd, 1);
		dup2(new_fd, 2);
	}

	command_t parse_cmd(std::string &cmd_line)
	{
		auto tokens = Parser::split(cmd_line);
		command_t cmd_result;

		std::vector<std::string> single_command;
		int pipe_to = 0;

		for (auto &token : tokens)
		{
			const bool is_pipe_symbol = token.find("|") != std::string::npos;
			if (is_pipe_symbol)
			{
				if (token.length() == 1)
				{
					pipe_to = 1;
				}
				else
				{
					pipe_to = std::atoi(token.c_str() + 1);
				}

				cmd_result.push_back(std::make_tuple(std::vector<std::string>(), pipe_to));
				std::get<CMD>(cmd_result.back()).swap(single_command);
			}
			else
			{
				single_command.push_back(token);
			}
		}

		if (single_command.size() != 0)
		{
			cmd_result.push_back(std::make_tuple(std::move(single_command), 0));
		}

		return cmd_result;
	}


	void run()
	{
		std::cout << get_MOTD();

		std::cout << "% ";
		while (std::getline(std::cin, cmd_line))
		{
			cmd_result = parse_cmd(cmd_line);

			bool is_go_ahead = specify_cmd(cmd_result);
			if (!is_go_ahead) break;

			int exec_size = verify_cmd(cmd_result, exectest);

			int infd, outfd;
			for (int i = 0; i < cmd_result.size(); i++)
			{
				int proc_id = exectest[i];
				if (proc_id != -1)
				{
					int pipe_to = std::get<PIPETO>(cmd_result[i]);

					bool is_need_pipe = (pipe_to != 0);

					if (is_need_pipe)
					{
						int pipe_id = proc_id + (pipe_to - 1);
						PipeManager::register_pipe(pipe_id);
					}
					execute(std::get<CMD>(cmd_result[i]), proc_id, pipe_to);
				}
				else
				{
					std::cerr << " Unknown command: [" << std::get<CMD>(cmd_result[i])[0] << "].\n";
					break;
				}
			}

			std::cout << "% ";
		}
	}


	bool specify_cmd(std::vector<std::tuple<std::vector<std::string>, int>> &cmd_result)
	{
		for (auto it = cmd_result.begin(); it != cmd_result.end(); ++it)
		{
			auto cmd  = std::get<CMD>(*it);
			if (cmd[0] == "printenv")
			{
				if (cmd.size() == 2)
					std::cout << cmd[1] << "=" << getenv(cmd[1].c_str()) << "\n";

				cmd_result.erase(it);
			}
			else if (cmd[0] == "setenv")
			{
				if (cmd.size() == 3)
					setenv(cmd[1].c_str(), cmd[2].c_str(), true);

				cmd_result.erase(it);
			}
			else if (cmd[0] == "exit")
			{
				return false;
			}

			if (cmd_result.size() < 1) break;
		}

		return true;
	}


	int verify_cmd(std::vector<std::tuple<std::vector<std::string>, int>> &cmd_result, std::vector<int> &exectest)
	{
		exectest.clear();
		int count = 0;
		bool is_terminated = false;
		std::string path_all(getenv("PATH"));
		std::vector<std::string> paths;

		paths = Parser::split(path_all, ":");

		bool is_found;
		for (auto &cmd : cmd_result)
		{
			is_found = false;

			if (is_terminated)
			{
				exectest.push_back(-1);
				continue;
			}

			// Test executable in each path
			for (auto &path : paths)
			{
				if (is_file_exist_future(std::get<CMD>(cmd)[0], path))
				{
					exectest.push_back(proc_counter++);
					count++;

					is_found = true;
					break;
				}
			}

			if (is_found == false)
			{
				exectest.push_back(-1);
				is_terminated = true;
			}
		}
		
		return count;
	}


	void execute(std::vector<std::string> &cmd_result, int proc_id = -1, int pipe_to = 0)
	{
		bool is_write_file = false;
		std::string ofilename;

		std::vector<char *> argv;
		for (auto &s : cmd_result)
		{
			if (s == ">")
			{
				is_write_file = true;
				continue;
			}

			if (is_write_file == false)
			{
				argv.push_back((char *)s.c_str());
			}
			else
			{
				ofilename = s;
			}
		}
		argv.push_back(NULL);

		pid_t child_pid = fork();
		if (child_pid < 0)
		{
			std::cerr << "fork child failed.\n";
			exit(1);
		}

		int infd, outfd;

		auto prev = PipeManager::pipe_lookup.find(proc_id - 1);

		if (child_pid == CHILD)
		{
			if (prev != PipeManager::pipe_lookup.end())
			{
				infd = std::get<PIPEOUT>(prev->second);
				dup2(infd, 0);
				close(std::get<PIPEIN>(prev->second));
			}

			if (pipe_to > 0)
			{
				auto next = PipeManager::pipe_lookup.find(proc_id + pipe_to - 1);

				if (next != PipeManager::pipe_lookup.end())
				{
					outfd = std::get<PIPEIN>(next->second);
					dup2(outfd, 1);
					close(std::get<PIPEOUT>(next->second));

					if (is_write_file)
					{
						// When writing to file, close the input-side of Pipe
						close(std::get<PIPEIN>(next->second)); 
					}
				}
			}

			if (is_write_file)
			{
				const int oflags = O_CREAT | O_WRONLY;
				const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

				int filefd = open(ofilename.c_str(), oflags, mode);
				if (filefd < 0)
				{
					std::cerr << "File open failed!\n";
					exit(EXIT_FAILURE);
				}

				dup2(filefd, 1);
			}

			execvp(argv[0], argv.data());
		}
		else
		{
			if (prev != PipeManager::pipe_lookup.end())
			{
				close(std::get<PIPEIN>(prev->second));
				close(std::get<PIPEOUT>(prev->second));
			}
			
			int status;
			if (wait(&status) < 0)
			{
				std::cerr << "Wait child failed.\n";
				exit(EXIT_FAILURE);
			}
		}
	}

	bool is_file_exist(std::string filename)
	{
		std::string path(getenv("PATH"));
		filename = path + "/" + filename;
		return access(filename.c_str(), F_OK) == 0;
	}

	bool is_file_exist_future(std::string &filename, std::string &prefix)
	{
		std::string testname = prefix + "/" + filename;

		bool is_exist = (access(testname.c_str(), F_OK) == 0);

		if (is_exist)
		{
			filename = testname;
		}

		return is_exist;
	}

	std::string get_MOTD()
	{
		std::string filename("etc/motd");
		
		char *motd_buffer;
		std::fstream motd_file(filename);

		if (!motd_file)
		{
			std::cerr << "MOTD file open failed!\n";
			exit(EXIT_FAILURE);
		}

		motd_file.seekg(0, motd_file.end);
		int file_length = motd_file.tellg();
		motd_file.seekg(0, motd_file.beg);
		
		motd_buffer = new char[file_length + 1];
		motd_file.read(motd_buffer, file_length);

		std::string motd(motd_buffer);

		motd_file.close();
		delete []motd_buffer;

		return motd;
	}
};

#endif

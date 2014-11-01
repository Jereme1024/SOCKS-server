#ifndef __CONSOLE_HPP__
#define __CONSOLE_HPP__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <tuple>
#include <iostream>
#include <map>

enum pid_dic { CHILD = 0 };
enum cmd_dic { CMD = 0, PIPETO = 1};
enum pip_dic { PIPEIN = 1, PIPEOUT = 0};
enum res_dic { DEFAULT, PIPE, FL};

struct Command
{
	int proc_id;
	std::string prefix;
	std::vector<std::string> argv;
	int resource_type;
	int pipe_to;
	std::string filename;
};

template <class Parser>
class Console : public Parser // Policy-based design class
{
private:
	std::string cmd_line;
	std::vector<std::tuple<std::vector<std::string>, int>> cmd_result;
	std::vector<int> exectest;
	std::map<int, std::tuple<int, int>> pipe_lookup;
	int proc_counter;

	typedef std::vector<std::tuple<std::vector<std::string>, int>> command_t;
	typedef std::vector<std::vector<std::string>> parse_tree;
	typedef std::vector<Command> command_vec;

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
		command_t cmd_result;

		if (cmd_line.find("/") != std::string::npos)
		{
			std::cout << "Permission denied.\n";
		}
		else
		{
			auto tokens = Parser::split(cmd_line);

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
		}

		return cmd_result;
	}

	void execute_cmd(command_t &cmd_result, std::vector<int> &exectest)
	{
		for (int i = 0; i < cmd_result.size(); i++)
		{
			int proc_id = exectest[i];
			if (proc_id != -1)
			{
				int pipe_to = std::get<PIPETO>(cmd_result[i]);

				const bool is_need_pipe = (pipe_to != 0);
				if (is_need_pipe)
				{
					const int pipe_id = proc_id + (pipe_to - 1);
					register_pipe(pipe_id);
				}
				execute(cmd_result[i], proc_id);

			}
			else
			{
				std::cerr << " Unknown command: [" << std::get<CMD>(cmd_result[i])[0] << "].\n";
				break;
			}
		}

	}


	command_vec setup_cmd(parse_tree &parsed_cmd)
	{
		command_vec commands;

		for (auto &argv : parsed_cmd)
		{
			Command cmd;
			cmd.resource_type = DEFAULT;

			if (argv.back().find("|") != std::string::npos)
			{
				cmd.resource_type = PIPE;

				int pipe_to = 1;
				if (argv.back().length() != 1)
				{
					pipe_to = std::atoi(argv.back().c_str() + 1);
				}

				cmd.pipe_to = pipe_to;
				argv.pop_back();
			}

			if (argv.size() > 2)
			{
				if (argv[argv.size() - 2] == ">")
				{
					cmd.resource_type = FL;
					
					cmd.filename = argv.back();
					argv.resize(argv.size() - 2);
				}
			}

			cmd.argv = argv;

			commands.push_back(std::move(cmd));
		}

		verify_cmd_future(commands);

		return commands;
	}

	void verify_cmd_future(command_vec &commands)
	{
		bool is_terminated = false;
		std::string path_all(getenv("PATH"));
		std::vector<std::string> paths;

		paths = Parser::split(path_all, ":");

		bool is_cmd_found;
		for (auto &cmd : commands)
		{
			is_cmd_found = false;

			// Test executable in each path
			for (auto &path : paths)
			{
				if (is_file_exist(cmd.argv[0], path))
				{
					cmd.prefix = path + "/";
					cmd.proc_id = proc_counter++;

					is_cmd_found = true;
					break;
				}
			}

			if (is_cmd_found == false)
			{
				cmd.proc_id = -1;
				break;
			}
		}
	}

	bool execute_builtin_cmd(command_vec &commands)
	{
		for (auto it = commands.begin(); it != commands.end(); ++it)
		{
			auto &cmd  = it->argv;
			if (cmd[0] == "printenv")
			{
				if (cmd.size() == 2)
					std::cout << cmd[1] << "=" << getenv(cmd[1].c_str()) << "\n";

				commands.erase(it);
			}
			else if (cmd[0] == "setenv")
			{
				if (cmd.size() == 3)
					setenv(cmd[1].c_str(), cmd[2].c_str(), true);

				commands.erase(it);
			}
			else if (cmd[0] == "exit")
			{
				return false;
			}

			if (commands.size() < 1) break;
		}
		return true;
	}

	void execute_cmd_future(command_vec &commands)
	{
		for (auto &cmd : commands)
		{
			if (cmd.proc_id != -1)
			{
				execute_future(cmd);
			}
			else
			{
				std::cerr << " Unknown command: [" << cmd.argv[0] << "].\n";
				break;
			}
		}
	}


	inline std::vector<char *> c_style(std::vector<std::string> &vec_str)
	{
		std::vector<char *> vec_charp;
		for (auto &str : vec_str)
		{
			vec_charp.push_back((char *)str.c_str());
		}
		vec_charp.push_back(NULL);

		return vec_charp;
	}

	void execute_future(Command &cmd)
	{
		if (cmd.resource_type == PIPE)
		{
			register_pipe(cmd.proc_id + cmd.pipe_to - 1);
		}

		pid_t child_pid = fork();
		if (child_pid < 0)
		{
			std::cerr << "fork child failed.\n";
			exit(1);
		}

		auto prev = pipe_lookup.find(cmd.proc_id - 1);

		if (child_pid == CHILD)
		{
			if (prev != pipe_lookup.end())
			{
				const int infd = std::get<PIPEOUT>(prev->second);
				dup2(infd, 0);
				close(std::get<PIPEIN>(prev->second));
			}
			auto next = prev;

			switch(cmd.resource_type)
			{
				case PIPE:
					next = pipe_lookup.find(cmd.proc_id + cmd.pipe_to - 1);
					if (next != pipe_lookup.end())
					{
						const int outfd = std::get<PIPEIN>(next->second);
						dup2(outfd, 1);
						close(std::get<PIPEOUT>(next->second));
					}
					break;

				case FL:
					const int oflags = O_CREAT | O_WRONLY;
					const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

					const int filefd = open(cmd.filename.c_str(), oflags, mode);
					if (filefd < 0)
					{
						std::cerr << "File open failed!\n";
						exit(EXIT_FAILURE);
					}

					dup2(filefd, 1);
			}

			auto argv = c_style(cmd.argv);
			execvp(argv[0], argv.data());
		}
		else
		{
			unregister_pipe(cmd.proc_id - 1);
			
			int status;
			if (wait(&status) < 0)
			{
				std::cerr << "Wait child failed.\n";
				exit(EXIT_FAILURE);
			}
		}
	}

	void run()
	{
		std::cout << get_MOTD();

		while (get_command())
		{
			auto parsed_result = parse_cmd_future(cmd_line);
			auto commands = setup_cmd(parsed_result);

			if (execute_builtin_cmd(commands) == false)
				break;

			execute_cmd_future(commands);
		}
	}

	parse_tree parse_cmd_future(std::string &cmd_line)
	{
		parse_tree result;

		if (cmd_line.find("/") != std::string::npos)
		{
			std::cout << "Permission denied.\n";
		}
		else
		{
			std::vector<std::string> single_command;

			auto tokens = Parser::split(cmd_line);
			for (auto &token : tokens)
			{
				single_command.push_back(token);
				
				const bool is_pipe_symbol = token.find("|") != std::string::npos;
				if (is_pipe_symbol)
				{
					result.push_back(std::vector<std::string>());
					result.back().swap(single_command);
				}
			}

			if (single_command.size() != 0)
			{
				result.push_back(std::move(single_command));
			}
		}

		return result;
	}

	void run_deprecated()
	{
		std::cout << get_MOTD();

		while (get_command())
		{
			cmd_result = parse_cmd(cmd_line);

			if (specify_cmd(cmd_result))
			{
				verify_cmd(cmd_result, exectest);
				execute_cmd(cmd_result, exectest);
			}
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
				if (is_file_exist(std::get<CMD>(cmd)[0], path))
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

	void execute(std::tuple<std::vector<std::string>, int> &cmd_result, int proc_id = -1)
	{
		bool is_write_file = false;
		std::string ofilename;

		std::vector<char *> argv;
		for (auto &s : std::get<CMD>(cmd_result))
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
		
		int pipe_to = std::get<PIPETO>(cmd_result);
		const bool is_need_pipe = (pipe_to != 0);
		if (is_need_pipe)
		{
			const int pipe_id = proc_id + (pipe_to - 1);
			register_pipe(pipe_id);
		}

		pid_t child_pid = fork();

		if (child_pid < 0)
		{
			std::cerr << "fork child failed.\n";
			exit(1);
		}

		int infd, outfd;

		auto prev = pipe_lookup.find(proc_id - 1);

		if (child_pid == CHILD)
		{
			if (prev != pipe_lookup.end())
			{
				infd = std::get<PIPEOUT>(prev->second);
				dup2(infd, 0);
				close(std::get<PIPEIN>(prev->second));
			}

			if (pipe_to > 0)
			{
				auto next = pipe_lookup.find(proc_id + pipe_to - 1);

				if (next != pipe_lookup.end())
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
			unregister_pipe(proc_id - 1);
			
			int status;
			if (wait(&status) < 0)
			{
				std::cerr << "Wait child failed.\n";
				exit(EXIT_FAILURE);
			}
		}
	}


	bool is_file_exist(std::string &filename, std::string &prefix)
	{
		std::string testname = prefix + "/" + filename;

		const bool is_exist = (access(testname.c_str(), F_OK) == 0);
		if (is_exist)
		{
			filename = testname;
		}

		return is_exist;
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

		pipe_lookup[pipe_id] = std::make_tuple(fd[PIPEOUT], fd[PIPEIN]); // fd[0], fd[1]
	}


	void unregister_pipe(int pipe_id)
	{
		auto it = pipe_lookup.find(pipe_id);
		if (it != pipe_lookup.end())
		{
			close(std::get<PIPEIN>(it->second));
			close(std::get<PIPEOUT>(it->second));
			
			pipe_lookup.erase(it);
		}
	}


	inline bool get_command()
	{
		std::cout << "% ";
		return std::getline(std::cin, cmd_line);
	}


	std::string get_MOTD()
	{
		std::string motd;
		motd  = "****************************************\n";
		motd += "** Welcome to the information server. **\n";
		motd += "****************************************\n";
		return motd;
	}
};

#endif

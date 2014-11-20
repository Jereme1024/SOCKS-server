#ifndef __CONSOLE_HPP__
#define __CONSOLE_HPP__

/// @file
/// @author Jeremy
/// @version 1.0
/// @section DESCRIPTION
///
/// The console class is able to handle shell commands and perfom basic and extened pipe and filedump operations.
/// It needs a parser-policy to complete this class.
/// It uses a pipe lookup table to store the pipe needed information of shell commands.
/// The parent of this conosle is roled a pipe fd resource manager including garbage collection of pipe.
/// @ see parser.hpp

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
enum pip_dic { PIPEIN = 1, PIPEOUT = 0};
enum fif_dic { FIFOIN = 1, FIFOOUT = 0};
enum res_dic { DEFAULT, PIPE, FL};


/// @brief This data structure is used to store coresspoding fileds of each shell command including process_id, arguments_vector, pipe_target, and file_target.
struct Command
{
	int proc_id;
	std::vector<std::string> argv;
	int pipe_to;
	std::string filename;
	int fifo_to;
	int fifo_from;

	/// @brief Initialize proc_id, pipe_to, and filename by the unused value.
	Command() : proc_id(-1), pipe_to(0), filename(), fifo_to(0), fifo_from(0)
	{}
};

struct FifoStatus;
class UserStatus;


/// @brief
/// The console class is able to handle shell commands and perfom basic and extened pipe and filedump operations.
template <class Parser>
class Console : public Parser // Policy-based design class
{
private:
	std::string cmd_line;
	std::map<int, std::tuple<int, int>> pipe_lookup;
	int proc_counter;
	bool is_exit_;
	int stdin_backup;
	int stdout_backup;
	int stderr_backup;
	bool is_fd_backup;

protected:
	int system_id;

	typedef std::vector<std::vector<std::string>> parse_tree;
	typedef std::vector<Command> command_vec;
	typedef Command command_t;

	FifoStatus *fifo_status;
	UserStatus *user_status;

public:
	std::string log;

	/// @brief Initialize PATH to be "bin:.".
	Console() : system_id(0), proc_counter(0), is_exit_(false), is_fd_backup(false)
	{
		setenv("PATH", "bin:.", true);
	}

	inline void backup_fd()
	{
		is_fd_backup = true;

		stdin_backup = dup(0);
		stdout_backup = dup(1);
		stderr_backup = dup(2);
	}

	inline void set_fifo_status(FifoStatus *fifost)
	{
		fifo_status = fifost;
	}

	inline void set_user_status(UserStatus *userst)
	{
		user_status = userst;
	}

	inline bool is_exit() { return is_exit_; }
	void unexit()
	{
		is_exit_ = false;
		proc_counter = 0;
		for (auto it = pipe_lookup.begin(); it != pipe_lookup.end(); it++)
		{
			unregister_pipe__(it);
		}

		pipe_lookup.clear();
	}

	/// @brief This method is used to replace the default number in the file description table.
	/// @param new_fd A new fd to be used.
	inline void replace_fd(int new_fd)
	{
		dup2(new_fd, 0);
		dup2(new_fd, 1);
		dup2(new_fd, 2);
	}

	inline void undo_fd()
	{
		if (is_fd_backup == false)
			return;

		dup2(stdin_backup, 0);
		dup2(stdout_backup, 1);
		dup2(stderr_backup, 2);
	}

	inline void set_system_id(int sid)
	{
		system_id = sid;
	}


	/// @brief This method is used to be the generic interface of this class, which telling how a process to deal with.
	void run()
	{
		std::cout << get_MOTD();

		while (get_command())
		{
			auto parsed_result = parse_cmd(cmd_line);
			auto commands = setup_cmd(parsed_result);

			if (execute_builtin_cmd(commands) == false)
				break;

			execute_cmd(commands);
		}
	}

	void issue(std::string cmd_line_in)
	{
		cmd_line = cmd_line_in;

		auto parsed_result = parse_cmd(cmd_line);
		auto commands = setup_cmd(parsed_result);

		if (execute_builtin_cmd(commands) == false)
			return;

		execute_cmd(commands);
	}


	/// @brief This method is used to parse a shell command and split it to mutiple single-command into a parsing tree.
	/// @param str A string of input shell command.
	/// @return A vector of single-command consists of mutiple single_command.
	/// @see parse_tree
	/// @see Command
	parse_tree parse_cmd(std::string &cmd_line)
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


	/// @brief This method is used to set up the fields needed by a Command, and also verify if it is available.
	/// @param parsed_cmd A praseing tree of shell command.
	/// @return A vector of Command consists of available commands.
	command_vec setup_cmd(parse_tree &parsed_cmd)
	{
		command_vec commands;

		for (auto &argv : parsed_cmd)
		{
			command_t cmd;

			if (argv.back().find("|") != std::string::npos)
			{
				int pipe_to = 1;
				if (argv.back().length() != 1)
				{
					pipe_to = std::atoi(argv.back().c_str() + 1);
				}

				cmd.pipe_to = pipe_to;
				argv.pop_back();
			}
			
			if (argv.size() > 1)
			{
				if (argv[argv.size() - 1][0] == '>')
				{
					int fifo_to = std::atoi(argv[argv.size() - 1].c_str() + 1);

					cmd.fifo_to = fifo_to;
					argv.pop_back();
				}
				else if (argv[argv.size() - 1][0] == '<')
				{
					int fifo_from = std::atoi(argv[argv.size() - 1].c_str() + 1);

					cmd.fifo_from = fifo_from;
					argv.pop_back();
				}
			}

			if (argv.size() > 2)
			{
				if (argv[argv.size() - 2] == ">")
				{
					cmd.filename = argv.back();
					argv.resize(argv.size() - 2);
				}
			}

			cmd.argv = argv;

			commands.push_back(std::move(cmd));
		}

		verify_cmd(commands);

		return commands;
	}

	command_vec setup_builtin_cmd(parse_tree &parsed_cmd)
	{
		command_vec commands;

		for (auto &argv : parsed_cmd)
		{
			command_t cmd;

			if (argv.back().find("|") != std::string::npos)
			{
				int pipe_to = 1;
				if (argv.back().length() != 1)
				{
					pipe_to = std::atoi(argv.back().c_str() + 1);
				}

				cmd.pipe_to = pipe_to;
				argv.pop_back();
			}
			
			cmd.argv = argv;

			commands.push_back(std::move(cmd));
		}

		return commands;
	}


	/// @brief This method is used to test whether this binary can be found in PATH.
	/// @param commands A command_vec to be tested and add the current path at the same time.
	void verify_cmd(command_vec &commands)
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
					cmd.proc_id = proc_counter++;

					is_cmd_found = true;
					break;
				}
			}

			if (is_cmd_found == false) break;
		}
	}


	/// @brief This method is used to execute the built-in shell commands.
	/// @param commands A command_vec to be executed.
	/// @return A boolean value represeted whether this Console running.
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
				is_exit_ = true;
				return false;
			}

			if (commands.size() < 1) break;
		}
		return true;
	}


	/// @brief This method is used to be the template(process) to execute mutiple single-commands.
	/// @param commands A command_vec to be executed.
	void execute_cmd(command_vec &commands)
	{
		log = "";

		for (auto &cmd : commands)
		{
			if (cmd.proc_id != -1)
			{
				execute(cmd);
			}
			else
			{
				std::cerr << "Unknown command: [" << cmd.argv[0] << "].\n";
				break;
			}
		}
	}


	
	/// @brief This method is used to translate the vector of string to vector of char* also add a NULL padding in the end of vector.
	/// @param vec_str A vector of string
	/// @return A vector of char* having a NULL end padding.
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

	
	/// @brief This method is used to execute the shell commands.
	/// Request the resources needed (e.g. pipe and file) to system and deal with fork/exec/wait(...) and open/close(...) controls.
	/// @param cmd A command_t (single-command) to be executed.
	void execute(command_t &cmd)
	{
		const bool need_pipe = (cmd.pipe_to > 0);
		const bool need_file = (cmd.filename.length() > 0);
		const bool need_fifo_to = (cmd.fifo_to > 0);
		const bool need_fifo_from = (cmd.fifo_from > 0);


		if (need_pipe)
		{
			register_pipe(cmd.proc_id + cmd.pipe_to - 1);
		}

		int fifo_in;
		int fifo_out;
		if (need_fifo_to)
		{
			if (user_status->is_available(cmd.fifo_to))
			{
				if (fifo_status->rwstatus[system_id][cmd.fifo_to] != 0)
				{
					//fifo_in = fifo_status->writefd[system_id][cmd.fifo_to];
					std::cout << "*** Error: the pipe #" << system_id << "->#" << cmd.fifo_to << " already exists. ***" << std::endl;
					return;
				}
				else
				{
					fifo_in = fifo_wr(system_id, cmd.fifo_to);
					fifo_status->writefd[system_id][cmd.fifo_to] = fifo_in;
					fifo_status->rwstatus[system_id][cmd.fifo_to] = 1;
				}

				fifo_status->rwstatus[system_id][cmd.fifo_to] = 1;
				dup2(fifo_in, 1);

				std::string me = user_status->users[system_id].name;
				std::string me_id = std::to_string(system_id);
				std::string who = user_status->users[cmd.fifo_to].name;
				std::string who_id = std::to_string(cmd.fifo_to);


				// Escape the '\r' symbol
				fix_return_symbol(cmd_line);
				log = "*** " + me + " (#" + me_id + ") just piped '" + cmd_line + "' to " + who + " (#" + who_id + ") ***\n";
			}
			else
			{
				std::cout << "*** Error: user #" << cmd.fifo_to << " does not exist yet. ***" << std::endl;
				return;
			}
		}
		
		if (need_fifo_from)
		{
			if (fifo_status->rwstatus[cmd.fifo_from][system_id] != 1)
			{
				std::cout << "*** Error: the pipe #" << cmd.fifo_from << "->#" << system_id << " does not exist yet. ***" << std::endl;

				unregister_pipe(cmd.proc_id - 1);

				return;
			}

			std::string me = user_status->users[system_id].name;
			std::string me_id = std::to_string(system_id);
			std::string who = user_status->users[cmd.fifo_from].name;
			std::string who_id = std::to_string(cmd.fifo_from);
			

			// Escape the '\r' symbol
			fix_return_symbol(cmd_line);
			log = "*** " + me + " (#" + me_id + ") just received from " + who + " (#" + who_id + ") by '" + cmd_line + "' ***\n";

			fifo_out = fifo_rd(cmd.fifo_from, system_id);

			fifo_status->rwstatus[cmd.fifo_from][system_id] = 2;

			kill(0, SIGUSR1); // close the input side of FIFO
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

				if (need_fifo_from)
					close(std::get<PIPEOUT>(prev->second));
			}

			if (need_pipe)
			{
				auto next = pipe_lookup.find(cmd.proc_id + cmd.pipe_to - 1);
				if (next != pipe_lookup.end())
				{
					if (need_file || need_fifo_to)
					{
						close(std::get<PIPEIN>(next->second));
						close(std::get<PIPEOUT>(next->second));
					}
					else
					{
						const int outfd = std::get<PIPEIN>(next->second);
						dup2(outfd, 1);
						close(std::get<PIPEOUT>(next->second));
					}
				}
			}

			if (need_file)
			{
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

			if (need_fifo_from)
			{
				dup2(fifo_out, 0);
			}

			//std::cout << "Execute cmd go..." << std::endl;
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

			if (need_fifo_from)
			{
				close(fifo_out);
				std::string fifo_name = get_fifo_name(cmd.fifo_from, system_id);
				fifo_status->rwstatus[cmd.fifo_from][system_id] = 0;

				if (unlink(fifo_name.c_str()) < 0)
				{
					std::cerr << "Unlink fifo " << fifo_name << " failed!\n";
					perror("Error");
					exit(EXIT_FAILURE);
				}
			}
		}
	}


	/// @brief This method is used to test a binary in a path is available.
	/// @param filename The name of binary.
	/// @param prefix A prefix path of binary.
	inline bool is_file_exist(std::string &filename, std::string &prefix)
	{
		std::string testname = prefix + "/" + filename;

		const bool is_exist = (access(testname.c_str(), F_OK) == 0);
		if (is_exist)
		{
			filename = testname;
		}

		return is_exist;
	}


	/// @brief This method is used to allocate a pipe and write to pipe lookup.
	/// @param pipe_id The id of a process.
	inline void register_pipe(int pipe_id)
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


	/// @brief This method is used to deallocate a pipe and remove from pipe lookup.
	/// @brief Usually, it's called in parent proecess with proc_id which is last one to use such pipe_id.
	/// @param pipe_id The id of a process.
	inline void unregister_pipe(int pipe_id)
	{
		auto it = pipe_lookup.find(pipe_id);
		if (it != pipe_lookup.end())
		{
			unregister_pipe__(it);
			pipe_lookup.erase(it);
		}
	}

	inline void unregister_pipe__(decltype(pipe_lookup.begin()) it)
	{
		close(std::get<PIPEIN>(it->second));
		close(std::get<PIPEOUT>(it->second));
	}

	inline std::string get_fifo_name(int from, int to)
	{
		return std::string("/tmp/5487_" + std::to_string(from) + "_" + std::to_string(to));
	}


	inline int fifo_wr(int from, int to)
	{
		const int PERMS = 0666;

		std::string fifo_name = get_fifo_name(from, to);
		int retval;
		
		if (mknod(fifo_name.c_str(), S_IFIFO | PERMS, 0) < 0)
		{
			if (unlink(fifo_name.c_str()) < 0)
			{
				std::cerr << "Unlink fifo " << fifo_name << " failed!\n";
				perror("Error");
				exit(EXIT_FAILURE);
			}
			std::cerr << "Create fifo " << fifo_name << " failed! " << strerror(errno) << "\n";
			perror("Error");
			exit(EXIT_FAILURE);
		}

		int tmp;

		if ((tmp = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			std::cerr << "Open fifo_out " << fifo_name << " failed!\n";
			perror("Error");
			exit(EXIT_FAILURE);
		}

		if ((retval = open(fifo_name.c_str(), O_WRONLY)) < 0)
		{
			std::cerr << "Open fifo_out " << fifo_name << " failed!\n";
			perror("Error");
			exit(EXIT_FAILURE);
		}

		return retval;
	}

	inline int fifo_rd(int from, int to)
	{
		std::string fifo_name = get_fifo_name(from, to);
		int retval;
		
		if ((retval = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK)) < 0)
		{
			std::cerr << "Open fifo_out " << fifo_name << " failed!\n";
			perror("Error");
			exit(EXIT_FAILURE);
		}

		return retval;
	}


	/// @brief This method is used to print a console symbol and perfom std::cin.
	/// @return A boolean value whether get a input string.
	inline bool get_command()
	{
		std::cout << "% ";
		return std::getline(std::cin, cmd_line);
	}


	/// @brief This method is used to get a "message of today" string.
	/// @return A string of MOTD.
	std::string get_MOTD()
	{
		std::string motd;
		motd  = "****************************************\n";
		motd += "** Welcome to the information server. **\n";
		motd += "****************************************\n";
		return motd;
	}

	void fix_return_symbol(std::string &str)
	{
		if (str[str.length() - 1] == '\r')
			str.resize(str.length() - 1);
	}

	~Console()
	{
		for (auto it = pipe_lookup.begin(); it != pipe_lookup.end(); it++)
		{
			unregister_pipe__(it);
		}

		undo_fd();
	}
};

#endif

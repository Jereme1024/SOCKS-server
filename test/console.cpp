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
		pipe(fd);

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

class Parser
{
public:
	void parse(std::vector<std::tuple<std::vector<std::string>, int>> &cmd_result, std::string &cmd_line)
	{
		std::vector<std::string> ws_result;

		cmd_result.clear();

		split_by_whitespace(ws_result, cmd_line);

		std::vector<std::string> prog;
		for (auto &token : ws_result)
		{
			bool is_pipe_symbol = (token.find("|") != std::string::npos);
			if (is_pipe_symbol)
			{
				if (token.size() == 1)
				{
					cmd_result.push_back(std::make_tuple(prog, 1));
				}
				else
				{
					cmd_result.push_back(std::make_tuple(prog, std::atoi(token.c_str() + 1)));
				}
				
				prog.clear();
			}
			else
			{
				prog.push_back(token);
			}
		}

		if (prog.size() != 0)
		{
			cmd_result.push_back(std::make_tuple(prog, 0));
		}
	}

	void split_by_whitespace(std::vector<std::string> &result, std::string &str)
	{
		std::string buf;
		std::stringstream ss(str);

		while (ss >> buf)
		{
			result.push_back(buf);
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

public:
	Console() : proc_counter(0)
	{
		std::cout << get_MOTD();
	}


	void run()
	{
		std::cout << "% ";
		while (std::getline(std::cin, cmd_line))
		{
			Parser::parse(cmd_result, cmd_line);

			specify_cmd(cmd_result);

			int exec_size = verify_cmd(cmd_result, exectest);

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

				}
				else
				{
					break;
				}
			}

			int infd, outfd;
			for (int i = 0; i < cmd_result.size(); i++)
			{
				int proc_id = exectest[i];
				if (proc_id != -1)
				{
					int pipe_to = std::get<PIPETO>(cmd_result[i]);
					execute(std::get<CMD>(cmd_result[i]), proc_id, pipe_to);
				}
				else
				{
					std::cerr << " Unknown command: [" << std::get<CMD>(cmd_result[i])[0] << "]\n";
					break;
				}
			}

			std::cout << "% ";
		}
	}


	void specify_cmd(std::vector<std::tuple<std::vector<std::string>, int>> &cmd_result)
	{
		for (auto it = cmd_result.begin(); it != cmd_result.end(); ++it)
		{
			auto cmd  = std::get<CMD>(*it);
			if (cmd[0] == "printenv")
			{
				std::cout << cmd[1] << "= " << getenv(cmd[1].c_str()) << "\n";
				cmd_result.erase(it);
			}
			else if (cmd[0] == "setenv")
			{
				setenv(cmd[1].c_str(), cmd[2].c_str(), true);
				cmd_result.erase(it);
			}

			if (cmd_result.size() < 1) break;
		}
	}


	int verify_cmd(std::vector<std::tuple<std::vector<std::string>, int>> &cmd_result, std::vector<int> &exectest)
	{
		exectest.clear();
		int count = 0;
		bool is_terminated = false;
		for (auto &cmd : cmd_result)
		{
			if (is_terminated)
			{
				exectest.push_back(-1);
				continue;
			}

			if (is_file_exist(std::get<CMD>(cmd)[0]))
			{
				exectest.push_back(proc_counter++);
				count++;
			}
			else
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

	std::string get_MOTD()
	{
		std::string filename("motd");
		
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


int main(int argc, char *argv[])
{
	//std::vector<std::tuple<std::vector<std::string>, int>> cmd_result;
	////std::string cmd_line = "ls -al -h | cat      | cat    |3 cat  ";
	//std::string cmd_line = "ls -al -h | cat | qq";

	//Parser parser;

	//parser.parse(cmd_result, cmd_line);

	//for (auto &t : cmd_result)
	//{
	//	std::cout << "CMD: ";
	//	for (auto &s : std::get<CMD>(t))
	//	{
	//		std::cout << s << " ";
	//	}
	//	std::cout << "\n";
	//	std::cout << "PIPETO: " << std::get<PIPETO>(t) << "\n";
	//}
	//
	
	Console<Parser, PipeManager> console;

	console.run();
	

	return 0;
}

#endif

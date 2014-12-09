#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
#include "server.hpp"
#include "parser.hpp"

#include <cstdio>

struct HtmlMeta
{
	std::string script_name;
	std::string script_path;
	std::string query_string;
	std::string request_method;

	HtmlMeta(std::string str)
	{
		parse(str);
	}

	void parse(std::string &str)
	{
		auto by_blank = SimpleParser::split(str);
		auto by_question = SimpleParser::split(by_blank[1], "?");
		auto by_and = SimpleParser::split(by_question[1], "&");

		request_method = by_blank[0];
		script_name = by_question[0];
		script_path = "." + by_question[0];
		query_string = by_question[1];
	}
};

class HttpdService
{
public:
	void block_io(int fd, char is_block)
	{
		if (ioctl(fd, FIONBIO, (char *)&is_block))
		{
			perror("ioctl");
			exit(-1);
		}
	}

	void enter(int clientfd, sockaddr_in &client_addr)
	{
		dup2(clientfd, 0);
		dup2(clientfd, 1);

		std::cout << "HTTP/1.1 200 OK\n";
	}

	void routine(int clientfd)
	{
		std::string input;
		std::vector<std::string> meta;

		int i = 0;
		while (std::getline(std::cin, input))
		{
			block_io(clientfd, false);
			meta.push_back(input);
		}
		block_io(clientfd, true);

		if (meta.size() == 0)
		{
			std::cout << "Content-type: text/html\n\n";
			std::cout << "500 Something wrong!\n";
		}

		auto html_meta = HtmlMeta(meta[0]);

		auto dot_pos = html_meta.script_name.find_last_of(".");
		std::string file_type = html_meta.script_name.c_str() + dot_pos + 1;

		if (!is_file_exist(html_meta.script_path))
		{
			std::cout << "Content-type: text/html\n\n";
			std::cout << "404 Not found!\n";
		}
		else
		{
			if (file_type == "cgi")
			{
				open_cgi(html_meta);
			}
			else if (file_type == "html")
			{
				std::cout << "Content-type: text/html\n\n";
				open_html(html_meta);
			}
			else
			{
				std::cout << "Content-type: text/html\n\n";
				std::cout << "403 Access deny!\n";
			}
		}
	}

	inline bool is_file_exist(std::string &filename)
	{
		const bool is_exist = (access(filename.c_str(), F_OK) == 0);
		return is_exist;
	}

	void open_html(HtmlMeta &html_meta)
	{
		std::fstream html_context(html_meta.script_path);
		std::string text;

		while (html_context >> text)
		{
			std::cout << text << "\n";
		}

		html_context.close();
	}

	void open_cgi(HtmlMeta &html_meta)
	{
		char **argv = new char *[2];
		argv[1] = NULL;

		argv[0] = (char *)html_meta.script_path.c_str();

		set_environment(html_meta);

		if (execvp(argv[0], argv) < 0)
		{
			perror("exec");
		}
	}

	void set_environment(HtmlMeta &html_meta)
	{
		setenv("QUERY_STRING", html_meta.query_string.c_str(), 1);
		setenv("CONTENT_LENGTH", "123", 1);
		setenv("REQUEST_METHOD", html_meta.request_method.c_str(), 1);
		setenv("SCRIPT_NAME", html_meta.script_name.c_str(), 1);
		setenv("REMOTE_HOST", "somewhere.nctu.edu.tw", 1);
		setenv("REMOTE_ADDR", "140.113.1.1", 1);
		setenv("AUTH_TYPE", "auth_type", 1);
		setenv("REMOTE_USER", "remote_user", 1);
		setenv("REMOTE_IDENT", "remote_ident", 1);
	}

	bool is_leave(int clientfd)
	{
		return true;
	}

	void leave(int clientfd)
	{
	}

	
};

int main()
{
	ServerMultiple<HttpdService> server;
	server.run();

	return 0;
}

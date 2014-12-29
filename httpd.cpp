#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <iostream>
#include <fstream>
#include "server.hpp"
#include "parser.hpp"

struct HtmlMeta
{
	std::string script_name;
	std::string script_path;
	std::string script_type;
	std::string query_string;
	std::string request_method;

	HtmlMeta(std::string str)
	{
		parse(str);
	}

	void parse(std::string &str)
	{
		// "GET /what.file?param1=val1&param2=val2...&paramN=valN HTTP/1.1"
		auto by_blank = SimpleParser::split(str);
		auto by_question = SimpleParser::split(by_blank[1], "?");
		auto by_and = SimpleParser::split(by_question[1], "&");

		request_method = by_blank[0];
		script_name = by_question[0];
		script_path = "." + by_question[0];
		script_type = script_name.c_str() + script_name.find_last_of(".") + 1;
		query_string = by_question[1];
	}
};

class HttpdService
{
public:
	void enter(int clientfd, sockaddr_in &client_addr)
	{
		dup2(clientfd, 0);
		dup2(clientfd, 1);

		std::cout << "HTTP/1.1 200 OK\n";
		std::cout.flush();
	}

	void routine(int clientfd)
	{
		std::string input;
		std::vector<std::string> meta;

		while (std::getline(std::cin, input))
		{
			if (input.size() <= 1) break;

			meta.push_back(input);
		}

		auto html_meta = HtmlMeta(meta[0]);

		if (!is_file_exist(html_meta.script_path))
		{
			std::cout << "Content-type: text/html; charset=utf-8\n\n";
			std::cout << "<h1>404 Not found! ヾ(`・ω・´)</h1>\n";
		}
		else
		{
			if (html_meta.script_type == "cgi")
			{
				open_cgi(html_meta);
			}
			else if (html_meta.script_type == "html")
			{
				open_html(html_meta);
			}
			else
			{
				std::cout << "Content-type: text/html; charset=utf-8\n\n";
				std::cout << "<h1>403 Forbidden! ヾ(`・ω・´)</h1>\n";
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

		std::cout << "Content-type: text/html\n\n";

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
		return false;
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

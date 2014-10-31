#include "server.hpp"
#include "console.hpp"
#include "parser.hpp"

template <class Parser>
class ww : public Parser
{
public:

	typedef std::vector<std::tuple<std::vector<std::string>, int>> command_t;

	ww()
	{}

	command_t parse_cmd(std::string &cmd_line)
	{
		auto tokens = this->Parser::split(cmd_line);
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
				std::get<0>(cmd_result.back()).swap(single_command);
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

};


int main(int argc, char *argv[])
{
	typedef Console<SimpleParser, PipeManager> Console_t;

	Server<Console_t> server;
	server.run();

	SimpleParser sp;

	std::string s = "Hello world! My name is Jeremy.";
	std::cout << s << std::endl;

	auto rs = sp.split(s, "!");

	for (auto &s : rs)
	{
		std::cout << s << std::endl;
	}
	
	//ww<SimpleParser> w;

	//std::string cmd_line = "ls -alh |3 cat    | cat";

	//auto yy = w.parse_cmd(cmd_line);

	//for (auto &c : yy)
	//{
	//	std::cout << "pipe to: " << std::get<1>(c);
	//	for (auto &a : std::get<0>(c))
	//	{
	//		std::cout << a << " ";
	//	}
	//	std::cout << std::endl;
	//}


	return 0;
}

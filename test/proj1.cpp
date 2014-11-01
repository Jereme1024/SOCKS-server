#include "server.hpp"
#include "console.hpp"
#include "parser.hpp"

int main(int argc, char *argv[])
{
	typedef Console<SimpleParser> Console_t;

	Server<Console_t> server;
	server.run();

	return 0;
}

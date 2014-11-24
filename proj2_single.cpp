#include "server.hpp"
#include "service.hpp"
#include "console.hpp"
#include "parser.hpp"

int main()
{
	typedef Console<SimpleParser> Console_t;
	ServerSingle<ServiceWrapperSingle<Console_t>> server;

	server.run();

	return 0;
}

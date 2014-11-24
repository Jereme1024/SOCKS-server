#include "server.hpp"
#include "service.hpp"
#include "console.hpp"
#include "parser.hpp"

int main()
{
	typedef Console<SimpleParser> Console_t;
	ServerMultiple<ServiceWrapperMultiple<Console_t>> server;

	server.run();

	return 0;
}

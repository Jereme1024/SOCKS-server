#include "server.hpp"
#include "console.hpp"

int main(int argc, char *argv[])
{
	typedef Console<Parser, PipeManager> Console_t;

	Server<Console_t> server;
	server.run();

	return 0;
}

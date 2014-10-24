#include "server.hpp"
#include "simple_service.hpp"

int main(int argc, char *argv[])
{
	Server<Simple_service> server;

	server.run();

	return 0;
};

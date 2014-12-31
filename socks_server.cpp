#include "socks.hpp"

int main()
{
	ServerMultiple<SocksServerService> socks_server;
	socks_server.run();
}


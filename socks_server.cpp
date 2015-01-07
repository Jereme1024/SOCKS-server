#include "socks.hpp"
#include "firewall.hpp"

int main()
{
	ServerMultiple<SocksServerService<FirewallSocks>> socks_server;
	socks_server.run();
}


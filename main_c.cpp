#include "client.hpp"

int main(int argc, char *argv[])
{
	Client client("127.0.0.1", 5487);

	client.run();

	return 0;
}

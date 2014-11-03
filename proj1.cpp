#include "server.hpp"
#include "console.hpp"
#include "parser.hpp"

/// @file
/// @author Jeremy
/// @version 1.0
/// @section DESCRIPTION
///
/// This is project 1 RAS program.

/// @brief Generate a RAS through Server with policy of Console. Then, execute run of server to wait connection.
int main(int argc, char *argv[])
{
	typedef Console<SimpleParser> Console_t;

	Server<Console_t> server;
	server.run();

	return 0;
}

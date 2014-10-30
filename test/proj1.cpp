#include "server.hpp"
#include "console.hpp"

int main(int argc, char *argv[])
{
	typedef Console<Parser, PipeManager> Console_t;

	//Server<Console_t> server;
	//server.run();

	//SimpleParser sp;

	//std::string s = "Hello world! My name is Jeremy.";
	//std::cout << s << std::endl;

	//auto rs = sp.split(s, "!");

	//for (auto &s : rs)
	//{
	//	std::cout << s << std::endl;
	//}
	
	Parser p;

	p.qq();
	return 0;
}

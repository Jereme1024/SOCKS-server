#include "iostream"

int main(int argc, char **argv, char **envp)
{
	std::cout << "Content-type: text/html\n\n";
	for (char **env = envp; *env != 0; env++)
	{
		std::cout << *env << "<br/>\n";
	}

	return 0;
}

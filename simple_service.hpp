#ifndef __SIMPLE_SERVICE_HPP__
#define __SIMPLE_SERVICE_HPP__

class Simple_service
{
public:
	void service(int clientfd, char *buffer, int len)
	{
		std::cout << "Service up!\n";

		read(clientfd, buffer, len);
		std::cout << "Get from client: " << buffer << "\n";

		strcpy(buffer, "Send to client: hello");
		write(clientfd, buffer, strlen(buffer));
	}
};

#endif

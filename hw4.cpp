#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <memory>

#include "client.hpp"
#include "parser.hpp"
#include "socks.hpp"

const int MAXUSER = 5;

class BatchExec
{
public:
	std::fstream batch;
	bool is_writeable;
	int is_exit_flag;
	int serverfd;
	int id;

public:
	BatchExec()
		: is_writeable(false), is_exit_flag(0), serverfd(-1), id(0)
	{}

	int contain_prompt ( char* line )
	{
		int i, prompt = 0 ;
		for (i=0; line[i]; ++i) {
			switch ( line[i] ) {
				case '%' : prompt = 1 ; break;
				case ' ' : if ( prompt ) return 1;
				default: prompt = 0;
			}
		}
		return 0;
	} 


	int recv_msg()
	{
		int from = serverfd;
		char buf[3000],*tmp;
		int len,i;

		len=recv(from,buf,sizeof(buf)-1, 0);
		if(len < 0) return -1;

		//std::cerr << "len = " << len << std::endl;

		buf[len] = 0;
		if(len>0)
		{
			for(tmp=strtok(buf,"\n"); tmp; tmp=strtok(NULL,"\n"))
			{
				if ( contain_prompt(tmp) ) is_writeable = true ;
				print_html(tmp);
			}
		}
		fflush(stdout); 
		return len;
	}


	void send_cmd()
	{
		const bool border = true;

		if (is_writeable && batch)
		{
			std::string buffer;
			std::getline(batch, buffer);

			print_html(buffer, border);

			buffer.append("\n");
			send(serverfd, buffer.c_str(), buffer.length(), 0);
			is_writeable = false;

			if (buffer.find("exit") != std::string::npos)
			{
				is_exit_flag = 1;
			}

			sleep(1);
		}
	}

	void remove_return_symbol(std::string &text)
	{
		for (int i = 0; i < text.length(); i++)
			if (text[i] == '\r') text[i] = ' ';
	}

	inline void print_html(std::string plaintext, const bool is_border = false)
	{
		remove_return_symbol(plaintext);

		std::cerr << "PH: " << plaintext << std::endl;

		std::cout << "<script>document.all['m" << id <<"'].innerHTML += \"";
		if (is_border) std::cout << "<b>";
		std::cout << plaintext;
		if (is_border) std::cout << "</b>";

		if (plaintext[0] != '%')
			std::cout << "<br/>";
		std::cout << "\";</script>" << std::endl;
	}


	void set_batch(std::string filename)
	{
		batch.open(filename);

		if (!batch)
		{
			std::cerr << "Batch file open failed! " << filename << std::endl;
			//exit(-1);
		}
	}


	void enter(int serverfd, sockaddr_in &server_addr)
	{
		std::cerr << "Done!!!\n";
		this->serverfd = serverfd;
	}

	inline bool is_connected()
	{
		return (serverfd != -1);
	}

	inline bool is_exit()
	{
		return is_exit_flag;
	}

	void routine(int serverfd)
	{
		return;
	}
};

class RbsCgi
{
public:
	typedef SocksClient<BatchExec> ClientType;
	typedef std::shared_ptr<ClientType> ClientPtr;
	std::map<int, ClientPtr> clients;
	std::map<std::string, std::string> params;
	fd_set rfds, afds, wfds, afdsw;

	RbsCgi()
	{
		FD_ZERO(&rfds);
		FD_ZERO(&afds);

		FD_ZERO(&wfds);
		FD_ZERO(&afdsw);
	}

	void set_query_string(std::string query_string)
	{
		auto by_and = SimpleParser::split(query_string, "&");
		
		for (auto &str : by_and)
		{
			auto by_equal = SimpleParser::split(str, "=");

			if (by_equal.size() == 2)
				params[by_equal[0]] = by_equal[1];
			else
				params[by_equal[0]] = "";
		}
	}

	void establish_connection()
	{
		for (int i = 1; i <= MAXUSER; i++)
		{
			std::string index = std::to_string(i);
			std::string ip = "h" + index;
			std::string port = "p" + index;
			std::string batch = "f" + index;
			std::string sip = "sh" + index;
			std::string sport = "sp" + index;


			std::cerr << "ip = " << params[ip] << " port = " << params[port] << " batch = " << params[batch] << "\n";
			std::cerr << "sip = " << params[sip] << " sport = " << params[sport] << "\n";

			if (params[ip] != "" && params[port] != "" && params[batch] != "")
			{
				char *ip_ = (char *)params[ip].c_str();
				int port_ = std::atoi(params[port].c_str());
				char *sip_;
				int sport_;

				if (params[sip] != "" && params[sport] != "")
				{
					sip_ = (char *)params[sip].c_str();
					sport_ = std::atoi(params[sport].c_str());
				}
				else
				{
					sip_ = NULL;
					sport_ = 0;
				}

				auto client = std::make_shared<ClientType>(sip_, sport_, ip_, port_);

				// FIXME: refactory here!!
				client->connect_noblocking();
				//client->connect();
				const int sockfd = client->sockfd;
				clients[sockfd] = client;

				client->set_batch(params[batch]);
				client->id = (i - 1);

				FD_SET(sockfd, &afdsw);
				FD_SET(sockfd, &afds);
			}
		}
	}

	void main()
	{
		int nfds = getdtablesize();

		show_html_top();

		while (true)
		{
    		memcpy(&rfds, &afds, sizeof(fd_set));
    		memcpy(&wfds, &afdsw, sizeof(fd_set));

    		if(select(nfds, &rfds, &wfds, NULL, NULL) < 0)
    		{
				perror("select");
    		}

			for (auto &c : clients)
			{
				const int sockfd = c.first;
				auto client = c.second;

				if (FD_ISSET(client->sockfd, &wfds))
				{
					std::cerr << "Write " << client->sockfd << std::endl;
					FD_CLR(client->sockfd, &afdsw);
					client->connect_noblocking_done();
				}

				if (FD_ISSET(client->sockfd, &rfds))
				{
					
					if (client->is_connected())
					{
						client->recv_msg();
						client->send_cmd();

						if (client->is_exit())
						{
							if (client->is_exit_flag == 1)
							{	client->is_exit_flag = 2;
								continue;
							}
							else if (client->is_exit_flag == 2)
							{
								client->recv_msg();
								disconnect(client->sockfd);
							}
						}
					}
					else
					{
						std::cerr << "Retry!\n";
						client->connect_noblocking();
						if (client->is_connect_timeout())
						{
							client->print_html("Connect timeout!", true);
							disconnect(client->sockfd);
						}
					}
				}
			}

			if (is_finish())
				break;
		}

		show_html_bottom();
	}

	void disconnect(int clientfd)
	{
		FD_CLR(clientfd, &afds);
		close(clientfd);
		clients.erase(clientfd);
	}

	inline bool is_finish()
	{
		//std::cerr << "SZ: " << clients.size() << "\n";
		return (clients.size() == 0);
	}

	void show_html_top()
	{
		std::cout << R"(
			<html>
			<head>
			<meta http-equiv="Content-Type" content="text/html; charset=big5" />
			<title>Network Programming Homework 3</title>
			</head>
			<body bgcolor=#336699>
			<font face="Courier New" size=2 color=#FFFF99>
			<table width="800" border="1">
			<tr>
		)";

		std::vector<char *> is_used(MAXUSER, NULL);

		for (auto &client : clients)
		{
			is_used[client.second->id] = client.second->ip;
		}

		for (int i = 0; i < MAXUSER; i++)
		{
			if (is_used[i] != NULL)
				std::cout << "<td>" << is_used[i] << "</td>";
			else
				std::cout << "<td></td>";
		}
		
		std::cout << R"(
			</tr>
			<tr>
		)";

		for (int i = 0; i < MAXUSER; i++)
		{
			std::cout << "<td valign=\"top\" id=\"m" << std::to_string(i) << "\"></td>";
		}


		std::cout << R"(
			</tr>
			</table>
		)";
	}

	void show_html_bottom()
	{
		std::cout << R"(
			</font>
			</body>
			</html>
		)";
	}
};

int main(int argc, char *argv[])
{
	std::cerr << "QUERY_STRING: " << getenv("QUERY_STRING") << std::endl;

	std::cout << "Content-Type: text/html\n\n";

	RbsCgi hw4cgi;
	
	hw4cgi.set_query_string(getenv("QUERY_STRING"));
	hw4cgi.establish_connection();
	hw4cgi.main();

	return 0;
}

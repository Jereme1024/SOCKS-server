#ifndef __FIREWALL_HPP__
#define __FIREWALL_HPP__

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include "parser.hpp"

struct Ip4_t
{
	std::vector<std::string> ip;

	Ip4_t() : ip(4)
	{}

	Ip4_t(std::string ip_str)
	{
		set(std::move(ip_str));
	}

	inline void set(std::string ip_str)
	{
		ip = SimpleParser::split(ip_str, ".");
	}
};


class FirewallIF
{
public:
	void update_rule(std::string ip_str, int mode)
	{
	}

	bool check_rule(std::string ip_str, int mode)
	{
		return true;
	}
};

class FirewallNull : public FirewallIF
{};

class FirewallSocks: public FirewallIF
{
private:
	inline bool check(std::vector<Ip4_t> &rules, std::string &&ip_str)
	{
		Ip4_t test(ip_str);

		for (auto &rule : rules)
		{
			int pass = 0;
			for (int i = 0; i < 4; i++)
			{
				if (rule.ip[i] == "*" || rule.ip[i] == test.ip[i])
					pass++;
			}

			if (pass == 4) return true;
		}

		return false;
	}

public:
	enum Mode { CONNECT = 1, BIND = 2 };

	std::vector<Ip4_t> permit_connect;
	std::vector<Ip4_t> permit_bind;

	bool check_rule(std::string ip_str, int mode)
	{
		if (mode == CONNECT)
			return check(permit_connect, std::move(ip_str));
		if (mode == BIND)
			return check(permit_bind, std::move(ip_str));

		return false;
	}


	void add_config(std::string filename)
	{
		std::fstream config(filename);

		if (!config)
		{
			std::cerr << "Firewall config open failed!\n";
		}

		std::string input;
		std::stringstream ss;
		while (std::getline(config, input))
		{
			auto in = SimpleParser::split(input);

			if (in[0] == "permit")
			{
				if (in[1] == "c") update_rule(in[2], CONNECT);
				else if (in[1] == "b") update_rule(in[2], BIND);
			}
		}
	}

	void update_rule(std::string ip_str, int mode)
	{
		if (mode == CONNECT)
			permit_connect.emplace_back(ip_str);
		else if (mode == BIND)
			permit_bind.emplace_back(ip_str);
	}
};

#endif

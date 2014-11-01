#ifndef __PARSER_HPP__
#define __PARSER_HPP__

#include <vector>
#include <string>
#include <sstream>

class SimpleParser
{
public:
	std::vector<std::string> split(std::string str)
	{
		return split_default(str);
	}

	std::vector<std::string> split(std::string str, std::string separator)
	{
		// The whiespace, tab and endline will be cut too.
		for (int i = 0; i < str.length(); i++)
		{
			if (separator.find(str[i]) != std::string::npos)
			{
				str[i] = ' ';
			}
		}

		return split_default(str);
	}

	inline std::vector<std::string> split_default(std::string &str)
	{
		std::stringstream ss(str);
		std::vector<std::string> result;
		std::string input;

		while (ss >> input)
		{
			result.push_back(input);
		}

		return result;
	}
};

#endif

#ifndef __PARSER_HPP__
#define __PARSER_HPP__

/// @file
/// @author Jeremy
/// @version 1.0
/// @section DESCRIPTION
///
/// The parser class is able to handle some basic operations to extract wanted context of string.

#include <vector>
#include <string>
#include <sstream>

/// @brief The parser class is able to handle some basic operations to extract wanted context of string.
class SimpleParser
{
public:
	/// @brief This method is used to split a string by whiespace, tab, and EOL.
	/// @param str The string to be split.
	/// @return A vector of string consists of cutting substrings.
	std::vector<std::string> split(std::string str)
	{
		return split_default(str);
	}


	/// @brief This method is used to split a string by whiespace, tab, EOL, and specifying separator(delimiter).
	/// @param str The string to be split.
	/// @param separator The separator for target string.
	/// @return A vector of string consists of cutting substrings.
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


	/// @brief This method is used to split a string by using a std::cin-like method.
	/// @param str The string to be split.
	/// @return A vector of string consists of cutting substrings.
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

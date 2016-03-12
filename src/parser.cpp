#include "parser.h"

namespace
{
	std::vector<std::string> split(const std::string& string, char separator)
	{
		std::vector<std::string> result;
		for (std::string::size_type begin = 0;;)
		{
			auto end = string.find_first_of(separator, begin);
			if (end == std::string::npos)
			{
				result.emplace_back(string.substr(begin));
				break;
			}
			result.emplace_back(string.substr(begin, end - begin));
			begin = end + 1;
		}
		return result;
	}
}

namespace parser
{
	std::vector<ParsedCommand> parse(const std::unordered_map<std::string, const Command*>& commands, const std::string& source)
	{
		std::vector<ParsedCommand> result;
		for (const auto& command_string : ::split(source, '|'))
		{
			std::string name;
			std::vector<std::string> arguments;
			auto end = std::string::npos;
			do
			{
				const auto begin = command_string.find_first_not_of(' ', end + 1);
				if (begin == std::string::npos)
					break;
				end = command_string.find_first_of(' ', begin + 1);
				auto&& part = command_string.substr(begin, end != std::string::npos ? end - begin : end);
				if (name.empty())
					name = std::move(part);
				else
					arguments.emplace_back(std::move(part));
			} while (end != std::string::npos);
			const auto command = commands.find(name);
			if (command == commands.end())
				throw std::runtime_error("Unknown command '" + name + "'");
			if (arguments.size() != command->second->arguments.size())
				throw std::runtime_error("Bad number of arguments for command '" + name + "'");
			result.emplace_back(command->second, std::move(arguments));
		}
		return result;
	}
}

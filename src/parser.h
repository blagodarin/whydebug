#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace parser
{
	struct Command
	{
		struct // For nicer initialization.
		{
			std::string primary;
			std::string alias;
		} names;
		std::vector<std::string> arguments;
		std::string description;
		std::function<void(const std::vector<std::string>&)> handler;
	};

	using ParsedCommand = std::pair<const Command*, std::vector<std::string>>;

	std::vector<ParsedCommand> parse(const std::unordered_map<std::string, const Command*>& commands, const std::string& source);
}

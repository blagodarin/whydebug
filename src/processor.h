#pragma once

#include "table.h"
#include <memory>
#include <unordered_map>

class Minidump;

namespace parser
{
	class Command;
}

class Processor
{
public:

	Processor(std::unique_ptr<Minidump>&&);
	~Processor();

	bool process(const std::string& commands);

private:

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

	const std::unique_ptr<Minidump> _dump;
	Table _table;
	const std::vector<parser::Command> _commands;
	std::unordered_map<std::string, const parser::Command*> _command_index;
	int _last_command_time = 0;
	int _last_print_time = 0;
};

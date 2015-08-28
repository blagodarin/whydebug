#pragma once

#include "table.h"
#include <memory>
#include <unordered_map>

class Minidump;

class Processor
{
public:

	Processor(std::unique_ptr<Minidump>&& dump);

	void print_summary() const;
	bool process(const std::string& commands);

private:

	using Command = std::function<void(const std::vector<std::string>&)>;

	const std::unique_ptr<Minidump> _dump;
	Table _table;
	const std::unordered_map<std::string, Command> _commands;
};

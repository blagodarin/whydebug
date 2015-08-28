#include "processor.h"
#include "minidump.h"
#include "utils.h"
#include <chrono>
#include <iostream>

namespace
{
	template <typename T>
	void check_arguments(const T& arguments, size_t min, size_t max)
	{
		if (arguments.size() < min || arguments.size() > max)
			throw std::runtime_error("Invalid arguments");
	}

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
		return std::move(result);
	}
}

Processor::Processor(std::unique_ptr<Minidump>&& dump)
	: _dump(std::move(dump))
	, _commands
	{
		{ "a", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				_table = _dump->print_memory();
			}
		},
		{ "ar", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				_table = _dump->print_memory_regions();
			}
		},
		{ "m", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				_table = _dump->print_modules();
			}
		},
		{ "s", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 1, 1);
				_table.sort(args[0]);
			}
		},
		{ "t", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 1);
				_table = args.empty()
					? _dump->print_threads()
					: _dump->print_thread_call_stack(::to_ulong(args[0]));
			}
		},
		{ "um", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				_table = _dump->print_unloaded_modules();
			}
		},
		{ "x", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				_table = _dump->print_exception_call_stack();
			}
		},
		{ "?time", [this](const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				Table table({{""}, {"", Table::Alignment::Right}});
				table.push_back({"Last command time:", std::to_string(_last_command_time) + " ms"});
				table.push_back({"Last print time:", std::to_string(_last_print_time) + " ms"});
				table.print(std::cout);
			}
		},
	}
{
}

void Processor::print_summary() const
{
	_dump->print_summary().print(std::cout);
}

bool Processor::process(const std::string& commands)
{
	bool print_table = true;

	std::vector<std::pair<Command, std::vector<std::string>>> parsed_commands;
	for (const auto& command_string : ::split(commands, '|'))
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
		const auto i = _commands.find(name);
		if (i == _commands.end())
		{
			std::cerr << "ERROR: Unknown command: " << name << std::endl;
			return false;
		}
		parsed_commands.emplace_back(i->second, std::move(arguments));
		print_table = name[0] != '?';
	}

	try
	{
		for (const auto& parsed_command : parsed_commands)
		{
			const auto start_time = std::chrono::steady_clock::now();
			parsed_command.first(parsed_command.second);
			const auto end_time = std::chrono::steady_clock::now();
			_last_command_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
		}
	}
	catch (const std::exception& e) // TODO: Should we catch std::logic_error here?
	{
		_table = {};
		std::cerr << "ERROR: " << e.what() << std::endl;
		return false;
	}

	if (print_table)
	{
		const auto start_time = std::chrono::steady_clock::now();
		_table.print(std::cout);
		const auto end_time = std::chrono::steady_clock::now();
		_last_print_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	}

	return true;
}
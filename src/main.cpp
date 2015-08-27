#include "check.h"
#include "minidump.h"
#include "table.h"
#include "utils.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace
{
	using Command = std::function<void(Table&, const std::vector<std::string>&)>;

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

int main(int argc, char** argv)
{
	if (argc != 2 && argc != 3)
	{
		std::cerr << "Usage:\n\t" << argv[0] << " DUMP [COMMAND]" << std::endl;
		return 1;
	}

	std::unique_ptr<Minidump> dump;
	try
	{
		dump = std::make_unique<Minidump>(argv[1]);
	}
	catch (const BadCheck& e)
	{
		std::cerr << "FATAL: " << e.what() << std::endl;
		return 1;
	}

	const std::unordered_map<std::string, Command> command_map =
	{
		{ "a", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				table = dump->print_memory();
			}
		},
		{ "ar", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				table = dump->print_memory_regions();
			}
		},
		{ "m", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				table = dump->print_modules();
			}
		},
		{ "s", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 1, 1);
				table.sort(args[0]);
			}
		},
		{ "t", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 1);
				table = args.empty()
					? dump->print_threads()
					: dump->print_thread_call_stack(::to_ulong(args[0]));
			}
		},
		{ "um", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				table = dump->print_unloaded_modules();
			}
		},
		{ "x", [&dump](Table& table, const std::vector<std::string>& args)
			{
				::check_arguments(args, 0, 0);
				table = dump->print_exception_call_stack();
			}
		},
	};

	const auto execute = [&command_map](const std::string& line)
	{
		std::vector<std::pair<Command, std::vector<std::string>>> commands;
		for (const auto& command_string : ::split(line, '|'))
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
			const auto i = command_map.find(name);
			if (i == command_map.end())
			{
				std::cerr << "Unknown command \"" << name << "\"" << std::endl;
				return false;
			}
			commands.emplace_back(i->second, std::move(arguments));
		}
		Table table;
		try
		{
			for (const auto& command : commands)
				command.first(table, command.second);
		}
		catch (const std::exception& e) // TODO: Should we catch std::logic_error here?
		{
			std::cerr << "ERROR: " << e.what() << std::endl;
			return false;
		}
		std::cout << table;
		return true;
	};

	if (argc == 3)
		return execute(argv[2]);

	std::cout << dump->print_summary();
	for (std::string line; ; )
	{
		std::cout << "?> ";
		if (!std::getline(std::cin, line))
		{
			std::cout << std::endl;
			break;
		}
		execute(line);
	}
	return 0;
}

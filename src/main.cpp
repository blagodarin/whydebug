#include "check.h"
#include "minidump.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

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

	const std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> commands =
	{
		{ "a", [&dump](const std::vector<std::string>& args)
			{
				if (args.empty())
					dump->print_memory(std::cout);
				else
					std::cerr << "ERROR: Bad arguments" << std::endl;
			}
		},
		{ "m", [&dump](const std::vector<std::string>& args)
			{
				if (args.empty())
					dump->print_modules(std::cout);
				else
					std::cerr << "ERROR: Bad arguments" << std::endl;
			}
		},
		{ "t", [&dump](const std::vector<std::string>& args)
			{
				if (args.empty())
					dump->print_threads(std::cout);
				else if (args.size() == 1)
					dump->print_thread_call_stack(std::cout, args[0]);
				else
					std::cerr << "ERROR: Bad arguments" << std::endl;
			}
		},
		{ "x", [&dump](const std::vector<std::string>& args)
			{
				if (args.empty())
					dump->print_exception_call_stack(std::cout);
				else
					std::cerr << "ERROR: Bad arguments" << std::endl;
			}
		},
	};

	const auto execute = [&commands](const std::string& line)
	{
		std::string command;
		std::vector<std::string> args;
		auto end = std::string::npos;
		do
		{
			const auto begin = line.find_first_not_of(' ', end + 1);
			if (begin == std::string::npos)
				break;
			end = line.find_first_of(' ', begin + 1);
			auto&& part = line.substr(begin, end != std::string::npos ? end - begin : end);
			if (command.empty())
				command = std::move(part);
			else
				args.emplace_back(std::move(part));
		} while (end != std::string::npos);

		const auto i = commands.find(command);
		if (i != commands.end())
			i->second(args);
		else
			std::cout << "Unknown command \"" << command << "\"" << std::endl;
	};

	if (argc == 3)
	{
		execute(argv[2]);
	}
	else
	{
		dump->print_summary(std::cout);
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
	}

	return 0;
}

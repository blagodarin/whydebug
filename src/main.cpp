#include "check.h"
#include "minidump.h"
#include <iostream>
#include <memory>
#include <unordered_map>

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;

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
				else
					std::cerr << "ERROR: Bad arguments" << std::endl;
			}
		},
	};

	dump->print_summary(std::cout);

	for (std::string line; ; )
	{
		std::cout << "?> ";
		if (!std::getline(std::cin, line))
		{
			std::cout << std::endl;
			break;
		}

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
	}

	return 0;
}

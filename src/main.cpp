#include "check.h"
#include "minidump.h"
#include <iostream>
#include <memory>
#include <unordered_map>

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;

	std::unique_ptr<Minidump> minidump;
	try
	{
		minidump = std::make_unique<Minidump>(argv[1]);
	}
	catch (const BadCheck& e)
	{
		std::cerr << "FATAL: " << e.what() << std::endl;
		return 1;
	}

	const std::unordered_map<std::string, std::function<void()>> commands =
	{
		{ "", []() {} },
		{ "modules", [&minidump]() { minidump->print_modules(std::cout); } },
		{ "summary", [&minidump]() { minidump->print_summary(std::cout); } },
		{ "threads", [&minidump]() { minidump->print_threads(std::cout); } },
	};

	minidump->print_summary(std::cout);

	std::string line;
	for (;;)
	{
		std::cout << "?> ";
		if (!std::getline(std::cin, line))
		{
			std::cout << std::endl;
			break;
		}
		const auto i = commands.find(line);
		if (i != commands.end())
			i->second();
		else
			std::cout << "Unknown command \"" << line << "\"" << std::endl;
	}

	return 0;
}

#include "check.h"
#include "minidump.h"
#include "processor.h"
#include <iostream>

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

	Processor processor(std::move(dump));
	if (argc == 3)
		return processor.process(argv[2]) ? 0 : 1;
	processor.print_summary();
	for (std::string line; ; )
	{
		std::cout << "?> ";
		if (!std::getline(std::cin, line))
		{
			std::cout << std::endl;
			break;
		}
		processor.process(line);
	}
	return 0;
}

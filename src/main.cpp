#include "minidump.h"
#include <memory>

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;
	std::unique_ptr<Minidump> minidump;
	try
	{
		minidump = std::make_unique<Minidump>(argv[1]);
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "FATAL: " << e.what() << std::endl;
		return 1;
	}
	std::cout << *minidump << std::endl;
	return 0;
}

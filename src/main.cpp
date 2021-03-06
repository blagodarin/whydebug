#include "check.h"
#include "minidump.h"
#include "processor.h"
#include <iostream>
#include <boost/optional/optional.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>

struct Options
{
	std::string dump;
	boost::optional<std::string> commands;
	bool summary = false;
};

int main(int argc, char** argv)
{
	Options options;
	{
		boost::program_options::options_description public_options("Options");
		public_options.add_options()
			("summary,S", boost::program_options::value<bool>()->zero_tokens());

		boost::program_options::options_description o;
		o.add(public_options).add_options()
			("dump", boost::program_options::value<std::string>()->required())
			("commands", boost::program_options::value<std::string>());

		boost::program_options::positional_options_description p;
		p.add("dump", 1).add("commands", 1);

		try
		{
			boost::program_options::variables_map vm;
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(o).positional(p).run(), vm);
			boost::program_options::notify(vm);
			options.dump = vm["dump"].as<std::string>();
			if (vm.count("commands"))
				options.commands = vm["commands"].as<std::string>();
			if (vm.count("summary"))
				options.summary = true;
		}
		catch (const boost::program_options::error&)
		{
			std::cerr << "Usage:\n  whydebug [OPTIONS] DUMP [COMMAND]\n\n" << public_options << std::endl;
			return 1;
		}
	}

	std::unique_ptr<Minidump> dump;
	try
	{
		dump = std::make_unique<Minidump>(options.dump, options.summary);
	}
	catch (const BadCheck& e)
	{
		std::cerr << "FATAL: " << e.what() << std::endl;
		return 1;
	}

	if (!options.summary)
	{
		Processor processor(std::move(dump));
		if (options.commands)
			return processor.process(*options.commands) ? 0 : 1;
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
	}
	return 0;
}

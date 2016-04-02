#include "processor.h"
#include "minidump.h"
#include "parser.h"
#include "utils.h"
#include <chrono>
#include <iostream>

Processor::Processor(std::unique_ptr<Minidump>&& dump)
	: _dump(std::move(dump))
	, _commands
	{
		{ { "a" }, {},
			"Build memory information.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_memory();
			}
		},
		{ { "ar" }, {},
			"Build memory region information.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_memory_regions();
			}
		},
		{ { "h" }, {},
			"Build handle information.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_handles();
			}
		},
		{ { "m" }, {},
			"Build loaded modules list.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_modules();
			}
		},
		{ { "t" }, { "INDEX" },
			"Build the stack of thread INDEX.",
			[this](const std::vector<std::string>& args)
			{
				_table = _dump->print_thread_call_stack(::to_ulong(args[0]));
			}
		},
		{ { "ts" }, {},
			"Build thread list.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_threads();
			}
		},
		{ { "um" }, {},
			"Build unloaded modules list.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_unloaded_modules();
			}
		},
		{ { "x" }, {},
			"Build the exception call stack.",
			[this](const std::vector<std::string>&)
			{
				_table = _dump->print_exception_call_stack();
			}
		},
		{ { "." }, {},
			"Do nothing.",
			[this](const std::vector<std::string>&)
			{
			}
		},
		{ { ".empty" }, { "COLUMN" },
			"Leave rows where value in COLUMN is empty.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], "", Table::Pass::EndingWith);
			}
		},
		{ { ".ends", ".e" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN ends with TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::EndingWith);
			}
		},
		{ {".eq"}, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is equal to TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::Equal);
			}
		},
		{ { ".first", ".f" }, { "N" },
			"Leave the first N rows.",
			[this](const std::vector<std::string>& args)
			{
				_table.leave_first_rows(::to_ulong(args[0]));
			}
		},
		{ { ".ge" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is not less than TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::GreaterOrEqual);
			}
		},
		{ { ".gt" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is greater than TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::Greater);
			}
		},
		{ { ".has" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN contains TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::Containing);
			}
		},
		{ { ".last", ".l" }, { "N" },
			"Leave the last N rows.",
			[this](const std::vector<std::string>& args)
			{
				_table.leave_last_rows(::to_ulong(args[0]));
			}
		},
		{ { ".le" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is not greater than TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::LessOrEqual);
			}
		},
		{ { ".lt" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is less than TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::Less);
			}
		},
		{ { ".ne" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN is not equal to TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::NotEqual);
			}
		},
		{ { ".orig" }, {},
			"Clear sorting and filtering of the current output.",
			[this](const std::vector<std::string>& args)
			{
				_table.set_original();
			}
		},
		{ { ".rs" }, { "COLUMN" },
			"Reverse sort rows by value of COLUMN.",
			[this](const std::vector<std::string>& args)
			{
				_table.reverse_sort(args[0]);
			}
		},
		{ { ".sort", ".s" }, { "COLUMN" },
			"Sort rows by value of COLUMN.",
			[this](const std::vector<std::string>& args)
			{
				_table.sort(args[0]);
			}
		},
		{ { ".starts", ".st" }, { "COLUMN", "TEXT" },
			"Leave rows where value in COLUMN starts with TEXT.",
			[this](const std::vector<std::string>& args)
			{
				_table.filter(args[0], args[1], Table::Pass::StartingWith);
			}
		},
		{ { "?" }, {},
			"Print all commands with descriptions.",
			[this](const std::vector<std::string>&)
			{
				Table table({{""}, {""}});
				table.reserve(_commands.size());
				for (const auto& command : _commands)
				{
					std::string signature = command.names.primary;
					if (!command.names.alias.empty())
						signature += " (" + command.names.alias + ')';
					for (const auto& argument : command.arguments)
						signature += ' ' + argument;
					table.push_back({signature, command.description});
				}
				table.print(std::cout);
			}
		},
		{ { "?rawstack" }, { "INDEX" },
			"Print raw stack data of thread INDEX.",
			[this](const std::vector<std::string>& args)
			{
				_dump->print_thread_raw_stack(::to_ulong(args[0]));
			}
		},
		{ { "?rows", "?r" }, {},
			"Print the number of rows in the current output (excluding filtered rows).",
			[this](const std::vector<std::string>&)
			{
				Table table({{""}, {"", Table::Alignment::Right}});
				table.push_back({"Rows:", std::to_string(_table.rows())});
				table.print(std::cout);
			}
		},
		{ { "?time", "?t" }, {},
			"Print the time used by the last command.",
			[this](const std::vector<std::string>&)
			{
				Table table({{""}, {"", Table::Alignment::Right}});
				table.push_back({"Last command time:", std::to_string(_last_command_time) + " ms"});
				table.push_back({"Last print time:", std::to_string(_last_print_time) + " ms"});
				table.print(std::cout);
			}
		},
	}
{
	for (const auto& command : _commands)
	{
		_command_index.emplace(command.names.primary, &command);
		if (!command.names.alias.empty())
			_command_index.emplace(command.names.alias, &command);
	}
}

Processor::~Processor() = default;

bool Processor::process(const std::string& commands)
{
	try
	{
		bool print_table = true;
		for (const auto& parsed_command : parser::parse(_command_index, commands))
		{
			const auto start_time = std::chrono::steady_clock::now();
			parsed_command.first->handler(parsed_command.second);
			const auto end_time = std::chrono::steady_clock::now();
			_last_command_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
			print_table = parsed_command.first->names.primary[0] != '?';
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
	catch (const std::exception& e) // TODO: Should we catch std::logic_error here?
	{
		_table = {};
		std::cerr << "ERROR: " << e.what() << std::endl;
		return false;
	}
}

#pragma once

#include <memory>
#include <string>

class MinidumpData;
class Table;

class Minidump
{
public:

	Minidump(const std::string& file_name);
	~Minidump();

	Minidump() = default;
	Minidump(const Minidump&) = default;
	Minidump(Minidump&&) = default;
	Minidump& operator=(const Minidump&) = default;
	Minidump& operator=(Minidump&&) = default;

	Table print_exception_call_stack() const;
	Table print_generic_information() const;
	Table print_handles() const;
	Table print_memory() const;
	Table print_memory_regions() const;
	Table print_modules() const;
	Table print_thread_call_stack(unsigned long thread_index) const;
	void print_thread_raw_stack(unsigned long thread_index) const;
	Table print_threads() const;
	Table print_unloaded_modules() const;

private:

	const std::unique_ptr<MinidumpData> _data;
};

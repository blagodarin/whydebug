#pragma once

#include <iosfwd>
#include <memory>
#include <string>

struct MinidumpData;

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

	void print_exception_call_stack(std::ostream&);
	void print_modules(std::ostream&);
	void print_summary(std::ostream&);
	void print_threads(std::ostream&);

private:

	std::string decode_code_address(uint64_t);

private:

	const std::unique_ptr<MinidumpData> _data;
};

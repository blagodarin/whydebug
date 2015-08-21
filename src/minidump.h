#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct Minidump
{
	struct MemoryUsage
	{
		uint64_t all_images = 0;
		uint32_t max_image = 0;
		uint64_t all_stacks = 0;
		uint32_t max_stack = 0;
	};

	struct Module
	{
		std::string file_path;
		std::string file_name;
		std::string file_version;
		std::string product_version;
		std::string timestamp;
		std::string pdb_path;
		std::string pdb_name;
		uint64_t    image_base = 0;
		uint32_t    image_size = 0;
	};

	struct Thread
	{
		uint32_t id = 0;
		uint64_t stack_base = 0;
		uint32_t stack_size = 0;
	};

	time_t timestamp = 0;
	uint32_t process_id = 0;
	time_t process_create_time = 0;
	uint32_t process_user_time = 0;
	uint32_t process_kernel_time = 0;
	std::vector<Module> modules;
	std::vector<Thread> threads;
	MemoryUsage memory_usage;
	bool is_32bit = true;

	Minidump(const std::string& filename);

	Minidump() = default;
	Minidump(const Minidump&) = default;
	Minidump(Minidump&&) = default;
	Minidump& operator=(const Minidump&) = default;
	Minidump& operator=(Minidump&&) = default;

	void print_modules(std::ostream&);
	void print_summary(std::ostream&);
	void print_threads(std::ostream&);
};

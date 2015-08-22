#pragma once

#include <memory>
#include <string>
#include <vector>

struct MinidumpData
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
		uint64_t start_address = 0;
		bool dumping = false;
		union
		{
			struct
			{
				uint32_t eip;
				uint32_t esp;
				uint32_t ebp;
			} x86;
		} context = {};
		std::unique_ptr<uint8_t[]> stack;
	};

	struct Exception
	{
		uint32_t thread_id = 0;
		Thread*  thread = nullptr;
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
	std::unique_ptr<Exception> exception;

	//
	static std::unique_ptr<MinidumpData> load(const std::string& file_name);
};

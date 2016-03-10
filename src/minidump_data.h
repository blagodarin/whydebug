#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

class MinidumpData
{
public:

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
		uint64_t    image_end = 0;
	};

	struct Thread
	{
		uint32_t id = 0;
		uint64_t stack_base = 0;
		uint64_t stack_end = 0;
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

	struct MemoryInfo
	{
		enum class Usage
		{
			Unknown, //
			Image,   //
			Stack,   // Thread stack.
		};

		uint64_t end = 0;                // Memory range end.
		Usage    usage = Usage::Unknown; //
		size_t   usage_index = 0;        // Module index for Image usage, thread index for Stack usage.
	};

	struct MemoryRegion
	{
		enum class State
		{
			Free,
			Reserved,
			Allocated,
		};

		uint64_t end = 0;
		State    state = State::Free;
	};

	struct UnloadedModule
	{
		std::string file_path;
		std::string file_name;
		std::string timestamp;
		uint64_t    image_base = 0;
		uint64_t    image_end = 0;
	};

	struct Handle
	{
		uint64_t    handle = 0;
		std::string type_name;
		std::string object_name;
	};

	time_t timestamp = 0;
	std::vector<Module> modules;
	std::vector<Thread> threads;
	MemoryUsage memory_usage;
	bool is_32bit = true;
	std::unique_ptr<Exception> exception;
	std::map<uint64_t, MemoryInfo> memory;
	std::map<uint64_t, MemoryRegion> memory_regions;
	std::vector<UnloadedModule> unloaded_modules;
	std::vector<Handle> handles;
	std::vector<std::pair<std::string, std::string>> generic;

	//
	static std::unique_ptr<MinidumpData> load(const std::string& file_name);
};

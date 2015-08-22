#include "minidump_read.h"
#include "check.h"
#include "file.h"
#include "minidump.h"
#include "minidump_format.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{
	std::u16string read_string(File& file, RVA rva)
	{
		CHECK(file.seek(rva), "Bad string offset");
		MINIDUMP_STRING string_header;
		CHECK(file.read(string_header), "Couldn't read string header");
		std::u16string string(string_header.Length / 2, u' ');
		CHECK(file.read(const_cast<char16_t*>(string.data()), string.size() * sizeof(char16_t)), "Couldn't read string");
		return std::move(string);
	}

	std::string version_to_string(uint32_t ms, uint32_t ls)
	{
		std::array<char, 24> buffer;
		::memset(buffer.data(), 0, buffer.size());
		::snprintf(buffer.data(), buffer.size(), "%d.%d.%d.%d", ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff);
		return buffer.data();
	}

	template <typename T>
	std::unique_ptr<T, void(*)(void*)> allocate_pod(size_t size)
	{
		return { static_cast<T*>(::calloc(size, 1)), ::free };
	}
}

void read_misc_info(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
{
	MINIDUMP_MISC_INFO misc_info;
	CHECK(stream.Location.DataSize >= sizeof misc_info, "Bad misc info stream");
	CHECK(file.seek(stream.Location.Rva), "Bad misc info offset");
	CHECK(file.read(misc_info), "Couldn't read misc info");
	if (misc_info.Flags1 & MINIDUMP_MISC1_PROCESS_ID)
		dump.process_id = misc_info.ProcessId;
	if (misc_info.Flags1 & MINIDUMP_MISC1_PROCESS_TIMES)
	{
		dump.process_create_time = misc_info.ProcessCreateTime;
		dump.process_user_time = misc_info.ProcessUserTime;
		dump.process_kernel_time = misc_info.ProcessKernelTime;
	}
	// TODO: Load processor power info (MINIDUMP_MISC_INFO_2).
}

void read_module_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
{
	CHECK(dump.modules.empty(), "Duplicate module list");

	MINIDUMP_MODULE_LIST module_list_header;
	CHECK(stream.Location.DataSize >= sizeof module_list_header, "Bad module list stream");
	CHECK(file.seek(stream.Location.Rva), "Bad module list offset");
	CHECK(file.read(module_list_header), "Couldn't read module list header");
	CHECK_GE(module_list_header.NumberOfModules, 0, "Bad module list size");

	std::vector<MINIDUMP_MODULE> modules(module_list_header.NumberOfModules);
	const auto module_list_size = modules.size() * sizeof(MINIDUMP_MODULE);
	CHECK(file.read(modules.data(), module_list_size) == module_list_size, "Couldn't read module list");
	for (const auto& module : modules)
	{
		Minidump::Module m;
		m.file_path = ::to_ascii(::read_string(file, module.ModuleNameRva));
		m.file_name = m.file_path.substr(m.file_path.find_last_of('\\') + 1);
		m.file_version = ::version_to_string(module.VersionInfo.dwFileVersionMS, module.VersionInfo.dwFileVersionLS);
		m.product_version = ::version_to_string(module.VersionInfo.dwProductVersionMS, module.VersionInfo.dwProductVersionLS);
		m.timestamp = ::time_t_to_string(module.TimeDateStamp);
		m.image_base = module.BaseOfImage;
		m.image_size = module.SizeOfImage;
		if (module.CvRecord.DataSize > 0)
		{
			try
			{
				CHECK_GE(module.CvRecord.DataSize, CodeViewRecordPDB70::MinSize, "Bad PDB reference size");
				const auto& cv = ::allocate_pod<CodeViewRecordPDB70>(module.CvRecord.DataSize + 1);
				CHECK(file.seek(module.CvRecord.Rva), "Bad PDB reference");
				CHECK(file.read(cv.get(), module.CvRecord.DataSize), "Couldn't read PDB reference");
				m.pdb_path = cv->pdb_name;
				m.pdb_name = m.pdb_path.substr(m.pdb_path.find_last_of('\\') + 1);
			}
			catch (const BadCheck& e)
			{
				std::cerr << "ERROR: [" << m.file_name << "] " << e.what() << std::endl;
			}
		}
		dump.modules.emplace_back(std::move(m));
		dump.memory_usage.all_images += m.image_size;
		dump.memory_usage.max_image = std::max(dump.memory_usage.max_image, m.image_size);
		dump.is_32bit = dump.is_32bit && m.image_base + m.image_size <= UINT32_MAX;
	}
}

void read_thread_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
{
	CHECK(dump.threads.empty(), "Duplicate thread list");

	MINIDUMP_THREAD_LIST thread_list_header;
	CHECK(stream.Location.DataSize >= sizeof thread_list_header, "Bad thread list stream");
	CHECK(file.seek(stream.Location.Rva), "Bad thread list offset");
	CHECK(file.read(thread_list_header), "Couldn't read thread list header");
	CHECK_GE(thread_list_header.NumberOfThreads, 0, "Bad thread list size");

	std::vector<MINIDUMP_THREAD> threads(thread_list_header.NumberOfThreads);
	const auto thread_list_size = threads.size() * sizeof(MINIDUMP_THREAD);
	CHECK(file.read(threads.data(), thread_list_size) == thread_list_size, "Couldn't read thread list");
	for (const auto& thread : threads)
	{
		const auto index = &thread - &threads.front() + 1;

		Minidump::Thread t;
		t.id = thread.ThreadId;
		t.stack_base = thread.Stack.StartOfMemoryRange;
		t.stack_size = thread.Stack.Memory.DataSize;

		try
		{
			ContextX86 context;
			CHECK_EQ(thread.ThreadContext.DataSize, sizeof context, "Bad thread context size");
			CHECK(file.seek(thread.ThreadContext.Rva), "Bad thread " << index << " context offset");
			CHECK(file.read(context), "Couldn't read thread " << index << " context");
			CHECK(::has_flags(context.flags, ContextX86::I386 | ContextX86::Control), "Bad thread context");
			t.context.x86.eip = context.eip;
			t.context.x86.esp = context.esp;
		}
		catch (const BadCheck& e)
		{
			std::cerr << "ERROR: " << e.what() << std::endl;
		}

		dump.threads.emplace_back(std::move(t));
		dump.memory_usage.all_stacks += t.stack_size;
		dump.memory_usage.max_stack = std::max(dump.memory_usage.max_stack, t.stack_size);
		dump.is_32bit = dump.is_32bit && t.stack_base + t.stack_size <= UINT32_MAX;
	}
}

void read_thread_info_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
{
	if (dump.threads.empty())
	{
		std::cerr << "ERROR: Thread info list found before thread list" << std::endl;
		return;
	}

	MINIDUMP_THREAD_INFO_LIST header;
	CHECK_GE(stream.Location.DataSize, sizeof header, "Bad thread info list stream");
	CHECK(file.seek(stream.Location.Rva), "Bad thread info list offset");
	CHECK(file.read(header), "Couldn't read thread info list header");
	CHECK_GE(header.SizeOfHeader, sizeof header, "Bad thread info list header size");

	MINIDUMP_THREAD_INFO entry;
	CHECK_GE(header.SizeOfEntry, sizeof entry, "Bad thread info size");
	const auto base = stream.Location.Rva + header.SizeOfHeader;
	for (uint32_t i = 0; i < header.NumberOfEntries; ++i)
	{
		CHECK(file.seek(base + i * header.SizeOfEntry), "Bad thread info list");
		CHECK(file.read(entry), "Couldn't read thread info entry");

		const auto j = std::find_if(dump.threads.begin(), dump.threads.end(), [&entry](const auto& thread)
		{
			return thread.id == entry.ThreadId;
		});
		if (j == dump.threads.end())
		{
			std::cerr << "ERROR: Thread info for unknown thread " << ::to_hex(entry.ThreadId) << std::endl;
			continue;
		}
		j->start_address = entry.StartAddress;
		j->dumping = entry.DumpFlags & MINIDUMP_THREAD_INFO_WRITING_THREAD;

		dump.is_32bit = dump.is_32bit && j->start_address <= UINT32_MAX;
	}
}

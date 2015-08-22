#include "minidump.h"
#include "check.h"
#include "file.h"
#include "minidump_format.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>

namespace
{
	std::string stream_type_to_string(MINIDUMP_STREAM_TYPE type)
	{
		switch (type)
		{
		case UnusedStream: return "UnusedStream";
		case ReservedStream0: return "ReservedStream0";
		case ReservedStream1: return "ReservedStream1";
		case ThreadListStream: return "ThreadListStream";
		case ModuleListStream: return "ModuleListStream";
		case MemoryListStream: return "MemoryListStream";
		case ExceptionStream: return "ExceptionStream";
		case SystemInfoStream: return "SystemInfoStream";
		case ThreadExListStream: return "ThreadExListStream";
		case Memory64ListStream: return "Memory64ListStream";
		case CommentStreamA: return "CommentStreamA";
		case CommentStreamW: return "CommentStreamW";
		case HandleDataStream: return "HandleDataStream";
		case FunctionTableStream: return "FunctionTableStream";
		case UnloadedModuleListStream: return "UnloadedModuleListStream";
		case MiscInfoStream: return "MiscInfoStream";
		case MemoryInfoListStream: return "MemoryInfoListStream";
		case ThreadInfoListStream: return "ThreadInfoListStream";
		case HandleOperationListStream: return "HandleOperationListStream";
		default: return "<unknown stream " + std::to_string(type) + ">";
		}
	}

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

	void load_misc_info(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
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

	void load_module_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		MINIDUMP_MODULE_LIST module_list_header;
		CHECK(stream.Location.DataSize >= sizeof module_list_header, "Bad module list stream");
		CHECK(file.seek(stream.Location.Rva), "Bad module list offset");
		CHECK(file.read(module_list_header), "Couldn't read module list header");
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

	void load_thread_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		MINIDUMP_THREAD_LIST thread_list_header;
		CHECK(stream.Location.DataSize >= sizeof thread_list_header, "Bad thread list stream");
		CHECK(file.seek(stream.Location.Rva), "Bad thread list offset");
		CHECK(file.read(thread_list_header), "Couldn't read thread list header");
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
				CHECK(context.flags & (ContextX86::CONTEXT_I386 | ContextX86::CONTEXT_CONTROL), "Bad thread context");
				t.context.x86.eip = context.eip;
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

	void load_thread_info_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
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
}

Minidump::Thread::Thread()
{
	::memset(&context, 0, sizeof context);
}

Minidump::Minidump(const std::string& filename)
{
	File file(filename);
	CHECK(file, "Couldn't open \"" << filename << "\"");

	MINIDUMP_HEADER header;
	CHECK(file.read(header), "Couldn't read header");
	CHECK_EQ(header.Signature, MINIDUMP_HEADER::SIGNATURE, "Header signature mismatch");
	CHECK_EQ(header.Version & MINIDUMP_HEADER::VERSION_MASK, MINIDUMP_HEADER::VERSION, "Header version mismatch");

	timestamp = header.TimeDateStamp;

	std::vector<MINIDUMP_DIRECTORY> streams(header.NumberOfStreams);
	CHECK(file.seek(header.StreamDirectoryRva), "Bad stream directory offset");
	const auto stream_directory_size = streams.size() * sizeof(MINIDUMP_DIRECTORY);
	CHECK(file.read(streams.data(), stream_directory_size) == stream_directory_size, "Couldn't read stream directory");

	for (const auto& stream : streams)
	{
		const auto& stream_name = stream_type_to_string(stream.StreamType);
		std::cerr << "Found " << stream_name << std::endl;
		switch (stream.StreamType)
		{
		case ThreadListStream:
			load_thread_list(*this, file, stream);
			break;
		case ModuleListStream:
			load_module_list(*this, file, stream);
			break;
		case MiscInfoStream:
			load_misc_info(*this, file, stream);
			break;
		case ThreadInfoListStream:
			load_thread_info_list(*this, file, stream);
			break;
		default:
			break;
		}
	}
}

void Minidump::print_modules(std::ostream& stream)
{
	std::vector<std::vector<std::string>> table;
	table.push_back({"#", "NAME", "VERSION", "IMAGE", "PDB"});
	for (const auto& module : modules)
	{
		table.push_back({
			std::to_string(&module - &modules.front() + 1),
			module.file_name,
			module.product_version,
			::to_hex(module.image_base, is_32bit) + " - " + ::to_hex(module.image_base + module.image_size, is_32bit),
			module.pdb_name
		});
	}
	for (const auto& row : ::format_table(table))
		stream << '\t' << row << '\n';
}

void Minidump::print_summary(std::ostream& stream)
{
	stream << "Timestamp: " << ::time_t_to_string(timestamp) << "\n";
	if (process_id)
		stream << "Process ID: " << process_id << "\n";
	if (process_create_time)
	{
		stream
			<< "Process creation time: " << ::time_t_to_string(process_create_time)
				<< " (calculated uptime: " << ::duration_to_string(timestamp - process_create_time) << ")\n"
			<< "Process user time: " << ::duration_to_string(process_user_time) << "\n"
			<< "Process kernel time: " << ::duration_to_string(process_kernel_time) << "\n";
	}
}

void Minidump::print_threads(std::ostream& stream)
{
	std::vector<std::vector<std::string>> table;
	table.push_back({"#", "ID", "STACK", "START", "CURRENT"});
	for (const auto& thread : threads)
	{
		std::string start_address;
		if (thread.start_address)
		{
			start_address = ::to_hex(thread.start_address, is_32bit);
			const auto i = std::find_if(modules.begin(), modules.end(), [&thread](const auto& module)
			{
				return thread.start_address >= module.image_base
					&& thread.start_address < module.image_base + module.image_size;
			});
			if (i != modules.end())
				start_address = i->file_name + "!" + start_address;
		}
		table.push_back({
			std::to_string(&thread - &threads.front() + 1),
			::to_hex(thread.id),
			::to_hex(thread.stack_base, is_32bit) + " - " + ::to_hex(thread.stack_base + thread.stack_size, is_32bit),
			decode_code_address(thread.start_address),
			decode_code_address(thread.context.x86.eip)
		});
	}
	for (const auto& row : ::format_table(table))
		stream << '\t' << row << '\n';
}

std::string Minidump::decode_code_address(uint64_t address)
{
	if (!address)
		return {};
	std::string result = ::to_hex(address, is_32bit);
	const auto i = std::find_if(modules.begin(), modules.end(), [address](const auto& module)
	{
		return address >= module.image_base && address < module.image_base + module.image_size;
	});
	if (i != modules.end())
		result = i->file_name + "!" + result;
	return std::move(result);
}

#include "minidump.h"
#include "file.h"
#include "minidump_format.h"
#include "utils.h"
#include <array>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#define ENSURE(value, message) \
	do { \
		if (!(value)) { \
			std::stringstream stream_; \
			stream_ << message; \
			throw std::runtime_error(stream_.str()); \
		} \
	} while (false) \

#define ENSURE_EQ(value, expected, message) \
	do { \
		if ((value) != (expected)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", expected = " << (expected); \
			throw std::runtime_error(stream_.str()); \
		} \
	} while (false) \

#define ENSURE_LE(value, max, message) \
	do { \
		if ((value) > (max)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", max = " << (max); \
			throw std::runtime_error(stream_.str()); \
		} \
	} while (false) \

#define ENSURE_GE(value, min, message) \
	do { \
		if ((value) < (min)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", min = " << (min); \
			throw std::runtime_error(stream_.str()); \
		} \
	} while (false) \

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
		ENSURE(file.seek(rva), "Bad string offset");
		MINIDUMP_STRING string_header;
		ENSURE(file.read(string_header), "Couldn't read string header");
		std::u16string string(string_header.Length / 2, u' ');
		ENSURE(file.read(const_cast<char16_t*>(string.data()), string.size() * sizeof(char16_t)), "Couldn't read string");
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
		ENSURE(stream.Location.DataSize >= sizeof misc_info, "Bad misc info stream");
		ENSURE(file.seek(stream.Location.Rva), "Bad misc info offset");
		ENSURE(file.read(misc_info), "Couldn't read misc info");
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
		ENSURE(stream.Location.DataSize >= sizeof module_list_header, "Bad module list stream");
		ENSURE(file.seek(stream.Location.Rva), "Bad module list offset");
		ENSURE(file.read(module_list_header), "Couldn't read module list header");
		std::vector<MINIDUMP_MODULE> modules(module_list_header.NumberOfModules);
		const auto module_list_size = modules.size() * sizeof(MINIDUMP_MODULE);
		ENSURE(file.read(modules.data(), module_list_size) == module_list_size, "Couldn't read module list");
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
					ENSURE_GE(module.CvRecord.DataSize, CodeViewRecordPDB70::MinSize, "Bad PDB reference size");
					const auto& cv = ::allocate_pod<CodeViewRecordPDB70>(module.CvRecord.DataSize + 1);
					ENSURE(file.seek(module.CvRecord.Rva), "Bad PDB reference");
					ENSURE(file.read(cv.get(), module.CvRecord.DataSize), "Couldn't read PDB reference");
					m.pdb_path = cv->pdb_name;
					m.pdb_name = m.pdb_path.substr(m.pdb_path.find_last_of('\\') + 1);
				}
				catch (const std::runtime_error& e)
				{
					std::cerr << "ERROR: [" << m.file_name << "] " << e.what() << std::endl;
				}
			}
			dump.modules.emplace_back(std::move(m));
			dump.memory_usage.all_images += m.image_size;
			dump.memory_usage.max_image = std::max(dump.memory_usage.max_image, m.image_size);
		}
	}

	void load_thread_list(Minidump& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		MINIDUMP_THREAD_LIST thread_list_header;
		ENSURE(stream.Location.DataSize >= sizeof thread_list_header, "Bad thread list stream");
		ENSURE(file.seek(stream.Location.Rva), "Bad thread list offset");
		ENSURE(file.read(thread_list_header), "Couldn't read thread list header");
		std::vector<MINIDUMP_THREAD> threads(thread_list_header.NumberOfThreads);
		const auto thread_list_size = threads.size() * sizeof(MINIDUMP_THREAD);
		ENSURE(file.read(threads.data(), thread_list_size) == thread_list_size, "Couldn't read thread list");
		for (const auto& thread : threads)
		{
			Minidump::Thread t;
			t.id = thread.ThreadId;
			t.stack_base = thread.Stack.StartOfMemoryRange;
			t.stack_size = thread.Stack.Memory.DataSize;
			dump.threads.emplace_back(std::move(t));
			dump.memory_usage.all_stacks += t.stack_size;
			dump.memory_usage.max_stack = std::max(dump.memory_usage.max_stack, t.stack_size);
		}
	}
}

Minidump::Minidump(const std::string& filename)
{
	File file(filename);
	ENSURE(file, "Couldn't open \"" << filename << "\"");

	MINIDUMP_HEADER header;
	ENSURE(file.read(header), "Couldn't read header");
	ENSURE_EQ(header.Signature, MINIDUMP_HEADER::SIGNATURE, "Header signature mismatch");
	ENSURE_EQ(header.Version & MINIDUMP_HEADER::VERSION_MASK, MINIDUMP_HEADER::VERSION, "Header version mismatch");

	timestamp = header.TimeDateStamp;

	std::vector<MINIDUMP_DIRECTORY> streams(header.NumberOfStreams);
	ENSURE(file.seek(header.StreamDirectoryRva), "Bad stream directory offset");
	const auto stream_directory_size = streams.size() * sizeof(MINIDUMP_DIRECTORY);
	ENSURE(file.read(streams.data(), stream_directory_size) == stream_directory_size, "Couldn't read stream directory");

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
		default:
			break;
		}
	}
}

std::ostream& operator<<(std::ostream& stream, const Minidump& dump)
{
	stream << "Timestamp: " << ::time_t_to_string(dump.timestamp) << "\n";
	if (dump.process_id)
		stream << "Process ID: " << dump.process_id << "\n";
	if (dump.process_create_time)
	{
		stream
			<< "Process creation time: " << ::time_t_to_string(dump.process_create_time)
				<< " (calculated uptime: " << ::duration_to_string(dump.timestamp - dump.process_create_time) << ")\n"
			<< "Process user time: " << ::duration_to_string(dump.process_user_time) << "\n"
			<< "Process kernel time: " << ::duration_to_string(dump.process_kernel_time) << "\n";
	}
	if (!dump.modules.empty())
	{
		stream << "Modules:\n";
		std::vector<std::vector<std::string>> table;
		for (const auto& module : dump.modules)
		{
			table.push_back({
				std::to_string(&module - &dump.modules.front() + 1),
				module.file_name,
				module.product_version,
				module.timestamp,
				module.pdb_name,
				::to_hex(module.image_base),
				::to_hex(module.image_size)
			});
		}
		for (const auto& row : ::format_table(table))
			stream << '\t' << row << '\n';
	}
	if (!dump.threads.empty())
	{
		stream << "Threads:\n";
		std::vector<std::vector<std::string>> table;
		table.push_back({"#", "ID", "Stack top", "Stack end", "Size"});
		for (const auto& thread : dump.threads)
		{
			table.push_back({
				std::to_string(&thread - &dump.threads.front() + 1),
				::to_hex(thread.id),
				::to_hex(thread.stack_base),
				::to_hex(thread.stack_base + thread.stack_size),
				::to_hex(thread.stack_size)
			});
		}
		for (const auto& row : ::format_table(table))
			stream << '\t' << row << '\n';
	}
	stream << "Memory usage:\n"
		"\tImages: " << ::to_human_readable(dump.memory_usage.all_images)
			<< " (max " << ::to_human_readable(dump.memory_usage.max_image) << ")\n"
		"\tStacks: " << ::to_human_readable(dump.memory_usage.all_stacks)
			<< " (max " << ::to_human_readable(dump.memory_usage.max_stack) << ")\n"
		;
	return stream;
}

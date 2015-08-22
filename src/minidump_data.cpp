#include "minidump_data.h"
#include "check.h"
#include "file.h"
#include "minidump_structs.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{
	template <typename T>
	std::unique_ptr<T, void(*)(void*)> allocate_pod(size_t size)
	{
		return { static_cast<T*>(::calloc(size, 1)), ::free };
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

	void load_exception(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		CHECK(!dump.exception, "Duplicate exception");

		MINIDUMP_EXCEPTION_STREAM exception;
		CHECK(stream.Location.DataSize >= sizeof exception, "Bad exception stream");
		CHECK(file.seek(stream.Location.Rva), "Bad exception offset");
		CHECK(file.read(exception), "Couldn't read exception");

		dump.exception = std::make_unique<MinidumpData::Exception>();
		dump.exception->thread_id = exception.ThreadId;
	}

	void load_misc_info(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
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

	void load_module_list(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		const auto version_to_string = [](uint32_t ms, uint32_t ls) -> std::string
		{
			std::array<char, 24> buffer;
			::memset(buffer.data(), 0, buffer.size());
			::snprintf(buffer.data(), buffer.size(), "%d.%d.%d.%d", ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff);
			return buffer.data();
		};

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
			MinidumpData::Module m;
			m.file_path = ::to_ascii(::read_string(file, module.ModuleNameRva));
			m.file_name = m.file_path.substr(m.file_path.find_last_of('\\') + 1);
			m.file_version = version_to_string(module.VersionInfo.dwFileVersionMS, module.VersionInfo.dwFileVersionLS);
			m.product_version = version_to_string(module.VersionInfo.dwProductVersionMS, module.VersionInfo.dwProductVersionLS);
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

	void load_system_info(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
	{
		CHECK(!dump.system_info, "Duplicate system info");

		SystemInfo system_info;
		CHECK(stream.Location.DataSize >= sizeof system_info, "Bad system info stream");
		CHECK(file.seek(stream.Location.Rva), "Bad system info offset");
		CHECK(file.read(system_info), "Couldn't read system info");

		dump.system_info = std::make_unique<MinidumpData::SystemInfo>();
		dump.system_info->processors = system_info.NumberOfProcessors;
		try
		{
			dump.system_info->version_name = ::to_ascii(::read_string(file, system_info.CSDVersionRva));
		}
		catch (const BadCheck& e)
		{
			std::cerr << "ERROR: " << e.what() << std::endl;
		}
	}

	void load_thread_list(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
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

			MinidumpData::Thread t;
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
				t.context.x86.ebp = context.ebp;
			}
			catch (const BadCheck& e)
			{
				std::cerr << "ERROR: " << e.what() << std::endl;
			}

			t.stack.reset(new uint8_t[t.stack_size]);
			CHECK(file.seek(thread.Stack.Memory.Rva), "Bad thread " << index << " stack offset");
			CHECK(file.read(t.stack.get(), t.stack_size), "Couldn't read thread " << index << " stack");

			dump.threads.emplace_back(std::move(t));
			dump.memory_usage.all_stacks += t.stack_size;
			dump.memory_usage.max_stack = std::max(dump.memory_usage.max_stack, t.stack_size);
			dump.is_32bit = dump.is_32bit && t.stack_base + t.stack_size <= UINT32_MAX;
		}
	}

	void load_thread_info_list(MinidumpData& dump, File& file, const MINIDUMP_DIRECTORY& stream)
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

std::unique_ptr<MinidumpData> MinidumpData::load(const std::string& file_name)
{
	const auto stream_type_to_string = [](MINIDUMP_STREAM_TYPE type) -> std::string
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
	};

	File file(file_name);
	CHECK(file, "Couldn't open \"" << file_name << "\"");

	auto dump = std::make_unique<MinidumpData>();

	MINIDUMP_HEADER header;
	CHECK(file.read(header), "Couldn't read header");
	CHECK_EQ(header.Signature, MINIDUMP_HEADER::SIGNATURE, "Header signature mismatch");
	CHECK_EQ(header.Version & MINIDUMP_HEADER::VERSION_MASK, MINIDUMP_HEADER::VERSION, "Header version mismatch");

	dump->timestamp = header.TimeDateStamp;

	std::vector<MINIDUMP_DIRECTORY> streams(header.NumberOfStreams);
	CHECK(file.seek(header.StreamDirectoryRva), "Bad stream directory offset");
	const auto stream_directory_size = streams.size() * sizeof(MINIDUMP_DIRECTORY);
	CHECK(file.read(streams.data(), stream_directory_size) == stream_directory_size, "Couldn't read stream directory");

	for (const auto& stream : streams)
	{
		switch (stream.StreamType)
		{
		case ThreadListStream:
			load_thread_list(*dump, file, stream);
			break;
		case ModuleListStream:
			load_module_list(*dump, file, stream);
			break;
		case ExceptionStream:
			load_exception(*dump, file, stream);
			break;
		case SystemInfoStream:
			load_system_info(*dump, file, stream);
			break;
		case MiscInfoStream:
			load_misc_info(*dump, file, stream);
			break;
		case ThreadInfoListStream:
			load_thread_info_list(*dump, file, stream);
			break;
		default:
			const auto& stream_name = stream_type_to_string(stream.StreamType);
			std::cerr << "WARNING: Skipped " << stream_name << std::endl;
			break;
		}
	}

	if (dump->exception)
	{
		const auto i = std::find_if(dump->threads.begin(), dump->threads.end(), [&dump](const auto& thread)
		{
			return thread.id == dump->exception->thread_id;
		});
		CHECK(i != dump->threads.end(), "Exception in unknown thread");
		dump->exception->thread = &*i;
	}

	return dump;
}

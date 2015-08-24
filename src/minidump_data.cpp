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

	std::u16string read_string(File& file, minidump::RVA rva)
	{
		CHECK(file.seek(rva), "Bad string offset");
		minidump::StringHeader header;
		CHECK(file.read(header), "Couldn't read string header");
		std::u16string string(header.Length / 2, u' ');
		CHECK(file.read(const_cast<char16_t*>(string.data()), string.size() * sizeof(char16_t)), "Couldn't read string");
		return std::move(string);
	}

	void load_exception(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.exception, "Duplicate exception");

		minidump::ExceptionStream exception;
		CHECK(stream.location.DataSize >= sizeof exception, "Bad exception stream");
		CHECK(file.seek(stream.location.Rva), "Bad exception offset");
		CHECK(file.read(exception), "Couldn't read exception");

		dump.exception = std::make_unique<MinidumpData::Exception>();
		dump.exception->thread_id = exception.ThreadId;
	}

	void load_misc_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		minidump::MiscInfo misc_info;
		CHECK(stream.location.DataSize >= sizeof misc_info, "Bad misc info stream");
		CHECK(file.seek(stream.location.Rva), "Bad misc info offset");
		CHECK(file.read(misc_info), "Couldn't read misc info");

		if (misc_info.flags & minidump::MiscInfo::ProcessId)
			dump.process_id = misc_info.process_id;
		if (misc_info.flags & minidump::MiscInfo::ProcessTimes)
		{
			dump.process_create_time = misc_info.process_create_time;
			dump.process_user_time = misc_info.process_user_time;
			dump.process_kernel_time = misc_info.process_kernel_time;
		}
		// TODO: Load processor power info (MINIDUMP_MISC_INFO_2).
	}

	void load_memory_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.memory.empty(), "Duplicate memory list");

		minidump::MemoryListHeader header;
		CHECK(stream.location.DataSize >= sizeof header, "Bad memory list stream");
		CHECK(file.seek(stream.location.Rva), "Bad memory list offset");
		CHECK(file.read(header), "Couldn't read memory list header");
		CHECK_GE(header.NumberOfMemoryRanges, 0, "Bad memory list size");

		std::vector<minidump::MemoryRange> memory(header.NumberOfMemoryRanges);
		const auto memory_list_size = memory.size() * sizeof(minidump::MemoryRange);
		CHECK(file.read(memory.data(), memory_list_size) == memory_list_size, "Couldn't read memory list");
		for (const auto& memory_range : memory)
		{
			MinidumpData::MemoryInfo m;
			m.end = memory_range.StartOfMemoryRange + memory_range.Memory.DataSize;

			dump.memory.emplace(memory_range.StartOfMemoryRange, std::move(m));
			dump.is_32bit = dump.is_32bit && m.end <= uint64_t{UINT32_MAX} + 1;
		}
	}

	void load_module_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		const auto version_to_string = [](uint32_t ms, uint32_t ls) -> std::string
		{
			std::array<char, 24> buffer;
			::memset(buffer.data(), 0, buffer.size());
			::snprintf(buffer.data(), buffer.size(), "%d.%d.%d.%d", ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff);
			return buffer.data();
		};

		CHECK(dump.modules.empty(), "Duplicate module list");

		minidump::ModuleListHeader header;
		CHECK(stream.location.DataSize >= sizeof header, "Bad module list stream");
		CHECK(file.seek(stream.location.Rva), "Bad module list offset");
		CHECK(file.read(header), "Couldn't read module list header");
		CHECK_GE(header.NumberOfModules, 0, "Bad module list size");

		std::vector<minidump::Module> modules(header.NumberOfModules);
		const auto module_list_size = modules.size() * sizeof(minidump::Module);
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
			m.image_end = module.BaseOfImage + module.SizeOfImage;
			if (module.CvRecord.DataSize > 0)
			{
				try
				{
					CHECK_GE(module.CvRecord.DataSize, minidump::CodeViewRecordPDB70::MinSize, "Bad PDB reference size");
					const auto& cv = ::allocate_pod<minidump::CodeViewRecordPDB70>(module.CvRecord.DataSize + 1);
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
			dump.memory_usage.all_images += m.image_end - m.image_base;
			dump.memory_usage.max_image = std::max<uint64_t>(dump.memory_usage.max_image, m.image_end - m.image_base);
			dump.is_32bit = dump.is_32bit && m.image_end <= uint64_t{UINT32_MAX} + 1;
		}
	}

	void load_system_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.system_info, "Duplicate system info");

		minidump::SystemInfo system_info;
		CHECK(stream.location.DataSize >= sizeof system_info, "Bad system info stream");
		CHECK(file.seek(stream.location.Rva), "Bad system info offset");
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

	void load_thread_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.threads.empty(), "Duplicate thread list");

		minidump::ThreadListHeader header;
		CHECK(stream.location.DataSize >= sizeof header, "Bad thread list stream");
		CHECK(file.seek(stream.location.Rva), "Bad thread list offset");
		CHECK(file.read(header), "Couldn't read thread list header");
		CHECK_GE(header.NumberOfThreads, 0, "Bad thread list size");

		std::vector<minidump::Thread> threads(header.NumberOfThreads);
		const auto thread_list_size = threads.size() * sizeof(minidump::Thread);
		CHECK(file.read(threads.data(), thread_list_size) == thread_list_size, "Couldn't read thread list");
		for (const auto& thread : threads)
		{
			const auto index = &thread - &threads.front() + 1;

			MinidumpData::Thread t;
			t.id = thread.ThreadId;
			t.stack_base = thread.Stack.StartOfMemoryRange;
			t.stack_end = thread.Stack.StartOfMemoryRange + thread.Stack.Memory.DataSize;

			try
			{
				minidump::ContextX86 context;
				CHECK_EQ(thread.ThreadContext.DataSize, sizeof context, "Bad thread context size");
				CHECK(file.seek(thread.ThreadContext.Rva), "Bad thread " << index << " context offset");
				CHECK(file.read(context), "Couldn't read thread " << index << " context");
				CHECK(::has_flags(context.flags, minidump::ContextX86::I386 | minidump::ContextX86::Control), "Bad thread context");
				t.context.x86.eip = context.eip;
				t.context.x86.esp = context.esp;
				t.context.x86.ebp = context.ebp;
			}
			catch (const BadCheck& e)
			{
				std::cerr << "ERROR: " << e.what() << std::endl;
			}

			t.stack.reset(new uint8_t[t.stack_end - t.stack_base]);
			CHECK(file.seek(thread.Stack.Memory.Rva), "Bad thread " << index << " stack offset");
			CHECK(file.read(t.stack.get(), t.stack_end - t.stack_base), "Couldn't read thread " << index << " stack");

			dump.threads.emplace_back(std::move(t));
			dump.memory_usage.all_stacks += t.stack_base + t.stack_end;
			dump.memory_usage.max_stack = std::max<uint64_t>(dump.memory_usage.max_stack, t.stack_base + t.stack_end);
			dump.is_32bit = dump.is_32bit && t.stack_end <= uint64_t{UINT32_MAX} + 1;
		}
	}

	void load_thread_info_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		if (dump.threads.empty())
		{
			std::cerr << "ERROR: Thread info list found before thread list" << std::endl;
			return;
		}

		minidump::ThreadInfoListHeader header;
		CHECK_GE(stream.location.DataSize, sizeof header, "Bad thread info list stream");
		CHECK(file.seek(stream.location.Rva), "Bad thread info list offset");
		CHECK(file.read(header), "Couldn't read thread info list header");
		CHECK_GE(header.SizeOfHeader, sizeof header, "Bad thread info list header size");

		minidump::ThreadInfo entry;
		CHECK_GE(header.SizeOfEntry, sizeof entry, "Bad thread info size");
		const auto base = stream.location.Rva + header.SizeOfHeader;
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
			j->dumping = entry.DumpFlags & minidump::MINIDUMP_THREAD_INFO_WRITING_THREAD;

			dump.is_32bit = dump.is_32bit && j->start_address <= uint64_t{UINT32_MAX} + 1;
		}
	}
}

std::unique_ptr<MinidumpData> MinidumpData::load(const std::string& file_name)
{
	const auto stream_type_to_string = [](minidump::Stream::Type type) -> std::string
	{
		using namespace minidump;
		switch (type)
		{
		case Stream::Type::Unused: return "UnusedStream";
		case Stream::Type::Reserved0: return "ReservedStream0";
		case Stream::Type::Reserved1: return "ReservedStream1";
		case Stream::Type::ThreadList: return "ThreadListStream";
		case Stream::Type::ModuleList: return "ModuleListStream";
		case Stream::Type::MemoryList: return "MemoryListStream";
		case Stream::Type::Exception: return "ExceptionStream";
		case Stream::Type::SystemInfo: return "SystemInfoStream";
		case Stream::Type::ThreadExList: return "ThreadExListStream";
		case Stream::Type::Memory64List: return "Memory64ListStream";
		case Stream::Type::CommentA: return "CommentStreamA";
		case Stream::Type::CommentW: return "CommentStreamW";
		case Stream::Type::HandleData: return "HandleDataStream";
		case Stream::Type::FunctionTable: return "FunctionTableStream";
		case Stream::Type::UnloadedModuleList: return "UnloadedModuleListStream";
		case Stream::Type::MiscInfo: return "MiscInfoStream";
		case Stream::Type::MemoryInfoList: return "MemoryInfoListStream";
		case Stream::Type::ThreadInfoList: return "ThreadInfoListStream";
		case Stream::Type::HandleOperationList: return "HandleOperationListStream";
		default: return "Stream" + std::to_string(std::underlying_type_t<minidump::Stream::Type>(type));
		}
	};

	File file(file_name);
	CHECK(file, "Couldn't open \"" << file_name << "\"");

	auto dump = std::make_unique<MinidumpData>();

	minidump::Header header;
	CHECK(file.read(header), "Couldn't read header");
	CHECK_EQ(header.signature, minidump::Header::SIGNATURE, "Header signature mismatch");
	CHECK_EQ(header.version & minidump::Header::VERSION_MASK, minidump::Header::VERSION, "Header version mismatch");

	dump->timestamp = header.time_date_stamp;

	std::vector<minidump::Stream> streams(header.number_of_streams);
	CHECK(file.seek(header.stream_directory_rva), "Bad stream directory offset");
	const auto stream_directory_size = streams.size() * sizeof(minidump::Stream);
	CHECK(file.read(streams.data(), stream_directory_size) == stream_directory_size, "Couldn't read stream directory");

	for (const auto& stream : streams)
	{
		switch (stream.type)
		{
		case minidump::Stream::Type::ThreadList:
			load_thread_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::ModuleList:
			load_module_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::MemoryList:
			load_memory_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::Exception:
			load_exception(*dump, file, stream);
			break;
		case minidump::Stream::Type::SystemInfo:
			load_system_info(*dump, file, stream);
			break;
		case minidump::Stream::Type::MiscInfo:
			load_misc_info(*dump, file, stream);
			break;
		case minidump::Stream::Type::ThreadInfoList:
			load_thread_info_list(*dump, file, stream);
			break;
		default:
			const auto& stream_name = stream_type_to_string(stream.type);
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

	for (auto& memory_range : dump->memory)
	{
		for (const auto& module : dump->modules)
		{
			if (memory_range.first >= module.image_base && memory_range.second.end <= module.image_end)
			{
				memory_range.second.usage = MinidumpData::MemoryInfo::Usage::Image;
				memory_range.second.usage_index = &module - &dump->modules.front() + 1;
				break;
			}
		}
		if (memory_range.second.usage != MinidumpData::MemoryInfo::Usage::Unknown)
			continue;
		for (const auto& thread : dump->threads)
		{
			if (memory_range.first >= thread.stack_base && memory_range.second.end <= thread.stack_end)
			{
				memory_range.second.usage = MinidumpData::MemoryInfo::Usage::Stack;
				memory_range.second.usage_index = &thread - &dump->threads.front() + 1;
				break;
			}
		}
	}

	return dump;
}

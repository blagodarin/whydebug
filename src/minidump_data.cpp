#include "minidump_data.h"
#include "check.h"
#include "file.h"
#include "minidump_format.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{
	// End of 32-bit address range.
	constexpr auto End32 = uint64_t{UINT32_MAX} + 1;

	template <typename T>
	std::unique_ptr<T, void(*)(void*)> allocate_pod(size_t size)
	{
		return { static_cast<T*>(::calloc(size, 1)), ::free };
	}

	std::u16string read_string(File& file, uint32_t offset)
	{
		minidump::StringHeader header;
		CHECK(file.seek(offset), "Bad string offset");
		CHECK(file.read(header), "Couldn't read string header");
		if (!header.size)
			return {};
		std::u16string string(header.size / 2, u' ');
		CHECK(file.read(const_cast<char16_t*>(string.data()), header.size), "Couldn't read string");
		return std::move(string);
	}

	void load_exception(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.exception, "Duplicate exception");

		minidump::ExceptionStream exception;
		CHECK(stream.location.size >= sizeof exception, "Bad exception stream");
		CHECK(file.seek(stream.location.offset), "Bad exception offset");
		CHECK(file.read(exception), "Couldn't read exception");

		dump.exception = std::make_unique<MinidumpData::Exception>();
		dump.exception->thread_id = exception.thread_id;
	}

	void load_handle_data(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.handles.empty(), "Duplicate handle data list");

		minidump::HandleDataHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad handle data stream");
		CHECK(file.seek(stream.location.offset), "Bad handle data list offset");
		CHECK(file.read(header), "Couldn't read handle data list header");
		CHECK_GE(header.entry_count, 0, "Bad handle data list size");
		CHECK_GE(header.entry_size, sizeof(minidump::HandleData), "Bad handle data size");

		minidump::HandleData2 entry;
		const auto entry_size = std::min(sizeof entry, header.entry_size);
		const auto base = stream.location.offset + header.header_size;
		for (uint32_t i = 0; i < header.entry_count; ++i)
		{
			CHECK(file.seek(base + i * header.entry_size), "Bad handle data list");
			CHECK(file.read(&entry, entry_size), "Couldn't read handle data");

			MinidumpData::Handle handle;
			handle.handle = entry.handle;
			if (entry.type_name_offset > 0)
			{
				try
				{
					handle.type_name = ::to_ascii(::read_string(file, entry.type_name_offset));
				}
				catch (const BadCheck& e)
				{
					std::cerr << "ERROR: Couldn't read handle type name: " << e.what() << std::endl;
				}
			}
			if (entry.object_name_offset > 0)
			{
				try
				{
					handle.type_name = ::to_ascii(::read_string(file, entry.object_name_offset));
				}
				catch (const BadCheck& e)
				{
					std::cerr << "ERROR: Couldn't read handle object name: " << e.what() << std::endl;
				}
			}
			dump.handles.emplace_back(handle);
		}
	}

	void load_misc_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		minidump::MiscInfo2 misc_info;
		CHECK(stream.location.size >= sizeof(minidump::MiscInfo), "Bad misc info stream");
		CHECK(file.seek(stream.location.offset), "Bad misc info offset");
		CHECK(file.read(&misc_info, std::min(stream.location.size, sizeof misc_info)), "Couldn't read misc info");

		if (misc_info.flags & minidump::MiscInfo::ProcessId)
			dump.process_id = misc_info.process_id;
		if (misc_info.flags & minidump::MiscInfo::ProcessTimes)
		{
			dump.process_create_time = misc_info.process_create_time;
			dump.process_user_time = misc_info.process_user_time;
			dump.process_kernel_time = misc_info.process_kernel_time;
		}

		if (stream.location.size < sizeof(minidump::MiscInfo2))
			return;

		if (misc_info.flags & minidump::MiscInfo2::ProcessorPowerInfo)
		{
			std::array<char, 32> buffer;
			::snprintf(buffer.data(), buffer.size(), "%g GHz", misc_info.processor_current_mhz / 1000.0);
			dump.cpu_frequency = buffer.data();
		}
	}

	void load_memory_info_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.memory_regions.empty(), "Duplicate memory info list");

		minidump::MemoryInfoListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad memory info list stream");
		CHECK(file.seek(stream.location.offset), "Bad memory info list offset");
		CHECK(file.read(header), "Couldn't read memory info list header");
		CHECK_GE(header.header_size, sizeof header, "Bad memory info list header size");

		minidump::MemoryInfo memory_info;
		CHECK_GE(header.entry_size, sizeof memory_info, "Bad memory info size");
		const auto base = stream.location.offset + header.header_size;
		for (uint32_t i = 0; i < header.entry_count; ++i)
		{
			CHECK(file.seek(base + i * header.entry_size), "Bad memory info list");
			CHECK(file.read(memory_info), "Couldn't read memory info entry");

			MinidumpData::MemoryRegion m;
			m.end = memory_info.base + memory_info.size;
			switch (memory_info.state)
			{
			case minidump::MEM_COMMIT:
				m.state = MinidumpData::MemoryRegion::State::Allocated;
				break;
			case minidump::MEM_RESERVE:
				m.state = MinidumpData::MemoryRegion::State::Reserved;
				break;
			default:
				CHECK_EQ(memory_info.state, minidump::MEM_FREE, "Bad memory region state");
			}

			// NOTE: Temporary collapsing code.
			const auto j = std::find_if(dump.memory_regions.rbegin(), dump.memory_regions.rend(), [&memory_info](const auto& memory_region)
			{
				return memory_region.second.end == memory_info.base;
			});
			if (j != dump.memory_regions.rend() && j->second.state == m.state)
				j->second.end = m.end;
			else
				dump.memory_regions.emplace(memory_info.base, std::move(m));

			dump.is_32bit = dump.is_32bit && m.end <= End32;
		}
	}

	void load_memory_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.memory.empty(), "Duplicate memory/memory64 list");

		minidump::MemoryListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad memory/memory64 list stream");
		CHECK(file.seek(stream.location.offset), "Bad memory/memory64 list offset");
		CHECK(file.read(header), "Couldn't read memory/memory64 list header");
		CHECK_GE(header.entry_count, 0, "Bad memory/memory64 list size");

		std::vector<minidump::MemoryRange> memory(header.entry_count);
		CHECK(file.read(memory.data(), memory.size() * sizeof(minidump::MemoryRange)), "Couldn't read memory/memory64 list");
		for (const auto& memory_range : memory)
		{
			MinidumpData::MemoryInfo m;
			m.end = memory_range.base + memory_range.location.size;

			dump.memory.emplace(memory_range.base, std::move(m));
			dump.is_32bit = dump.is_32bit && m.end <= End32;
		}
	}

	void load_memory64_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.memory.empty(), "Duplicate memory/memory64 list");

		minidump::Memory64ListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad memory/memory64 list stream");
		CHECK(file.seek(stream.location.offset), "Bad memory/memory64 list offset");
		CHECK(file.read(header), "Couldn't read memory/memory64 list header");
		CHECK_GE(header.entry_count, 0, "Bad memory/memory64 list size");

		std::vector<minidump::Memory64Range> memory(header.entry_count);
		CHECK(file.read(memory.data(), memory.size() * sizeof(minidump::Memory64Range)), "Couldn't read memory/memory64 list");
		for (const auto& memory_range : memory)
		{
			MinidumpData::MemoryInfo m;
			m.end = memory_range.base + memory_range.size;

			dump.memory.emplace(memory_range.base, std::move(m));
			dump.is_32bit = dump.is_32bit && m.end <= End32;
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
		CHECK(stream.location.size >= sizeof header, "Bad module list stream");
		CHECK(file.seek(stream.location.offset), "Bad module list offset");
		CHECK(file.read(header), "Couldn't read module list header");
		CHECK_GE(header.entry_count, 0, "Bad module list size");

		std::vector<minidump::Module> modules(header.entry_count);
		CHECK(file.read(modules.data(), modules.size() * sizeof(minidump::Module)), "Couldn't read module list");
		for (const auto& module : modules)
		{
			MinidumpData::Module m;
			m.file_path = ::to_ascii(::read_string(file, module.name_offset));
			m.file_name = m.file_path.substr(m.file_path.find_last_of('\\') + 1);
			m.file_version = version_to_string(module.version_info.dwFileVersionMS, module.version_info.dwFileVersionLS);
			m.product_version = version_to_string(module.version_info.dwProductVersionMS, module.version_info.dwProductVersionLS);
			m.timestamp = ::time_t_to_string(module.time_date_stamp);
			m.image_base = module.image_base;
			m.image_end = module.image_base + module.image_size;
			if (module.cv_record.size > 0)
			{
				try
				{
					CHECK_GE(module.cv_record.size, minidump::CodeViewRecordPDB70::MinSize, "Bad PDB reference size");
					const auto& cv = ::allocate_pod<minidump::CodeViewRecordPDB70>(module.cv_record.size + 1);
					CHECK(file.seek(module.cv_record.offset), "Bad PDB reference");
					CHECK(file.read(cv.get(), module.cv_record.size), "Couldn't read PDB reference");
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
			dump.is_32bit = dump.is_32bit && m.image_end <= End32;
		}
	}

	void load_system_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.system_info, "Duplicate system info");

		minidump::SystemInfo system_info;
		CHECK(stream.location.size >= sizeof system_info, "Bad system info stream");
		CHECK(file.seek(stream.location.offset), "Bad system info offset");
		CHECK(file.read(system_info), "Couldn't read system info");
		CHECK(system_info.cpu_architecture != minidump::SystemInfo::Unknown, "Unknown CPU architecture");
		CHECK(system_info.cpu_architecture == minidump::SystemInfo::X86, "Unsupported CPU architecture: " << system_info.cpu_architecture);

		dump.system_info = std::make_unique<MinidumpData::SystemInfo>();
		if (!::memcmp(system_info.cpu.x86.vendor_id, "GenuineIntel", sizeof system_info.cpu.x86.vendor_id))
		{
			static const std::tuple<uint16_t, uint8_t, std::string> names[] =
			{
				std::make_tuple( 6, 26, "Nehalem"     ), // Bloomfield and Nehalem-EP.
				std::make_tuple( 6, 30, "Nehalem"     ), // Clarksfield, Lynnfield and Jasper Forest.
				std::make_tuple( 6, 37, "Westmere"    ), // Arrandale and Clarksdale.
				std::make_tuple( 6, 42, "SandyBridge" ), // SandyBridge.
				std::make_tuple( 6, 44, "Westmere"    ), // Gulftown and Westmere-EP.
				std::make_tuple( 6, 45, "SandyBridge" ), // SandyBridge-E, SandyBridge-EN and SandyBridge-EP.
				std::make_tuple( 6, 46, "Nehalem"     ), // Nehalem-EX.
				std::make_tuple( 6, 47, "Westmere"    ), // Westmere-EX.
				std::make_tuple( 6, 58, "IvyBridge"   ), // IvyBridge.
			};

			dump.system_info->cpu_description = "Intel";
			const auto i = std::find_if(std::begin(names), std::end(names), [&system_info](const auto& entry)
				{ return std::get<0>(entry) == system_info.cpu_family && std::get<1>(entry) == system_info.cpu_model; });
			if (i != std::end(names))
				dump.system_info->cpu_description += ' ' + std::get<2>(*i);
		}
		else if (!::memcmp(system_info.cpu.x86.vendor_id, "AuthenticAMD", sizeof system_info.cpu.x86.vendor_id))
			dump.system_info->cpu_description = "AMD";
		else
			dump.system_info->cpu_description = "Unknown";
		dump.system_info->cpu_description += " (family " + std::to_string(system_info.cpu_family)
			+ ", model " + std::to_string(system_info.cpu_model)
			+ ", stepping " + std::to_string(system_info.cpu_stepping) + ')';
		dump.system_info->cpu_cores = system_info.cpu_cores;
		if (system_info.platform_id == minidump::SystemInfo::WindowsNt)
		{
			static const std::tuple<uint32_t, uint32_t, std::string, std::string> names[] =
			{
				std::make_tuple(  5, 0, "Windows 2000",  "Windows 2000"           ),
				std::make_tuple(  5, 1, "Windows XP",    "Windows XP"             ),
				std::make_tuple(  5, 2, "Windows XP",    "Windows Server 2003"    ), // 64-bit XP and both 2003 and 2003 R2.
				std::make_tuple(  6, 0, "Windows Vista", "Windows Server 2008"    ),
				std::make_tuple(  6, 1, "Windows 7",     "Windows Server 2008 R2" ),
				std::make_tuple(  6, 2, "Windows 8",     "Windows Server 2012"    ),
				std::make_tuple(  6, 3, "Windows 8.1",   "Windows Server 2012 R2" ),
				std::make_tuple( 10, 0, "Windows 10",    "Windows Server 2016"    ),
			};

			const auto i = std::find_if(std::begin(names), std::end(names), [&system_info](const auto& entry)
				{ return std::get<0>(entry) == system_info.major_version && std::get<1>(entry) == system_info.minor_version; });
			if (i != std::end(names))
			{
				dump.system_info->os_name = (system_info.product_type == minidump::SystemInfo::Server) ? std::get<3>(*i) : std::get<2>(*i);
				try
				{
					const auto& service_pack = ::to_ascii(::read_string(file, system_info.service_pack_name_offset));
					if (!service_pack.empty())
						dump.system_info->os_name += " (" + service_pack + ')';
				}
				catch (const BadCheck& e)
				{
					std::cerr << "ERROR: Couldn't read OS service pack: " << e.what() << std::endl;
				}
			}
		}
	}

	void load_thread_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.threads.empty(), "Duplicate thread list");

		minidump::ThreadListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad thread list stream");
		CHECK(file.seek(stream.location.offset), "Bad thread list offset");
		CHECK(file.read(header), "Couldn't read thread list header");
		CHECK_GE(header.entry_count, 0, "Bad thread list size");

		std::vector<minidump::Thread> threads(header.entry_count);
		CHECK(file.read(threads.data(), threads.size() * sizeof(minidump::Thread)), "Couldn't read thread list");
		for (const auto& thread : threads)
		{
			const auto index = &thread - &threads.front() + 1;

			MinidumpData::Thread t;
			t.id = thread.id;
			t.stack_base = thread.stack.base;
			t.stack_end = thread.stack.base + thread.stack.location.size;

			try
			{
				minidump::ContextX86 context;
				CHECK_EQ(thread.context.size, sizeof context, "Bad thread context size");
				CHECK(file.seek(thread.context.offset), "Bad thread " << index << " context offset");
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
			CHECK(file.seek(thread.stack.location.offset), "Bad thread " << index << " stack offset");
			CHECK(file.read(t.stack.get(), t.stack_end - t.stack_base), "Couldn't read thread " << index << " stack");

			dump.threads.emplace_back(std::move(t));
			dump.memory_usage.all_stacks += t.stack_base + t.stack_end;
			dump.memory_usage.max_stack = std::max<uint64_t>(dump.memory_usage.max_stack, t.stack_base + t.stack_end);
			dump.is_32bit = dump.is_32bit && t.stack_end <= End32;
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
		CHECK_GE(stream.location.size, sizeof header, "Bad thread info list stream");
		CHECK(file.seek(stream.location.offset), "Bad thread info list offset");
		CHECK(file.read(header), "Couldn't read thread info list header");
		CHECK_GE(header.header_size, sizeof header, "Bad thread info list header size");

		minidump::ThreadInfo thread_info;
		CHECK_GE(header.entry_size, sizeof thread_info, "Bad thread info size");
		const auto base = stream.location.offset + header.header_size;
		for (uint32_t i = 0; i < header.entry_count; ++i)
		{
			CHECK(file.seek(base + i * header.entry_size), "Bad thread info list");
			CHECK(file.read(thread_info), "Couldn't read thread info entry");

			const auto j = std::find_if(dump.threads.begin(), dump.threads.end(), [&thread_info](const auto& thread)
			{
				return thread.id == thread_info.ThreadId;
			});
			if (j == dump.threads.end())
			{
				std::cerr << "ERROR: Thread info for unknown thread " << ::to_hex(thread_info.ThreadId) << std::endl;
				continue;
			}
			j->start_address = thread_info.StartAddress;
			j->dumping = thread_info.DumpFlags & minidump::MINIDUMP_THREAD_INFO_WRITING_THREAD;

			dump.is_32bit = dump.is_32bit && j->start_address <= End32;
		}
	}

	void load_unloaded_module_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(dump.unloaded_modules.empty(), "Duplicate unloaded module list");

		minidump::UnloadedModuleListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad unloaded module list stream");
		CHECK(file.seek(stream.location.offset), "Bad unloaded module list offset");
		CHECK(file.read(header), "Couldn't read unloaded module list header");
		CHECK_GE(header.header_size, sizeof header, "Bad unloaded module list header size");

		minidump::UnloadedModule unloaded_module;
		CHECK_GE(header.entry_size, sizeof unloaded_module, "Bad unloaded module entry size");
		dump.unloaded_modules.reserve(header.entry_count);
		const auto base = stream.location.offset + header.header_size;
		for (uint32_t i = 0; i < header.entry_count; ++i)
		{
			CHECK(file.seek(base + i * header.entry_size), "Bad unloaded module list");
			CHECK(file.read(unloaded_module), "Couldn't unloaded module entry");

			MinidumpData::UnloadedModule m;
			m.file_path = ::to_ascii(::read_string(file, unloaded_module.name_offset));
			m.file_name = m.file_path.substr(m.file_path.find_last_of('\\') + 1);
			m.timestamp = ::time_t_to_string(unloaded_module.time_date_stamp);
			m.image_base = unloaded_module.image_base;
			m.image_end = unloaded_module.image_base + unloaded_module.image_size;
			dump.unloaded_modules.emplace_back(std::move(m));
			dump.is_32bit = dump.is_32bit && m.image_end <= End32;
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
		default: return "Stream" + std::to_string(std::underlying_type_t<Stream::Type>(type));
		}
	};

	File file(file_name);
	CHECK(file, "Couldn't open \"" << file_name << "\"");

	auto dump = std::make_unique<MinidumpData>();

	minidump::Header header;
	CHECK(file.read(header), "Couldn't read header");
	CHECK_EQ(header.signature, minidump::Header::Signature, "Header signature mismatch");
	CHECK_EQ(header.version, minidump::Header::Version, "Header version mismatch");

	dump->timestamp = header.timestamp;

	std::vector<minidump::Stream> streams(header.stream_count);
	CHECK(file.seek(header.stream_list_offset), "Bad stream list offset");
	CHECK(file.read(streams.data(), streams.size() * sizeof(minidump::Stream)), "Couldn't read stream list");

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
		case minidump::Stream::Type::Memory64List:
			load_memory64_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::Exception:
			load_exception(*dump, file, stream);
			break;
		case minidump::Stream::Type::SystemInfo:
			load_system_info(*dump, file, stream);
			break;
		case minidump::Stream::Type::HandleData:
			load_handle_data(*dump, file, stream);
			break;
		case minidump::Stream::Type::UnloadedModuleList:
			load_unloaded_module_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::MiscInfo:
			load_misc_info(*dump, file, stream);
			break;
		case minidump::Stream::Type::MemoryInfoList:
			load_memory_info_list(*dump, file, stream);
			break;
		case minidump::Stream::Type::ThreadInfoList:
			load_thread_info_list(*dump, file, stream);
			break;
		default:
			if (stream.type == minidump::Stream::Type::Unused && stream.location.offset == 0 && stream.location.size == 0)
				break; // A valid stream list may end with such entries.
			std::cerr << "WARNING: Skipped " << stream_type_to_string(stream.type)
				<< " (0x" << ::to_hex(stream.location.offset) << ", " << stream.location.size << " bytes)" << std::endl;
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

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

	std::string stream_name(minidump::Stream::Type type)
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
		case Stream::Type::Tokens: return "TokenStream";
		case Stream::Type::JavaScriptData: return "JavaScriptDataStream";
		case Stream::Type::SystemMemoryInfo: return "SystemMemoryInfoStream";
		case Stream::Type::ProcessVmCounters: return "ProcessVmCountersStream";
		default: return "0x" + ::to_hex(::to_raw(type));
		}
	}

	void check_extra_data(const minidump::Stream& stream, uint32_t expected)
	{
		if (stream.location.size > expected)
		{
			std::cerr << "WARNING: Extra data in " << ::stream_name(stream.type) << " ("
				<< stream.location.size - expected << " bytes at 0x" << ::to_hex(stream.location.offset + expected) << ")" << std::endl;
		}
	}

	std::unique_ptr<MinidumpData::Context> load_thread_context(File& file, const minidump::Location& location)
	{
		auto result = std::make_unique<MinidumpData::Context>();
		minidump::ThreadContext context;
		CHECK(file.seek(location.offset), "Bad thread context offset");
		switch (location.size)
		{
		case sizeof context.x86:
			CHECK(file.read(context.x86), "Couldn't read x86 thread context");
			CHECK(::has_flags(context.x86.context_flags, minidump::ThreadContext::X86 | minidump::ThreadContext::Control), "Bad x86 thread context");
			result->x86.eip = context.x86.eip;
			result->x86.esp = context.x86.esp;
			result->x86.ebp = context.x86.ebp;
			break;
		case sizeof context.x64: // Assuming WoW64.
			CHECK(file.read(context.x64), "Couldn't read x64 thread context");
			CHECK(::has_flags(context.x64.context_flags, minidump::ThreadContext::X64 | minidump::ThreadContext::Control), "Bad x64 thread context");
			result->x86.eip = context.x64.rip;
			result->x86.esp = context.x64.rsp;
			result->x86.ebp = context.x64.rbp;
			break;
		default:
			CHECK(false, "Bad thread context size " << location.size);
		}
		return result;
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

	std::string to_range(uint64_t base, uint64_t size)
	{
		return std::to_string(base) + "~" + std::to_string(base + size - 1);
	}
}

namespace
{
	class Loader
	{
	public:
		Loader(bool summary) : _summary(summary) {}

		std::unique_ptr<MinidumpData> load(const std::string& file_name);

	private:
		void load_exception(MinidumpData&, File&, const minidump::Stream&);
		void load_handle_data(MinidumpData&, File&, const minidump::Stream&);
		void load_memory_info_list(MinidumpData&, File&, const minidump::Stream&);
		void load_memory_list(MinidumpData&, File&, const minidump::Stream&);
		void load_memory64_list(MinidumpData&, File&, const minidump::Stream&);
		void load_misc_info(MinidumpData&, File&, const minidump::Stream&);
		void load_module_list(MinidumpData&, File&, const minidump::Stream&);
		void load_system_info(MinidumpData&, File&, const minidump::Stream&);
		void load_system_memory_info(MinidumpData&, File&, const minidump::Stream&);
		void load_thread_list(MinidumpData&, File&, const minidump::Stream&);
		void load_thread_info_list(MinidumpData&, File&, const minidump::Stream&);
		void load_tokens(MinidumpData&, File&, const minidump::Stream&);
		void load_unloaded_module_list(MinidumpData&, File&, const minidump::Stream&);
		void load_vm_counters(MinidumpData&, File&, const minidump::Stream&);

	private:
		const bool _summary;
		std::vector<std::tuple<uint64_t, uint64_t, uint8_t*>> _loading_stacks;
		std::unique_ptr<std::pair<uint64_t, uint64_t>> _wow64_ntdll;
	};

	std::unique_ptr<MinidumpData> Loader::load(const std::string& file_name)
	{
		File file(file_name);
		CHECK(file, "Couldn't open \"" << file_name << "\"");

		auto dump = std::make_unique<MinidumpData>();

		minidump::Header header;
		CHECK(file.read(header), "Couldn't read header");
		CHECK_EQ(header.signature, minidump::Header::Signature, "Header signature mismatch");
		CHECK_EQ(header.version, minidump::Header::Version, "Header version mismatch");

		if (_summary)
		{
			std::cout << "MINIDUMP_HEADER: # " << ::to_range(0, sizeof header)
				<< "\n\tVersion[31~16]: 0x" << ::to_hex(header.implementation_specific)
				<< "\n\tCheckSum: 0x" << ::to_hex(header.checksum)
				<< "\n\tTimeDateStamp: " << ::time_t_to_string(header.timestamp)
				<< "\n\tFlags: 0x" << ::to_hex(header.flags);
			if (!header.flags) std::cout << "\n\t\t- MiniDumpNormal";
			if (header.flags & 0x0000000000000001) std::cout << "\n\t\t- MiniDumpWithDataSegs";
			if (header.flags & 0x0000000000000002) std::cout << "\n\t\t- MiniDumpWithFullMemory";
			if (header.flags & 0x0000000000000004) std::cout << "\n\t\t- MiniDumpWithHandleData";
			if (header.flags & 0x0000000000000008) std::cout << "\n\t\t- MiniDumpFilterMemory";
			if (header.flags & 0x0000000000000010) std::cout << "\n\t\t- MiniDumpScanMemory";
			if (header.flags & 0x0000000000000020) std::cout << "\n\t\t- MiniDumpWithUnloadedModules";
			if (header.flags & 0x0000000000000040) std::cout << "\n\t\t- MiniDumpWithIndirectlyReferencedMemory";
			if (header.flags & 0x0000000000000080) std::cout << "\n\t\t- MiniDumpFilterModulePaths";
			if (header.flags & 0x0000000000000100) std::cout << "\n\t\t- MiniDumpWithProcessThreadData";
			if (header.flags & 0x0000000000000200) std::cout << "\n\t\t- MiniDumpWithPrivateReadWriteMemory";
			if (header.flags & 0x0000000000000400) std::cout << "\n\t\t- MiniDumpWithoutOptionalData";
			if (header.flags & 0x0000000000000800) std::cout << "\n\t\t- MiniDumpWithFullMemoryInfo";
			if (header.flags & 0x0000000000001000) std::cout << "\n\t\t- MiniDumpWithThreadInfo";
			if (header.flags & 0x0000000000002000) std::cout << "\n\t\t- MiniDumpWithCodeSegs";
			if (header.flags & 0x0000000000004000) std::cout << "\n\t\t- MiniDumpWithoutAuxiliaryState";
			if (header.flags & 0x0000000000008000) std::cout << "\n\t\t- MiniDumpWithFullAuxiliaryState";
			if (header.flags & 0x0000000000010000) std::cout << "\n\t\t- MiniDumpWithPrivateWriteCopyMemory";
			if (header.flags & 0x0000000000020000) std::cout << "\n\t\t- MiniDumpIgnoreInaccessibleMemory";
			if (header.flags & 0x0000000000040000) std::cout << "\n\t\t- MiniDumpWithTokenInformation";
			if (header.flags & 0x0000000000080000) std::cout << "\n\t\t- MiniDumpWithModuleHeaders";
			if (header.flags & 0x0000000000100000) std::cout << "\n\t\t- MiniDumpFilterTriage";
			if (header.flags & 0xffffffffffe00000) std::cout << "\n\t\t- 0x" + ::to_hex(header.flags & 0xffffffffffe00000);
			std::cout << std::endl;
		}

		dump->timestamp = header.timestamp;

		std::vector<minidump::Stream> streams(header.stream_count);
		CHECK(file.seek(header.stream_list_offset), "Bad stream list offset");
		CHECK(file.read(streams.data(), streams.size() * sizeof(minidump::Stream)), "Couldn't read stream list");
		if (_summary)
		{
			std::cout << "\nMINIDUMP_DIRECTORY: # " << ::to_range(header.stream_list_offset, streams.size() * sizeof(minidump::Stream));
			for (const auto& stream : streams)
				std::cout << "\n\t- " << ::stream_name(stream.type);
			std::cout << std::endl;
		}
		std::sort(streams.begin(), streams.end(), [](const minidump::Stream& a, const minidump::Stream& b) { return a.location.offset < b.location.offset; });

		static const std::map<minidump::Stream::Type, void (Loader::*)(MinidumpData&, File&, const minidump::Stream&)> handlers =
		{
			{ minidump::Stream::Type::ThreadList, &Loader::load_thread_list },
			{ minidump::Stream::Type::ModuleList, &Loader::load_module_list },
			{ minidump::Stream::Type::MemoryList, &Loader::load_memory_list },
			{ minidump::Stream::Type::Memory64List, &Loader::load_memory64_list },
			{ minidump::Stream::Type::Exception, &Loader::load_exception },
			{ minidump::Stream::Type::SystemInfo, &Loader::load_system_info },
			{ minidump::Stream::Type::HandleData, &Loader::load_handle_data },
			{ minidump::Stream::Type::UnloadedModuleList, &Loader::load_unloaded_module_list },
			{ minidump::Stream::Type::MiscInfo, &Loader::load_misc_info },
			{ minidump::Stream::Type::MemoryInfoList, &Loader::load_memory_info_list },
			{ minidump::Stream::Type::ThreadInfoList, &Loader::load_thread_info_list },
			{ minidump::Stream::Type::Tokens, &Loader::load_tokens },
			{ minidump::Stream::Type::SystemMemoryInfo, &Loader::load_system_memory_info },
			{ minidump::Stream::Type::ProcessVmCounters, &Loader::load_vm_counters },
		};

		for (const auto& stream : streams)
		{
			if (stream.type == minidump::Stream::Type::Unused && stream.location.offset == 0 && stream.location.size == 0)
				continue; // This is a placeholder entry.
			const auto i = handlers.find(stream.type);
			if (i != handlers.end())
			{
				(this->*i->second)(*dump, file, stream);
			}
			else
			{
				std::cerr << "WARNING: Skipped stream " << ::stream_name(stream.type)
					<< " (" << stream.location.size << " bytes at 0x" << ::to_hex(stream.location.offset) << ")" << std::endl;
			}
		}

		CHECK(dump->is_32bit, "64-bit dumps are not supported");
		CHECK(_loading_stacks.empty(), "Failed to load all stacks");

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

	void Loader::load_exception(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.exception, "Duplicate exception");

		minidump::ExceptionStream exception;
		CHECK(stream.location.size >= sizeof exception, "Bad exception stream");
		CHECK(file.seek(stream.location.offset), "Bad exception offset");
		CHECK(file.read(exception), "Couldn't read exception");
		check_extra_data(stream, sizeof exception);

		auto&& result = std::make_unique<MinidumpData::Exception>();
		result->thread_id = exception.thread_id;
		result->code = exception.ExceptionRecord.ExceptionCode;
		result->context = ::load_thread_context(file, exception.context);
		if (exception.ExceptionRecord.ExceptionCode == 0xc0000005)
		{
			CHECK_EQ(exception.ExceptionRecord.NumberParameters, 2, "Bad access violation parameter count");
			switch (exception.ExceptionRecord.ExceptionInformation[0])
			{
			case 0:
				result->operation = MinidumpData::Exception::Operation::Reading;
				break;
			case 1:
				result->operation = MinidumpData::Exception::Operation::Writing;
				break;
			case 8:
				result->operation = MinidumpData::Exception::Operation::Executing;
				break;
			default:
				CHECK(false, "Bad access violation access type (0x" << ::to_hex(exception.ExceptionRecord.ExceptionInformation[0]) << ")");
			}
			result->address = exception.ExceptionRecord.ExceptionInformation[1];
			if (dump.is_32bit && result->address >= End32)
				dump.is_32bit = false;
		}

		dump.exception = std::move(result);
	}

	void Loader::load_handle_data(MinidumpData& dump, File& file, const minidump::Stream& stream)
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

	void Loader::load_memory_info_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
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
			case minidump::MemoryInfo::State::Committed:
				m.state = MinidumpData::MemoryRegion::State::Allocated;
				break;
			case minidump::MemoryInfo::State::Reserved:
				m.state = MinidumpData::MemoryRegion::State::Reserved;
				break;
			default:
				CHECK(memory_info.state == minidump::MemoryInfo::State::Free, "Unknown memory state (0x" << ::to_hex(::to_raw(memory_info.state)) << ")");
			}
			switch (memory_info.type)
			{
			case minidump::MemoryInfo::Type::Private:
			case minidump::MemoryInfo::Type::Mapped:
			case minidump::MemoryInfo::Type::Image:
				CHECK(memory_info.state != minidump::MemoryInfo::State::Free, "Bad free memory type (0x" << ::to_hex(::to_raw(memory_info.type)) << ")");
				break;
			default:
				CHECK(memory_info.type == minidump::MemoryInfo::Type::Undefined, "Unknown memory type (0x" << ::to_hex(::to_raw(memory_info.type)) << ")");
				CHECK(memory_info.state == minidump::MemoryInfo::State::Free, "Bad undefined memory state (0x" << ::to_hex(::to_raw(memory_info.state)) << ")");
			}

			if (dump.is_32bit && m.end > End32 && !(_wow64_ntdll
				&& ((memory_info.base == 0x000000007fff0000 && m.end == _wow64_ntdll->first)
					|| (memory_info.base >= _wow64_ntdll->first && m.end <= _wow64_ntdll->second)
					|| (memory_info.base == _wow64_ntdll->second && m.end == 0x00007fffffff0000))))
			{
				// FIXME: Some 32-bit dumps contain weird memory in range 0xfffffffffff00000 - 0xffffffffffff0000.
				if (memory_info.base >= 0xfffffffffff00000)
					continue;
				dump.is_32bit = false;
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
		}
	}

	void Loader::load_memory_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.threads.empty(), "Loading memory/memory64 list before thread list is not supported");
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
			m.end = uint64_t{memory_range.base} + memory_range.location.size;
			CHECK(m.end <= End32, "Bad memory list");
			dump.memory.emplace(memory_range.base, std::move(m));

			for (auto i = _loading_stacks.begin(); i != _loading_stacks.end(); )
			{
				const auto stack_base = std::get<0>(*i);
				const auto stack_end = std::get<1>(*i);
				if (stack_base >= memory_range.base && stack_end <= memory_range.base + memory_range.location.size)
				{
					CHECK(file.seek(memory_range.location.offset + (stack_base - memory_range.base)), "Alarm!");
					CHECK(file.read(std::get<2>(*i), stack_end - stack_base), "Alarm!");
					i = _loading_stacks.erase(i);
				}
				else
					++i;
			}
		}
	}

	void Loader::load_memory64_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.threads.empty(), "Loading memory/memory64 list before thread list is not supported");
		CHECK(!dump.modules.empty(), "Loading memory/memory64 list before module list is not supported");
		CHECK(dump.memory.empty(), "Duplicate memory/memory64 list");

		minidump::Memory64ListHeader header;
		CHECK(stream.location.size >= sizeof header, "Bad memory/memory64 list stream");
		CHECK(file.seek(stream.location.offset), "Bad memory/memory64 list offset");
		CHECK(file.read(header), "Couldn't read memory/memory64 list header");
		CHECK_GE(header.entry_count, 0, "Bad memory/memory64 list size");

		std::vector<minidump::Memory64Range> memory(header.entry_count);
		CHECK(file.read(memory.data(), memory.size() * sizeof(minidump::Memory64Range)), "Couldn't read memory/memory64 list");
		auto offset = header.offset;
		for (const auto& memory_range : memory)
		{
			MinidumpData::MemoryInfo m;
			m.end = memory_range.base + memory_range.size;
			if (dump.is_32bit && m.end > End32 && !(_wow64_ntdll && memory_range.base >= _wow64_ntdll->first && m.end <= _wow64_ntdll->second))
			{
				// FIXME: Some 32-bit dumps contain weird memory in range 0xfffffffffff00000 - 0xffffffffffff0000.
				if (memory_range.base >= 0xfffffffffff00000)
					continue;
				dump.is_32bit = false;
			}
			dump.memory.emplace(memory_range.base, std::move(m));

			for (auto i = _loading_stacks.begin(); i != _loading_stacks.end(); )
			{
				const auto stack_base = std::get<0>(*i);
				const auto stack_end = std::get<1>(*i);
				if (stack_base >= memory_range.base && stack_end <= memory_range.base + memory_range.size)
				{
					CHECK(file.seek(offset + (stack_base - memory_range.base)), "Bad stack data");
					CHECK(file.read(std::get<2>(*i), stack_end - stack_base), "Couldn't read stack data");
					i = _loading_stacks.erase(i);
				}
				else
					++i;
			}

			offset += memory_range.size;
		}
	}

	void Loader::load_misc_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		minidump::MiscInfo5 misc_info;
		CHECK(stream.location.size == sizeof(minidump::MiscInfo)
			|| stream.location.size == sizeof(minidump::MiscInfo2)
			|| stream.location.size == sizeof(minidump::MiscInfo3)
			|| stream.location.size == sizeof(minidump::MiscInfo4)
			|| stream.location.size >= sizeof(minidump::MiscInfo5), "Bad misc info stream");
		CHECK(file.seek(stream.location.offset), "Bad misc info offset");
		CHECK(file.read(&misc_info, std::min(stream.location.size, sizeof misc_info)), "Couldn't read misc info");
		check_extra_data(stream, sizeof misc_info);

		if (_summary)
		{
			switch (stream.location.size)
			{
			case sizeof(minidump::MiscInfo): std::cout << "\nMINIDUMP_MISC_INFO"; break;
			case sizeof(minidump::MiscInfo2): std::cout << "\nMINIDUMP_MISC_INFO_2"; break;
			case sizeof(minidump::MiscInfo3): std::cout << "\nMINIDUMP_MISC_INFO_3"; break;
			case sizeof(minidump::MiscInfo4): std::cout << "\nMINIDUMP_MISC_INFO_4"; break;
			case sizeof(minidump::MiscInfo5): std::cout << "\nMINIDUMP_MISC_INFO_5"; break;
			default: std::cout << "\nMINIDUMP_MISC_INFO_5+"; break;
			}
			std::cout << ": # " << ::to_range(stream.location.offset, std::min(stream.location.size, sizeof misc_info));
			std::cout << "\n\tFlags1: 0x" << ::to_hex(misc_info.flags);
			if (misc_info.flags & 0x00000001) std::cout << "\n\t\t- MINIDUMP_MISC1_PROCESS_ID";
			if (misc_info.flags & 0x00000002) std::cout << "\n\t\t- MINIDUMP_MISC1_PROCESS_TIMES";
			if (misc_info.flags & 0x00000004) std::cout << "\n\t\t- MINIDUMP_MISC1_PROCESSOR_POWER_INFO";
			if (misc_info.flags & 0x00000010) std::cout << "\n\t\t- MINIDUMP_MISC3_PROCESS_INTEGRITY";
			if (misc_info.flags & 0x00000020) std::cout << "\n\t\t- MINIDUMP_MISC3_PROCESS_EXECUTE_FLAGS";
			if (misc_info.flags & 0x00000040) std::cout << "\n\t\t- MINIDUMP_MISC3_TIMEZONE";
			if (misc_info.flags & 0x00000080) std::cout << "\n\t\t- MINIDUMP_MISC3_PROTECTED_PROCESS";
			if (misc_info.flags & 0x00000100) std::cout << "\n\t\t- MINIDUMP_MISC4_BUILDSTRING";
			if (misc_info.flags & 0x00000200) std::cout << "\n\t\t- MINIDUMP_MISC5_PROCESS_COOKIE";
			if (misc_info.flags & 0xfffffc08) std::cout << "\n\t\t- 0x" << ::to_hex(misc_info.flags & 0xfffffc08);
			if (misc_info.flags & minidump::MiscInfo::ProcessId)
				std::cout << "\n\tProcessId: " << misc_info.process_id;
			if (misc_info.flags & minidump::MiscInfo::ProcessTimes)
			{
				std::cout << "\n\tProcessCreateTime: " << ::time_t_to_string(misc_info.process_create_time);
				std::cout << " # Uptime: " << ::seconds_to_string(dump.timestamp - misc_info.process_create_time);
				std::cout << "\n\tProcessUserTime: " << ::seconds_to_string(misc_info.process_user_time);
				std::cout << "\n\tProcessKernelTime: " << ::seconds_to_string(misc_info.process_kernel_time);
			}
			if (stream.location.size >= sizeof(minidump::MiscInfo2))
			{
				if (misc_info.flags & minidump::MiscInfo2::ProcessorPowerInfo)
				{
					std::cout << "\n\tProcessorMaxMhz: " << misc_info.processor_max_mhz;
					std::cout << "\n\tProcessorCurrentMhz: " << misc_info.processor_current_mhz;
					std::cout << "\n\tProcessorMhzLimit: " << misc_info.processor_mhz_limit;
					std::cout << "\n\tProcessorMaxIdleState: 0x" << ::to_hex(misc_info.processor_max_idle_state);
					std::cout << "\n\tProcessorCurrentIdleState: 0x" << ::to_hex(misc_info.processor_current_idle_state);
				}
			}
			if (stream.location.size >= sizeof(minidump::MiscInfo3))
			{
				if (misc_info.flags & minidump::MiscInfo3::ProcessIntegrity)
					std::cout << "\n\tProcessIntegrityLevel: 0x" << ::to_hex(misc_info.process_integrity_level);
				if (misc_info.flags & minidump::MiscInfo3::ProcessExecuteFlags)
					std::cout << "\n\tProcessExecuteFlags: 0x" << ::to_hex(misc_info.process_execute_flags);
				if (misc_info.flags & minidump::MiscInfo3::ProtectedProcess)
					std::cout << "\n\tProtectedProcess: 0x" << ::to_hex(misc_info.protected_process);
				// TODO: Time zone information.
			}
			if (stream.location.size >= sizeof(minidump::MiscInfo4))
			{
				if (misc_info.flags & minidump::MiscInfo4::BuildString)
				{
					std::cout << "\n\tBuildString: \"" << ::to_ascii(misc_info.build_string) << "\"";
					std::cout << "\n\tDbgBldStr: \"" << ::to_ascii(misc_info.debug_build_string) << "\"";
				}
			}
			if (stream.location.size >= sizeof(minidump::MiscInfo5))
			{
				// TODO: XState.
				if (misc_info.flags & minidump::MiscInfo5::ProcessCookie)
					std::cout << "\n\tProcessCookie: 0x" << ::to_hex(misc_info.process_cookie);
			}
			std::cout << std::endl;
		}
	}

	void Loader::load_module_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		const auto version_to_string = [](const uint16_t (&parts)[4]) -> std::string
		{
			std::array<char, 24> buffer;
			::snprintf(buffer.data(), buffer.size(), "%d.%d.%d.%d", parts[1], parts[0], parts[3], parts[2]);
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
			if (!module.version_info.signature && !module.version_info.version)
				continue; // No version information is present.
			CHECK_EQ(module.version_info.signature, minidump::Module::VersionInfo::Signature, "Bad module version signature");
			CHECK_EQ(module.version_info.version, minidump::Module::VersionInfo::Version, "Bad module version version");
		}

		for (const auto& module : modules)
		{
			MinidumpData::Module m;
			m.file_path = ::to_ascii(::read_string(file, module.name_offset));
			m.file_name = m.file_path.substr(m.file_path.find_last_of('\\') + 1);
			if (module.version_info.signature)
			{
				m.file_version = version_to_string(module.version_info.file_version);
				m.product_version = version_to_string(module.version_info.product_version);
			}
			m.timestamp = ::time_t_to_string(module.timestamp);
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
			dump.memory_usage.all_images += m.image_end - m.image_base;
			dump.memory_usage.max_image = std::max<uint64_t>(dump.memory_usage.max_image, m.image_end - m.image_base);
			if (m.image_base == 0x00007ff876fa0000 && m.file_name == "ntdll.dll")
			{
				CHECK(!_wow64_ntdll, "Duplicate WoW64 ntdll.dll");
				_wow64_ntdll = std::make_unique<std::pair<uint64_t, uint64_t>>(m.image_base, m.image_end);
			}
			else if (dump.is_32bit && m.image_end > End32)
				dump.is_32bit = true;
			dump.modules.emplace_back(std::move(m));
		}
	}

	void Loader::load_system_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		minidump::SystemInfo system_info;
		CHECK(stream.location.size >= sizeof system_info, "Bad system info stream");
		CHECK(file.seek(stream.location.offset), "Bad system info offset");
		CHECK(file.read(system_info), "Couldn't read system info");
		check_extra_data(stream, sizeof system_info);

		if (_summary)
		{
			std::cout << "\nMINIDUMP_SYSTEM_INFO: # " << ::to_range(stream.location.offset, sizeof system_info)
				<< "\n\tProcessorArchitecture: ";
			if (system_info.cpu_architecture == minidump::SystemInfo::X86)
				std::cout << "PROCESSOR_ARCHITECTURE_INTEL";
			else if (system_info.cpu_architecture == minidump::SystemInfo::X64)
				std::cout << "PROCESSOR_ARCHITECTURE_AMD64";
			else
				std::cout << "0x" << ::to_hex(::to_raw(system_info.cpu_architecture));
			std::cout << "\n\tProcessorLevel: " << system_info.cpu_family;
			std::cout << "\n\tProcessorRevision: 0x" << ::to_hex(system_info.ProcessorRevision);
			if (system_info.cpu_architecture == minidump::SystemInfo::X86 || system_info.cpu_architecture == minidump::SystemInfo::X64)
				std::cout << " # Model " << (system_info.ProcessorRevision >> 8) << ", Stepping " << (system_info.ProcessorRevision & 0xff);
			if (system_info.cpu_cores == 0 && system_info.product_type == 0)
				std::cout << "\n\tReserved0: 0x0000";
			else
			{
				std::cout << "\n\tNumberOfProcessors: " << unsigned{system_info.cpu_cores};
				std::cout << "\n\tProductType: ";
				switch (system_info.product_type)
				{
				case 1: std::cout << "VER_NT_WORKSTATION"; break;
				case 2: std::cout << "VER_NT_DOMAIN_CONTROLLER"; break;
				case 3: std::cout << "VER_NT_SERVER"; break;
				default: std::cout << unsigned{system_info.product_type}; break;
				}
			}
			std::cout << "\n\tMajorVersion+MinorVersion+BuildNumber: "
				<< std::to_string(system_info.major_version)
				<< "." << std::to_string(system_info.minor_version)
				<< "." << std::to_string(system_info.build_number);
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
					std::cout << " # " << ((system_info.product_type == minidump::SystemInfo::Server) ? std::get<3>(*i) : std::get<2>(*i));
			}
			std::cout << "\n\tPlatformId: ";
			if (system_info.platform_id == minidump::SystemInfo::WindowsNt)
				std::cout << "VER_PLATFORM_WIN32_NT";
			else
				std::cout << "0x" << ::to_hex(system_info.platform_id);
			std::cout << "\n\tCSDVersion: \"";
			try
			{
				std::cout << ::to_ascii(::read_string(file, system_info.service_pack_name_offset));
			}
			catch (const BadCheck& e)
			{
				std::cerr << "ERROR: Couldn't read OS service pack: " << e.what() << std::endl;
			}
			std::cout << "\"";
			std::cout << "\n\tSuiteMask: 0x" << ::to_hex(system_info.suite_mask);
			std::cout << "\n\tReserved2: 0x" << ::to_hex(system_info._reserved);
			if (system_info.cpu_architecture == minidump::SystemInfo::X86)
			{
				std::cout << "\n\tX86CpuInfo:"
					<< "\n\t\tVendorId: \"" << std::string(reinterpret_cast<const char*>(system_info.cpu.x86.vendor_id), sizeof system_info.cpu.x86.vendor_id) << "\""
					<< "\n\t\tVersionInformation: 0x" << ::to_hex(system_info.cpu.x86.VersionInformation)
					<< "\n\t\tFeatureInformation: 0x" << ::to_hex(system_info.cpu.x86.FeatureInformation)
					<< "\n\t\tAMDExtendedCpuFeatures: 0x" << ::to_hex(system_info.cpu.x86.AMDExtendedCpuFeatures);
			}
			else
			{
				std::cout << "\n\tOtherCpuInfo:";
				for (const auto feature : system_info.cpu.other.features)
					std::cout << "\n\t\t- 0x" << ::to_hex(feature);
			}
			std::cout << std::endl;
		}

		CHECK(system_info.cpu_architecture != minidump::SystemInfo::Unknown, "Unknown CPU architecture");
		CHECK(system_info.cpu_architecture == minidump::SystemInfo::X86
			|| system_info.cpu_architecture == minidump::SystemInfo::X64, "Unsupported CPU architecture: " << system_info.cpu_architecture);
	}

	void Loader::load_system_memory_info(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		minidump::SystemMemoryInfo1 system_memory_info;
		CHECK(stream.location.size == sizeof system_memory_info, "Bad SystemMemoryInfoStream");
		CHECK(file.seek(stream.location.offset), "Bad SystemMemoryInfoStream offset");
		CHECK(file.read(system_memory_info), "Couldn't read SystemMemoryInfoStream");
		CHECK_EQ(system_memory_info.revision, minidump::SystemMemoryInfo1::Revision, "Unsupported SystemMemoryInfoStream revision");

		if (_summary)
		{
			std::cout
				<< "\nMINIDUMP_SYSTEM_MEMORY_INFO_1: # " << ::to_range(stream.location.offset, sizeof system_memory_info)
				<< "\n\tRevision: " << system_memory_info.revision
				<< "\n\tFlags: 0x" << ::to_hex(system_memory_info.flags);
			if (system_memory_info.flags & 0x0001) std::cout << "\n\t\t- MINIDUMP_SYSMEMINFO1_FILECACHE_TRANSITIONREPURPOSECOUNT_FLAGS";
			if (system_memory_info.flags & 0x0002) std::cout << "\n\t\t- MINIDUMP_SYSMEMINFO1_BASICPERF";
			if (system_memory_info.flags & 0x0004) std::cout << "\n\t\t- MINIDUMP_SYSMEMINFO1_PERF_CCTOTALDIRTYPAGES_CCDIRTYPAGETHRESHOLD";
			if (system_memory_info.flags & 0x0008) std::cout << "\n\t\t- MINIDUMP_SYSMEMINFO1_PERF_RESIDENTAVAILABLEPAGES_SHAREDCOMMITPAGES";
			if (system_memory_info.flags & 0xfff0) std::cout << "\n\t\t- 0x" << ::to_hex(system_memory_info.flags & 0xfff0u);
			std::cout
				<< "\n\tBasicInfo:"
				<< "\n\t\tTimerResolution: " << system_memory_info.basic_info.TimerResolution
				<< "\n\t\tPageSize: " << ::to_human_readable(system_memory_info.basic_info.PageSize)
				<< "\n\t\tNumberOfPhysicalPages: " << system_memory_info.basic_info.NumberOfPhysicalPages
					<< " # " << ::to_human_readable(uint64_t{system_memory_info.basic_info.NumberOfPhysicalPages} * system_memory_info.basic_info.PageSize)
				<< "\n\t\tLowestPhysicalPageNumber: " << system_memory_info.basic_info.LowestPhysicalPageNumber
				<< "\n\t\tHighestPhysicalPageNumber: " << system_memory_info.basic_info.HighestPhysicalPageNumber
				<< "\n\t\tAllocationGranularity: " << ::to_human_readable(system_memory_info.basic_info.AllocationGranularity)
				<< "\n\t\tMinimumUserModeAddress: 0x" << ::to_hex(system_memory_info.basic_info.MinimumUserModeAddress)
				<< "\n\t\tMaximumUserModeAddress: 0x" << ::to_hex(system_memory_info.basic_info.MaximumUserModeAddress)
					<< " # " << ::to_human_readable(system_memory_info.basic_info.MaximumUserModeAddress + 1 - system_memory_info.basic_info.MinimumUserModeAddress)
				<< "\n\t\tActiveProcessorsAffinityMask: 0x" << ::to_hex(system_memory_info.basic_info.ActiveProcessorsAffinityMask)
				<< "\n\t\tNumberOfProcessors: " << system_memory_info.basic_info.NumberOfProcessors;
			// TODO: FileCacheInfo.
			if (system_memory_info.flags & minidump::SystemMemoryInfo1::BasicPerf)
				std::cout
					<< "\n\tBasicPerfInfo:"
					<< "\n\t\tAvailablePages: " << ::to_human_readable(system_memory_info.basic_perf_info.AvailablePages)
					<< "\n\t\tCommittedPages: " << ::to_human_readable(system_memory_info.basic_perf_info.CommittedPages)
					<< "\n\t\tCommitLimit: " << ::to_human_readable(system_memory_info.basic_perf_info.CommitLimit)
					<< "\n\t\tPeakCommitment: " << ::to_human_readable(system_memory_info.basic_perf_info.PeakCommitment);
			// TODO: PerfInfo.
			std::cout << std::endl;
		}
	}

	void Loader::load_thread_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
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
			t.context = ::load_thread_context(file, thread.context);

			t.stack.reset(new uint8_t[t.stack_end - t.stack_base]);
			if (thread.stack.location.offset)
			{
				CHECK(file.seek(thread.stack.location.offset), "Bad thread " << index << " stack offset");
				CHECK(file.read(t.stack.get(), t.stack_end - t.stack_base), "Couldn't read thread " << index << " stack");
			}
			else
			{
				_loading_stacks.emplace_back(t.stack_base, t.stack_end, t.stack.get());
			}

			dump.memory_usage.all_stacks += t.stack_base + t.stack_end;
			dump.memory_usage.max_stack = std::max<uint64_t>(dump.memory_usage.max_stack, t.stack_base + t.stack_end);
			if (dump.is_32bit && t.stack_end > End32)
				dump.is_32bit = false;
			dump.threads.emplace_back(std::move(t));
		}
	}

	void Loader::load_thread_info_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		CHECK(!dump.threads.empty(), "Loading thread info before thread list is not supported");

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
			CHECK_EQ(thread_info.dump_flags & ~minidump::ThreadInfo::WritingThread, 0, "Unsupported thread flags");

			const auto j = std::find_if(dump.threads.begin(), dump.threads.end(), [&thread_info](const auto& thread)
			{
				return thread.id == thread_info.thread_id;
			});
			CHECK(j != dump.threads.end(), "Found thread info for unknown thread 0x" << ::to_hex(thread_info.thread_id));
			j->start_address = thread_info.start_address;

			if (dump.is_32bit && j->start_address >= End32)
				dump.is_32bit = false;
		}
	}

	void Loader::load_tokens(MinidumpData&, File& file, const minidump::Stream& stream)
	{
		minidump::TokenInfoListHeader header;
		CHECK_GE(stream.location.size, sizeof header, "Bad token info list stream");
		CHECK(file.seek(stream.location.offset), "Bad token info list offset");
		CHECK(file.read(header), "Couldn't read token info list header");
		CHECK_EQ(header.total_size, stream.location.size, "Bad token stream header");
		CHECK_GE(header.header_size, sizeof header, "Bad token stream header");

		minidump::TokenInfoHeader token_info_header;
		CHECK_GE(header.entry_header_size, sizeof token_info_header, "Bad token entry header size");
		auto base = stream.location.offset + header.header_size;
		for (uint32_t i = 0; i < header.entry_count; ++i)
		{
			CHECK(file.seek(base), "Bad token stream");
			CHECK(file.read(token_info_header), "Couldn't read token entry header");
		}
	}

	void Loader::load_unloaded_module_list(MinidumpData& dump, File& file, const minidump::Stream& stream)
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
			if (dump.is_32bit && m.image_end > End32)
				dump.is_32bit = false;
			dump.unloaded_modules.emplace_back(std::move(m));
		}
	}

	void Loader::load_vm_counters(MinidumpData& dump, File& file, const minidump::Stream& stream)
	{
		union
		{
			minidump::VmCounters1 vm_counters;
			minidump::VmCounters2 vm_counters2;
		};
		CHECK(file.seek(stream.location.offset), "Bad ProcessVmCountersStream offset");
		switch (stream.location.size)
		{
		case sizeof(minidump::VmCounters1):
			CHECK(file.read(vm_counters), "Couldn't read ProcessVmCountersStream");
			CHECK_EQ(vm_counters.revision, minidump::VmCounters1::Revision, "Unsupported ProcessVmCountersStream revision " << vm_counters.revision);
			CHECK(vm_counters.flags == 0, "Unsupported ProcessVmCountersStream flags 0x" << ::to_hex(vm_counters.flags));
			break;
		case sizeof(minidump::VmCounters2):
			CHECK(file.read(vm_counters2), "Couldn't read ProcessVmCountersStream");
			CHECK_EQ(vm_counters.revision, minidump::VmCounters2::Revision, "Unsupported ProcessVmCountersStream revision " << vm_counters.revision);
			break;
		default:
			CHECK(false, "Bad ProcessVmCountersStream size " << stream.location.size);
		}

		if (_summary)
		{
			if (vm_counters.revision == minidump::VmCounters1::Revision)
				std::cout << "\nMINIDUMP_PROCESS_VM_COUNTERS_1";
			else
				std::cout << "\nMINIDUMP_PROCESS_VM_COUNTERS_2";
			std::cout << ": # " << ::to_range(stream.location.offset, stream.location.size);
			std::cout << "\n\tRevision: " << vm_counters.revision;
			if (vm_counters.revision == minidump::VmCounters2::Revision)
			{
				std::cout << "\n\tFlags: 0x" << ::to_hex(vm_counters.flags);
				if (vm_counters.flags & 0x0001) std::cout << "\n\t\t- MINIDUMP_PROCESS_VM_COUNTERS";
				if (vm_counters.flags & 0x0002) std::cout << "\n\t\t- MINIDUMP_PROCESS_VM_COUNTERS_VIRTUALSIZE";
				if (vm_counters.flags & 0x0004) std::cout << "\n\t\t- MINIDUMP_PROCESS_VM_COUNTERS_EX";
				if (vm_counters.flags & 0x0008) std::cout << "\n\t\t- MINIDUMP_PROCESS_VM_COUNTERS_EX2";
				if (vm_counters.flags & 0x0010) std::cout << "\n\t\t- MINIDUMP_PROCESS_VM_COUNTERS_JOB";
			}
			if (vm_counters.revision == minidump::VmCounters1::Revision
				|| (vm_counters.revision == minidump::VmCounters2::Revision && vm_counters.flags & minidump::VmCounters2::Basic))
			{
				std::cout << "\n\tPageFaultCount: " << vm_counters.page_fault_count;
				std::cout << "\n\tPeakWorkingSetSize: " << ::to_human_readable(vm_counters.peak_working_set_size);
				std::cout << "\n\tWorkingSetSize: " << ::to_human_readable(vm_counters.working_set_size);
				std::cout << "\n\tQuotaPeakPagedPoolUsage: " << ::to_human_readable(vm_counters.peak_paged_pool_usage);
				std::cout << "\n\tQuotaPagedPoolUsage: " << ::to_human_readable(vm_counters.paged_pool_usage);
				std::cout << "\n\tQuotaPeakNonPagedPoolUsage: " << ::to_human_readable(vm_counters.peak_non_paged_pool_usage);
				std::cout << "\n\tQuotaNonPagedPoolUsage: " << ::to_human_readable(vm_counters.non_paged_pool_usage);
				std::cout << "\n\tPagefileUsage: " << ::to_human_readable(vm_counters.page_file_usage);
				std::cout << "\n\tPeakPagefileUsage: " << ::to_human_readable(vm_counters.peak_page_file_usage);
			}
			if (vm_counters.revision == minidump::VmCounters2::Revision && vm_counters.flags & minidump::VmCounters2::VirtualSize)
			{
				std::cout << "\n\tPeakVirtualSize: " << ::to_human_readable(vm_counters2.peak_virtual_size);
				std::cout << "\n\tVirtualSize: " << ::to_human_readable(vm_counters2.virtual_size);
			}
			if (vm_counters.revision == minidump::VmCounters1::Revision)
				std::cout << "\n\tPrivateUsage: " << ::to_human_readable(vm_counters.private_usage);
			else if (vm_counters.revision == minidump::VmCounters2::Revision && vm_counters.flags & minidump::VmCounters2::Ex)
				std::cout << "\n\tPrivateUsage: " << ::to_human_readable(vm_counters2.private_usage);
			if (vm_counters.revision == minidump::VmCounters2::Revision && vm_counters.flags & minidump::VmCounters2::Ex2)
			{
				std::cout << "\n\tPrivateWorkingSetSize: " << ::to_human_readable(vm_counters2.private_working_set_size);
				std::cout << "\n\tSharedCommitUsage: " << ::to_human_readable(vm_counters2.shared_commit_usage);
			}
			if (vm_counters.revision == minidump::VmCounters2::Revision && vm_counters.flags & minidump::VmCounters2::Job)
			{
				std::cout << "\n\tJobSharedCommitUsage: " << ::to_human_readable(vm_counters2.job_shared_commit_usage);
				std::cout << "\n\tJobPrivateCommitUsage: " << ::to_human_readable(vm_counters2.job_private_commit_usage);
				std::cout << "\n\tJobPeakPrivateCommitUsage: " << ::to_human_readable(vm_counters2.job_peak_private_commit_usage);
				std::cout << "\n\tJobPrivateCommitLimit: " << ::to_human_readable(vm_counters2.job_private_commit_limit);
				std::cout << "\n\tJobTotalCommitLimit: " << ::to_human_readable(vm_counters2.job_total_commit_limit);
			}
			std::cout << std::endl;
		}
	}
}

std::unique_ptr<MinidumpData> MinidumpData::load(const std::string& file_name, bool summary)
{
	return Loader(summary).load(file_name);
}

std::string MinidumpData::Exception::to_string(bool is_32bit) const
{
	std::string result = "[0x" + ::to_hex(code) + "]";
	switch (code)
	{
	case 0xc0000005:
		result += " Access violation";
		switch (operation)
		{
		case Operation::Reading:
			result += " reading";
			break;
		case Operation::Writing:
			result += " writing";
			break;
		case Operation::Executing:
			result += " executing";
			break;
		default:
			throw std::logic_error("Bad access violation operation");
		}
		result += " 0x" + ::to_hex(address, is_32bit);
		break;
	case 0xe06d7363:
		result += " Unhandled C++ exception";
		break;
	}
	return result;
}

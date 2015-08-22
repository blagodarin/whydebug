#include "minidump.h"
#include "check.h"
#include "file.h"
#include "minidump_format.h"
#include "minidump_read.h"
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
			read_thread_list(*this, file, stream);
			break;
		case ModuleListStream:
			read_module_list(*this, file, stream);
			break;
		case MiscInfoStream:
			read_misc_info(*this, file, stream);
			break;
		case ThreadInfoListStream:
			read_thread_info_list(*this, file, stream);
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
				<< " (calculated uptime: " << ::seconds_to_string(timestamp - process_create_time) << ")\n"
			<< "Process user time: " << ::seconds_to_string(process_user_time) << "\n"
			<< "Process kernel time: " << ::seconds_to_string(process_kernel_time) << "\n";
	}
}

void Minidump::print_threads(std::ostream& stream)
{
	std::vector<std::vector<std::string>> table;
	table.push_back({"#", "ID", "STACK", "START", "CURRENT", "ESP"});
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
			decode_code_address(thread.context.x86.eip),
			::to_hex(thread.context.x86.esp)
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

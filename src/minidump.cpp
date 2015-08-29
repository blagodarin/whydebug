#include "minidump.h"
#include "minidump_data.h"
#include "table.h"
#include "utils.h"
#include <algorithm>
#include <iostream>

namespace
{
	std::string decode_code_address(const MinidumpData& dump, uint64_t address)
	{
		if (!address)
			return {};
		std::string result = ::to_hex(address, dump.is_32bit);
		const auto i = std::find_if(dump.modules.begin(), dump.modules.end(), [address](const auto& module)
		{
			return address >= module.image_base && address < module.image_end;
		});
		if (i != dump.modules.end())
			result = i->file_name + "!" + result;
		return std::move(result);
	}

	std::vector<std::pair<uint32_t, uint32_t>> build_call_chain(const MinidumpData::Thread& thread)
	{
		std::vector<std::pair<uint32_t, uint32_t>> chain;
		auto ebp = thread.context.x86.ebp;
		chain.emplace_back(ebp, thread.context.x86.eip);
		while (ebp >= thread.stack_base && ebp + 2 * sizeof(ebp) < thread.stack_end)
		{
			const auto stack_offset = ebp - thread.stack_base;
			const auto return_address = reinterpret_cast<uint32_t&>(thread.stack[stack_offset + sizeof ebp]);
			ebp = reinterpret_cast<uint32_t&>(thread.stack[stack_offset]);
			chain.emplace_back(ebp, return_address);
		}
		return chain;
	}

	Table print_call_stack(const MinidumpData& dump, const MinidumpData::Thread& thread)
	{
		if (!thread.start_address || !thread.context.x86.eip || !thread.context.x86.ebp)
			return {};
		Table table({{"EBP"}, {"FUNCTION"}});
		for (const auto& entry : build_call_chain(thread))
		{
			table.push_back({
				::to_hex(entry.first, dump.is_32bit),
				::to_hex(entry.second, dump.is_32bit),
				decode_code_address(dump, entry.second),
			});
		}
		return std::move(table);
	}
}

Minidump::Minidump(const std::string& file_name)
	: _data(MinidumpData::load(file_name))
{
}

Minidump::~Minidump()
{
}

Table Minidump::print_exception_call_stack() const
{
	if (!_data->exception)
		return {};
	return ::print_call_stack(*_data, *_data->exception->thread);
}

Table Minidump::print_memory() const
{
	const auto usage_to_string = [this](const MinidumpData::MemoryInfo& memory_info) -> std::string
	{
		switch (memory_info.usage)
		{
		case MinidumpData::MemoryInfo::Usage::Image:
			return _data->modules[memory_info.usage_index - 1].file_name;
		case MinidumpData::MemoryInfo::Usage::Stack:
			return "< stack " + std::to_string(memory_info.usage_index) + " >";
		default:
			return {};
		}
	};

	Table table({{"BASE"}, {"END"}, {"SIZE", Table::Alignment::Right}, {"USAGE"}});
	table.reserve(_data->memory.size());
	for (const auto& memory_range : _data->memory)
	{
		table.push_back({
			::to_hex(memory_range.first, _data->is_32bit),
			::to_hex(memory_range.second.end, _data->is_32bit),
			::to_hex_min(memory_range.second.end - memory_range.first),
			usage_to_string(memory_range.second),
		});
	}
	return std::move(table);
}

Table Minidump::print_memory_regions() const
{
	const auto state_to_string = [this](MinidumpData::MemoryRegion::State state) -> std::string
	{
		switch (state)
		{
		case MinidumpData::MemoryRegion::State::Free:
			return "Free";
		case MinidumpData::MemoryRegion::State::Reserved:
			return "Reserved";
		case MinidumpData::MemoryRegion::State::Allocated:
			return "Allocated";
		default:
			return {};
		}
	};

	Table table({{"BASE"}, {"END"}, {"SIZE", Table::Alignment::Right}, {"STATE"}});
	table.reserve(_data->memory_regions.size());
	for (const auto& memory_region : _data->memory_regions)
	{
		table.push_back({
			::to_hex(memory_region.first, _data->is_32bit),
			::to_hex(memory_region.second.end, _data->is_32bit),
			::to_hex_min(memory_region.second.end - memory_region.first),
			state_to_string(memory_region.second.state),
		});
	}
	return std::move(table);
}

Table Minidump::print_modules() const
{
	Table table({{"#", Table::Alignment::Right}, {"NAME"}, {"VERSION"}, {"IMAGE"}, {"END"}, {"SIZE", Table::Alignment::Right}, {"PDB"}});
	table.reserve(_data->modules.size());
	for (const auto& module : _data->modules)
	{
		table.push_back({
			std::to_string(&module - &_data->modules.front() + 1),
			module.file_name,
			module.product_version,
			::to_hex(module.image_base, _data->is_32bit),
			::to_hex(module.image_end, _data->is_32bit),
			::to_hex_min(module.image_end - module.image_base),
			module.pdb_name,
		});
	}
	return std::move(table);
}

Table Minidump::print_summary() const
{
	Table table({{""}, {""}});
	table.push_back({"Timestamp:", ::time_t_to_string(_data->timestamp)});
	if (_data->process_id)
		table.push_back({"Process ID:", std::to_string(_data->process_id)});
	if (_data->process_create_time)
	{
		table.push_back({"Process creation time:", ::time_t_to_string(_data->process_create_time)});
		table.push_back({"Calculated uptime:", ::seconds_to_string(_data->timestamp - _data->process_create_time)});
		table.push_back({"Process user time:", ::seconds_to_string(_data->process_user_time)});
		table.push_back({"Process kernel time:", ::seconds_to_string(_data->process_kernel_time)});
	}
	if (_data->system_info)
	{
		table.push_back({"Number of processors:", std::to_string(_data->system_info->processors)});
		if (!_data->system_info->version_name.empty())
			table.push_back({"System version name:", _data->system_info->version_name});
	}
	return std::move(table);
}

Table Minidump::print_thread_call_stack(unsigned long thread_index) const
{
	if (thread_index == 0 || thread_index > _data->threads.size())
		throw std::invalid_argument("Bad thread " + std::to_string(thread_index));
	return ::print_call_stack(*_data, _data->threads[thread_index - 1]);
}

Table Minidump::print_threads() const
{
	Table table({{"#", Table::Alignment::Right}, {"ID"}, {"STACK"}, {"END"}, {"START"}, {"CURRENT"}});
	table.reserve(_data->threads.size());
	for (const auto& thread : _data->threads)
	{
		table.push_back({
			std::to_string(&thread - &_data->threads.front() + 1),
			::to_hex(thread.id),
			::to_hex(thread.stack_base, _data->is_32bit),
			::to_hex(thread.stack_end, _data->is_32bit),
			decode_code_address(*_data, thread.start_address),
			decode_code_address(*_data, thread.context.x86.eip),
		});
	}
	return std::move(table);
}

Table Minidump::print_unloaded_modules() const
{
	Table table({{"#", Table::Alignment::Right}, {"NAME"}, {"IMAGE"}, {"END"}, {"SIZE", Table::Alignment::Right}});
	table.reserve(_data->unloaded_modules.size());
	for (const auto& module : _data->unloaded_modules)
	{
		table.push_back({
			std::to_string(&module - &_data->unloaded_modules.front() + 1),
			module.file_name,
			::to_hex(module.image_base, _data->is_32bit),
			::to_hex(module.image_end, _data->is_32bit),
			::to_hex_min(module.image_end - module.image_base),
		});
	}
	return std::move(table);
}

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
		const auto i = std::find_if(dump.modules.begin(), dump.modules.end(), [address](const auto& module)
		{
			return address >= module.image_base && address < module.image_end;
		});
		return i != dump.modules.end()
			? i->file_name + "!" + ::to_hex(address, dump.is_32bit)
			: ::to_hex(address, dump.is_32bit);
	}

	std::vector<std::pair<uint32_t, uint32_t>> build_call_chain(const MinidumpData::Thread& thread, const MinidumpData::Exception* exception)
	{
		std::vector<std::pair<uint32_t, uint32_t>> chain;
		auto ebp = exception ? exception->context->x86.ebp : thread.context->x86.ebp;
		chain.emplace_back(ebp, exception ? exception->context->x86.eip : thread.context->x86.eip);
		while (ebp >= thread.stack_base && ebp + 8 < thread.stack_end)
		{
			const auto stack_offset = ebp - thread.stack_base;
			const auto return_address = reinterpret_cast<uint32_t&>(thread.stack[stack_offset + 4]);
			ebp = reinterpret_cast<uint32_t&>(thread.stack[stack_offset]);
			chain.emplace_back(ebp, return_address);
		}
		return chain;
	}

	Table print_call_stack(const MinidumpData& dump, const MinidumpData::Thread& thread, const MinidumpData::Exception* exception)
	{
		if (!thread.start_address || !thread.context->x86.eip || !thread.context->x86.ebp)
			return {};
		if (exception && exception->thread_id == thread.id)
		{
			Table table({{"EBP"}, {"RETURN"}, {"FUNCTION"}, {"EXCEPTION"}});
			for (const auto& entry : build_call_chain(thread, exception))
			{
				table.push_back({
					::to_hex(entry.first, dump.is_32bit),
					::to_hex(entry.second, dump.is_32bit),
					decode_code_address(dump, entry.second),
					table.rows() == 0 ? exception->to_string(dump.is_32bit) : "",
				});
			}
			return table;
		}
		else
		{
			Table table({{"EBP"}, {"RETURN"}, {"FUNCTION"}});
			for (const auto& entry : build_call_chain(thread, nullptr))
			{
				table.push_back({
					::to_hex(entry.first, dump.is_32bit),
					::to_hex(entry.second, dump.is_32bit),
					decode_code_address(dump, entry.second),
				});
			}
			return table;
		}
	}
}

Minidump::Minidump(const std::string& file_name, bool summary)
	: _data(MinidumpData::load(file_name, summary))
{
}

Minidump::~Minidump()
{
}

Table Minidump::print_exception_call_stack() const
{
	if (!_data->exception)
		return {};
	return ::print_call_stack(*_data, *_data->exception->thread, _data->exception.get());
}

Table Minidump::print_handles() const
{
	Table table({{"#", Table::Alignment::Right}, {"HANDLE", Table::Alignment::Right}, {"TYPE"}, {"OBJECT"}});
	table.reserve(_data->handles.size());
	for (const auto& handle : _data->handles)
	{
		table.push_back({
			std::to_string(&handle - &_data->handles.front() + 1),
			::to_hex_min(handle.handle),
			handle.type_name,
			handle.object_name,
		});
	}
	return table;
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
	return table;
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
	return table;
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
	return table;
}

Table Minidump::print_thread_call_stack(unsigned long thread_index) const
{
	if (thread_index == 0 || thread_index > _data->threads.size())
		throw std::invalid_argument("Bad thread " + std::to_string(thread_index));
	return ::print_call_stack(*_data, _data->threads[thread_index - 1], _data->exception.get());
}

void Minidump::print_thread_raw_stack(unsigned long thread_index) const
{
	if (thread_index == 0 || thread_index > _data->threads.size())
		throw std::invalid_argument("Bad thread " + std::to_string(thread_index));
	const auto& thread = _data->threads[thread_index - 1];
	::print_end_data(thread.stack_base, reinterpret_cast<uint32_t*>(thread.stack.get()), thread.stack_end - thread.stack_base);
}

Table Minidump::print_threads() const
{
	Table table({{"#", Table::Alignment::Right}, {"ID"}, {"STACK"}, {"END"}, {"START"}, {"CURRENT"}, {"NOTES"}});
	table.reserve(_data->threads.size());
	for (const auto& thread : _data->threads)
	{
		table.push_back({
			std::to_string(&thread - &_data->threads.front() + 1),
			::to_hex(thread.id),
			::to_hex(thread.stack_base, _data->is_32bit),
			::to_hex(thread.stack_end, _data->is_32bit),
			decode_code_address(*_data, thread.start_address),
			decode_code_address(*_data, thread.context->x86.eip),
			_data->exception && _data->exception->thread_id == thread.id ? "(exception)" : "",
		});
	}
	return table;
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
	return table;
}

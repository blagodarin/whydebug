#include "minidump.h"
#include "minidump_data.h"
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
			return address >= module.image_base && address < module.image_base + module.image_size;
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
		while (ebp >= thread.stack_base && ebp + 2 * sizeof(ebp) <= thread.stack_base + thread.stack_size)
		{
			const auto stack_offset = ebp - thread.stack_base;
			const auto return_address = reinterpret_cast<uint32_t&>(thread.stack[stack_offset + sizeof ebp]);
			ebp = reinterpret_cast<uint32_t&>(thread.stack[stack_offset]);
			chain.emplace_back(ebp, return_address);
		}
		return chain;
	}

	void print_call_stack(std::ostream& stream, const MinidumpData& dump, const MinidumpData::Thread& thread)
	{
		if (!thread.start_address || !thread.context.x86.eip || !thread.context.x86.ebp)
			return;
		std::vector<std::vector<std::string>> table;
		table.push_back({"EBP", "FUNCTION"});
		for (const auto& entry : build_call_chain(thread))
		{
			table.push_back({
				::to_hex(entry.first, dump.is_32bit),
				::to_hex(entry.second, dump.is_32bit),
				decode_code_address(dump, entry.second)
			});
		}
		for (const auto& row : ::format_table(table))
			stream << '\t' << row << '\n';
	}
}

Minidump::Minidump(const std::string& file_name)
	: _data(MinidumpData::load(file_name))
{
}

Minidump::~Minidump()
{
}

void Minidump::print_exception_call_stack(std::ostream& stream)
{
	if (!_data->exception)
		return;
	::print_call_stack(stream, *_data, *_data->exception->thread);
}

void Minidump::print_modules(std::ostream& stream)
{
	std::vector<std::vector<std::string>> table;
	table.push_back({"#", "NAME", "VERSION", "IMAGE", "PDB"});
	for (const auto& module : _data->modules)
	{
		table.push_back({
			std::to_string(&module - &_data->modules.front() + 1),
			module.file_name,
			module.product_version,
			::to_hex(module.image_base, _data->is_32bit) + " - " + ::to_hex(module.image_base + module.image_size, _data->is_32bit),
			module.pdb_name
		});
	}
	for (const auto& row : ::format_table(table))
		stream << '\t' << row << '\n';
}

void Minidump::print_summary(std::ostream& stream)
{
	stream << "Timestamp: " << ::time_t_to_string(_data->timestamp) << "\n";
	if (_data->process_id)
		stream << "Process ID: " << _data->process_id << "\n";
	if (_data->process_create_time)
	{
		stream
			<< "Process creation time: " << ::time_t_to_string(_data->process_create_time)
				<< " (calculated uptime: " << ::seconds_to_string(_data->timestamp - _data->process_create_time) << ")\n"
			<< "Process user time: " << ::seconds_to_string(_data->process_user_time) << "\n"
			<< "Process kernel time: " << ::seconds_to_string(_data->process_kernel_time) << "\n";
	}
}

void Minidump::print_thread_call_stack(std::ostream& stream, const std::string& thread_index)
{
	unsigned long index = 0;
	try
	{
		index = std::stoul(thread_index);
	}
	catch (const std::logic_error&)
	{
	}
	if (index == 0 || index > _data->threads.size())
	{
		std::cerr << "ERROR: Bad thread # " << thread_index << std::endl;
		return;
	}
	::print_call_stack(stream, *_data, _data->threads[index - 1]);
}

void Minidump::print_threads(std::ostream& stream)
{
	std::vector<std::vector<std::string>> table;
	table.push_back({"#", "ID", "STACK", "START", "CURRENT", "ESP"});
	for (const auto& thread : _data->threads)
	{
		table.push_back({
			std::to_string(&thread - &_data->threads.front() + 1),
			::to_hex(thread.id),
			::to_hex(thread.stack_base, _data->is_32bit) + " - " + ::to_hex(thread.stack_base + thread.stack_size, _data->is_32bit),
			decode_code_address(*_data, thread.start_address),
			decode_code_address(*_data, thread.context.x86.eip),
			::to_hex(thread.context.x86.esp)
		});
	}
	for (const auto& row : ::format_table(table))
		stream << '\t' << row << '\n';
}

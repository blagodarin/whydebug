#include "minidump.h"
#include "minidump_data.h"
#include "utils.h"
#include <algorithm>
#include <iostream>

Minidump::Minidump(const std::string& file_name)
	: _data(MinidumpData::load(file_name))
{
}

Minidump::~Minidump()
{
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
	std::string result = ::to_hex(address, _data->is_32bit);
	const auto i = std::find_if(_data->modules.begin(), _data->modules.end(), [address](const auto& module)
	{
		return address >= module.image_base && address < module.image_base + module.image_size;
	});
	if (i != _data->modules.end())
		result = i->file_name + "!" + result;
	return std::move(result);
}

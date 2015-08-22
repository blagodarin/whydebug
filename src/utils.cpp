#include "utils.h"
#include <array>
#include <cinttypes>
#include <cstring>
#include <iomanip>
#include <iostream>

std::string time_t_to_string(time_t time)
{
	::tm tm;
	::memset(&tm, 0, sizeof tm);
	::localtime_r(&time, &tm);
	std::array<char, 32> buffer;
	::memset(buffer.data(), 0, buffer.size());
	::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02d %02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buffer.data();
}

std::string duration_to_string(uint32_t duration)
{
	const auto seconds = duration % 60;
	const auto minutes = (duration / 60) % 60;
	const auto hours = (duration / 60) / 60;
	std::array<char, 32> buffer;
	::memset(buffer.data(), 0, buffer.size());
	::snprintf(buffer.data(), buffer.size(), "%d:%02d:%02d", hours, minutes, seconds);
	return buffer.data();
}

std::vector<std::string> format_table(const std::vector<std::vector<std::string>>& table)
{
	size_t max_row_size = 0;
	for (const auto& row : table)
		max_row_size = std::max(max_row_size, row.size());

	std::vector<size_t> widths(max_row_size);
	for (const auto& row : table)
		for (size_t i = 0; i < row.size(); ++i)
			widths[i] = std::max(widths[i], row[i].size());

	std::vector<std::string> result;
	for (const auto& row : table)
	{
		std::string value;
		for (size_t i = 0; i < row.size(); ++i)
		{
			if (i > 0)
				value += "  ";
			value += row[i] + std::string(widths[i] - row[i].size(), ' ');
		}
		result.emplace_back(std::move(value));
	}
	
	return result;
}

std::string to_ascii(const std::u16string& string)
{
	std::string ascii;
	ascii.reserve(string.size());
	for (const auto c : string)
		ascii.push_back(c < 128 ? c : '?');
	return ascii;
}

std::string to_hex(uint32_t value)
{
	std::array<char, 9> buffer;
	::snprintf(buffer.data(), buffer.size(), "%08" PRIx32, value);
	return buffer.data();
}

std::string to_hex(uint64_t value)
{
	std::array<char, 17> buffer;
	::snprintf(buffer.data(), buffer.size(), "%016" PRIx64, value);
	return buffer.data();
}

std::string to_human_readable(uint64_t bytes)
{
	if (bytes < 1024)
		return std::to_string(bytes) + " B";
	bytes /= 1024;
	if (bytes < 1024)
		return std::to_string(bytes) + " K";
	bytes /= 1024;
	if (bytes < 1024)
		return std::to_string(bytes) + " M";
	bytes /= 1024;
	if (bytes < 1024)
		return std::to_string(bytes) + " G";
	return std::to_string(bytes) + " T";
}

void print_data(const uint32_t* data, size_t bytes)
{
	static const size_t line_size = 16;
	const auto size = bytes / sizeof *data;
	for (size_t i = 0; i < size; ++i)
	{
		std::cout << (i % line_size == 0 ? '\t' : ' ');
		std::cout << std::hex << std::setfill('0') << std::setw(2 * sizeof *data) << data[i] << std::dec;
		if ((i + 1) % line_size == 0 || (i + 1) == size)
			std::cout << std::endl;
	}
}

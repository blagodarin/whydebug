#include "utils.h"
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstring>
#include <iomanip>
#include <iostream>

std::string seconds_to_string(uint32_t duration)
{
	const auto seconds = duration % 60;
	const auto minutes = (duration / 60) % 60;
	const auto hours = (duration / 60) / 60;
	std::array<char, 32> buffer;
	::memset(buffer.data(), 0, buffer.size());
	::snprintf(buffer.data(), buffer.size(), "%d:%02d:%02d", hours, minutes, seconds);
	return buffer.data();
}

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

std::string to_ascii(const std::u16string& string)
{
	std::string ascii;
	ascii.reserve(string.size());
	for (const auto c : string)
		ascii.push_back(c < 128 ? c : '?');
	return ascii;
}

std::string to_hex(uint16_t value)
{
	std::array<char, 5> buffer;
	::snprintf(buffer.data(), buffer.size(), "%04" PRIx16, value);
	return buffer.data();
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

std::string to_hex_min(uint64_t value)
{
	std::array<char, 17> buffer;
	::snprintf(buffer.data(), buffer.size(), "%" PRIx64, value);
	return buffer.data();
}

std::string to_human_readable(uint64_t bytes)
{
	double result = bytes;
	if (result < 1024)
		return ::to_string(result) + " B";
	result /= 1024;
	if (result < 1024)
		return ::to_string(result) + " KiB";
	result /= 1024;
	if (result < 1024)
		return ::to_string(result) + " MiB";
	result /= 1024;
	if (result < 1024)
		return ::to_string(result) + " GiB";
	return ::to_string(result) + " TiB";
}

std::string to_string(double value)
{
	std::array<char, 64> buffer;
	const auto n = ::snprintf(buffer.data(), buffer.size(), "%.1f", value);
	if (n > 2 && buffer[n - 1] == '0' && buffer[n - 2] == '.')
		buffer[n - 2] = '\0';
	return buffer.data();
}

unsigned long to_ulong(const std::string& value)
{
	try
	{
		return std::stoul(value);
	}
	catch (const std::logic_error&)
	{
		throw std::runtime_error("Invalid number: " + value);
	}
}

void print_data(const uint32_t* data, size_t bytes, size_t columns)
{
	assert(columns > 0);
	const auto size = bytes / sizeof *data;
	for (size_t i = 0; i < size; ++i)
	{
		std::cout << (i % columns == 0 ? '\t' : ' ');
		std::cout << std::hex << std::setfill('0') << std::setw(2 * sizeof *data) << data[i] << std::dec;
		if ((i + 1) % columns == 0 || (i + 1) == size)
			std::cout << std::endl;
	}
}

void print_data(uint32_t base, const uint32_t* data, size_t bytes, size_t columns)
{
	assert(columns > 0);
	const auto size = bytes / sizeof *data;
	for (size_t i = 0; i < size; ++i)
	{
		if (i % columns == 0)
			std::cout << '\t' << std::hex << std::setfill('0') << std::setw(2 * sizeof base) << (base + i * sizeof base) << " : ";
		else
			std::cout << ' ';
		std::cout << std::hex << std::setfill('0') << std::setw(2 * sizeof *data) << data[i] << std::dec;
		if ((i + 1) % columns == 0 || (i + 1) == size)
			std::cout << std::endl;
	}
}

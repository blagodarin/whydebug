#pragma once

#include <string>
#include <vector>

std::string time_t_to_string(time_t);
std::string duration_to_string(uint32_t);
std::vector<std::string> format_table(const std::vector<std::vector<std::string>>&);
std::string to_ascii(const std::u16string&);
std::string to_hex(uint32_t);
std::string to_hex(uint64_t);
inline std::string to_hex(uint64_t value, bool as_uint32) { return as_uint32 ? ::to_hex(static_cast<uint32_t>(value)) : ::to_hex(value); }
std::string to_human_readable(uint64_t);
void print_data(const uint32_t* data, size_t bytes);

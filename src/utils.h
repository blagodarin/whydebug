#pragma once

#include <string>
#include <vector>

// Check if all specified flags are set in a value.
template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
constexpr bool has_flags(T value, T flags)
{
	return (value & flags) == flags;
}

// Check if all specified flags are set in a value.
template <typename T, typename = std::enable_if_t<std::is_enum<T>::value>>
constexpr bool has_flags(T value, std::underlying_type_t<T> flags)
{
	return (value & flags) == flags;
}

//
std::string seconds_to_string(uint32_t);

//
std::string time_t_to_string(time_t);

//
std::string to_ascii(const std::u16string&);

//
std::string to_hex(uint16_t);
std::string to_hex(uint32_t);
std::string to_hex(uint64_t);

//
inline std::string to_hex(uint64_t value, bool as_uint32)
{
	return as_uint32 ? ::to_hex(static_cast<uint32_t>(value)) : ::to_hex(value);
}

//
std::string to_hex_min(uint64_t);

//
std::string to_human_readable(uint64_t);

//
template <typename T, typename = std::enable_if_t<std::is_enum<T>::value>>
constexpr std::underlying_type_t<T> to_raw(T value)
{
	return static_cast<std::underlying_type_t<T>>(value);
}

//
std::string to_string(double);

//
unsigned long to_ulong(const std::string&);

//
void print_data(const uint32_t* data, size_t bytes, size_t columns = 16);

//
void print_data(uint32_t base, const uint32_t* data, size_t bytes, size_t columns = 16);
void print_data(uint64_t base, const uint64_t* data, size_t bytes, size_t columns = 8);

//
void print_end_data(uint32_t base, const uint32_t* data, size_t bytes, size_t columns = 16);

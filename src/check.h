#pragma once

#include <exception>
#include <sstream>

class BadCheck : public std::exception
{
public:
	BadCheck(std::string&& what) : _what(std::move(what)) {}
	const char* what() const noexcept override { return _what.c_str(); }
private:
	std::string _what;
};

#define CHECK(value, message) \
	do { \
		if (!(value)) { \
			std::stringstream stream_; \
			stream_ << message; \
			throw BadCheck(stream_.str()); \
		} \
	} while (false)

#define CHECK_EQ(value, expected, message) \
	do { \
		if ((value) != (expected)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", expected = " << (expected); \
			throw BadCheck(stream_.str()); \
		} \
	} while (false)

#define CHECK_LE(value, max, message) \
	do { \
		if ((value) > (max)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", max = " << (max); \
			throw BadCheck(stream_.str()); \
		} \
	} while (false)

#define CHECK_GE(value, min, message) \
	do { \
		if ((value) < (min)) { \
			std::stringstream stream_; \
			stream_ << message << ": value = " << (value) << ", min = " << (min); \
			throw BadCheck(stream_.str()); \
		} \
	} while (false)

#pragma once

#include <cstdio>
#include <string>

class File
{
public:

	File() = default;
	File(const File&) = delete;
	File(File&& file) : _file(file._file) { file._file = nullptr; }
	~File();
	File& operator=(const File&) = delete;
	File& operator=(File&&);
	explicit operator bool() const { return _file; }

	File(const std::string& name);

	size_t read(void* buffer, size_t size);
	bool seek(uint64_t offset);

	template <typename T>
	bool read(T& buffer)
	{
		return read(&buffer, sizeof buffer) == sizeof buffer;
	}

private:
	FILE* _file = nullptr;
};

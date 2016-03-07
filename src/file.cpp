#include "file.h"
#include <limits>
#include <stdexcept>

File::~File()
{
	if (_file)
		::fclose(_file);
}

File& File::operator=(File&& file)
{
	if (_file)
		::fclose(_file);
	_file = file._file;
	file._file = nullptr;
	return *this;
}

File::File(const std::string& name)
	: _file(::fopen(name.c_str(), "rb"))
{
}

bool File::read(void* buffer, size_t size)
{
	return ::fread(buffer, 1, size, _file) == size;
}

bool File::seek(uint64_t offset)
{
	if (offset > std::numeric_limits<long>::max())
		throw std::logic_error("Large file support is not implemented");
	return 0 == ::fseek(_file, offset, SEEK_SET);
}

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class Table
{
public:

	Table(std::vector<std::string>&& header);

	void push_back(std::vector<std::string>&& row);
	void sort(const std::string& column_prefix);

	Table() = default;
	Table(const Table&) = delete;
	Table(Table&&) = default;
	Table& operator=(const Table&) = delete;
	Table& operator=(Table&&) = default;

private:

	std::vector<std::string> _header;
	std::vector<std::vector<std::string>> _data;

	friend std::ostream& operator<<(std::ostream&, const Table&);
};

std::ostream& operator<<(std::ostream&, const Table&);

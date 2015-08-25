#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class Table
{
public:

	Table(std::vector<std::string>&& header) { _table.emplace_back(std::move(header)); }

	void push_back(std::vector<std::string>&& row) { _table.emplace_back(std::move(row)); }

	Table() = default;

private:

	std::vector<std::vector<std::string>> _table;

	friend std::ostream& operator<<(std::ostream&, const Table&);
};

std::ostream& operator<<(std::ostream&, const Table&);

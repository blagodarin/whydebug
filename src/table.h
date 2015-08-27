#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class Table
{
public:

	enum class Alignment
	{
		Left,
		Right,
	};

	struct ColumnHeader
	{
		std::string name;
		Alignment alignment = Alignment::Left;

		ColumnHeader(const std::string& name) : name(name) {}
		ColumnHeader(const std::string& name, Alignment alignment) : name(name), alignment(alignment) {}
	};

	Table(std::vector<ColumnHeader>&& header);

	void push_back(std::vector<std::string>&& row);
	void sort(const std::string& column_prefix);

	Table() = default;
	Table(const Table&) = delete;
	Table(Table&&) = default;
	Table& operator=(const Table&) = delete;
	Table& operator=(Table&&) = default;

private:

	std::vector<ColumnHeader> _header;
	std::vector<std::vector<std::string>> _data;

	friend std::ostream& operator<<(std::ostream&, const Table&);
};

std::ostream& operator<<(std::ostream&, const Table&);

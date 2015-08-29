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

	void filter(const std::string& prefix, const std::string& value);
	void print(std::ostream&) const;
	void push_back(std::vector<std::string>&& row);
	void sort(const std::string& prefix);

	Table() = default;
	Table(const Table&) = delete;
	Table(Table&&) = default;
	Table& operator=(const Table&) = delete;
	Table& operator=(Table&&) = default;

private:

	size_t match_column(const std::string& prefix) const;

private:

	bool _empty_header = true;
	std::vector<std::string> _header;
	std::vector<Alignment> _alignment;
	std::vector<std::vector<std::string>> _data;
};

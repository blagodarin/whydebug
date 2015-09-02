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

	enum class Pass
	{
		Equal,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,
		Containing,
		StartingWith,
		EndingWith,
	};

	Table(std::vector<ColumnHeader>&& header);

	void filter(const std::string& prefix, const std::string& value, Pass pass);
	void leave_first_rows(size_t count);
	void leave_last_rows(size_t count);
	void print(std::ostream&) const;
	void push_back(std::vector<std::string>&& row);
	void reserve(size_t rows);
	void reverse_sort(const std::string& prefix);
	auto rows() const { return _data.size(); }
	void set_original();
	void sort(const std::string& prefix);

	Table() = default;
	Table(const Table&) = delete;
	Table(Table&&) = default;
	Table& operator=(const Table&) = delete;
	Table& operator=(Table&&) = default;

private:

	size_t match_column(std::string prefix) const;

private:

	bool _empty_header = true;
	std::vector<std::string> _header;
	std::vector<Alignment> _alignment;
	std::vector<std::vector<std::string>> _data;
	std::vector<size_t> _indices;
};

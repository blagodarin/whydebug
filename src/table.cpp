#include "table.h"
#include <algorithm>
#include <iostream>

Table::Table(std::vector<ColumnHeader>&& header)
{
	_header.reserve(header.size());
	_alignment.reserve(header.size());
	for (auto& column : header)
	{
		if (!column.name.empty())
			_empty_header = false;
		_header.emplace_back(std::move(column.name));
		_alignment.emplace_back(std::move(column.alignment));
	}
}

void Table::print(std::ostream& stream) const
{
	std::vector<size_t> widths(_header.size(), 0);
	for (size_t i = 0; i < _header.size(); ++i)
		widths[i] = std::max(widths[i], _header[i].size());
	for (const auto& row : _data)
		for (size_t i = 0; i < row.size(); ++i)
			widths[i] = std::max(widths[i], row[i].size());

	const auto row_to_string = [this, &widths](const std::vector<std::string>& row) -> std::string
	{
		std::string result;
		for (size_t i = 0; i < row.size(); ++i)
		{
			if (i > 0)
				result += "  ";
			const auto& cell = row[i];
			std::string padding(widths[i] - cell.size(), ' ');
			if (_alignment[i] == Table::Alignment::Left)
				result += cell + padding;
			else
				result += std::move(padding) + cell;
		}
		return std::move(result);
	};

	std::vector<std::string> rows;
	if (!_empty_header)
		stream << '\t' << row_to_string(_header) << '\n';
	for (const auto& row : _data)
		stream << '\t' << row_to_string(row) << '\n';
}

void Table::push_back(std::vector<std::string>&& row)
{
	if (_header.size() < row.size())
	{
		_header.resize(row.size());
		_alignment.resize(row.size());
	}
	_data.emplace_back(std::move(row));
}

void Table::sort(const std::string& column_prefix)
{
	size_t best_match_size = 0;
	size_t best_match = 0;
	for (const auto& column : _header)
	{
		if (column.find(column_prefix) == 0 && column_prefix.size() > best_match_size)
		{
			best_match_size = column_prefix.size();
			best_match = &column - &_header.front();
		}
	}
	if (best_match_size == 0)
		return;
	std::sort(_data.begin(), _data.end(), [best_match](const auto& left, const auto& right)
	{
		return left[best_match] < right[best_match];
		// TODO: Proper sorting of right-aligned data.
	});
}

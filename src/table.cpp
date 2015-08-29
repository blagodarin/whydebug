#include "table.h"
#include <algorithm>
#include <cstring>
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

	static const size_t column_spacing = 2;

	size_t total_width = column_spacing * (widths.size() - 1);
	for (const auto column_width : widths)
		total_width += column_width;

	std::string buffer(1 + total_width + 1, ' ');
	buffer.front() = '\t';
	buffer.back() = '\n';
	const auto print_row = [this, &stream, &widths, &buffer](const std::vector<std::string>& row)
	{
		size_t offset = 1;
		for (size_t i = 0; i < row.size(); ++i)
		{
			const auto& cell = row[i];
			const auto column_width = widths[i];
			const auto padding = column_width - cell.size();
			if (_alignment[i] == Table::Alignment::Left)
			{
				::memcpy(&buffer[offset], cell.data(), cell.size());
				::memset(&buffer[offset + cell.size()], ' ', padding);
			}
			else
			{
				::memset(&buffer[offset], ' ', padding);
				::memcpy(&buffer[offset + padding], cell.data(), cell.size());
			}
			offset += column_width + column_spacing;
		}
		stream << buffer;
	};

	if (!_empty_header)
		print_row(_header);
	for (const auto& row : _data)
		print_row(row);
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
	std::sort(_data.begin(), _data.end(), [this, best_match](const auto& left, const auto& right)
	{
		const auto& left_cell = left[best_match];
		const auto& right_cell = right[best_match];
		if (_alignment[best_match] == Table::Alignment::Right && left_cell.size() != right_cell.size())
			return left_cell.size() < right_cell.size();
		return left_cell < right_cell;
	});
}

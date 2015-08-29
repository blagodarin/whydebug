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

void Table::filter(const std::string& prefix, const std::string& value)
{
	const auto column = match_column(prefix);
	if (column == _header.size())
		return;
	for (auto i = _indices.begin(); i != _indices.end();)
	{
		if (_data[*i][column] != value)
			i = _indices.erase(i);
		else
			++i;
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
	for (const auto index : _indices)
		print_row(_data[index]);
}

void Table::push_back(std::vector<std::string>&& row)
{
	_indices.emplace_back(_data.size());
	row.resize(_header.size());
	_data.emplace_back(std::move(row));
}

void Table::sort(const std::string& prefix)
{
	const auto column = match_column(prefix);
	if (column == _header.size())
		return;
	std::sort(_indices.begin(), _indices.end(), [this, column](const auto lhs_row, const auto rhs_row)
	{
		const auto& lhs_cell = _data[lhs_row][column];
		const auto& rhs_cell = _data[rhs_row][column];
		if (_alignment[column] == Table::Alignment::Right && lhs_cell.size() != rhs_cell.size())
			return lhs_cell.size() < rhs_cell.size();
		return lhs_cell < rhs_cell;
	});
}

size_t Table::match_column(const std::string& prefix) const
{
	size_t best_match_size = 0;
	size_t best_match_index = _header.size();
	for (const auto& column : _header)
	{
		if (column.find(prefix) == 0 && prefix.size() > best_match_size)
		{
			best_match_size = prefix.size();
			best_match_index = &column - &_header.front();
		}
	}
	return best_match_index;
}

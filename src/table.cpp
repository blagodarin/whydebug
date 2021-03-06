#include "table.h"
#include <algorithm>
#include <cassert>
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

void Table::filter(const std::string& prefix, const std::string& value, Pass pass)
{
	const auto column = match_column(prefix);
	if (column == _header.size())
		return;
	size_t next_index = 0;
	for (const auto row : _indices)
	{
		const auto& cell = _data[row][column];
		bool passed = false;
		switch (pass)
		{
		case Pass::Equal:
			passed = cell == value;
			break;
		case Pass::NotEqual:
			passed = cell != value;
			break;
		case Pass::Less:
			passed = (_alignment[column] == Table::Alignment::Right && cell.size() != value.size())
				? cell.size() < value.size()
				: cell < value;
			break;
		case Pass::LessOrEqual:
			passed = (_alignment[column] == Table::Alignment::Right && cell.size() != value.size())
				? cell.size() < value.size()
				: cell <= value;
			break;
		case Pass::Greater:
			passed = (_alignment[column] == Table::Alignment::Right && cell.size() != value.size())
				? cell.size() > value.size()
				: cell > value;
			break;
		case Pass::GreaterOrEqual:
			passed = (_alignment[column] == Table::Alignment::Right && cell.size() != value.size())
				? cell.size() > value.size()
				: cell >= value;
			break;
		case Pass::Containing:
			passed = cell.find(value) != std::string::npos;
			break;
		case Pass::StartingWith:
			passed = cell.find(value) == 0;
			break;
		case Pass::EndingWith:
			passed = cell.size() >= value.size() && cell.find(value) == cell.size() - value.size();
			break;
		}
		if (passed)
		{
			_indices[next_index] = row;
			++next_index;
		}
	}
	_indices.resize(next_index);
}

void Table::leave_first_rows(size_t count)
{
	if (_indices.size() > count)
		_indices.erase(_indices.begin() + count, _indices.end());
}

void Table::leave_last_rows(size_t count)
{
	if (_indices.size() > count)
		_indices.erase(_indices.begin(), _indices.end() - count);
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
	for (const auto row : _indices)
		print_row(_data[row]);
}

void Table::push_back(std::vector<std::string>&& row)
{
	assert(row.size() == _header.size());
	_indices.emplace_back(_data.size());
	_data.emplace_back(std::move(row));
}

void Table::reserve(size_t rows)
{
	_data.reserve(rows);
	_indices.reserve(rows);
}

void Table::reverse_sort(const std::string& prefix)
{
	const auto column = match_column(prefix);
	if (column == _header.size())
		return;
	std::sort(_indices.begin(), _indices.end(), [this, column](const auto lhs_row, const auto rhs_row)
	{
		const auto& lhs_cell = _data[lhs_row][column];
		const auto& rhs_cell = _data[rhs_row][column];
		if (_alignment[column] == Table::Alignment::Right && lhs_cell.size() != rhs_cell.size())
			return lhs_cell.size() > rhs_cell.size();
		return lhs_cell > rhs_cell;
	});
}

void Table::set_original()
{
	_indices.clear();
	for (size_t row = 0; row < _data.size(); ++row)
		_indices.emplace_back(row);
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

size_t Table::match_column(std::string prefix) const
{
	for (auto& c : prefix)
		c = std::toupper(c);
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

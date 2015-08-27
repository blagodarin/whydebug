#include "table.h"
#include <algorithm>
#include <iostream>

namespace
{
	std::vector<std::string> format_table(const std::vector<std::string>& header, const std::vector<std::vector<std::string>>& data)
	{
		std::vector<size_t> widths(header.size());
		for (size_t i = 0; i < header.size(); ++i)
			widths[i] = std::max(widths[i], header[i].size());
		for (const auto& row : data)
			for (size_t i = 0; i < row.size(); ++i)
				widths[i] = std::max(widths[i], row[i].size());

		std::vector<std::string> result;
		std::string header_row;
		for (size_t i = 0; i < header.size(); ++i)
		{
			if (i > 0)
				header_row += "  ";
			header_row += header[i] + std::string(widths[i] - header[i].size(), ' ');
		}
		result.emplace_back(std::move(header_row));
		for (const auto& row : data)
		{
			std::string data_row;
			for (size_t i = 0; i < row.size(); ++i)
			{
				if (i > 0)
					data_row += "  ";
				data_row += row[i] + std::string(widths[i] - row[i].size(), ' ');
			}
			result.emplace_back(std::move(data_row));
		}
		
		return result;
	}
}

Table::Table(std::vector<std::string>&& header)
	: _header(std::move(header))
{
}

void Table::push_back(std::vector<std::string>&& row)
{
	row.resize(_header.size());
	_data.emplace_back(std::move(row));
}

void Table::sort(const std::string& column_prefix)
{
	size_t best_match_size = 0;
	size_t best_match = 0;
	for (const auto& column_name : _header)
	{
		if (column_name.find(column_prefix) == 0 && column_prefix.size() > best_match_size)
		{
			best_match_size = column_prefix.size();
			best_match = &column_name - &_header.front();
		}
	}
	if (best_match_size == 0)
		return;
	std::sort(_data.begin(), _data.end(), [best_match](const auto& left, const auto& right)
	{
		return left[best_match] < right[best_match];
	});
}

std::ostream& operator<<(std::ostream& stream, const Table& table)
{
	for (const auto& row : ::format_table(table._header, table._data))
		stream << '\t' << row << '\n';
	return stream;
}

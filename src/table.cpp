#include "table.h"
#include <algorithm>
#include <iostream>

namespace
{
	std::vector<std::string> format_table(const std::vector<Table::ColumnHeader>& header, const std::vector<std::vector<std::string>>& data)
	{
		std::vector<size_t> widths(header.size());
		for (size_t i = 0; i < header.size(); ++i)
			widths[i] = std::max(widths[i], header[i].name.size());
		for (const auto& row : data)
			for (size_t i = 0; i < row.size(); ++i)
				widths[i] = std::max(widths[i], row[i].size());

		std::vector<std::string> result;
		std::string header_row;
		for (size_t i = 0; i < header.size(); ++i)
		{
			if (i > 0)
				header_row += "  ";
			auto&& text = header[i].name;
			const auto& cell_text = header[i].name;
			std::string padding(widths[i] - cell_text.size(), ' ');
			if (header[i].alignment == Table::Alignment::Left)
				header_row += cell_text + padding;
			else
				header_row += std::move(padding) + cell_text;
		}
		result.emplace_back(std::move(header_row));
		for (const auto& row : data)
		{
			std::string data_row;
			for (size_t i = 0; i < row.size(); ++i)
			{
				if (i > 0)
					data_row += "  ";
				const auto& cell_text = row[i];
				std::string padding(widths[i] - cell_text.size(), ' ');
				if (header[i].alignment == Table::Alignment::Left)
					data_row += cell_text + padding;
				else
					data_row += std::move(padding) + cell_text;
			}
			result.emplace_back(std::move(data_row));
		}
		
		return result;
	}
}

Table::Table(std::vector<ColumnHeader>&& header)
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
	for (const auto& column : _header)
	{
		if (column.name.find(column_prefix) == 0 && column_prefix.size() > best_match_size)
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

std::ostream& operator<<(std::ostream& stream, const Table& table)
{
	for (const auto& row : ::format_table(table._header, table._data))
		stream << '\t' << row << '\n';
	return stream;
}

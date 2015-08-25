#include "table.h"
#include <iostream>

namespace
{
	std::vector<std::string> format_table(const std::vector<std::vector<std::string>>& table)
	{
		size_t max_row_size = 0;
		for (const auto& row : table)
			max_row_size = std::max(max_row_size, row.size());

		std::vector<size_t> widths(max_row_size);
		for (const auto& row : table)
			for (size_t i = 0; i < row.size(); ++i)
				widths[i] = std::max(widths[i], row[i].size());

		std::vector<std::string> result;
		for (const auto& row : table)
		{
			std::string value;
			for (size_t i = 0; i < row.size(); ++i)
			{
				if (i > 0)
					value += "  ";
				value += row[i] + std::string(widths[i] - row[i].size(), ' ');
			}
			result.emplace_back(std::move(value));
		}
		
		return result;
	}
}

std::ostream& operator<<(std::ostream& stream, const Table& table)
{
	for (const auto& row : ::format_table(table._table))
		stream << '\t' << row << '\n';
	return stream;
}

#include "TableSorter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace sorttable {
namespace {

std::optional<double> try_parse_double(const std::string& value)
{
    if (value.empty()) return std::nullopt;

    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        while (consumed < value.size()
               && std::isspace(static_cast<unsigned char>(value[consumed]))) {
            ++consumed;
        }
        if (consumed != value.size() || !std::isfinite(parsed)) return std::nullopt;
        return parsed;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

const std::string& cell_at(const std::vector<std::string>& row, int column)
{
    static const std::string empty;
    return column < static_cast<int>(row.size()) ? row[column] : empty;
}

bool all_numeric(const TabularData& table, int column)
{
    for (const auto& row : table.rows) {
        const auto& value = cell_at(row, column);
        if (!value.empty() && !try_parse_double(value)) return false;
    }
    return !table.rows.empty();
}

} // namespace

SortResult sort(TabularData& table, const SortOptions& options)
{
    if (options.sort_column < 0
        || options.sort_column >= static_cast<int>(table.header.size())) {
        throw std::out_of_range("sort_column is outside the supplied table header");
    }

    const bool numeric = all_numeric(table, options.sort_column);
    const bool ascending = options.order == SortOrder::Ascending;

    std::stable_sort(table.rows.begin(), table.rows.end(),
        [&](const auto& left, const auto& right) {
            const auto& left_value = cell_at(left, options.sort_column);
            const auto& right_value = cell_at(right, options.sort_column);

            if (numeric) {
                const double left_number = try_parse_double(left_value).value_or(0.0);
                const double right_number = try_parse_double(right_value).value_or(0.0);
                return ascending ? left_number < right_number : left_number > right_number;
            }
            return ascending ? left_value < right_value : left_value > right_value;
        });

    return {numeric ? "numeric" : "lexicographic"};
}

} // namespace sorttable

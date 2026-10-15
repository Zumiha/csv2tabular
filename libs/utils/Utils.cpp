#include "Utils.h"

#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace {

std::optional<double> TryParseAggregateValue(const std::string& value)
{
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        while (consumed < value.size()
               && std::isspace(static_cast<unsigned char>(value[consumed]))) {
            ++consumed;
        }
        if (consumed != value.size() || !std::isfinite(parsed))
            return std::nullopt;
        return parsed;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

double ParseAggregateValue(const std::string& value, int row, int column)
{
    if (value.empty())
        return 0.0;

    if (const auto parsed = TryParseAggregateValue(value))
        return *parsed;
    throw std::invalid_argument(
        "AggregateByColumn: invalid numeric value in row "
        + std::to_string(row) + ", column " + std::to_string(column)
        + ": \"" + value + "\"");
}

} // namespace

std::vector<ColumnAggregate> AggregateByColumn(
    const TabularData& table,
    int group_column,
    int sum_column)
{
    std::vector<ColumnAggregate> aggregates;
    std::map<std::string, std::size_t> aggregate_indices;

    for (std::size_t row_index = 0; row_index < table.rows.size(); ++row_index) {
        const auto& fields = table.rows[row_index];
        const int row_number = static_cast<int>(row_index + 1);
        // Check if the specified columns are within the bounds of the current row
        for (int column : {group_column, sum_column}) {
            if (column < 0 || column >= static_cast<int>(fields.size())) {
                throw std::out_of_range(
                    "AggregateByColumn: column index " + std::to_string(column)
                    + " is out of range for row " + std::to_string(row_number)
                    + " with " + std::to_string(fields.size()) + " columns");
            }
        }
        // Value from group_column
        const std::string& value = fields[group_column];
        // Push value to map, if already present then `inserted` will be false
        auto [it, inserted] = aggregate_indices.emplace(value, aggregates.size());
        // If inserted is true, it means this value was not present in the map, so we need to add a new ColumnAggregate
        if (inserted)
            aggregates.push_back({value, 0, 0.0});
        
        auto& aggregate = aggregates[it->second]; // Find ColumnAggregate for current value
        const double row_value = ParseAggregateValue(fields[sum_column], row_number, sum_column); // Parse value from sum_column
        const double updated_sum = aggregate.sum + row_value; // Calculate new sum
        if (!std::isfinite(updated_sum)) {
            throw std::overflow_error(
                "AggregateByColumn: sum overflow for value \"" + value + "\"");
        }
        ++aggregate.count;
        aggregate.sum = updated_sum;
    }

    return aggregates;
}

#include "Utils.h"

#include <algorithm>
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

bool IsBlank(const std::string& value)
{
    return value.empty()
        || std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isspace(character);
           });
}

} // namespace

std::vector<ColumnAggregate> AggregateByColumn(
    const TabularData& table,
    int group_column,
    int sum_column,
    const std::vector<std::string>& requested_group_values)
{
    std::vector<ColumnAggregate> aggregates;
    std::map<std::string, std::size_t> aggregate_indices;

    for (const auto& value : requested_group_values) {
        if (IsBlank(value))
            continue;

        if (aggregate_indices.emplace(value, aggregates.size()).second)
            aggregates.push_back({value, 0, 0.0});
    }

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
        const std::string& value = fields[group_column];
        if (IsBlank(value))
            continue;

        const auto row_value = fields[sum_column].empty()
            ? std::optional<double>{}
            : TryParseAggregateValue(fields[sum_column]);
        if (!row_value)
            continue;

        auto [it, inserted] = aggregate_indices.emplace(value, aggregates.size());
        if (inserted)
            aggregates.push_back({value, 0, 0.0});

        auto& aggregate = aggregates[it->second];
        const double updated_sum = aggregate.sum + *row_value;
        if (!std::isfinite(updated_sum)) {
            throw std::overflow_error(
                "AggregateByColumn: sum overflow for value \"" + value + "\"");
        }
        ++aggregate.count;
        aggregate.sum = updated_sum;
    }

    return aggregates;
}

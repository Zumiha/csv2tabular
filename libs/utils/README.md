## Grouped column aggregation

`AggregateByColumn` groups the data rows of a `TabularData` value by one
zero-based column and returns each group's original string value, row count,
and sum from a second zero-based column. Groups retain first-seen order. Empty
sum cells contribute zero; non-numeric or non-finite values and missing columns
raise an exception.

```cpp
#include "LaTeXExporter.h"
#include "Utils.h"

const auto aggregates = AggregateByColumn(table, 0, 1);
TabularData summary{{"Group", "Count", "Sum"}, {}};
for (const auto& aggregate : aggregates) {
    summary.rows.push_back({
        aggregate.value,
        std::to_string(aggregate.count),
        std::to_string(aggregate.sum)
    });
}
LaTeXExporter::write("summary.tex", summary, TableLayoutOptions{});
```
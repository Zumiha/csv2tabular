## Grouped column aggregation

The `Utils` library's `AggregateByColumn` helper groups a row-indexed table by
one zero-based column and
returns each group's original string value, row count, and sum from a second
zero-based column. Groups are returned in first-seen order. Empty sum cells
contribute zero; non-numeric or non-finite values and missing columns raise an
exception.

```cpp
#include "Utils.h"

const auto aggregates = AggregateByColumn(table, 0, 1);
for (const auto& aggregate : aggregates) {
    // aggregate.value, aggregate.count, aggregate.sum
}
```
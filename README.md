# csv2xltabular
Small executable to convert csv table to LaTeX xltabular 

`AggregateByColumn` in the Utils library skips rows with a blank grouping
value or a blank/non-numeric sum value. Only rows with both a non-blank group
and a valid numeric sum contribute to the group's count and sum.

Pass an optional list of group values as the fourth argument to include groups
even when the table has no matching rows. Requested values appear first in the
given order; other groups found in the table follow in first-seen order.
Requested groups without valid matching rows have a count and sum of zero.

```cpp
const auto aggregates = AggregateByColumn(table, 0, 1, {"1", "2", "3", "4", "5"});
```

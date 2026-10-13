# TableSorter

## Purpose

`TableSorter` is a small sorting helper used by `csv2xltabular` during LaTeX
export. It reorders the data rows in a `sorttable::TabularData` table by one
column. It does not generate LaTeX or decide which data belongs in the table.

Its narrow responsibility keeps sorting independent of CSV parsing and output
formatting. The converter remains responsible for constructing the table,
selecting columns, filtering rows, limiting row count, resolving layout
overrides, and rendering the final output.

## Data and API

`TabularData` holds a header and data rows separately:

```cpp
struct TabularData {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
};
```

The public entry point is:

```cpp
SortResult sort(TabularData& table, const SortOptions& options);
```

`SortOptions::sort_column` is a zero-based index into the supplied table,
`SortOptions::order` selects ascending or descending order, and the result's
`sort_mode` reports whether numeric or lexicographic comparison was used.
Sorting modifies `table.rows` in place and leaves `table.header` unchanged.

An invalid sort-column index throws `std::out_of_range`. Row vectors shorter
than the header are supported: a missing cell is treated as an empty string.

## Sort behavior

The sorter checks the values in the selected column:

- If every non-empty cell parses as a finite `double`, numeric comparison is
  used. Empty or missing cells are treated as `0.0`.
- If any non-empty cell is not a finite number, all values are compared as
  strings using the standard `std::string` lexicographic ordering. Empty or
  missing cells are treated as empty strings.
- Sorting is stable, so rows with equal comparison values retain their
  relative order.
- With no data rows, the sort mode is reported as `"lexicographic"` because
  there are no values from which to infer a numeric mode.

Numeric detection and sorting both use `try_parse_double`, so a value is
numeric only if `std::stod` consumes the complete value apart from trailing
whitespace and the result is finite. Leading whitespace accepted by
`std::stod` is also accepted.

## Internal helper functions

The implementation's helpers are private to `TableSorter.cpp`'s anonymous
namespace; callers use only `sort`.

| Helper | Purpose |
|---|---|
| `try_parse_double(value)` | Parses one complete finite floating-point value. Returns `std::nullopt` for empty, invalid, partially parsed, out-of-range, or non-finite values. |
| `cell_at(row, column)` | Safely reads a row cell by index. Returns a shared empty-string value when the row is shorter than the requested column. |
| `all_numeric(table, column)` | Inspects non-empty cells in the chosen column to select numeric mode. Returns false when the table contains no rows. |
| `sort(table, options)` | Validates the column, chooses a comparison mode, stable-sorts only `table.rows` in the requested direction, and returns that mode. |

`std::stable_sort` supplies the stable ordering; its comparator uses
`try_parse_double` for numeric values or string comparison otherwise.

## Use through the converter

Sorting is enabled through `LatexDraftOptions::sort_options`. In the converter's
export preparation flow, the converter first builds the header and rows,
applies its selected-column mapping and optional empty-row filtering, then
invokes `sort`. It applies `LatexDraftOptions::max_rows` after sorting and
continues through the existing layout resolution and LaTeX renderer.

For example:

```cpp
LatexDraftOptions options;
options.column_order = {0, 1, 2};
options.sort_options = sorttable::SortOptions{
    1, sorttable::SortOrder::Descending
};
options.max_rows = 10; // -1 means all rows; 0 means no data rows
options.layout_overrides.table_title = "Top scores";
converter.exportToFile("sorted_table.tex", options);
```

The sort-column index refers to the table after `column_order` has been
applied. It does not refer to the original CSV column index when columns have
been selected or reordered. If no `sort_options` is set, rows are not sorted.

`TableSorter` itself does not implement column selection, filtering, row
limits, layout overrides, or rendering. Those behaviors intentionally remain
in the converter so sorted and unsorted exports share the same output logic.

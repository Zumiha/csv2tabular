# sorttable

`sorttable` is a row-sorting helper used by `csv2xltabular`. It accepts the
converter's `TabularData` shape and mutates only its data rows. It does not
select or reorder columns, filter rows, apply row limits, resolve layout, or
render LaTeX.

## Sorted converter export

Configure sorting and any desired converter layout overrides on
`LatexDraftOptions`, then use the converter's regular LaTeX export path:

```cpp
LatexDraftOptions options;
options.column_order = {0, 1, 2}; // Converter selects/reorders these first
options.sort_options = sorttable::SortOptions{
    1, sorttable::SortOrder::Descending // Column index in the selected output table
};
options.max_rows = 10; // -1 = all, 0 = no data rows
options.layout_overrides.table_title = "Top scores";
options.layout_overrides.table_width = 150;
converter.exportToFile("sorted_table.tex", options);
```

The converter constructs the header and rows, applies column selection and
empty-row filtering, invokes the sorter, applies `max_rows`, resolves layout
overrides, and renders the LaTeX. Consequently sorting uses the column indices
of the selected table when `column_order` is non-empty.

## Sorting API

```cpp
sorttable::SortResult sorttable::sort(
    sorttable::TabularData& table,
    const sorttable::SortOptions& options);
```

`SortOptions::sort_column` is a zero-based index into `TabularData::header`.
Numeric ordering is used if every non-empty value parses as a finite `double`;
otherwise values are compared lexicographically. Empty and missing cells sort
as zero in numeric mode and as empty strings in lexicographic mode. Sorting is
stable, including for equal values. An invalid column throws
`std::out_of_range`. The result reports the selected mode as `"numeric"` or
`"lexicographic"`.

Run the focused sorter tests with CTest after configuring the project:

```text
ctest --test-dir build --output-on-failure
```

#ifndef LATEX_EXPORTER_H
#define LATEX_EXPORTER_H

#include "ConversionSettings.h"
#include "TableSorter.h"
#include "TabularData.h"

#include <optional>
#include <string>
#include <vector>

struct LatexDraftOptions {
    std::vector<int> column_order;
    std::string header_line_override;
    bool use_full_header_in_repeated_head = true;
    bool ignore_empty_rows = false;
    std::optional<sorttable::SortOptions> sort_options;
    int max_rows = -1;
    struct LayoutOverrides {
        std::optional<TableType> convert_type;
        std::optional<std::string> table_title;
        std::optional<int> table_width;
        std::optional<int> max_columns;
        std::optional<int> remdnr_min;
        std::optional<char> custom_column_type;
        std::optional<bool> numbered_header_line;
        std::optional<std::vector<int>> custom_column_number;
        std::optional<std::vector<int>> custom_column_width;
    } layout_overrides;
};

class LaTeXExporter {
public:
    static void write(
        const std::string& output_path,
        const TabularData& table,
        const TableLayoutOptions& layout,
        const LatexDraftOptions& options = {});

private:
    struct TableColumnRange {
        int col_start;
        int col_end;
    };

    struct CalculatedTableConfiguration {
        int table_width = 0;
        float row_header_width = 0.0f;
        std::vector<float> column_widths;
        std::vector<TableColumnRange> tables_rows_config;
    };

    std::string latex_string_;

    std::string draftLaTeX(
        const TabularData& table,
        const LatexDraftOptions& options,
        const TableLayoutOptions& layout);
    std::string headerLineRender(int start_cell, int end_cell, const std::vector<std::string>& header);
    std::string cellColorPrefix(
        int column,
        const std::string& value,
        const TableLayoutOptions& layout) const;
    void tableRender(
        int table_width,
        int start_cell,
        int end_cell,
        const std::vector<std::vector<std::string>>& rows,
        const std::string& header_line,
        const std::string& repeated_header_line,
        const TableLayoutOptions& layout,
        const std::vector<float>& column_widths,
        float row_header_width);
    CalculatedTableConfiguration calculateTableConfig(
        int header_size,
        const TableLayoutOptions& layout) const;
};

#endif // LATEX_EXPORTER_H

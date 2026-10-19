#include "LaTeXExporter.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

std::vector<std::string> SelectColumns(
    const std::vector<std::string>& row,
    const std::vector<int>& column_order)
{
    if (column_order.empty()) {
        return row;
    }

    std::vector<std::string> selected;
    selected.reserve(column_order.size());
    for (int col : column_order) {
        if (col >= 0 && col < static_cast<int>(row.size())) {
            selected.push_back(row[col]);
        } else {
            selected.emplace_back();
        }
    }
    return selected;
}

std::string BuildNumberedHeaderLine(
    size_t column_count,
    bool has_leading_number_column = false,
    bool add_horizontal_rule = true)
{
    std::string line = "\\rowcolor[RGB]{150,255,255} ";
    if (has_leading_number_column) {
        line += "\\cellcolor{white}& ";
    }
    for (size_t i = 0; i < column_count; ++i) {
        line += std::to_string(i + 1);
        if (i + 1 < column_count) {
            line += " & ";
        } else {
            line += add_horizontal_rule ? "\\\\ \\hline\n" : "\\\\\n";
        }
    }
    return line;
}

std::string AddLeadingHeaderCell(
    const std::string& header_line,
    bool use_numbered_header,
    size_t total_column_count)
{
    std::string result = "\\cellcolor{white}& " + header_line;
    const std::string horizontal_rule = "\\\\ \\hline\n";
    if (use_numbered_header && result.size() >= horizontal_rule.size() &&
        result.compare(result.size() - horizontal_rule.size(), horizontal_rule.size(), horizontal_rule) == 0) {
        result.replace(
            result.size() - horizontal_rule.size(),
            horizontal_rule.size(),
            "\\\\ \\cline{2-" + std::to_string(total_column_count) + "}\n");
    }
    return result;
}

std::string AppendNumberedHeaderLine(
    const std::string& header_line,
    const std::string& numbered_header_line,
    bool preserve_horizontal_rule = false)
{
    std::string combined = header_line;
    const std::string horizontal_rule = "\\hline\n";
    if (!preserve_horizontal_rule &&
        combined.size() >= horizontal_rule.size() &&
        combined.compare(combined.size() - horizontal_rule.size(), horizontal_rule.size(), horizontal_rule) == 0) {
        combined.erase(combined.size() - horizontal_rule.size());
    }
    if (combined.empty() || combined.back() != '\n') {
        combined += '\n';
    }
    combined += numbered_header_line;
    return combined;
}

bool IsCellEmpty(const std::string& value)
{
    if (value.empty()) {
        return true;
    }

    const auto first = value.find_first_not_of(" \t\r\n");
    return first == std::string::npos;
}

std::vector<std::vector<std::string>> FilterEmptyRows(
    const std::vector<std::vector<std::string>>& rows)
{
    std::vector<std::vector<std::string>> filtered;
    for (const auto& row : rows) {
        const bool has_empty_cell = std::any_of(row.begin(), row.end(), IsCellEmpty);
        if (!has_empty_cell) {
            filtered.push_back(row);
        }
    }
    return filtered;
}

char ResolveColumnType(char column_type)
{
    return column_type == '\0' ? 'm' : column_type;
}

void ValidateCellColorSettings(const TableLayoutOptions& layout, std::size_t column_count)
{
    const bool has_cell_colors = layout.cell_color_column.has_value() ||
        !layout.cell_color_values.empty() || !layout.cell_colors.empty();
    if (!has_cell_colors) {
        return;
    }

    if (!layout.cell_color_column) {
        throw std::invalid_argument("Cell colors require a target column");
    }
    if (*layout.cell_color_column < 0 ||
        static_cast<std::size_t>(*layout.cell_color_column) >= column_count) {
        throw std::invalid_argument("Cell color target column is outside the exported table");
    }
    if (layout.cell_color_values.empty()) {
        throw std::invalid_argument("Cell colors require at least one value/color rule");
    }
    if (layout.cell_color_values.size() != layout.cell_colors.size()) {
        throw std::invalid_argument("Cell color values and colors must have matching lengths");
    }
    for (const auto& color : layout.cell_colors) {
        if (color.empty() || color.find_first_of("\\{}\r\n") != std::string::npos) {
            throw std::invalid_argument("Cell colors must be non-empty xcolor specifications");
        }
    }
}

std::string BuildColumnSpec(char column_type, float width)
{
    std::ostringstream spec;
    spec << column_type;
    if (column_type != 'l' && column_type != 'c' && column_type != 'r' && column_type != 'X') {
        spec << "{" << std::fixed << std::setprecision(2) << width << "mm}";
    }
    return spec.str();
}

} // namespace

namespace {

TableLayoutOptions ResolveLayout(
    const TableLayoutOptions& defaults,
    const LatexDraftOptions::LayoutOverrides& overrides)
{
    TableLayoutOptions layout = defaults;
    if (overrides.convert_type) layout.convert_type = *overrides.convert_type;
    if (overrides.table_title) layout.table_title = *overrides.table_title;
    if (overrides.table_width) layout.table_width = *overrides.table_width;
    if (overrides.max_columns) layout.max_columns = *overrides.max_columns;
    if (overrides.remdnr_min) layout.remdnr_min = *overrides.remdnr_min;
    if (overrides.custom_column_type) layout.custom_column_type = *overrides.custom_column_type;
    if (overrides.numbered_header_line) layout.numbered_header_line = *overrides.numbered_header_line;
    if (overrides.custom_column_number) layout.custom_column_number = *overrides.custom_column_number;
    if (overrides.custom_column_width) layout.custom_column_width = *overrides.custom_column_width;
    return layout;
}

} // namespace

void LaTeXExporter::write(
    const std::string& output_path,
    const TabularData& table,
    const TableLayoutOptions& layout,
    const LatexDraftOptions& options)
{
    LaTeXExporter exporter;
    const auto contents = exporter.draftLaTeX(table, options, ResolveLayout(layout, options.layout_overrides));

    std::ofstream output(output_path);
    if (!output.is_open())
        throw std::runtime_error("Failed to open output file: " + output_path);

    output << contents;
    if (!output)
        throw std::runtime_error("Failed to write output file: " + output_path);
}

std::string LaTeXExporter::draftLaTeX(
    const TabularData& source_table,
    const LatexDraftOptions& options,
    const TableLayoutOptions& layout)
{
    TabularData table = source_table;
    table.header = SelectColumns(table.header, options.column_order);
    for (auto& row : table.rows)
        row = SelectColumns(row, options.column_order);

    if (options.ignore_empty_rows) {
        const auto before_count = table.rows.size();
        table.rows = FilterEmptyRows(table.rows);
        std::cout << "[DBG] ignore_empty_rows: kept " << table.rows.size() << " / " << before_count
                  << " rows across " << table.header.size() << " selected columns\n";
    }
    if (options.max_rows < -1)
        throw std::invalid_argument("max_rows must be -1 or greater");
    if (options.sort_options)
        sorttable::sort(table, *options.sort_options);
    if (options.max_rows >= 0 && table.rows.size() > static_cast<std::size_t>(options.max_rows))
        table.rows.resize(static_cast<std::size_t>(options.max_rows));

    ValidateCellColorSettings(layout, table.header.size());
    if (table.header.empty()) {
        latex_string_.clear();
        std::cerr << "[WARN] draftLaTeX: header is empty, returning empty LaTeX string\n";
        return latex_string_;
    }

    for (std::size_t i = 0; i < table.rows.size(); ++i) {
        if (table.rows[i].size() != table.header.size()) {
            throw std::invalid_argument(
                "LaTeXExporter: row " + std::to_string(i + 1)
                + " has " + std::to_string(table.rows[i].size())
                + " columns; expected " + std::to_string(table.header.size()));
        }
    }

    const auto& header = table.header;

    // first_head is the full header shown on the first page; repeated_head can be either the same header
    // or a numbered-only header depending on the caller's preference for page continuation.
    const std::string full_header = options.header_line_override.empty()
        ? headerLineRender(0, static_cast<int>(header.size()) - 1, header)
        : options.header_line_override;
    const bool use_numbered_header = layout.numbered_header_line || !options.use_full_header_in_repeated_head;
    const bool has_number_column = layout.convert_type == TableType::NumbColumn;
    const std::string full_header_with_number = has_number_column
        ? AddLeadingHeaderCell(full_header, use_numbered_header, header.size() + 1)
        : full_header;
    const std::string numbered_header = BuildNumberedHeaderLine(header.size(), has_number_column);
    const std::string first_head = use_numbered_header
                    ? AppendNumberedHeaderLine(full_header_with_number, numbered_header, true)
                    : full_header_with_number;
    const std::string repeated_head = use_numbered_header
                    ? BuildNumberedHeaderLine(header.size(), has_number_column, !has_number_column)
                    : full_header_with_number;

    latex_string_.clear();
    // latex_string_ += "\\newcounter{tablefigure}[section]\n"
    //                 "\\renewcommand{\\thetablefigure}{\\thesection.\\arabic{tablefigure}}\\n\n";

    int header_size = static_cast<int>(header.size());

    if (layout.convert_type == TableType::Default) {
        std::cout << "[LOG] Drafting LaTeX as Default format table\n";
    } else if (layout.convert_type == TableType::HeadColumn) {
        std::cout << "\n[LOG] Drafting LaTeX as Head Column format table\n";
    } else if (layout.convert_type == TableType::NumbColumn) {
        std::cout << "\n[LOG] Drafting LaTeX as Number Column format table\n";
    } else {
        std::cout << "Chosen convert type \"" << to_string(layout.convert_type) << "\" not implemented.";
        latex_string_.clear();
        return latex_string_;
    }

    auto calculated_table_configuration = calculateTableConfig(header_size, layout);

    for (size_t i = 0; i < calculated_table_configuration.tables_rows_config.size(); ++i) {
        int cell_start = calculated_table_configuration.tables_rows_config[i].col_start;
        int cell_end = calculated_table_configuration.tables_rows_config[i].col_end;
        std::string page_header = first_head;
        std::string page_repeated_header = repeated_head;

        // For multi-page splits, keep the first page full header while the continuation header can be reduced to numbering.
        if (cell_start != 0 || cell_end != header_size - 1) {
            const std::string page_full_header = options.header_line_override.empty()
                ? headerLineRender(cell_start, cell_end, header)
                : options.header_line_override;
            const std::string page_numbered_header = BuildNumberedHeaderLine(static_cast<size_t>(cell_end - cell_start + 1));
            const std::string page_full_header_with_number = has_number_column
                ? AddLeadingHeaderCell(
                    page_full_header,
                    use_numbered_header,
                    static_cast<size_t>(cell_end - cell_start + 2))
                : page_full_header;
            const std::string page_numbered_header_with_number = BuildNumberedHeaderLine(
                static_cast<size_t>(cell_end - cell_start + 1),
                has_number_column);
            page_header = use_numbered_header
                ? AppendNumberedHeaderLine(
                    page_full_header_with_number,
                    page_numbered_header_with_number,
                    true)
                : page_full_header_with_number;
            page_repeated_header = use_numbered_header
                ? BuildNumberedHeaderLine(
                    static_cast<size_t>(cell_end - cell_start + 1),
                    has_number_column,
                    !has_number_column)
                : page_full_header_with_number;
        }

        tableRender(
            calculated_table_configuration.table_width,
            cell_start,
            cell_end,
            table.rows,
            page_header,
            page_repeated_header,
            layout,
            calculated_table_configuration.column_widths,
            calculated_table_configuration.row_header_width
        );
    }    

    std::cout << "[LOG] Drafting LaTeX completed\n";
    return latex_string_;
}

std::string LaTeXExporter::headerLineRender(int start_cell, int end_cell, const std::vector<std::string>& header_)
{
    std::string line;
    for (auto i = start_cell; i <= end_cell; ++i) {
        line += header_[i];
        if (i < end_cell) {
            line += " & ";
        } else {
            line += "\\\\ \\hline\n";
        }
    }
    return line;
}

std::string LaTeXExporter::cellColorPrefix(
    int column,
    const std::string& value,
    const TableLayoutOptions& layout) const
{
    if (!layout.cell_color_column || column != *layout.cell_color_column) {
        return {};
    }

    for (std::size_t i = 0; i < layout.cell_color_values.size(); ++i) {
        if (value == std::to_string(layout.cell_color_values[i])) {
            return "\\cellcolor{" + layout.cell_colors[i] + "} ";
        }
    }
    return {};
}

void LaTeXExporter::tableRender(
    int _table_width,
    int start_cell,
    int end_cell,
    const std::vector<std::vector<std::string>>& rows,
    const std::string& header_line_,
    const std::string& repeated_header_line_,
    const TableLayoutOptions& layout,
    const std::vector<float>& column_widths,
    float row_header_width
) {
    auto table_width = end_cell - start_cell + 1;
    std::string col_spec;
    std::string diagbox;
    int total_cols = 0;
    switch (layout.convert_type)
    {
        case TableType::Default:
            total_cols = table_width;
            // for Default tables repeated pattern
            col_spec = "|";
            diagbox = "";
            break;
        case TableType::HeadColumn:
            total_cols = table_width + 1; // +1 for the row-header column
            // Row-header column ("|c|") only exists for HeadColumn tables
            col_spec = "|" + BuildColumnSpec(ResolveColumnType(layout.custom_column_type), row_header_width) + "|";
            diagbox = "\\diagbox{час}{L,мм} & ";
            break;
        case TableType::NumbColumn:
            total_cols = table_width + 1;
            col_spec = "|>{\\columncolor[RGB]{224,224,224}}" +
                BuildColumnSpec(ResolveColumnType(layout.custom_column_type), row_header_width) + "|";
            break;
        default:
            throw std::runtime_error("tableRender: unsupported TableType");
    }

    std::ostringstream oss;
    for (int i = start_cell; i <= end_cell; ++i) {
        oss << BuildColumnSpec(ResolveColumnType(layout.custom_column_type), column_widths[i]) << "|";
    }
    col_spec += oss.str();

    oss.str(""); // Clear the stream for reuse
    oss.clear(); // Clear any error flags

    oss << "\\setlength\\LTleft{0cm}\n"
        // << "\\stepcounter{tablefigure}\n"
        << "\\stepcounter{table}\n"
        // << "\\noindent Таблица~\\thetablefigure: " << layout.table_title << "~\\vspace{-0.75em}\n"
        << "\\noindent Таблица~\\thetable: " << layout.table_title << "~\\vspace{-0.75em}\n\\footnotesize\n"
        << "\\begin{xltabular}{" << std::to_string(_table_width) << "mm}{" << col_spec << "} \\hline\n"        << diagbox << header_line_
        << "\\endfirsthead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{@{}l}{\\small\\sl продолжение на предыдущей странице}\\\\ \\hline\n"
        << diagbox << repeated_header_line_
        << "\\endhead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{r}{\\small\\sl продолжение на следующей странице}\\\\ \n"
        << "\\endfoot\n"
        << "\\endlastfoot\n";

    // Fill table rows
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
        const auto& fields = rows[row_index];
        // If need Row Header
        if (layout.convert_type == TableType::HeadColumn) {
            oss << fields[0] << " & ";
        } else if (layout.convert_type == TableType::NumbColumn) {
            oss << row_index + 1 << " & ";
        }

        for (size_t i = start_cell; i < static_cast<size_t>(end_cell) + 1; ++i) {
            oss << cellColorPrefix(static_cast<int>(i), fields[i], layout) << fields[i];
            if (static_cast<int>(i) < end_cell) {
                oss << " & ";
            } else {
                oss << "\\\\ \\hline\n";
            }
        }
    }
    oss << "\\end{xltabular}%\n\\vspace{0em}\n\n";
    latex_string_ += oss.str();
}

LaTeXExporter::CalculatedTableConfiguration LaTeXExporter::calculateTableConfig(
    int header_size,
    const TableLayoutOptions& layout) const
{
    if (layout.table_width <= 0 || layout.max_columns <= 0 || layout.remdnr_min <= 0) {
        throw std::invalid_argument("Table layout width and column limits must be positive");
    }
    if (layout.custom_column_number.size() != layout.custom_column_width.size()) {
        std::cerr << "[WARN] Custom column numbers and widths must have matching lengths. "
                  << "custom_column_number.size() = " << layout.custom_column_number.size()
                  << ", custom_column_width.size() = " << layout.custom_column_width.size() << "\n";
        throw std::invalid_argument("Custom column numbers and widths must have matching lengths");
    }

    CalculatedTableConfiguration table_settings;
    table_settings.table_width = layout.table_width;
    const bool has_leading_column =
        layout.convert_type == TableType::HeadColumn || layout.convert_type == TableType::NumbColumn;
    const int offset = has_leading_column ? 1 : 0;
    const int first_data_column = layout.convert_type == TableType::HeadColumn ? 1 : 0;
    if (layout.max_columns <= offset) {
        throw std::invalid_argument("Table layout max_columns must exceed the leading-column offset");
    }

    // ── Step 1: resolve row-header width FIRST (default, or custom override) ──
    const float row_header_width = static_cast<float>(layout.table_width) / layout.max_columns;
    table_settings.row_header_width = row_header_width;

    // ── Step 2: default column width, now using the RESOLVED row header width ──
    int effective_columns = layout.max_columns - offset;
    float effective_table_width = static_cast<float>(layout.table_width) - (offset * row_header_width);
    float def_col_width = effective_table_width / effective_columns;

    std::cout << "\noffset=" << offset << " row_header_width=" << row_header_width
               << " effective_columns=" << effective_columns
               << " effective_table_width=" << effective_table_width
               << " def_col_width=" << def_col_width << "\n";

    // ── Step 3: build per-column width array, indexed by RAW field position ──
    std::vector<float> columns_width_array(header_size, def_col_width);

    for (size_t k = 0; k < layout.custom_column_number.size(); ++k) {
        int raw_idx = layout.custom_column_number[k];
        float width = static_cast<float>(layout.custom_column_width[k]);

        if (raw_idx < 0 || raw_idx >= header_size) {
            std::cerr << "[WARN] table_settings: column index " << (raw_idx + 1)
                      << " is outside the active table width (" << header_size << " columns). Ignoring custom width.\n";
            continue;
        }

        if (layout.convert_type == TableType::HeadColumn && raw_idx == 0)
            continue; // already resolved into row_header_width above

        columns_width_array[raw_idx] = width;
    }
    table_settings.column_widths = columns_width_array;

    // ── Guard: a single column wider than the largest possible page target
    //    can never be fixed by shrinking i, so fail fast instead of looping ──
    for (int col = first_data_column; col < header_size; ++col) {
        if (columns_width_array[col] > effective_table_width) {
            throw std::runtime_error(
                "calculateTableConfig: column " + std::to_string(col + 1) +
                " width (" + std::to_string(columns_width_array[col]) +
                "mm) exceeds the maximum available table width (" +
                std::to_string(effective_table_width) + "mm) — no page split could fit it");
        }
    }

    if (!layout.custom_column_number.empty()) {
        // Custom widths affect page breaks, but the baseline width remains
        // based on max_columns. Keep each page within both the width and
        // column-count limits; a short final page is valid.
        table_settings.tables_rows_config.clear();
        int col_start = first_data_column;
        int page_column_count = 0;
        float running_width = 0.0f;

        for (int col = first_data_column; col < header_size; ++col) {
            const float width = columns_width_array[col];
            if (page_column_count > 0 &&
                (page_column_count >= effective_columns ||
                 running_width + width > effective_table_width)) {
                table_settings.tables_rows_config.push_back({col_start, col - 1});
                col_start = col;
                page_column_count = 0;
                running_width = 0.0f;
            }

            running_width += width;
            ++page_column_count;
        }

        if (col_start < header_size) {
            table_settings.tables_rows_config.push_back({col_start, header_size - 1});
        }

        for (size_t p = 0; p < table_settings.tables_rows_config.size(); ++p) {
            std::cout << "Table " << p + 1 << ": col_start = " << table_settings.tables_rows_config[p].col_start
                      << ", col_end = " << table_settings.tables_rows_config[p].col_end << "\n";
        }
    } else {
        // Preserve the existing uniform-width page balancing when no custom
        // widths are configured.
        const float min_table_width = layout.remdnr_min * def_col_width;

        int i = 0;
        while (true) {
            float target_width = effective_table_width - i * def_col_width;
            if (target_width <= 0.0f)
                throw std::runtime_error("calculateTableConfig: no valid column split found. Change max_columns or min_columns to allow a valid split.");

            table_settings.tables_rows_config.clear();
            int col_start = first_data_column;
            float running_width = 0.0f;

            for (int col = first_data_column; col < header_size; ++col) {
                float w = columns_width_array[col];
                if (running_width > 0.0f && running_width + w > target_width) {
                    table_settings.tables_rows_config.push_back({col_start, col - 1});
                    col_start = col;
                    running_width = 0.0f;
                }
                running_width += w;
            }
            table_settings.tables_rows_config.push_back({col_start, header_size - 1});

            auto& last = table_settings.tables_rows_config.back();
            float last_page_width = 0.0f;
            for (int c = last.col_start; c <= last.col_end; ++c) last_page_width += columns_width_array[c];

            bool acceptable = (table_settings.tables_rows_config.size() == 1) || (last_page_width >= min_table_width);

            for (size_t p = 0; p < table_settings.tables_rows_config.size(); ++p) {
                std::cout << "Table " << p + 1 << ": col_start = " << table_settings.tables_rows_config[p].col_start
                           << ", col_end = " << table_settings.tables_rows_config[p].col_end << "\n";
            }

            if (acceptable) {
                std::cout << "Accepted at i=" << i << ", target_width=" << target_width
                           << ", last_page_width=" << last_page_width << " (min " << min_table_width << ")\n";
                break;
            }

            std::cout << "Rejected: last_page_width=" << last_page_width << " < min_table_width=" << min_table_width
                       << ". Retrying with i=" << (i + 1) << "\n";
            ++i;
        }
    }

    return table_settings;
}

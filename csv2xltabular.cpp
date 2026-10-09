#include "csv2xltabular.h"

#include <clocale>

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

std::string BuildNumberedHeaderLine(size_t column_count)
{
    std::string line;
    line += "\\rowcolor[RGB]{52,192,235}";
    for (size_t i = 0; i < column_count; ++i) {
        line += std::to_string(i + 1);
        if (i + 1 < column_count) {
            line += " & ";
        } else {
            line += "\\\\ \\hline\n";
        }
    }
    return line;
}

std::string AppendNumberedHeaderLine(
    const std::string& header_line,
    const std::string& numbered_header_line)
{
    std::string combined = header_line;
    const std::string horizontal_rule = "\\hline\n";
    if (combined.size() >= horizontal_rule.size() &&
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

char ResolveColumnType(char column_type)
{
    return column_type == '\0' ? 'm' : column_type;
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

CSVtoXLTABularConverter::CSVtoXLTABularConverter(const std::string& csv_filename, const std::string& ini_filename)
    : csv_filename_(csv_filename)
{
    ini_parser_ = std::make_unique<IniParser>(ini_filename);
    std::setlocale(LC_ALL, "Russian"); // Set locale to the user's environment default
}

CSVtoXLTABularConverter::~CSVtoXLTABularConverter() = default;

void CSVtoXLTABularConverter::convert()
{
    if (!ini_parser_)
        throw std::runtime_error("convert(): no ini file loaded — use the 2-arg constructor, or call Convert() directly with a ConversionSettings");

    settings_ = LoadSettingsFromIni(*ini_parser_);
    Convert(csv_filename_, settings_);
}

void CSVtoXLTABularConverter::Convert(const std::string& csv_filename, const ConversionSettings& settings)
{
    settings_ = settings;
    csv_parser_ = std::make_unique<CSVParser>(csv_filename);

    std::cout << "[LOG] Parsing CSV file\n";
    {
        IndentGuard guard(std::cerr, "\t");
        parsed_table_ = csv_parser_->parse_all(settings_.start_row, settings_.start_col);
    }

    project_table_.clear();
    if (!settings_.prj_cols.empty()) {
        project_table_ = ExtractConstantColumns(parsed_table_, settings_.prj_cols, settings_.prj_cols_header);
        normalizePrjCols(project_table_[2]);
    }
    std::cout << "[LOG] Parsing completed\n";

    std::cout << "[LOG] Converting table\n";
    transformTable();
    formatTable();
    std::cout << "[LOG] Conversion completed\n";
    table_converted_ = true;
}

void CSVtoXLTABularConverter::exportToFile(const std::string& output_filename) {
    exportToFileImpl(output_filename, {});
}

void CSVtoXLTABularConverter::exportToFile(const std::string& output_filename, const LatexDraftOptions& options) {
    exportToFileImpl(output_filename, options);
}

void CSVtoXLTABularConverter::exportToCSV(const std::string &output_filename, bool export_prj_table)
{
    std::cout << "[LOG] Exporting formatted table to CSV\n";
    if (table_converted_) {
        IndentGuard guard(std::cout, "\t");
        csv_parser_->export_csv(formatted_table_, output_filename);
    } else {
        std::cout << "[WARN] File was not converted, can't export" << std::endl;
        return;
    }

    if (export_prj_table) {
        // Extract project data from csv if available
        if (!settings_.prj_cols.empty()) {
            IndentGuard guard(std::cout, "\t");
            std::cout << "[DBG] Extracting project data" << std::endl;
            csv_parser_->export_csv(project_table_, "prj_info.csv");
        }
    }
}

void CSVtoXLTABularConverter::exportToFileImpl(const std::string& output_filename, const LatexDraftOptions& options) {
    if (!table_converted_) {
        throw std::runtime_error("Cannot export LaTeX before converting a CSV file");
    }
    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_filename);
    }
    const auto layout = ResolveLayout(settings_.table_layout, options.layout_overrides);
    outfile << draftLaTeX(buildLaTeXData(options), options, layout);
    outfile.close();
}

void CSVtoXLTABularConverter::transformTable()
{
    converted_table_ = parsed_table_; // Start with the raw parsed table
    // MTM SpreadSheet table conversion block
    if (!settings_.delete_cols.empty()) {
        // Delete columns not used in spreadsheet
        std::cout << "[LOG] Deleting specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            auto reversed_list = Apply1BasedTo0Based(settings_.delete_cols); // Apply 0 based counter
            std::sort(reversed_list.rbegin(), reversed_list.rend()); // Sort in descending order to avoid index shifting issues when deleting
            csv_parser_->deleteColumns(converted_table_, reversed_list); // Remove columns not used in spreadsheet
        }
        std::cout << "[LOG] Deletion completed\n";
    }

    // Merge columns
    if (!settings_.merge_from.empty()) {
        std::cout << "[LOG] Merging specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->mergeColumns(converted_table_, settings_.merge_from, settings_.merge_into);
        }
        std::cout << "[LOG] Merging completed\n";
    }

    // Check for column reordering
    if (!settings_.move_new_order.empty()) {
        std::cout << "[LOG] Reordering columns based on new_order\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(converted_table_, settings_.move_new_order);
        }
        std::cout << "[LOG] Moving columns complete\n";
    } else if (!settings_.move_from.empty()) {
        std::cout << "[LOG] Moving columns based on from+to\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(converted_table_, settings_.move_from, settings_.move_to);
        }
        std::cout << "[LOG] Moving columns complete\n";
    }

    // Normalize decimal columns
    if (!settings_.decimal_normalizations.empty()) {
        std::cout << "[LOG] Normalizing decimal columns\n";
        for (const auto& [precision, cols] : settings_.decimal_normalizations) {
            normalizeDecCols(converted_table_, cols, precision, settings_.decimal_delimiter);
        }
        std::cout << "[LOG] Normalization completed\n";
    }

}

void CSVtoXLTABularConverter::formatTable()
{
    formatted_table_ = converted_table_;
    if (settings_.include_header) {
        std::cout << "[LOG] Creating header for SpreadSheet" << std::endl;
        formatted_table_[0] = settings_.sheet_header;
    }
}

CSVtoXLTABularConverter::TabularData CSVtoXLTABularConverter::buildLaTeXData(
    const LatexDraftOptions& options) const
{
    TabularData data;
    auto row = converted_table_.begin();
    if (settings_.include_header) {
        data.header = settings_.sheet_header;
    } else if (row != converted_table_.end()) {
        data.header = row->second;
        ++row;
    }

    data.header = SelectColumns(data.header, options.column_order);
    for (; row != converted_table_.end(); ++row) {
        data.rows.push_back(SelectColumns(row->second, options.column_order));
    }

    if (options.ignore_empty_rows) {
        const auto before_count = data.rows.size();
        data.rows = FilterEmptyRows(data.rows);
        std::cout << "[DBG] ignore_empty_rows: kept " << data.rows.size() << " / " << before_count
                  << " rows across " << data.header.size() << " selected columns\n";
    }
    return data;
}

std::string CSVtoXLTABularConverter::draftLaTeX(
    const TabularData& source_table,
    const LatexDraftOptions& options,
    const TableLayoutOptions& layout)
{
    if (source_table.header.empty()) {
        latex_string_.clear();
        std::cerr << "[WARN] draftLaTeX: header is empty, returning empty LaTeX string\n";
        return latex_string_;
    }

    const auto& header = source_table.header;

    // first_head is the full header shown on the first page; repeated_head can be either the same header
    // or a numbered-only header depending on the caller's preference for page continuation.
    const std::string full_header = options.header_line_override.empty()
        ? headerLineRender(0, static_cast<int>(header.size()) - 1, header)
        : options.header_line_override;
    const std::string numbered_header = BuildNumberedHeaderLine(header.size());
    const bool use_numbered_header = layout.numbered_header_line || !options.use_full_header_in_repeated_head;
    const std::string first_head = use_numbered_header
                    ? AppendNumberedHeaderLine(full_header, numbered_header)
                    : full_header;
    const std::string repeated_head = use_numbered_header
                    ? numbered_header
                    : full_header;

    latex_string_.clear();
    // latex_string_ += "\\newcounter{tablefigure}[section]\n"
    //                 "\\renewcommand{\\thetablefigure}{\\thesection.\\arabic{tablefigure}}\\n\n";

    int header_size = static_cast<int>(header.size());

    if (layout.convert_type == TableType::Default) {
        std::cout << "[LOG] Drafting LaTeX as Default format table\n";
    } else if (layout.convert_type == TableType::HeadColumn) {
        std::cout << "\n[LOG] Drafting LaTeX as Head Column format table\n";
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
            page_header = use_numbered_header
                ? AppendNumberedHeaderLine(page_full_header, page_numbered_header)
                : page_full_header;
            page_repeated_header = use_numbered_header
                ? page_numbered_header
                : page_full_header;
        }

        tableRender(
            calculated_table_configuration.table_width,
            cell_start,
            cell_end,
            source_table.rows,
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

void CSVtoXLTABularConverter::normalizeDecCols(std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns_list, int precision, const std::string &delimiter)
{
    if (precision < 0)
        throw std::invalid_argument("precision must be >= 0");
    if (delimiter.empty())
        throw std::invalid_argument("delimiter must not be empty");

    for (auto& [row_num, fields] : table) {
        for (int col : columns_list) {
            int idx = col;

            if (idx < 0 || idx >= static_cast<int>(fields.size()))
                continue;

            std::string& val = fields[idx];
            if (val.empty()) continue;

            size_t dpos = val.find(delimiter);

            if (precision == 0) {
                // Remove delimiter and everything after it
                if (dpos != std::string::npos)
                    val.resize(dpos);
                continue;
            }

            if (dpos == std::string::npos) {
                // No delimiter found — append it and pad with zeros
                val += delimiter + std::string(precision, '0');
            } else {
                size_t after = val.size() - dpos - delimiter.size();

                if (after > static_cast<size_t>(precision)) {
                    // Too many digits — truncate
                    val.resize(dpos + delimiter.size() + precision);
                } else if (after < static_cast<size_t>(precision)) {
                    // Too few digits — pad with zeros
                    val += std::string(precision - after, '0');
                }
                // exact match: nothing to do
            }
        }
    }
}

void CSVtoXLTABularConverter::normalizePrjCols(std::vector<std::string>& row)
{
    std::vector<int> all_cols; all_cols.reserve(row.size());
    for (size_t i = 0; i < row.size(); ++i)
        all_cols.push_back(static_cast<int>(i));

    std::map<int, std::vector<std::string>> edit_map = {{0, row}};
    normalizeDecCols(edit_map, all_cols, 1, settings_.decimal_delimiter);
    row = edit_map[0];
}

bool CSVtoXLTABularConverter::IsEmptyRow(const std::vector<std::string> &row)
{
    for (const auto& s : row)
        if (!s.empty() && s != "\"\"") return false;
    return true;
}

std::map<int, std::vector<std::string>> CSVtoXLTABularConverter::ExtractColumns(const std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns)
{
    std::map<int, std::vector<std::string>> extracted;
    for (const auto& [row_num, fields] : table) {
        for (int col : columns) {
            if (col < 0 || col >= static_cast<int>(fields.size())) {
                throw std::out_of_range(
                    "ExtractColumns: column index " + std::to_string(col) +
                    " is out of range for row " + std::to_string(row_num) +
                    " with " + std::to_string(fields.size()) + " columns");
            }
            extracted[row_num].push_back(fields[col]);
        }
    }
    return extracted;
}

ConstantColumnsCheck CSVtoXLTABularConverter::CheckConstantRows(const std::map<int, std::vector<std::string>> &table)
{
    ConstantColumnsCheck result;

    for (const auto& [key, row] : table) {
        if (IsEmptyRow(row))
            continue; // skip empty rows and \"\" rows

        if (result.reference_row.empty()) {
            result.reference_row = row; // capture first non-empty row as reference
            continue;
        }

        if (row.size() != result.reference_row.size()) {
            std::ostringstream oss;
            oss << "Row " << key << ": column count mismatch ("
                << row.size() << " vs expected " << result.reference_row.size() << ")";
            result.errors.push_back(oss.str());
            continue; // can't compare element-wise against a different length
        }

        for (std::size_t i = 0; i < result.reference_row.size(); ++i) {
            if (row[i] != result.reference_row[i]) {
                std::ostringstream oss;
                oss << "Row " << key << ", col " << i
                    << ": value mismatch (\"" << row[i]
                    << "\" vs expected \"" << result.reference_row[i] << "\")";
                result.errors.push_back(oss.str());
            }
        }
    }

    return result;
}

std::map<int, std::vector<std::string>> CSVtoXLTABularConverter::ExtractConstantColumns(const std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns, const std::vector<std::string> &headers)
{
    auto extracted = ExtractColumns(table, columns);
    auto check = CheckConstantRows(extracted);
    if (!check.errors.empty())
        throw std::runtime_error(check.errors.front());

    std::map<int, std::vector<std::string>> result;
    if (!headers.empty())
        result[1] = headers;
    result[2] = check.reference_row;
    return result;
}

std::string CSVtoXLTABularConverter::headerLineRender(int start_cell, int end_cell, const std::vector<std::string> &header_)
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

void CSVtoXLTABularConverter::tableRender(
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
        << "\\begin{xltabular}{" << std::to_string(_table_width) << "mm}{" << col_spec << "}\n"
        << "\\hline\n"
        << diagbox << header_line_
        << "\\endfirsthead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{@{}l}{\\small\\sl продолжение на предыдущей странице}\\\\ \\hline\n"
        << diagbox << repeated_header_line_
        << "\\endhead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{r}{\\small\\sl продолжение на следующей странице}\\\\ \n"
        << "\\endfoot\n"
        << "\\endlastfoot\n";

    // Fill table rows
    for (const auto& fields : rows) {
        // If need Row Header
        if (layout.convert_type == TableType::HeadColumn) {
            oss << fields[0] << " & ";
        }

        for (size_t i = start_cell; i < static_cast<size_t>(end_cell) + 1; ++i) {
            oss << fields[i];
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

CSVtoXLTABularConverter::CalculatedTableConfiguration CSVtoXLTABularConverter::calculateTableConfig(
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
    const int offset = (layout.convert_type == TableType::HeadColumn) ? 1 : 0;
    if (layout.max_columns <= offset) {
        throw std::invalid_argument("Table layout max_columns must exceed the row-header offset");
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

        if (offset == 1 && raw_idx == 0) continue; // already resolved into row_header_width above

        columns_width_array[raw_idx] = width;
    }
    table_settings.column_widths = columns_width_array;

    // ── Guard: a single column wider than the largest possible page target
    //    can never be fixed by shrinking i, so fail fast instead of looping ──
    for (int col = offset; col < header_size; ++col) {
        if (columns_width_array[col] > effective_table_width) {
            throw std::runtime_error(
                "calculateTableConfig: column " + std::to_string(col + 1) +
                " width (" + std::to_string(columns_width_array[col]) +
                "mm) exceeds the maximum available table width (" +
                std::to_string(effective_table_width) + "mm) — no page split could fit it");
        }
    }

    // ── Step 4: pack pages by width, shrinking target width until the last
    //    page clears min_table_width (or there's only one page total) ──
    const float min_table_width = layout.remdnr_min * def_col_width;

    int i = 0;
    while (true) {
        float target_width = effective_table_width - i * def_col_width;
        if (target_width <= 0.0f)
            throw std::runtime_error("calculateTableConfig: no valid column split found. Change max_columns or min_columns to allow a valid split.");

        table_settings.tables_rows_config.clear();
        int col_start = offset;
        float running_width = 0.0f;

        for (int col = offset; col < header_size; ++col) {
            float w = columns_width_array[col];
            if (running_width > 0.0f && running_width + w > target_width) {
                table_settings.tables_rows_config.push_back({col_start, col - 1});
                col_start = col;
                running_width = 0.0f;
            }
            running_width += w;
        }
        table_settings.tables_rows_config.push_back({col_start, header_size - 1}); // post-loop push to add the last table

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

    return table_settings;
}

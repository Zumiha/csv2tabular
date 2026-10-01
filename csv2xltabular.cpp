#include "csv2xltabular.h"

#include <clocale>

namespace {

std::map<int, std::vector<std::string>> BuildExportTable(
    const std::map<int, std::vector<std::string>>& table,
    const LatexDraftOptions& options)
{
    std::map<int, std::vector<std::string>> export_table;
    for (const auto& [row_num, row] : table) {
        std::vector<std::string> export_row;
        if (options.column_order.empty()) {
            export_row = row;
        } else {
            export_row.reserve(options.column_order.size());
            for (int col : options.column_order) {
                if (col >= 0 && col < static_cast<int>(row.size())) {
                    export_row.push_back(row[col]);
                } else {
                    export_row.push_back("");
                }
            }
        }
        export_table[row_num] = std::move(export_row);
    }
    return export_table;
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

std::map<int, std::vector<std::string>> FilterEmptyRows(
    const std::map<int, std::vector<std::string>>& table)
{
    std::map<int, std::vector<std::string>> filtered;
    auto it = table.begin();
    if (it != table.end()) {
        filtered[it->first] = it->second;
        ++it;
    }

    for (; it != table.end(); ++it) {
        const auto& row = it->second;
        const bool has_empty_cell = std::any_of(row.begin(), row.end(), IsCellEmpty);
        if (!has_empty_cell) {
            filtered[it->first] = row;
        }
    }

    return filtered;
}

TableLayoutOptions LayoutFromSettings(const ConversionSettings& settings)
{
    TableLayoutOptions layout;
    layout.convert_type = settings.convert_type;
    layout.table_title = settings.table_title;
    layout.table_width = settings.table_width;
    layout.max_columns = settings.max_columns;
    layout.remdnr_min = settings.remdnr_min;
    layout.custom_column_type = settings.custom_column_type;
    layout.numbered_header_line = settings.numbered_header_line;
    layout.custom_column_number = settings.custom_column_number;
    layout.custom_column_width = settings.custom_column_width;
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
    std::cout << "[LOG] Parsing completed\n";

    std::cout << "[LOG] Converting table\n";
    draftTable();
    std::cout << "[LOG] Conversion completed\n";
    table_converted_ = true;
}

void CSVtoXLTABularConverter::exportToFile(const std::string& output_filename)
{
    exportToFileImpl(output_filename, {}, LayoutFromSettings(settings_));
}

void CSVtoXLTABularConverter::exportToFile(
    const std::string& output_filename,
    const LatexDraftOptions& options)
{
    exportToFileImpl(output_filename, options, options.layout);
}

void CSVtoXLTABularConverter::exportToFileImpl(
    const std::string& output_filename,
    const LatexDraftOptions& options,
    const TableLayoutOptions& layout)
{
    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_filename);
    }
    const auto export_table = BuildExportTable(parsed_table_, options);
    draftLaTeX(export_table, options, layout);
    outfile << this->latex_string_;
    outfile.close();
}

void CSVtoXLTABularConverter::draftTable()
{
    // Extract project data from csv if available
    if (!settings_.prj_cols.empty()) {
        std::cout << "[LOG] Extracting project data\n";
        auto prj_info_table = ExtractConstantColumns(parsed_table_, settings_.prj_cols, settings_.prj_cols_header);
        normalizePrjCols(prj_info_table);
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->export_csv(prj_info_table, "prj_info.csv");
        }
        std::cout << "[LOG] Project data exported\n";
    }

    // MTM SpreadSheet table conversion block
    if (!settings_.delete_cols.empty()) {
        // Delete columns not used in spreadsheet
        std::cout << "[LOG] Deleting specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            auto reversed_list = Apply1BasedTo0Based(settings_.delete_cols); // Apply 0 based counter
            std::sort(reversed_list.rbegin(), reversed_list.rend()); // Sort in descending order to avoid index shifting issues when deleting
            csv_parser_->deleteColumns(parsed_table_, reversed_list); // Remove columns not used in spreadsheet
        }
        std::cout << "[LOG] Deletion completed\n";
    }

    // Merge columns
    if (!settings_.merge_from.empty()) {
        std::cout << "[LOG] Merging specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->mergeColumns(parsed_table_, settings_.merge_from, settings_.merge_into);
        }
        std::cout << "[LOG] Merging completed\n";
    }

    // Check for column reordering
    if (!settings_.move_new_order.empty()) {
        std::cout << "[LOG] Reordering columns based on new_order\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(parsed_table_, settings_.move_new_order);
        }
        std::cout << "[LOG] Moving columns complete\n";
    } else if (!settings_.move_from.empty()) {
        std::cout << "[LOG] Moving columns based on from+to\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(parsed_table_, settings_.move_from, settings_.move_to);
        }
        std::cout << "[LOG] Moving columns complete\n";
    }

    // Normalize decimal columns
    if (!settings_.decimal_normalizations.empty()) {
        std::cout << "[LOG] Normalizing decimal columns\n";
        for (const auto& [precision, cols] : settings_.decimal_normalizations) {
            normalizeDecCols(parsed_table_, cols, precision, settings_.decimal_delimiter);
        }
        std::cout << "[LOG] Normalization completed\n";
    }

    // Attach header, if enabled
    if (settings_.include_header) {
        std::cout << "[LOG] Creating header for SpreadSheet" << std::endl;
        parsed_table_[0] = settings_.sheet_header;
    }

    // Custom column widths were already validated (non-empty + equal length)
    // by LoadSettingsFromIni() / ValidateSettings() before reaching here.
}

std::string CSVtoXLTABularConverter::draftLaTeX(
    const std::map<int, std::vector<std::string>>& source_table,
    const LatexDraftOptions& options,
    const TableLayoutOptions& layout)
{
    auto table = source_table;

    if (table.empty()) {
        latex_string_.clear();
        std::cerr << "[WARN] draftLaTeX: table is empty, returning empty LaTeX string\n";
        return latex_string_;
    }

    // Use the first row as the active header; it can be overridden explicitly by caller-provided LaTeX.
    std::vector<std::string> header = table.begin()->second;
    if (header.empty()) {
        latex_string_.clear();
        std::cerr << "[WARN] draftLaTeX: header row is empty, returning empty LaTeX string\n";
        return latex_string_;
    }

    if (options.ignore_empty_rows) {
        const auto before_count = table.size();
        table = FilterEmptyRows(table);
        std::cout << "[DBG] ignore_empty_rows: kept " << table.size() << " / " << before_count
                  << " rows across " << header.size() << " selected columns\n";
    }

    // first_head is the full header shown on the first page; repeated_head can be either the same header
    // or a numbered-only header depending on the caller's preference for page continuation.
    const std::string full_header = options.header_line_override.empty()
        ? headerLineRender(0, static_cast<int>(header.size()) - 1, header)
        : options.header_line_override;
    const std::string numbered_header = BuildNumberedHeaderLine(header.size());
    const bool append_numbered_header = layout.numbered_header_line || !options.use_full_header_in_repeated_head;
    const bool numbered_only_repeated_header =
        layout.numbered_header_line || !options.use_full_header_in_repeated_head;
    const std::string first_head = append_numbered_header
        ? AppendNumberedHeaderLine(full_header, numbered_header)
        : full_header;
    const std::string repeated_head = numbered_only_repeated_header
        ? numbered_header
        : full_header;

    latex_string_.clear();
    // latex_string_ += "\\newcounter{tablefigure}[section]\n"
    //                 "\\renewcommand{\\thetablefigure}{\\thesection.\\arabic{tablefigure}}\\n\n";

    int header_size = static_cast<int>(header.size());
    auto calculated_table_configuration = calculateTableConfig(header_size, layout);

    for (size_t i = 0; i < calculated_table_configuration.tables_rows_config.size(); ++i) {
        int cell_start = calculated_table_configuration.tables_rows_config[i].col_start;
        int cell_end = calculated_table_configuration.tables_rows_config[i].col_end;
        int table_size = cell_end - cell_start + 1;

        std::string page_header = first_head;
        std::string page_repeated_header = repeated_head;

        // For multi-page splits, keep the first page full header while the continuation header can be reduced to numbering.
        if (cell_start != 0 || cell_end != header_size - 1) {
            const std::string page_full_header = options.header_line_override.empty()
                ? headerLineRender(cell_start, cell_end, header)
                : options.header_line_override;
            const std::string page_numbered_header =
                BuildNumberedHeaderLine(static_cast<size_t>(cell_end - cell_start + 1));
            page_header = append_numbered_header
                ? AppendNumberedHeaderLine(page_full_header, page_numbered_header)
                : page_full_header;
            page_repeated_header = numbered_only_repeated_header
                ? page_numbered_header
                : page_full_header;
        }

        tableRender(
            calculated_table_configuration.table_width,
            table_size,
            cell_start,
            cell_end,
            table,
            page_header,
            page_repeated_header,
            layout,
            calculated_table_configuration.column_widths,
            calculated_table_configuration.row_header_width
        );
    }

    if (layout.convert_type == TableType::Default) {
        std::cout << "[LOG] Drafting LaTeX as Default format table\n";
    } else if (layout.convert_type == TableType::HeadColumn) {
        std::cout << "\n[LOG] Drafting LaTeX as Head Column format table\n";
    } else {
        std::cout << "Chosen convert type \"" << to_string(layout.convert_type) << "\" not implemented.";
        latex_string_.clear();
        return latex_string_;
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

void CSVtoXLTABularConverter::normalizePrjCols(std::map<int, std::vector<std::string>> &table)
{
    auto vec_size = table[2].size();
    std::vector<int> all_cols{}; all_cols.reserve(vec_size);
    for (size_t i = 0; i < vec_size; i++) all_cols.push_back(static_cast<int>(i));

    std::map<int, std::vector<std::string>> edit_map;
    auto it = table.find(2);
    edit_map.insert(*it);

    normalizeDecCols(edit_map, all_cols, 1, this->settings_.decimal_delimiter);
    table[2] = edit_map[2];
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
    int table_size,
    int start_cell,
    int end_cell,
    const std::map<int, std::vector<std::string>>& table_,
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
    const int header_row = table_.begin()->first;
    for (const auto& [row_num, fields] : table_) {
        if (row_num == header_row) {
            continue; // Skip header row
        }
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
    (void)table_size;
}

CSVtoXLTABularConverter::CalculatedTableConfiguration CSVtoXLTABularConverter::calculateTableConfig(
    int header_size,
    const TableLayoutOptions& layout) const
{
    if (layout.table_width <= 0 || layout.max_columns <= 0 || layout.remdnr_min <= 0) {
        throw std::invalid_argument("Table layout width and column limits must be positive");
    }
    if (layout.custom_column_number.size() != layout.custom_column_width.size()) {
        throw std::invalid_argument("Custom column numbers and widths must have matching lengths");
    }
    const char column_type = ResolveColumnType(layout.custom_column_type);

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

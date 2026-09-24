#include "csv2xltabular.h"

#include <clocale>

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
    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_filename);
    }
    draftLaTeX();
    outfile << this->latex_string_;
    outfile.close();
}

void CSVtoXLTABularConverter::draftTable()
{
    // Extract project data from csv if available
    if (!settings_.prj_cols.empty()) {
        std::cout << "[LOG] Extracting project data\n";
        auto prj_info_table = extractAndValidate(parsed_table_, settings_.prj_cols, settings_.prj_cols_header);
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

void CSVtoXLTABularConverter::draftLaTeX()
{
    // Reset LaTeX string
    latex_string_.clear();

    std::vector<std::string> header;
    switch (settings_.convert_type)
    {
    case TableType::Default:
        std::cout << "[LOG] Drafting LaTeX as Default format table\n";
        break;
    case TableType::HeadColumn:
        std::cout << "\n[LOG] Drafting LaTeX as Head Column format table\n";
        break;
    default:
        std::cout << "Chosen convert type \"" << to_string(settings_.convert_type) << "\" not implemented.";
        return;
    }

    if (settings_.include_header) {
        // If header settings are available, use it as the first row for LaTeX rendering
        header = parsed_table_[0];
    } else {
        // If no header settings, use the first row of the parsed table as the header for LaTeX rendering
        header = parsed_table_[1];
    }

    // Table measurement conversion block
    int header_size = static_cast<int>(header.size());
    std::cout << "\nheader_size: " << header_size << "\n";
    auto calculated_table_configuration = calculateTableConfig(header_size);

    std::string header_line;
    latex_string_ += "\\newcounter{tablefigure}[section]\n"
                      "\\renewcommand{\\thetablefigure}{\\thesection.\\arabic{tablefigure}}\n\n";
    for (size_t i = 0; i < calculated_table_configuration.tables_rows_config.size(); i++) {
        // Render header line
        int cell_start = calculated_table_configuration.tables_rows_config[i].col_start;
        int cell_end = calculated_table_configuration.tables_rows_config[i].col_end;
        int table_size = cell_end - cell_start + 1;

        std::cout << "\ncolumn_start: " << cell_start << " column_end: " << cell_end << "\n";
        header_line = headerLineRender(
            cell_start,
            cell_end,
            header
        );
        std::cout << header_line << std::endl;
        // LaTeX tabular format

        tableRender(
            calculated_table_configuration.table_width,
            table_size,
            cell_start,
            cell_end,
            parsed_table_,
            header_line,
            calculated_table_configuration.column_widths,
            calculated_table_configuration.row_header_width
        );
    }

    std::cout << "[LOG] Drafting LaTeX completed\n";
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

    normalizeDecCols(edit_map, all_cols, 1);
    table[2] = edit_map[2];
}

bool CSVtoXLTABularConverter::isEmptyRow(const std::vector<std::string> &vec)
{
    bool all_empty = true;
    for (const auto& s : vec)
        if (!s.empty() && s != "\"\"") all_empty = false;
    return all_empty;
}

std::map<int, std::vector<std::string>> CSVtoXLTABularConverter::extractAndValidate(const std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns_list, const std::vector<std::string> &header_list)
{
    std::cout << "[LOG] Extracting and validating project data.\n";

    auto data_from_table = csv_parser_->extractTable(table, columns_list);

    std::vector<std::string> values; // first non-empty row captured here
    for (const auto& [key, row] : data_from_table) {
        if (isEmptyRow(row)) continue; // skip empty rows and \"\" rows
        if (values.empty()) {
            values = row; // capture first non-empty row as reference
            continue;
        }

        // Subsequent non-empty rows: must match values exactly
        if (row.size() != values.size()) {
            std::ostringstream oss;
            oss << "Row " << key << ": column count mismatch ("
                << row.size() << " vs expected " << values.size() << ")";
            throw std::runtime_error(oss.str());
        }

        for (std::size_t i = 0; i < values.size(); ++i) {
            if (row[i] != values[i] ) {
                std::ostringstream oss;
                oss << "Row " << key << ", col " << i
                    << ": value mismatch (\"" << row[i]
                    << "\" vs expected \"" << values[i] << "\")";
                throw std::runtime_error(oss.str());
            }
        }
    }
    std::map<int, std::vector<std::string>> prj_info_table;

    prj_info_table[1] = header_list;
    prj_info_table[2] = values;

    return prj_info_table;
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
    const std::vector<float>& column_widths,
    float row_header_width
) {
    auto table_width = end_cell - start_cell + 1;
    std::string col_spec;
    std::string diagbox;
    int total_cols = 0;
    switch (settings_.convert_type)
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
            col_spec = "|m{" + std::to_string(row_header_width) + "mm}|";
            diagbox = "\\diagbox{час}{L,мм} & ";
            break;
        default:
            throw std::runtime_error("tableRender: unsupported TableType");
    }

    std::ostringstream oss;
    for (int i = start_cell; i <= end_cell; ++i) {
        oss << "m{" << std::fixed << std::setprecision(2) << column_widths[i] << "mm}|";
    }
    col_spec += oss.str();

    oss.str(""); // Clear the stream for reuse
    oss.clear(); // Clear any error flags

    oss << "\\setlength\\LTleft{0cm}\n"
        << "\\stepcounter{tablefigure}\n"
        << "\\noindent Таблица~\\thetablefigure: " << settings_.table_title << "~\\vspace{-0.75em}\n"
        << "\\begin{xltabular}{" << std::to_string(_table_width) << "mm}{" << col_spec << "}\n"
        << "\\hline\n"
        << diagbox << header_line_
        << "\\endfirsthead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{@{}l}{\\small\\sl продолжение на предыдущей странице}\\\\ \\hline\n"
        << diagbox << header_line_
        << "\\endhead\n"
        << "\\multicolumn{" << std::to_string(total_cols) << "}{r}{\\small\\sl продолжение на следующей странице}\\\\ \n"
        << "\\endfoot\n"
        << "\\endlastfoot\n";

    // Fill table rows
    int header_row = settings_.include_header ? 0 : 1; // Header row is 0 if header added in settings, if no header in settings then 1
    for (const auto& [row_num, fields] : table_) {
        if (row_num == header_row) {
            continue; // Skip header row
        }
        // If need Row Header
        if (settings_.convert_type == TableType::HeadColumn) {
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

ConversionSettings CSVtoXLTABularConverter::calculateTableConfig(int _header_size)
{
    ConversionSettings table_settings = this->settings_;
    const int offset = (settings_.convert_type == TableType::HeadColumn) ? 1 : 0;

    // ── Step 1: resolve row-header width FIRST (default, or custom override) ──
    float _row_header_width = static_cast<float>(table_settings.table_width) / table_settings.max_columns;
    if (offset == 1) {
        for (size_t k = 0; k < settings_.custom_column_number.size(); ++k) {
            if (settings_.custom_column_number[k] == 0) {
                float custom_width = static_cast<float>(settings_.custom_column_width[k]);
                if (custom_width < _row_header_width) {
                    std::cerr << "[WARN] table_settings: custom row-header width (" << custom_width
                               << "mm) is smaller than default (table_width / max_clolumns = " << _row_header_width
                               << "mm). Keeping default.\n";
                } else {
                    table_settings.row_header_width = custom_width;
                }
                break; // raw index 0 can only appear once
            }
        }
    }
    table_settings.row_header_width = _row_header_width;

    // ── Step 2: default column width, now using the RESOLVED row header width ──
    int effective_columns = table_settings.max_columns - offset;
    float effective_table_width = static_cast<float>(table_settings.table_width) - (offset * _row_header_width);
    float def_col_width = effective_table_width / effective_columns;

    std::cout << "\noffset=" << offset << " row_header_width=" << _row_header_width
               << " effective_columns=" << effective_columns
               << " effective_table_width=" << effective_table_width
               << " def_col_width=" << def_col_width << "\n";

    // ── Step 3: build per-column width array, indexed by RAW field position ──
    std::vector<float> columns_width_array(_header_size, def_col_width);

    for (size_t k = 0; k < settings_.custom_column_number.size(); ++k) {
        int raw_idx = settings_.custom_column_number[k];
        float width = static_cast<float>(settings_.custom_column_width[k]);

        if (raw_idx < 0 || raw_idx >= _header_size)
            throw std::runtime_error("table_settings.column_number: index " + std::to_string(raw_idx + 1) +
                                      " out of range for " + std::to_string(_header_size) + " columns");

        if (offset == 1 && raw_idx == 0) continue; // already resolved into row_header_width above

        columns_width_array[raw_idx] = width;
    }
    table_settings.column_widths = columns_width_array;

    // ── Guard: a single column wider than the largest possible page target
    //    can never be fixed by shrinking i, so fail fast instead of looping ──
    for (int col = offset; col < _header_size; ++col) {
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
    const float min_table_width = table_settings.remdnr_min * def_col_width;

    int i = 0;
    while (true) {
        float target_width = effective_table_width - i * def_col_width;
        if (target_width <= 0.0f)
            throw std::runtime_error("calculateTableConfig: no valid column split found. Change max_columns or min_columns to allow a valid split.");

        table_settings.tables_rows_config.clear();
        int col_start = offset;
        float running_width = 0.0f;

        for (int col = offset; col < _header_size; ++col) {
            float w = columns_width_array[col];
            if (running_width > 0.0f && running_width + w > target_width) {
                table_settings.tables_rows_config.push_back({col_start, col - 1});
                col_start = col;
                running_width = 0.0f;
            }
            running_width += w;
        }
        table_settings.tables_rows_config.push_back({col_start, _header_size - 1}); // post-loop push to add the last table

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

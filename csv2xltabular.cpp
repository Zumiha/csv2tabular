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
    if (!ini_parser_) {
        invalidateConversion();
        throw std::runtime_error("convert(): no ini file loaded — use the 2-arg constructor, or call Convert() directly with a ConversionSettings");
    }

    ConversionSettings settings;
    try {
        settings = LoadSettingsFromIni(*ini_parser_);
    } catch (...) {
        invalidateConversion();
        throw;
    }
    Convert(csv_filename_, settings);
}

void CSVtoXLTABularConverter::Convert(const std::string& csv_filename, const ConversionSettings& settings)
{
    invalidateConversion();
    try {
        settings_ = settings;
        csv_parser_ = std::make_unique<CSVParser>(csv_filename);

        std::cout << "[LOG] Parsing CSV file\n";
        {
            IndentGuard guard(std::cerr, "\t");
            parsed_table_ = csv_parser_->parse_all(settings_.start_row, settings_.start_col);
        }

        if (!settings_.prj_cols.empty()) {
            const auto project_check = CheckProjectColumns(settings_);
            if (!project_check.errors.empty())
                throw std::runtime_error(project_check.errors.front());
        }
        std::cout << "[LOG] Parsing completed\n";

        std::cout << "[LOG] Converting table\n";
        transformTable();
        std::cout << "[LOG] Conversion completed\n";
        table_converted_ = true;
    } catch (...) {
        invalidateConversion();
        throw;
    }
}

void CSVtoXLTABularConverter::invalidateConversion() noexcept
{
    table_converted_ = false;
    parsed_table_ = {};
    converted_table_ = {};
    csv_parser_.reset();
}

void CSVtoXLTABularConverter::exportToFile(const std::string& output_filename) {
    exportToFileImpl(output_filename, {});
}

void CSVtoXLTABularConverter::exportToFile(const std::string& output_filename, const LatexDraftOptions& options) {
    exportToFileImpl(output_filename, options);
}

void CSVtoXLTABularConverter::exportToCSV(const std::string& output_filename)
{
    std::cout << "[LOG] Exporting formatted table to CSV\n";
    if (!table_converted_) {
        std::cout << "[WARN] File was not converted, can't export" << std::endl;
        return;
    }
    TabularData export_data;
    if (settings_.include_header) {
        export_data.header = settings_.sheet_header;
        export_data.rows.reserve(converted_table_.rows.size() + 1);
        export_data.rows.push_back(converted_table_.header);
        export_data.rows.insert(
            export_data.rows.end(), converted_table_.rows.begin(), converted_table_.rows.end());
    } else {
        export_data = converted_table_;
    }
    {
        IndentGuard guard(std::cout, "\t");
        csv_parser_->export_csv(export_data, output_filename);
    }

    if (!settings_.prj_cols.empty() && !settings_.prj_cols_header.empty())
        exportProjectInfoCSV(settings_);
}

void CSVtoXLTABularConverter::exportProjectInfoCSV(
    const ConversionSettings& settings,
    const std::string& output_filename)
{
    if (!table_converted_ || !csv_parser_)
        throw std::runtime_error("Cannot export project information before converting a CSV file");

    if (settings.prj_cols.empty() && settings.prj_cols_header.empty()) {
        std::cout << "[WARN] Project columns are not configured; no project info file was created\n";
        return;
    }
    if (settings.prj_cols.empty() || settings.prj_cols_header.empty()) {
        throw std::invalid_argument(
            "Project info export requires both prj_cols and prj_cols_header");
    }
    if (settings.prj_cols.size() != settings.prj_cols_header.size()) {
        throw std::invalid_argument(
            "Project info export requires prj_cols and prj_cols_header to have matching lengths");
    }

    IndentGuard guard(std::cout, "\t");
    std::cout << "[DBG] Extracting project data" << std::endl;
    csv_parser_->export_csv(buildProjectTable(settings), output_filename);
}

void CSVtoXLTABularConverter::exportToFileImpl(const std::string& output_filename, const LatexDraftOptions& options) {
    if (!table_converted_) {
        throw std::runtime_error("Cannot export LaTeX before converting a CSV file");
    }
    LaTeXExporter::write(output_filename, buildLaTeXData(), settings_.table_layout, options);
}

void CSVtoXLTABularConverter::transformTable()
{
    converted_table_ = parsed_table_;
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

TabularData CSVtoXLTABularConverter::buildLaTeXData() const
{
    TabularData data;
    if (settings_.include_header) {
        data.header = settings_.sheet_header;
        data.rows.reserve(converted_table_.rows.size() + 1);
        data.rows.push_back(converted_table_.header);
        data.rows.insert(data.rows.end(), converted_table_.rows.begin(), converted_table_.rows.end());
    } else {
        data.header = converted_table_.header;
        data.rows = converted_table_.rows;
    }

    return data;
}

void CSVtoXLTABularConverter::normalizeDecCols(TabularData& table, const std::vector<int>& columns_list, int precision, const std::string& delimiter)
{
    if (precision < 0)
        throw std::invalid_argument("precision must be >= 0");
    if (delimiter.empty())
        throw std::invalid_argument("delimiter must not be empty");

    const auto normalize_row = [&](std::vector<std::string>& fields) {
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
    };
    normalize_row(table.header);
    for (auto& fields : table.rows)
        normalize_row(fields);
}

TabularData CSVtoXLTABularConverter::ExtractColumns(const TabularData& table, const std::vector<int>& columns) const
{
    TabularData extracted;
    const auto extract_row = [&](const std::vector<std::string>& fields, std::vector<std::string>& result, int row_num) {
        for (int col : columns) {
            if (col < 0 || col >= static_cast<int>(fields.size())) {
                throw std::out_of_range(
                    "ExtractColumns: column index " + std::to_string(col) +
                    " is out of range for row " + std::to_string(row_num) +
                    " with " + std::to_string(fields.size()) + " columns");
            }
            result.push_back(fields[col]);
        }
    };
    extract_row(table.header, extracted.header, 1);
    for (std::size_t i = 0; i < table.rows.size(); ++i) {
        extracted.rows.emplace_back();
        extract_row(table.rows[i], extracted.rows.back(), static_cast<int>(i + 2));
    }
    return extracted;
}

ConstantColumnsCheck CSVtoXLTABularConverter::CheckConstantRows(const TabularData& table) const
{
    ConstantColumnsCheck result;

    std::vector<const std::vector<std::string>*> rows;
    rows.reserve(table.rows.size() + 1);
    rows.push_back(&table.header);
    for (const auto& row : table.rows)
        rows.push_back(&row);
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
        const auto& row = *rows[row_index];
        const int key = static_cast<int>(row_index + 1);
        const bool empty = std::all_of(row.begin(), row.end(), [](const std::string& value) {
            return value.empty() || value == "\"\"";
        });
        if (empty)
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

ConstantColumnsCheck CSVtoXLTABularConverter::CheckProjectColumns(
    const ConversionSettings& settings) const
{
    return CheckConstantRows(ExtractColumns(parsed_table_, settings.prj_cols));
}

TabularData CSVtoXLTABularConverter::buildProjectTable(
    const ConversionSettings& settings) const
{
    const auto check = CheckProjectColumns(settings);
    if (!check.errors.empty())
        throw std::runtime_error(check.errors.front());

    TabularData project_table;
    project_table.header = settings.prj_cols_header;
    project_table.rows.push_back(check.reference_row);
    std::vector<int> columns;
    columns.reserve(project_table.header.size());
    for (std::size_t i = 0; i < project_table.header.size(); ++i)
        columns.push_back(static_cast<int>(i));
    TabularData project_values;
    project_values.rows = project_table.rows;
    normalizeDecCols(project_values, columns, 1, settings.decimal_delimiter);
    project_table.rows = std::move(project_values.rows);
    return project_table;
}

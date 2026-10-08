#ifndef CSV2XTABULAR_H
#define CSV2XTABULAR_H

#include "ConversionSettings.h"
#include "csvParser.h"
#include "IniParser.h"
#include "Utils.h"

#include <memory>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>

#include <algorithm>
#include <iomanip>

struct TableLayoutOptions {
    TableType convert_type = TableType::Default;
    std::string table_title = "default_title";
    int table_width = 180;
    int max_columns = 12;
    int remdnr_min = 8;
    char custom_column_type = '\0';
    bool numbered_header_line = false;
    std::vector<int> custom_column_number;
    std::vector<int> custom_column_width;
};

struct LatexDraftOptions {
    std::vector<int> column_order;        // 0-based; empty => current order
    std::string header_line_override;     // optional explicit header line to use
    bool use_full_header_in_repeated_head = true; // false => first head full + numbering, repeated head numbering only
    bool ignore_empty_rows = false;        // if true, drop rows with any empty selected column
    TableLayoutOptions layout;
};

class CSVtoXLTABularConverter {
public:
    // GUI-friendly: no file I/O at construction. Build a ConversionSettings
    // (by hand or via LoadSettingsFromIni()) and call Convert() directly.
    CSVtoXLTABularConverter() = default;

    // CLI convenience: matches the original 2-arg constructor exactly —
    // opens the ini file eagerly, ready for a convert() call.
    CSVtoXLTABularConverter(const std::string& csv_filename, const std::string& ini_filename);

    CSVtoXLTABularConverter(const CSVtoXLTABularConverter&) = delete;
    ~CSVtoXLTABularConverter();

    // The core engine: pure function of (csv path, settings) — no reference
    // to any ini file. This is what the GUI calls directly.
    void Convert(const std::string& csv_filename, const ConversionSettings& settings);

    // CLI convenience wrapper, unchanged behavior from before the refactor:
    // loads settings from the ini file given to the constructor, then calls
    // Convert(). Throws if the 2-arg constructor wasn't used.
    void convert();

    void exportToFile(const std::string& output_filename = "wt_table.tex");
    void exportToFile(const std::string& output_filename, const LatexDraftOptions& options);
    void exportToCSV(const std::string& output_filename = "debug.csv", bool export_prj_table = false);

    std::map<int, std::vector<std::string>> getParsedTable() const {
        return parsed_table_;
    }

private:
    std::string csv_filename_;         // set by the 2-arg ctor, for convert()'s sake
    std::unique_ptr<CSVParser> csv_parser_; // (re)created fresh inside Convert()
    std::unique_ptr<IniParser> ini_parser_; // set by the 2-arg ctor, for convert()'s sake

    ConversionSettings settings_;
    std::map<int, std::vector<std::string>> parsed_table_; // raw table after CSV parsing
    std::map<int, std::vector<std::string>> converted_table_; // holds the table after all transformations applied
    std::map<int, std::vector<std::string>> formatted_table_; // converted table + added header (if enabled)
    std::map<int, std::vector<std::string>> project_table_;
    bool table_converted_ = false;

    void draftTable();
    void exportToFileImpl(const std::string& output_filename, const LatexDraftOptions& options, const TableLayoutOptions& layout);
    std::string draftLaTeX(const std::map<int, std::vector<std::string>>& table, const LatexDraftOptions& options, const TableLayoutOptions& layout);

    void normalizeDecCols(std::map<int, std::vector<std::string>>& table, const std::vector<int>& columns_list, int precision = 0, const std::string& delimiter = ",");
    void normalizePrjCols(std::map<int, std::vector<std::string>>& table);
    
    bool IsEmptyRow(const std::vector<std::string>& row);
    // Pulls the given raw column indices out of every row. No validation, no
    // instance state needed — pure reshape. Reusable for previews/other checks
    // beyond the constant-column use case below.
    std::map<int, std::vector<std::string>> ExtractColumns(const std::map<int, std::vector<std::string>>& table,const std::vector<int>& columns);

    // Result of checking that a (typically already-extracted) table's non-empty
    // rows all hold the same values. Non-throwing — same "check, report every
    // problem" shape as ValidateSettings, so callers can preview data quality
    // before committing to a run.
    ConstantColumnsCheck CheckConstantRows(const std::map<int, std::vector<std::string>>& table);

    // Extract + check + reshape into a 1-or-2-row table, in one call — what
    // draftTable() actually needs for prj_cols. Throws on the first check
    // failure (this call site wants "value or exception", not a report).
    // headers empty (default): result is just {2: reference_row}, no header row.
    // headers non-empty: result is {1: headers, 2: reference_row}.
    std::map<int, std::vector<std::string>> ExtractConstantColumns(
        const std::map<int, std::vector<std::string>>& table,
        const std::vector<int>& columns,
        const std::vector<std::string>& headers = {});

    std::string latex_string_;

    std::string headerLineRender(int start_cell, int end_cell, const std::vector<std::string>& header_);
    void tableRender(
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
    );

    struct CalculatedTableConfiguration {
        int table_width = 0;
        float row_header_width = 0.0f;
        std::vector<float> column_widths;
        std::vector<TablesRows> tables_rows_config;
    };
    CalculatedTableConfiguration calculateTableConfig(
        int header_size,
        const TableLayoutOptions& layout) const;
};

#endif // CSV2XTABULAR_H

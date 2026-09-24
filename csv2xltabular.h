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

#include <algorithm>
#include <iomanip>

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
    void exportToCSV(const std::string& output_filename = "debug.csv") const {
        if (table_converted_) {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->export_csv(parsed_table_, output_filename);
        } else {
            std::cout << "File was not converted, can't export";
        }
    }

private:
    std::string csv_filename_;         // set by the 2-arg ctor, for convert()'s sake
    std::unique_ptr<CSVParser> csv_parser_; // (re)created fresh inside Convert()
    std::unique_ptr<IniParser> ini_parser_; // set by the 2-arg ctor, for convert()'s sake

    ConversionSettings settings_;
    std::map<int, std::vector<std::string>> parsed_table_;
    bool table_converted_ = false;

    void draftTable();
    void draftLaTeX();

    void normalizeDecCols(std::map<int, std::vector<std::string>>& table, const std::vector<int>& columns_list, int precision = 0, const std::string& delimiter = ",");
    void normalizePrjCols(std::map<int, std::vector<std::string>>& table);

    static bool isEmptyRow(const std::vector<std::string>& vec);
    std::map<int, std::vector<std::string>> extractAndValidate(const std::map<int, std::vector<std::string>>& table, const std::vector<int>& columns_list, const std::vector<std::string>& header_list);

    std::string latex_string_;

    std::string headerLineRender(int start_cell, int end_cell, const std::vector<std::string>& header_);
    void tableRender(
        int _table_width,
        int table_size,
        int start_cell,
        int end_cell,
        const std::map<int, std::vector<std::string>>& table_,
        const std::string& header_line_,
        const std::vector<float>& column_widths,
        float row_header_width
    );
    ConversionSettings calculateTableConfig(int _header_size);
};

#endif // CSV2XTABULAR_H

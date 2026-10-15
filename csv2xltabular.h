#ifndef CSV2XTABULAR_H
#define CSV2XTABULAR_H

#include "ConversionSettings.h"
#include "LaTeXExporter.h"
#include "csvParser.h"
#include "IniParser.h"
#include "Utils.h"
#include "TableSorter.h"

#include <memory>
#include <iostream>
#include <fstream>
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
    void exportToFile(const std::string& output_filename, const LatexDraftOptions& options);
    void exportToCSV(const std::string& output_filename = "debug.csv");
    // Writes project metadata from the last parsed table using explicitly
    // supplied settings; it does not require the converter's INI constructor.
    void exportProjectInfoCSV(const ConversionSettings& settings, const std::string& output_filename = "const_val_rows.csv");

    TabularData getParsedTable() const {
        return parsed_table_;
    }

    // Returns the transformed table by value so callers can work on a copy
    // without mutating the converter's internal conversion state.
    TabularData getConvertedTable() const {
        return converted_table_;
    }

private:
    std::string csv_filename_;         // set by the 2-arg ctor, for convert()'s sake
    std::unique_ptr<CSVParser> csv_parser_; // (re)created fresh inside Convert()
    std::unique_ptr<IniParser> ini_parser_; // set by the 2-arg ctor, for convert()'s sake

    ConversionSettings settings_;
    TabularData parsed_table_; // raw table after CSV parsing
    TabularData converted_table_; // converted table prepared for CSV output
    bool table_converted_ = false;

    void transformTable();
    void exportToFileImpl(const std::string& output_filename, const LatexDraftOptions& options);
    TabularData buildLaTeXData() const;

    static void normalizeDecCols(TabularData& table, const std::vector<int>& columns_list, int precision = 0, const std::string& delimiter = ",");

    // Pulls the given raw column indices out of every row. No validation, no
    // instance state needed — pure reshape. Reusable for previews/other checks
    // beyond the constant-column use case below.
    TabularData ExtractColumns(const TabularData& table, const std::vector<int>& columns) const;

    // Result of checking that a (typically already-extracted) table's non-empty
    // rows all hold the same values. Non-throwing — same "check, report every
    // problem" shape as ValidateSettings, so callers can preview data quality
    // before committing to a run.
    ConstantColumnsCheck CheckConstantRows(const TabularData& table) const;

    ConstantColumnsCheck CheckProjectColumns(const ConversionSettings& settings) const;
    void invalidateConversion() noexcept;
    TabularData buildProjectTable(const ConversionSettings& settings) const;

};

#endif // CSV2XTABULAR_H

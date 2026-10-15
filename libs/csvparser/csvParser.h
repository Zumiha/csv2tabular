#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <optional>
#include "TabularData.h"

// CSVParser: Streaming parser for RFC 4180 compliant CSV files
// Uses state machine for quoted field handling
namespace dataformat {
    struct Row {
        std::vector<std::string> fields;
        size_t line_number;
    };
}

class CSVParser {
public:
    explicit CSVParser(const std::string& filename, char delimiter = ',');

    // Parse the first CSV row as the header and the remaining rows as data.
    // Throws runtime_error on parse failures
    TabularData parse_all(int start_row = 1, int start_col = 1);
    
    // Column interactions
    void mergeColumns(TabularData& table, size_t primary_col, size_t secondary_col);
    void mergeColumns(TabularData& table, const std::vector<int>& from, const std::vector<int>& into);
    
    void reorderColumns(TabularData& table, const std::vector<int>& new_order);
    void reorderColumns(TabularData& table, const std::vector<int>& from, const std::vector<int>& to);

    void deleteColumn(TabularData& table, size_t column_index);
    void deleteColumns(TabularData& table, const std::vector<int>& columns_list);

    TabularData extractTable(const TabularData& table, const std::vector<int>& columns_list);
    void export_csv(const TabularData& table, const std::string& filename) const;

    size_t current_line() const { return line_num_; }
            
private:
    std::ifstream file_;
    char delimiter_;
    size_t line_num_;

    // Parse next row from CSV file
    // Returns nullopt when EOF reached
    std::optional<dataformat::Row> next_row(int start_col);

    // State machine parser for handling quoted fields with embedded delimiters/newlines
    std::vector<std::string> parse_line(const std::string& line, int start_col);

    // Reset file stream to beginning
    void reset_stream();
};

#endif // CSV_PARSER_H
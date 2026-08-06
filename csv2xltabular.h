#ifndef CSV2XTABULAR_H
#define CSV2XTABULAR_H

#include "csvParser.h"
#include "IniParser.h"
#include "Utils.h"

#include <memory>

#include <iostream>
#include <fstream>
#include <string>

#include <algorithm>

struct TablesRows {
    bool remain_table = false;
    int col_start;
    int col_end;
};

class TableConfig {
public:
    std::vector<TablesRows> tables_rows_config;
    
    int max_columns = 12; // Render default max columns per page
    int remdnr_min = 8; // Minimum remainder

    int table_width = 180; // mm, total width of the table in LaTeX
    int columns_sum_width = 102; // mm, sum of column widths (excluding the first column)
    float table_column_width; // mm, width of each column in the main tables

    std::vector<int> prj_cols;
    std::vector<std::string> prj_cols_header;
    
    std::vector<int> delete_cols;
    
    std::vector<int> move_new_order;
    std::vector<int> move_from;
    std::vector<int> move_to;

    std::vector<int> merge_from;
    std::vector<int> merge_into;

    std::map<int, std::vector<int>> decimal_normalizations;
    std::string decimal_delimiter;

};

enum class TableType {
    Default,
    HeadColumn,
    Other
};

enum class MoveOption {
    Default,
    NewOrder,
    FromTo
};

// Helper function
constexpr std::string_view to_string(TableType t_) {
    switch (t_) {
        case TableType::Default:        return "Default";
        case TableType::HeadColumn:     return "HeadColumn";
        case TableType::Other:          return "Other";
        default:                        return "Unknown";
    }
}

class CSVtoXLTABularConverter {
public:
    CSVtoXLTABularConverter() = delete;
    CSVtoXLTABularConverter(const CSVtoXLTABularConverter&) = delete;

    CSVtoXLTABularConverter(const std::string& csv_filename, const std::string& ini_filename); 
    ~CSVtoXLTABularConverter();      

    // Convert CSV to LaTeX tabular format based on INI configuration
    void convert();  
    void exportToFile(const std::string& output_filename = "wt_table.tex");
    void exportToCSV(const std::string& output_filename = "debug.csv") const {
        if (table_converted_) {
            {
                IndentGuard guard(std::cout, "\t");
                csv_parser_->export_csv(parsed_table_, output_filename);
            }
        } else {
            std::cout << "File was not converted, can't export";
        }
    }    
    
    private:

    std::unique_ptr<CSVParser> csv_parser_;
    std::unique_ptr<IniParser> ini_parser_;

    int start_colum_ = 1;
    int start_row_ = 1;
    
    TableConfig table_config_;

    void loadSettings();

    // Parsed table as map
    std::map<int, std::vector<std::string>> parsed_table_;
    TableType convert_type_;
    bool table_converted_ = false;

    void modDefault();
    void modHeadColumn();

    void normalizeDecCols(std::map<int, std::vector<std::string>>& table, const std::vector<int>& columns_list, int precision = 0, const std::string& delimiter = ",");
    void normalizePrjCols(std::map<int, std::vector<std::string>>& table);


    bool has_column_prj = false;
    bool has_delete_cols = false;
    bool has_merge_cols = false;
    MoveOption has_column_moves_ = MoveOption::Default;
    std::vector<std::pair<int,int>> column_moves_;
    bool has_column_normalize = false;

    std::vector<int> apply1basedTo0based(const std::vector<int>& one_based_indices); 

    bool isEmptyRow(const std::vector<std::string>& vec);
    std::map<int, std::vector<std::string>> extractAndValidate(const std::map<int, std::vector<std::string>>& table, const std::vector<int>& columns_list, const std::vector<std::string>& header_list);
    
    std::string latex_string_ = ""; // LaTeX tabular format string
    
    std::string headerLineRender(
        int start_cell, 
        int end_cell, 
        const std::vector<std::string>& header_
    );
    void tableRender(
        int _table_width, 
        float _column_width, 
        int table_size, 
        int start_cell, 
        int end_cell, 
        const std::map<int, std::vector<std::string>>& table_, 
        const std::string& header_line_
    );
    TableConfig calculateTableConfig(int _header_size);
};

#endif // CSV2XTABULAR_H
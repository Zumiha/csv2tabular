#include "csv2xltabular.h"

CSVtoXLTABularConverter::CSVtoXLTABularConverter(const std::string &csv_filename, const std::string &ini_filename) {
    ini_parser_ = std::make_unique<IniParser>(ini_filename);
    csv_parser_ = std::make_unique<CSVParser>(csv_filename);
    std::setlocale(LC_ALL, "Russian"); // Set locale to the user's environment default    
}

CSVtoXLTABularConverter::~CSVtoXLTABularConverter()
{
}

void CSVtoXLTABularConverter::convert()
{
    loadSettings(); 
    std::cout << "[LOG] Parsing CSV file\n";
    {
        IndentGuard guard(std::cerr, "\t");
        parsed_table_ = csv_parser_->parse_all(start_row_, start_colum_);
    }
    std::cout << "[LOG] Parsing completed\n";

    std::cout << "[LOG] Converting table\n";
    draftTable();
    std::cout << "[LOG] Conversion completed\n";
    table_converted_ = true;    
}

void CSVtoXLTABularConverter::exportToFile(const std::string &output_filename) {    
    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_filename);
    }
    draftLaTeX();
    outfile << this->latex_string_;
    outfile.close();
}

void CSVtoXLTABularConverter::loadSettings()
{
    std::cout << "\n[LOG] Loading settings from INI\n";
    if (ini_parser_->hasKey("source_csv.type")) {
        convert_type_ = static_cast<TableType>(ini_parser_->getValue<int>("source_csv.type"));
    } else {
        std::cout << "No document type settings found. Converting to default type.\n"; // Using default type
        convert_type_ = TableType::Default;
    }

    // Check for start row-column
    if (ini_parser_->hasKey("source_csv.start_row")) {
        start_row_ = ini_parser_->getValue<int>("source_csv.start_row");
    } else {
        std::cout << "No start row settings found.\n"; // Using default value
    }
    if (ini_parser_->hasKey("source_csv.start_column")) {
        start_colum_ = ini_parser_->getValue<int>("source_csv.start_column");
    } else {
        std::cout << "No start column settings found.\n"; // Using default value
    }

    // Load settings for LaTeX
    if (ini_parser_->hasKey("table_settings.table_title")) {
        table_config_.table_title = ini_parser_->getValue<std::string>("table_settings.table_title");
    } else {
        std::cout << "No table title settings found. Using default title.\n"; // Using default value
    }
    table_config_.max_columns = ini_parser_->getValue<int>("table_settings.max_columns");
    table_config_.remdnr_min = ini_parser_->getValue<int>("table_settings.min_columns");

    // Check for columns to delete
    if (ini_parser_->hasKey("column_del.delete_cols")) {
        table_config_.delete_cols = ini_parser_->getValue<std::vector<int>>("column_del.delete_cols");
        has_delete_cols = true;
    }

    // Check for columns to merge
    if (ini_parser_->hasSection("column_merge")) {
        bool has_merge_from = ini_parser_->hasKey("column_merge.from");
        bool has_merge_into = ini_parser_->hasKey("column_merge.into");

        if (has_merge_from && has_merge_into) {
            table_config_.merge_from = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("column_merge.from"));
            table_config_.merge_into = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("column_merge.into"));
            if (table_config_.merge_from.size() != table_config_.merge_into.size()) {
                std::cerr << "[WARN] column_merge: from-into parameters need to have same number of columns. No columns will be merged." << std::endl;
            } else {
                has_merge_cols = true;
            }
        } else {
            std::cerr << "[WARN] column_merge: failed to detect from-into columns to merge. No columns will be merged.\n" << std::endl;
        }
    } else {
        std::cout << "No merge columns settings found. Default order would be kept.\n";
    }

    // Check for columns to move
    if (ini_parser_->hasSection("column_moves")) {
        bool has_new_order = ini_parser_->hasKey("column_moves.new_order");
        bool has_from_to   = ini_parser_->hasKey("column_moves.from") && ini_parser_->hasKey("column_moves.to");

        if (has_new_order ^ has_from_to) {
            // Exactly one approach configured — load accordingly
            if (has_new_order) {
                has_column_moves_ = MoveOption::NewOrder;
                table_config_.move_new_order = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("column_moves.new_order"));
            } else {
                has_column_moves_ = MoveOption::FromTo;
                table_config_.move_from = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("column_moves.from"));
                table_config_.move_to = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("column_moves.to"));
            }
        } else {
            std::cerr << "[WARN] column_moves: specify either new_order OR from+to, not both/neither. No columns will be moved.\n";
        }
    } else {
        std::cout << "No move columns settings found. Default order would be kept.\n";
    }

    // Check if columns for prj_info file specified 
    if (ini_parser_->hasKey("column_prj.prj_cols") && ini_parser_->hasKey("column_prj.prj_cols_header")) {
        table_config_.prj_cols = ini_parser_->getValue<std::vector<int>>("column_prj.prj_cols");
        table_config_.prj_cols_header = ini_parser_->getValue<std::vector<std::string>>("column_prj.prj_cols_header");
        has_column_prj = true;
    } else {
        std::cout << "No project columns settings found. No project info file will be created.\n"; 
    }

    // Check if columns for decimal normalization specified
    if (ini_parser_->hasSection("normalize_decimals")) {
        auto columns   = apply1basedTo0based(ini_parser_->getValue<std::vector<int>>("normalize_decimals.columns"));
        auto precision = ini_parser_->getValue<std::vector<int>>("normalize_decimals.precision");
        std::string delimiter;
        if (ini_parser_->hasKey("normalize_decimals.delimiter")) {
            delimiter = ini_parser_->getValue<std::string>("normalize_decimals.delimiter");
        } else {
            delimiter = ",";
        }

        if (columns.size() != precision.size())
            throw std::runtime_error("normalize_decimals: 'columns' and 'precision' must have equal length");
        
        // Group columns by precision — mirrors existing call-site pattern
        std::set<int> seen_columns;
        std::map<int, std::vector<int>> grouped; // precision → column list
        for (size_t i = 0; i < columns.size(); ++i) {
            if (seen_columns.count(columns[i]))
                throw std::runtime_error("normalize_decimals: column " +
                                        std::to_string(columns[i] + 1) + // report as 1-based for user
                                        " listed more than once");

            seen_columns.insert(columns[i]);
            grouped[precision[i]].push_back(columns[i]);
        }
        table_config_.decimal_normalizations = std::move(grouped);
        table_config_.decimal_delimiter = delimiter;
        has_column_normalize = true;
    } else { 
        std::cout << "No decimal precision columns settings found. Values will not be normalized.\n"; 
    }
}

void CSVtoXLTABularConverter::draftTable()
{   
    // Extract project data from csv if available
    if (has_column_prj) {
        std::cout << "[LOG] Extracting project data\n";
        auto prj_info_table = extractAndValidate(parsed_table_, table_config_.prj_cols, table_config_.prj_cols_header);
        normalizePrjCols(prj_info_table);
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->export_csv(prj_info_table, "prj_info.csv");
        }
        std::cout << "[LOG] Project data exported\n";
    }
    
    // MTM SpreadSheet table conversion block
    if (has_delete_cols) {
        // Delete columns not used in spreadsheet
        std::cout << "[LOG] Deleting specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            auto reversed_list = apply1basedTo0based(table_config_.delete_cols); // Apply 0 based counter
            std::sort(reversed_list.rbegin(), reversed_list.rend()); // Sort in descending order to avoid index shifting issues when deleting
            csv_parser_->deleteColumns(parsed_table_, reversed_list); // Remove columns not used in spreadsheet
        }
        std::cout << "[LOG] Deletion completed\n";
    }
    
    // Merge columns
    // auto kp_pos = static_cast<size_t>(ini_parser_->getValue<int>("column_del.kp_col") - 1); // Convert to 0-based index
    // csv_parser_->mergeColumns(parsed_table_, 0, kp_pos);
    if (has_merge_cols) {
        std::cout << "[LOG] Merging specified columns\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->mergeColumns(parsed_table_, table_config_.merge_from, table_config_.merge_into);
        }
        std::cout << "[LOG] Merging completed\n";
    }

    // Check for column reordering
    switch (has_column_moves_)
    {
    case MoveOption::NewOrder:
        std::cout << "[LOG] Reordering columns based on new_order\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(parsed_table_, table_config_.move_new_order);
        }
        std::cout << "[LOG] Moving columns complete\n";
        break;
    case MoveOption::FromTo:
        std::cout << "[LOG] Moving columns based on from+to\n";
        {
            IndentGuard guard(std::cout, "\t");
            csv_parser_->reorderColumns(parsed_table_, table_config_.move_from, table_config_.move_to);
        }
        std::cout << "[LOG] Moving columns complete\n";
        break;
    default:
        break;
    }
    
    // Normalize decimal columns
    if (has_column_normalize) {
        std::cout << "[LOG] Normalizing decimal columns\n";
        for (const auto& [precision, cols] : table_config_.decimal_normalizations) {
            normalizeDecCols(parsed_table_, cols, precision, table_config_.decimal_delimiter);
        }
        std::cout << "[LOG] Normalization completed\n";
    }

    // Check if header needed, attach header
    if (ini_parser_->hasSection("sheet_settings")) {
        if (ini_parser_->hasKey("sheet_settings.SpSh_header_val") && ini_parser_->hasKey("sheet_settings.SpSh_header")) {
            auto header = ini_parser_->getValue<std::string>("sheet_settings.SpSh_header_val");
            if (header == "true") {
                std::cout << "[LOG] Creating header for SpreadSheet" << std::endl;
                auto table_header = ini_parser_->getValue<std::vector<std::string>>("sheet_settings.SpSh_header");
                parsed_table_[0] = table_header;
            }
            has_column_header = true;
        } else {
            std::cout << "Some header settings missing. No header will be created.\n";
        }
    } else {
        std::cout << "No header settings found. No header will be created.\n";
    }
}


void CSVtoXLTABularConverter::draftLaTeX()
{
    // Reset LaTeX string
    latex_string_.clear();

    std::vector<std::string> header;
    switch (convert_type_)
    {
    case TableType::Default:
        std::cout << "[LOG] Drafting LaTeX as Default format table\n";
        break;
    case TableType::HeadColumn:
        std::cout << "\n[LOG] Drafting LaTeX as Head Column format table\n";
        break;
    default:
        std::cout << "Chosen convert type \"" << to_string(convert_type_) << "\" not implemented.";
        return;
    }

    if (has_column_header) {
        // If header settings are available, use it as the first row for LaTeX rendering
        header = parsed_table_[0];
    } else {
        // If no header settings, use the first row of the parsed table as the header for LaTeX rendering
        header = parsed_table_[1];
    }
    
    // Table measurement conversion block
    int header_size = static_cast<int>(header.size()); 
    std::cout << "\nheader_size: " << header_size << "\n";
    auto table_config_ = calculateTableConfig(header_size);

    std::string header_line;
    latex_string_ +="\\newcounter{tablefigure}[section]\n"
                    "\\renewcommand{\\thetablefigure}{\\thesection.\\arabic{tablefigure}}\n\n";
    for (int i = 0; i < table_config_.tables_rows_config.size(); i++) {
        // Render header line
        int cell_start = table_config_.tables_rows_config[i].col_start;
        int cell_end = table_config_.tables_rows_config[i].col_end;
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
            table_config_.table_width, 
            table_config_.table_column_width,  
            table_size, 
            cell_start, 
            cell_end, 
            parsed_table_, 
            header_line
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
    for (auto i = 0; i < vec_size; i++) all_cols.push_back(i);

    std::map<int, std::vector<std::string>> edit_map;
    auto it = table.find(2);
    edit_map.insert(*it);

    normalizeDecCols(edit_map, all_cols, 1);
    table[2] = edit_map[2];
}

std::vector<int> CSVtoXLTABularConverter::apply1basedTo0based(const std::vector<int> &one_based_indices)
{
    std::vector<int> zero_based_indices;
    zero_based_indices.reserve(one_based_indices.size());
    for (int index : one_based_indices) {
        zero_based_indices.push_back(index - 1);
    }
    return zero_based_indices;
}

bool CSVtoXLTABularConverter::isEmptyRow(const std::vector<std::string> &vec) {
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
    float _column_width,  
    int table_size, 
    int start_cell, 
    int end_cell, 
    const std::map<int, std::vector<std::string>>& table_, 
    const std::string& header_line_
) {
    auto table_width = end_cell - start_cell + 1;
    std::string table_title;
    std::string col_spec;
    std::string diagbox;
    int total_cols = 0;
    switch (convert_type_)
    {
        case TableType::Default:
            total_cols = table_width;
            // for Default tables repeated pattern
            col_spec = "*{" + std::to_string(table_size) + "}{m{" + std::to_string(_column_width) + "mm}|}";
            diagbox = "";
            break;
        case TableType::HeadColumn:
            total_cols = table_width + 1; // +1 for the row-header column
            // Row-header column ("|c|") only exists for HeadColumn tables
            col_spec = "|c|*{" + std::to_string(table_size) + "}{m{" + std::to_string(_column_width) + "mm}|}";
            diagbox = "\\diagbox{час}{L,мм} & ";
            break;
        default:
            throw std::runtime_error("tableRender: unsupported TableType");
    }        
    
    const bool has_row_header = (convert_type_ == TableType::HeadColumn);

    latex_string_ +="\\setlength\\LTleft{0cm}\n"
                    "\\stepcounter{tablefigure}\n"
                    "\\noindent Таблица~\\thetablefigure: " + table_config_.table_title + "~\\vspace{-0.75em}\n"
                    "\\begin{xltabular}{" + std::to_string(_table_width) + "mm}{" + col_spec + "}\n"
                    "\\hline\n";
    latex_string_ += diagbox + header_line_;
    latex_string_ +="\\endfirsthead\n"
                    "\\multicolumn{" + std::to_string(total_cols) + "}{@{}l}{\\small\\sl продолжение на предыдущей странице}\\\\ \\hline\n";
    latex_string_ += diagbox + header_line_;
    latex_string_ +="\\endhead\n"
                    "\\multicolumn{" + std::to_string(total_cols) + "}{r}{\\small\\sl продолжение на следующей странице}\\\\ \n"
                    "\\endfoot\n"
                    "\\endlastfoot\n";
                
    // Fill table rows
    int header_row = has_column_header ? 0 : 1; // Header row is 0 if header added in settings, if no header in settings then 1
    for (const auto& [row_num, fields] : table_) {
        if (row_num == header_row) {
            continue; // Skip header row
        }
        // If need Row Header
        if (has_row_header) {
            latex_string_ += (fields[0] + " & ");
        }

        for (size_t i = start_cell; i < end_cell + 1; ++i) {
            latex_string_ += fields[i];
            if (i < end_cell) {
                latex_string_ += " & ";
            } else {
                latex_string_ += "\\\\ \\hline\n";
            }
        }
    }
    latex_string_ += "\\end{xltabular}%\n\\vspace{0em}\n\n";
}

TableConfig CSVtoXLTABularConverter::calculateTableConfig(int _header_size) {
    TableConfig table_settings{};
    // HeadColumn reserves fields[0] as the row label, so data columns start at 1;
    // Default has no row-label column, so data columns start at 0.
    const int offset = (convert_type_ == TableType::HeadColumn) ? 1 : 0;
    auto effective_columns = _header_size - offset; 
    std::cout << "\neffective_columns: " << effective_columns << "\n";

    // max_columns is the max PHYSICAL columns per page (includes the row-header
    // column for HeadColumn). Data-column budget per page is reduced by offset.
    const int max_data_columns = this->table_config_.max_columns - offset;

    for (int i = max_data_columns; i >= 1; --i) {
        // Iterate from max_columns down to 1 to find the largest number of columns that can fit in a table
        int k = effective_columns / i; // Number of full tables with i columns
        int r = effective_columns % i; // Remaining columns that don't fit in full tables

        // One full table without remaining columns or no full tables
        if ((k == 1 && r == 0) || k == 0) {
            std::cout << "CSV columns fit in one table: " << effective_columns;
            table_settings.tables_rows_config.push_back({false, offset, offset + effective_columns - 1});
            table_settings.table_column_width = static_cast<float>(table_settings.columns_sum_width) / effective_columns;
            std::cout << " with column width: " << table_settings.table_column_width << " mm\n";
            return table_settings;
        }

        // If there are multiple tables without remaining columns or the remaining columns are above the minimum threshold
        if (r == 0 || r >= this->table_config_.remdnr_min) {
            std::cout << "\nNumber of tables with " << i << " columns: " << k << "\n";

            int col_start = offset, col_end = offset + i - 1;
            for (int j = 0; j < k; ++j) {
                table_settings.tables_rows_config.push_back({false, col_start, col_end});
                std::cout << "Table " << j + 1 << ": col_start = " << col_start << ", col_end = " << col_end << "\n";
                col_start += i;
                col_end += i;
            }

            if (r > 0) {
                table_settings.tables_rows_config.push_back({true, col_start, col_start + r - 1});
                std::cout << "Plus table with " << r << " columns: ";
                std::cout << "col_start = " << col_start << ", col_end = " << (col_start + r - 1) << "\n";
            }
            
            table_settings.table_column_width = static_cast<float>(table_settings.columns_sum_width) / i;
            std::cout << " with column width: " << table_settings.table_column_width << " mm\n";

            std::cout << std::endl;
            return table_settings;
        }
    }
    throw std::runtime_error("calculateTableConfig: no valid column split found");
}
#include "ConversionSettings.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

#include "IniParser.h"

std::vector<int> Apply1BasedTo0Based(const std::vector<int>& one_based)
{
    std::vector<int> zero_based;
    zero_based.reserve(one_based.size());
    for (int v : one_based)
        zero_based.push_back(v - 1);
    return zero_based;
}

namespace {

std::string JoinInts(const std::vector<int>& values, int write_offset = 0)
{
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            oss << ", ";
        oss << (values[i] + write_offset);
    }
    return oss.str();
}

std::string JoinStrings(const std::vector<std::string>& values)
{
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            oss << ", ";
        oss << values[i];
    }
    return oss.str();
}

} // namespace

ConversionSettings LoadSettingsFromIni(IniParser& ini)
{
    ConversionSettings s;

    std::cout << "\n[LOG] Loading settings from INI\n";

    if (ini.hasKey("source_csv.type")) {
        s.table_layout.convert_type = static_cast<TableType>(ini.getValue<int>("source_csv.type"));
    } else {
        std::cout << "No document type settings found. Converting to default type.\n";
    }

    if (ini.hasKey("source_csv.start_row")) {
        s.start_row = ini.getValue<int>("source_csv.start_row");
    } else {
        std::cout << "No start row settings found.\n";
    }
    if (ini.hasKey("source_csv.start_column")) {
        s.start_col = ini.getValue<int>("source_csv.start_column");
    } else {
        std::cout << "No start column settings found.\n";
    }

    // Kept 1-based as typed — matches the original library's behavior exactly
    // (delete_cols was never run through the 1-based->0-based conversion at
    // load time; the conversion happens later, right before use).
    if (ini.hasKey("column_del.delete_cols")) {
        s.delete_cols = ini.getValue<std::vector<int>>("column_del.delete_cols");
    }

    if (ini.hasSection("column_merge")) {
        bool has_from = ini.hasKey("column_merge.from");
        bool has_into = ini.hasKey("column_merge.into");
        if (has_from && has_into) {
            s.merge_from = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("column_merge.from"));
            s.merge_into = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("column_merge.into"));
            if (s.merge_from.size() != s.merge_into.size()) {
                std::cerr << "[WARN] column_merge: from-into parameters need to have same number of columns. No columns will be merged." << std::endl;
                s.merge_from.clear();
                s.merge_into.clear();
            }
        } else {
            std::cerr << "[WARN] column_merge: failed to detect from-into columns to merge. No columns will be merged.\n" << std::endl;
        }
    } else {
        std::cout << "No merge columns settings found. Default order would be kept.\n";
    }

    if (ini.hasSection("column_moves")) {
        bool has_new_order = ini.hasKey("column_moves.new_order");
        bool has_from_to = ini.hasKey("column_moves.from") && ini.hasKey("column_moves.to");
        if (has_new_order ^ has_from_to) {
            if (has_new_order) {
                s.move_new_order = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("column_moves.new_order"));
            } else {
                s.move_from = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("column_moves.from"));
                s.move_to = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("column_moves.to"));
            }
        } else {
            std::cerr << "[WARN] column_moves: specify either new_order OR from+to, not both/neither. No columns will be moved.\n";
        }
    } else {
        std::cout << "No move columns settings found. Default order would be kept.\n";
    }

    // Kept 1-based as typed — matches the original library exactly (prj_cols
    // was likewise never converted; extractTable() uses it as a raw offset).
    if (ini.hasKey("column_prj.prj_cols") && ini.hasKey("column_prj.prj_cols_header")) {
        s.prj_cols = ini.getValue<std::vector<int>>("column_prj.prj_cols");
        s.prj_cols_header = ini.getValue<std::vector<std::string>>("column_prj.prj_cols_header");
    } else {
        std::cout << "No project columns settings found. No project info file will be created.\n";
    }

    if (ini.hasSection("normalize_decimals")) {
        auto columns = Apply1BasedTo0Based(ini.getValue<std::vector<int>>("normalize_decimals.columns"));
        auto precision = ini.getValue<std::vector<int>>("normalize_decimals.precision");
        s.decimal_delimiter = ini.hasKey("normalize_decimals.delimiter")
            ? ini.getValue<std::string>("normalize_decimals.delimiter")
            : ",";

        if (columns.size() != precision.size())
            throw std::runtime_error("normalize_decimals: 'columns' and 'precision' must have equal length");

        std::set<int> seen;
        std::map<int, std::vector<int>> grouped;
        for (size_t i = 0; i < columns.size(); ++i) {
            if (seen.count(columns[i]))
                throw std::runtime_error("normalize_decimals: column " + std::to_string(columns[i] + 1) + " listed more than once");
            seen.insert(columns[i]);
            grouped[precision[i]].push_back(columns[i]);
        }
        s.decimal_normalizations = std::move(grouped);
    } else {
        std::cout << "No decimal precision columns settings found. Values will not be normalized.\n";
    }

    if (ini.hasSection("table_settings")) {
        if (ini.hasKey("table_settings.table_title"))
            s.table_layout.table_title = ini.getValue<std::string>("table_settings.table_title");
        else
            std::cout << "No table title settings found. Using default title.\n";

        if (ini.hasKey("table_settings.table_width"))
            s.table_layout.table_width = ini.getValue<int>("table_settings.table_width");
        else
            std::cout << "No table width settings found. Using default width.\n";

        s.table_layout.max_columns = ini.getValue<int>("table_settings.max_columns");
        s.table_layout.remdnr_min = ini.getValue<int>("table_settings.min_columns");

        if (ini.hasKey("table_settings.custom_column_type")) {
            const auto column_type = ini.getValue<std::string>("table_settings.custom_column_type");
            if (!column_type.empty()) {
                s.table_layout.custom_column_type = column_type.front();
            }
        }
        if (ini.hasKey("table_settings.numbered_header_line")) {
            s.table_layout.numbered_header_line =
                ini.getValue<std::string>("table_settings.numbered_header_line") == "true";
        }

        if (ini.hasKey("table_settings.column_number") && ini.hasKey("table_settings.column_width")) {
            auto col_nums = ini.getValue<std::vector<int>>("table_settings.column_number");
            auto col_widths = ini.getValue<std::vector<int>>("table_settings.column_width");
            if (!col_nums.empty() && col_nums.size() == col_widths.size()) {
                s.table_layout.custom_column_number = Apply1BasedTo0Based(col_nums);
                s.table_layout.custom_column_width = col_widths;
            } else {
                std::cerr << "[WARN] table_settings: column_number and column_width must be non-empty and equal length ("
                          << col_nums.size() << " vs " << col_widths.size() << "). Falling back to uniform column width.\n";
            }
        } else {
            std::cout << "No custom column width settings found. Using uniform column width.\n";
        }
    }

    // Originally read during table formatting purely because that
    // was convenient, not because it depends on the parsed table — the value
    // itself is a plain setting. Reading it here, upfront, is what lets
    // Convert() stop reaching back into IniParser mid-conversion.
    if (ini.hasSection("sheet_settings")) {
        if (ini.hasKey("sheet_settings.SpSh_header_val") && ini.hasKey("sheet_settings.SpSh_header")) {
            auto header_val = ini.getValue<std::string>("sheet_settings.SpSh_header_val");
            s.sheet_header = ini.getValue<std::vector<std::string>>("sheet_settings.SpSh_header");
            // Fixed: the original set has_column_header = true whenever both keys
            // were merely *present*, even if SpSh_header_val was "false". Tying it
            // to the actual value is what "enable header" was clearly meant to do.
            s.include_header = (header_val == "true");
        } else {
            std::cout << "Some header settings missing. No header will be created.\n";
        }
    } else {
        std::cout << "No header settings found. No header will be created.\n";
    }

    return s;
}

void WriteSettingsToIni(const ConversionSettings& s, const std::string& path)
{
    std::ofstream out(path);
    if (!out.is_open())
        throw std::runtime_error("Cannot open settings file for writing: " + path);

    out << "[source_csv]\n";
    out << "start_column = " << s.start_col << "\n";
    out << "start_row = " << s.start_row << "\n";
    out << "type = " << static_cast<int>(s.table_layout.convert_type) << "\n\n";

    if (!s.prj_cols.empty()) {
        out << "[column_prj]\n";
        out << "prj_cols = " << JoinInts(s.prj_cols) << "\n";
        out << "prj_cols_header = " << JoinStrings(s.prj_cols_header) << "\n\n";
    }

    if (!s.delete_cols.empty()) {
        out << "[column_del]\n";
        out << "delete_cols = " << JoinInts(s.delete_cols) << "\n\n";
    }

    if (!s.merge_from.empty()) {
        out << "[column_merge]\n";
        out << "from = " << JoinInts(s.merge_from, 1) << "\n";
        out << "into = " << JoinInts(s.merge_into, 1) << "\n\n";
    }

    if (!s.move_new_order.empty() || !s.move_from.empty()) {
        out << "[column_moves]\n";
        if (!s.move_new_order.empty()) {
            out << "new_order = " << JoinInts(s.move_new_order, 1) << "\n";
        } else {
            out << "from = " << JoinInts(s.move_from, 1) << "\n";
            out << "to = " << JoinInts(s.move_to, 1) << "\n";
        }
        out << "\n";
    }

    if (!s.decimal_normalizations.empty()) {
        out << "[normalize_decimals]\n";
        std::vector<int> columns;
        std::vector<int> precision;
        for (const auto& [prec, cols] : s.decimal_normalizations) {
            for (int c : cols) {
                columns.push_back(c);
                precision.push_back(prec);
            }
        }
        out << "columns = " << JoinInts(columns, 1) << "\n";
        out << "precision = " << JoinInts(precision) << "\n";
        out << "delimiter = " << s.decimal_delimiter << "\n\n";
    }

    if (!s.sheet_header.empty()) {
        out << "[sheet_settings]\n";
        out << "SpSh_header_val = " << (s.include_header ? "true" : "false") << "\n";
        out << "SpSh_header = " << JoinStrings(s.sheet_header) << "\n\n";
    }

    out << "[table_settings]\n";
    if (!s.table_layout.table_title.empty())
        out << "table_title = \"" << s.table_layout.table_title << "\"\n";
    out << "max_columns = " << s.table_layout.max_columns << "\n";
    out << "min_columns = " << s.table_layout.remdnr_min << "\n";
    if (s.table_layout.custom_column_type != '\0')
        out << "custom_column_type = " << s.table_layout.custom_column_type << "\n";
    out << "numbered_header_line = " << (s.table_layout.numbered_header_line ? "true" : "false") << "\n";
    if (!s.table_layout.custom_column_number.empty()) {
        out << "column_number = " << JoinInts(s.table_layout.custom_column_number, 1) << "\n";
        out << "column_width = " << JoinInts(s.table_layout.custom_column_width) << "\n";
    }
    out << "table_width = " << s.table_layout.table_width << "\n";

    if (out.fail())
        throw std::runtime_error("Write error on settings file: " + path);
}

std::vector<std::string> ValidateSettings(const ConversionSettings& s)
{
    std::vector<std::string> errors;

    if (s.merge_from.size() != s.merge_into.size())
        errors.push_back("Merge: 'from' and 'into' column lists must be the same length.");

    if (!s.move_new_order.empty() && (!s.move_from.empty() || !s.move_to.empty()))
        errors.push_back("Moves: specify either new_order OR from/to, not both.");
    if (s.move_from.size() != s.move_to.size())
        errors.push_back("Moves: 'from' and 'to' column lists must be the same length.");

    if (s.prj_cols.size() != s.prj_cols_header.size())
        errors.push_back("Project columns: column list and header list must be the same length.");

    for (const auto& [precision, cols] : s.decimal_normalizations) {
        (void)cols;
        if (precision < 0) {
            errors.push_back("Decimal normalize: precision must be >= 0.");
            break;
        }
    }

    if (s.table_layout.custom_column_number.size() != s.table_layout.custom_column_width.size())
        errors.push_back("Custom column widths: column list and width list must be the same length.");

    if (s.include_header && s.sheet_header.empty())
        errors.push_back("Header is enabled but no header text was provided.");

    if (s.table_layout.max_columns <= 0)
        errors.push_back("Max columns must be greater than 0.");
    if (s.table_layout.table_width <= 0)
        errors.push_back("Table width must be greater than 0.");
    if (s.start_row < 1 || s.start_col < 1)
        errors.push_back("Start row/column must be 1 or greater.");

    return errors;
}

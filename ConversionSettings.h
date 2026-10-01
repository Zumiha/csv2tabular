#ifndef CONVERSION_SETTINGS_H
#define CONVERSION_SETTINGS_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

class IniParser;

// Shared 1-based (human, as typed in a settings file) -> 0-based (array index)
// conversion. Exposed here since both the settings loader and the conversion
// engine need it, and it's tied to the settings' own indexing convention.
std::vector<int> Apply1BasedTo0Based(const std::vector<int>& one_based);

enum class TableType {
    Default,
    HeadColumn,
    Other
};

constexpr std::string_view to_string(TableType t)
{
    switch (t) {
        case TableType::Default:    return "Default";
        case TableType::HeadColumn: return "HeadColumn";
        case TableType::Other:      return "Other";
        default:                    return "Unknown";
    }
}

// One rendered page's column range. Computed by CSVtoXLTABularConverter,
// not something a caller sets by hand.
struct TablesRows {
    int col_start;
    int col_end;
};

// All conversion parameters in one place. Built either by LoadSettingsFromIni()
// (CLI / "Load" button) or filled in directly by the GUI settings form — Convert()
// doesn't care which. Presence in a list *is* the flag: an empty delete_cols means
// "skip the delete step", there is no separate has_delete_cols bool to keep in sync.
struct ConversionSettings {
    TableType convert_type = TableType::Default;
    int start_row = 1;
    int start_col = 1;

    std::vector<int> delete_cols; // as typed (1-based) — matches the on-disk format verbatim

    std::vector<int> merge_from; // 0-based
    std::vector<int> merge_into; // 0-based

    std::vector<int> move_new_order; // 0-based; mutually exclusive with move_from/move_to
    std::vector<int> move_from;      // 0-based
    std::vector<int> move_to;        // 0-based

    std::vector<int> prj_cols; // as typed (1-based) — matches the on-disk format verbatim
    std::vector<std::string> prj_cols_header;

    std::map<int, std::vector<int>> decimal_normalizations; // precision -> 0-based columns
    std::string decimal_delimiter = ",";

    bool include_header = false;
    std::vector<std::string> sheet_header;

    std::string table_title = "default_title";
    int table_width = 180; // mm
    int max_columns = 12;
    int remdnr_min = 8; // minimum columns on a page's tail split
    char custom_column_type = '\0';
    bool numbered_header_line = false;

    std::vector<int> custom_column_number; // 0-based, parallel to custom_column_width
    std::vector<int> custom_column_width;  // mm

    // --- Populated internally by CSVtoXLTABularConverter once the CSV's column
    // count is known (see calculateTableConfig()). Leave these at defaults when
    // building settings by hand — they are output, not input. ---
    std::vector<float> column_widths;
    float row_header_width = 0.0f;
    std::vector<TablesRows> tables_rows_config;
};

// Reads [source_csv]/[column_*]/[table_settings]/[sheet_settings] from an
// already-open IniParser into a ConversionSettings. On a structural problem
// (e.g. column_merge.from/into different lengths) the offending group is left
// empty and a [WARN] is printed — mirrors the original library's behavior of
// degrading gracefully rather than throwing for that class of problem.
ConversionSettings LoadSettingsFromIni(IniParser& parser);

// Writes settings back out in the same section/key format LoadSettingsFromIni
// reads, so round-tripping (Load -> edit in GUI -> Save) is lossless, and files
// it writes are ordinary settings.ini files usable by the CLI too.
void WriteSettingsToIni(const ConversionSettings& settings, const std::string& path);

// Structural checks only — paired-list lengths, mutually exclusive options.
// Column-index-vs-CSV-width checks happen later, inside Convert(), once the
// CSV is actually parsed and its column count is known. Returns one message
// per problem found; empty means OK. Meant for the GUI to call before Convert(),
// since its form fields aren't funneled through ini text (and thus never got
// the ini loader's own defensive checks).
std::vector<std::string> ValidateSettings(const ConversionSettings& settings);

#endif // CONVERSION_SETTINGS_H

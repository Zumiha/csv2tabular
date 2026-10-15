#include "csv2xltabular.h"

int main(int argc, char* argv[]) {
    try {
        CSVtoXLTABularConverter converter("data.csv", "settings.ini");
        converter.convert();
        // Default export from converted data using the configured INI header/layout.
        converter.exportToFile("default_table.tex");
        // Optional export to CSV
        converter.exportToCSV("final.csv");

        // Sample custom export: reorder columns and keep rows with values in every selected column.
        // Sorted export example: converter-owned transformations and layout
        // remain in effect; sorting only rearranges the selected data rows.
        LatexDraftOptions options;
        options.layout_overrides.table_title = "Top rows by F";
        options.layout_overrides.custom_column_type = 'M'; // Use 'm' for middle-aligned columns
        options.column_order = {5, 6, 1, 7, 8, 9, 0, 2};
        options.layout_overrides.max_columns = 10;
        options.layout_overrides.table_width = 140;
        options.layout_overrides.custom_column_number = std::vector<int>{0, 1};
        options.layout_overrides.custom_column_width = std::vector<int>{25, 25};
        options.ignore_empty_rows = true;
        options.layout_overrides.numbered_header_line = false;
        options.header_line_override = "lat & lon & anom num & met state & F & $T_{safe}$ & KP & length \\\\ \\hline\n";
        options.use_full_header_in_repeated_head = true;
        options.sort_options = sorttable::SortOptions{
            4, sorttable::SortOrder::Ascending // selected-output column index
        };
        options.max_rows = 5;
        converter.exportToFile("sorted_table.tex", options);
        
        
        // IniParser project_settings_parser("settings.ini");
        // const auto project_settings = LoadSettingsFromIni(project_settings_parser);
        // converter.exportProjectInfoCSV(project_settings);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (int e) {
        std::cerr << "Error. Incorrect function exit: " << e << std::endl;
    }
    return 0;
}
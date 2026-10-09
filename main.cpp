#include "csv2xltabular.h"

int main(int argc, char* argv[]) {
    try {
        CSVtoXLTABularConverter converter("data.csv", "settings.ini");
        converter.convert();
        // Default export from converted data using the configured INI header/layout.
        converter.exportToFile("default_table.tex");

        // Sample custom export: reorder columns and keep rows with values in every selected column.
        LatexDraftOptions options;
        options.layout_overrides.custom_column_type = 'M'; // Use 'm' for middle-aligned columns
        options.column_order = {5, 6, 1, 7, 8, 0, 2, 9};
        options.layout_overrides.table_width = 180;
        options.layout_overrides.max_columns = static_cast<int>(options.column_order.size());
        options.layout_overrides.custom_column_number = std::vector<int>{0, 1, 6, 7};
        options.layout_overrides.custom_column_width = std::vector<int>{30, 30, 10, 10};

        options.ignore_empty_rows = true;
        options.header_line_override = "lat & lon & anom num & met state & F & KP & length & $T_{safe}$ \\\\ \\hline\n";
        options.use_full_header_in_repeated_head = false;
        converter.exportToFile("custom_table.tex", options);
        
        // Optional export to CSV
        converter.exportToCSV("final.csv", true);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (int e) {
        std::cerr << "Error. Incorrect function exit: " << e << std::endl;
    }
    return 0;
}
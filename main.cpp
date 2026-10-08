#include "csv2xltabular.h"

int main(int argc, char* argv[]) {
    try {
        CSVtoXLTABularConverter converter("data.csv", "settings.ini");
        converter.convert();
        // Default export from parsed_table_
        converter.exportToFile("default_table.tex");

        // Sample custom export: reorder columns and keep rows with values in every selected column.
        LatexDraftOptions options;
        options.layout.custom_column_type = 'M'; // Use 'm' for middle-aligned columns
        options.column_order = {5, 6, 1, 7, 8, 0, 2, 9};
        options.ignore_empty_rows = true;
        options.header_line_override = "lat & lon & anom num & met state & F & KP & length & $T_{safe}$ \\\\ \\hline\n";
        options.use_full_header_in_repeated_head = false;
        converter.exportToFile("custom_table.tex", options);
        
        // Optional export to CSV (unchanged behavior)
        converter.exportToCSV("final.csv", true);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (int e) {
        std::cerr << "Error. Incorrect function exit: " << e << std::endl;
    }
    return 0;
}
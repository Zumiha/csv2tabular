#include "csv2xltabular.h"

int main(int argc, char* argv[]) {
    try {
        CSVtoXLTABularConverter converter("data.csv", "settings.ini");
        converter.convert();
        converter.exportToFile();
        converter.exportToCSV(); 
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    } catch (int e) {
        std::cerr << "Error. Incorrect function exit: " << e << std::endl;
    }
    return 0;
}
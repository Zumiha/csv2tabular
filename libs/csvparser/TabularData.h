#ifndef TABULAR_DATA_H
#define TABULAR_DATA_H

#include <string>
#include <vector>

struct TabularData {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
};

#endif // TABULAR_DATA_H

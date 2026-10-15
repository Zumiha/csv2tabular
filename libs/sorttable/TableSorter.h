#ifndef TABLE_SORTER_H
#define TABLE_SORTER_H

#include <string>
#include <vector>
#include "TabularData.h"

namespace sorttable {

using ::TabularData;

enum class SortOrder { Ascending, Descending };

struct SortOptions {
    int sort_column = 0; // 0-based index in the supplied TabularData
    SortOrder order = SortOrder::Ascending;
};

struct SortResult {
    std::string sort_mode; // "numeric" or "lexicographic"
};

// Sorts only rows. Header selection, column mapping, filtering, row limits,
// layout, and rendering remain the converter's responsibility.
SortResult sort(TabularData& table, const SortOptions& options);

} // namespace sorttable

#endif // TABLE_SORTER_H

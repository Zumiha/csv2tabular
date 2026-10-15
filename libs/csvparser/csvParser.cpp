#include "csvParser.h"

CSVParser::CSVParser(const std::string& filename, char delimiter)
    : file_(filename), delimiter_(delimiter), line_num_(0) 
{
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
}

TabularData CSVParser::parse_all(int start_row, int start_col)
{
    // 1-based interface — reject zero or negative values
    if (start_row < 1 || start_col < 1) throw std::invalid_argument("start_row and start_col must be >= 1");

    TabularData table;
    reset_stream(); // Ensure starting from beginning

    // ── Phase 1: confirm parameters and reset state ──────────────────────────
    std::cerr << "[DBG] parse_all\n(start_row=" << start_row << ", start_col=" << start_col << ")\n";
    std::cerr << "line_num_ after reset = " << line_num_ << "\n";
    std::cerr << "expected to skip " << (start_row - 1) << std::endl;

    // ── Phase 2: dummy skip loop ─────────────────────────────────────────────
    std::string dummy;
    int skipped = 0;

    /** 
     * Do not increment line_num_ here — next_row() will do it correctly
     * Using skipped counter instead of line_num_ to track skipped lines, since line_num_ handled later
    **/   
    while (static_cast<size_t>(skipped) < static_cast<size_t>(start_row - 1) && std::getline(file_, dummy)) 
    {
        ++skipped;
        // std::cerr << "[DBG] skipped row " << skipped << ": '" << dummy << "'\n";
    }
    std::cerr << "skip phase done -- skipped=" << skipped << std::endl;

    // ── Phase 3: parse rows into table ───────────────────────────────────────
    while (auto row = next_row(start_col)) {
        if (!row->fields.empty()) {
            if (table.header.empty()) {
                table.header = std::move(row->fields);
            } else {
                table.rows.push_back(std::move(row->fields));
            }
        } else {
            std::cerr << "[WARN] row " << row->line_number << " has 0 fields — skipped\n";
        }
    }

    // ── Phase 4: summary ─────────────────────────────────────────────────────
    std::cerr << "parse_all complete -- table rows=" << table.rows.size() << "\n-----------------------------\n";

    if (table.header.empty())
        throw std::runtime_error("No data found from row " + std::to_string(start_row) + ", col " + std::to_string(start_col));
    return table;
}

void CSVParser::mergeColumns(TabularData& table, size_t primary_col, size_t secondary_col)
{
    std::cout << "Merging columns " << primary_col << " and " << secondary_col << "\n";

    if (table.header.size() < std::max(primary_col, secondary_col) + 1) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table.header.size() << " columns, but need at least " << std::max(primary_col, secondary_col) + 1;
        throw std::runtime_error(oss.str());
    }

    const auto merge_row = [&](std::vector<std::string>& field, std::size_t row) {
        if (field[primary_col].empty() || field[primary_col] == "\"\"") {
            field[primary_col] = field[secondary_col]; // merge into empty primary
            return;
        } 
        if (!field[secondary_col].empty() && field[secondary_col] != "\"\"") {
            std::ostringstream oss;
            oss << "Row " << row << ": cannot merge non-empty columns " << primary_col << " and " << secondary_col;
            throw std::runtime_error(oss.str());
        }
    };
    merge_row(table.header, 0);
    for (std::size_t i = 0; i < table.rows.size(); ++i)
        merge_row(table.rows[i], i + 1);
}

void CSVParser::mergeColumns(TabularData& table, const std::vector<int>& from, const std::vector<int>& into)
{
    if (table.header.empty()) return;
    if (from.size() != into.size())
        throw std::invalid_argument("[ERROR] mergeColumns: 'from' and 'into' must have equal length");
    
    std::set<int> seen_from;
    if (!from.empty()) {
        // Validate that columns are unique and not merging into themselves
        for (size_t i = 0; i < from.size(); ++i) {
            if (seen_from.count(from[i]))
                throw std::runtime_error("[ERROR] column_merge: column " + std::to_string(from[i] + 1) + " listed more than once in 'from'");
            seen_from.insert(from[i]);
                
            if (from[i] == into[i])
                throw std::runtime_error("[ERROR] column_merge: column " + std::to_string(from[i] + 1) + " cannot merge into itself");
        }       
        for (size_t i = 0; i < from.size(); ++i)
            mergeColumns(table, into[i], from[i]); // merge from → into

        std::vector<int> to_delete = from;
        std::sort(to_delete.rbegin(), to_delete.rend());
        for (int idx : to_delete)
            deleteColumn(table, idx);   // then remove from           
    }
}

void CSVParser::reorderColumns(TabularData& table, const std::vector<int>& new_order)
{
    if (table.header.empty()) return;
    size_t col_count = table.header.size();
    // Validate: new_order must be a permutation of 1..col_count
    if (new_order.size() != col_count) throw std::invalid_argument("reorderColumns: new_order size must match column count");

    std::vector<bool> seen(col_count, false);
    for (int idx : new_order) {
        if (idx < 0 || idx >= static_cast<int>(col_count))
            throw std::invalid_argument("reorderColumns: index " + std::to_string(idx) + " out of range");
        if (seen[idx])
            throw std::invalid_argument("reorderColumns: duplicate index " + std::to_string(idx));
        seen[idx] = true;
    }

    // Single-pass rebuild per row — no chained insert/delete, no ordering issue
    const auto reorder_row = [&](std::vector<std::string>& fields) {
        std::vector<std::string> rebuilt;
        rebuilt.reserve(fields.size());
        for (int idx : new_order)
            rebuilt.push_back(fields[idx]); // 0-based expected
        fields = std::move(rebuilt);
    };
    reorder_row(table.header);
    for (auto& fields : table.rows)
        reorder_row(fields);
}

void CSVParser::reorderColumns(TabularData& table, const std::vector<int>& from, const std::vector<int>& to)
{
    if (from.size() != to.size())
        throw std::invalid_argument("[ERROR] reorderColumns: from and to must have equal length");
    if (table.header.empty()) return;
    int col_count = static_cast<int>(table.header.size());

    // 1st build destination→source map
    // key = destination index, value = source index
    std::map<int, int> placement;
    for (size_t i = 0; i < from.size(); ++i) {
        if (from[i] < 0 || from[i] >= col_count)    throw std::invalid_argument("reorderColumns: from[" + std::to_string(i) + "]=" + std::to_string(from[i]) + " out of range");
        if (to[i] < 0 || to[i] >= col_count)        throw std::invalid_argument("reorderColumns: to[" + std::to_string(i) + "]=" + std::to_string(to[i]) + " out of range");
        if (placement.count(to[i]))                 throw std::invalid_argument("reorderColumns: duplicate destination index " + std::to_string(to[i]));
        placement[to[i]] = from[i];  // destination → source
    }

    // 2nd mark moved sources
    std::set<int> moved_sources;
    // Collect moved elements in order
    for (const auto& [dst, src] : placement) moved_sources.insert(src);

    // 3rd rebuild each row
    const auto reorder_row = [&](std::vector<std::string>& fields) {
        // Collect non-moved elements in original order
        std::vector<std::string> remaining;
        remaining.reserve(col_count - static_cast<int>(moved_sources.size()));
        for (int i = 0; i < col_count; ++i)
            if (!moved_sources.count(i)) remaining.push_back(fields[i]);
    
        std::vector<std::string> result(col_count);

        // Place moved elements at their destinations
        for (const auto& [dst, src] : placement) result[dst] = fields[src];

        // Fill remaining positions with non-moved elements in order
        int rem_idx = 0;
        for (int i = 0; i < col_count; ++i)
            if (!placement.count(i)) result[i] = remaining[rem_idx++];
        
        fields = std::move(result);
    };
    reorder_row(table.header);
    for (auto& fields : table.rows)
        reorder_row(fields);
}

void CSVParser::deleteColumn(TabularData& table, size_t column_index)
{
    std::cout << "Deleting column " << column_index << "\n";
    if (table.header.size() < column_index + 1) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table.header.size() << " columns";
        throw std::runtime_error(oss.str());
    } 
    table.header.erase(table.header.begin() + column_index);
    for (auto& field : table.rows)
        field.erase(field.begin() + column_index);
}

void CSVParser::deleteColumns(TabularData& table, const std::vector<int>& columns_list)
{
    std::cout << "[DBG] Deleting specified columns\n";    
    if (columns_list.empty()) return;
    if (table.header.size() < static_cast<std::size_t>(*std::max_element(columns_list.begin(), columns_list.end()) + 1)) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table.header.size() << " columns, but need at least " << *std::max_element(columns_list.begin(), columns_list.end()) + 1;
        throw std::runtime_error(oss.str());
    }
    auto erase_columns = [&](std::vector<std::string>& fields) {
        for (const auto element : columns_list) {
            if (element < 0 || element >= static_cast<int>(fields.size()))
                throw std::out_of_range("deleteColumns: column index out of range");
            fields.erase(fields.begin() + element);
        }
    };
    erase_columns(table.header);
    for (auto& fields : table.rows)
        erase_columns(fields);
}

TabularData CSVParser::extractTable(const TabularData& table, const std::vector<int>& columns_list)
{
    TabularData extracted;
    const auto extract_row = [&](const std::vector<std::string>& fields, std::vector<std::string>& output) {
        for (const auto& element : columns_list) {
            if (element < 0 || element >= static_cast<int>(fields.size()))
                throw std::out_of_range("extractTable: column index out of range");
            output.push_back(fields[element]);
        }
    };
    extract_row(table.header, extracted.header);
    for (const auto& row : table.rows) {
        extracted.rows.emplace_back();
        extract_row(row, extracted.rows.back());
    }
    return extracted;
}



void CSVParser::export_csv(const TabularData& table, const std::string& filename) const
{
    std::ofstream out(filename);
    if (!out.is_open())
        throw std::runtime_error("Cannot open output file: " + filename);

    const auto write_row = [&](const std::vector<std::string>& fields) {
        for (size_t i = 0; i < fields.size(); ++i) {
            const std::string& field = fields[i];

            // Re-quote fields that contain delimiter, quotes, or newlines
            bool needs_quotes = field.find(delimiter_) != std::string::npos ||
                                field.find('"')        != std::string::npos ||
                                field.find('\n')       != std::string::npos;

            if (needs_quotes) {
                std::string escaped = field;
                size_t pos = 0;
                while ((pos = escaped.find('"', pos)) != std::string::npos) {
                    escaped.insert(pos, 1, '"'); // escape " → ""
                    pos += 2;
                }
                out << '"' << escaped << '"';
            } else {
                out << field;
            }

            if (i < fields.size() - 1) out << delimiter_;
        }
        out << '\n';
    };
    if (!table.header.empty())
        write_row(table.header);
    for (const auto& row : table.rows)
        write_row(row);

    if (out.fail())
        throw std::runtime_error("Write error on file: " + filename);

    std::cout << "[DBG] Exported "
              << table.rows.size() + (table.header.empty() ? 0 : 1)
              << " rows to: " << filename << "\n";
}

std::optional<dataformat::Row> CSVParser::next_row(int start_col) {
    std::string line;
    while (std::getline(file_, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // strip CRLF
        if (line.empty()) continue; // blank line — skip, don't return
        ++line_num_;

        dataformat::Row row;
        row.line_number = line_num_;
        row.fields = parse_line(line, start_col);
        return row; // return immediately — avoids stale line value after EOF
    }
    return std::nullopt; // only reached at true EOF
}

// RFC 4180 state machine: handles quotes, embedded delimiters, escaped quotes
std::vector<std::string> CSVParser::parse_line(const std::string& line, int start_col) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;
    int col = 1; // Track column index for start_col, 1-based
    
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        
        if (in_quotes) {
            if (c == '"') {
                // Check for escaped quote (double quote)
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i; // Skip next quote
                } else {
                    in_quotes = false; // End of quoted field
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == delimiter_) {
                if (col >= start_col) fields.push_back(field); // accumulate only needed cols
                field.clear();
                ++col;
            } else {
                field += c;
            }
        }
    }
    
    if (col >= start_col) fields.push_back(field); // Add last field
    
    if (in_quotes) {
        throw std::runtime_error("Unterminated quoted field at line " + std::to_string(line_num_));
    }
    
    return fields;
}

void CSVParser::reset_stream() {
    file_.clear(); // Clear EOF flag
    file_.seekg(0); // Rewind to beginning
    line_num_ = 0;
}

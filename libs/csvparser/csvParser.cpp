#include "csvParser.h"

CSVParser::CSVParser(const std::string& filename, char delimiter)
    : file_(filename), delimiter_(delimiter), line_num_(0) 
{
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
}

// Parse entire CSV file into map structure
// map with row number and vector<string> of values
std::map<int, std::vector<std::string>> CSVParser::parse_all(int start_row, int start_col)
{
    // 1-based interface — reject zero or negative values
    if (start_row < 1 || start_col < 1) throw std::invalid_argument("start_row and start_col must be >= 1");

    std::map<int, std::vector<std::string>> table;
    reset_stream(); // Ensure starting from beginning

    // ── Phase 1: confirm parameters and reset state ──────────────────────────
    std::cerr << "[DBG] parse_all(start_row=" << start_row << ", start_col=" << start_col << ")\n";
    std::cerr << "[DBG] line_num_ after reset = " << line_num_ << "\n";
    std::cerr << "[DBG] expected to skip " << (start_row - 1) << std::endl;

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
    std::cerr << "\n[DBG] skip phase done -- skipped=" << skipped << std::endl;

    // ── Phase 3: parse rows into table ───────────────────────────────────────
    while (auto row = next_row(start_col)) {
        // std::cerr << "[DBG] next_row -> line=" << row->line_number
        //           << " fields=" << row->fields.size()
        //           << " first='" << (!row->fields.empty() ? row->fields[0] : "<empty>")
        //           << "'\t";

        if (!row->fields.empty()) {
            table[static_cast<int>(row->line_number)] = std::move(row->fields);
            // std::cerr << "[DBG] added at key=" << row->line_number << "\n";
        } else {
            std::cerr << "[WARN] row " << row->line_number << " has 0 fields — skipped\n";
        }
    }

    // ── Phase 4: summary ─────────────────────────────────────────────────────
    std::cerr << "[DBG] parse_all complete -- table size=" << table.size() << "\n-----------------------------\n";
    
    // for (const auto& [k, v] : table)
    //     std::cerr << "[DBG] table[" << k << "] = " << v.size() << " fields" << " | first='" << (!v.empty() ? v[0] : "<empty>") << "'\n";

    if (table.empty())
        throw std::runtime_error("No data found from row " + std::to_string(start_row) + ", col " + std::to_string(start_col));
    return table;
}

void CSVParser::mergeColumns(std::map<int, std::vector<std::string>> &table, size_t primary_col, size_t secondary_col)
{
    std::cout << "Merging columns " << primary_col << " and " << secondary_col << "\n";

    if (table[1].size() < std::max(primary_col, secondary_col) + 1) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table[1].size() << " columns, but need at least " << std::max(primary_col, secondary_col) + 1;
        throw std::runtime_error(oss.str());
    }

    for (auto& [row, field] : table) {        
        if (field[primary_col].empty() || field[primary_col] == "\"\"") {
            field[primary_col] = field[secondary_col]; // merge into empty primary
            continue;
        } 
        if (!field[secondary_col].empty() && field[secondary_col] != "\"\"") {
            std::ostringstream oss;
            oss << "Row " << row << ": cannot merge non-empty columns " << primary_col << " and " << secondary_col;
            throw std::runtime_error(oss.str());
        }
    }
}

void CSVParser::mergeColumns(std::map<int, std::vector<std::string>> &table, const std::vector<int> &from, const std::vector<int> &into)
{
    if (table.empty()) return;
    if (from.size() != into.size())
        throw std::invalid_argument("[ERROR] mergeColumns: 'from' and 'into' must have equal length");
    
    std::set<int> seen_from;
    std::vector<std::pair<int,int>> merges; // {from, into}
    if (!from.empty()) {
        // Validate that 'from' columns are unique and not merging into themselves
        // Build pairs and sort by 'from' descending — deleting high indices first
        // prevents earlier deletes from shifting later merge targets

        for (size_t i = 0; i < from.size(); ++i) {
            if (seen_from.count(from[i]))
                throw std::runtime_error("column_merge: column " + std::to_string(from[i] + 1) + " listed more than once in 'from'");
            seen_from.insert(from[i]);
                
            if (from[i] == into[i])
                throw std::runtime_error("column_merge: column " + std::to_string(from[i] + 1) + " cannot merge into itself");
            
            merges.emplace_back(from[i], into[i]);
        }

        std::sort(merges.begin(), merges.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        
        for (const auto& [from, into] : merges) {
            mergeColumns(table, into, from); // merge from → into
            deleteColumn(table, from);       // then remove from
        }                
    }
}

void CSVParser::reorderColumns(std::map<int, std::vector<std::string>> &table, const std::vector<int> &new_order)
{
    if (table.empty()) return;
    size_t col_count = table.begin()->second.size();
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
    for (auto& [row_num, fields] : table) {
        std::vector<std::string> rebuilt;
        rebuilt.reserve(fields.size());
        for (int idx : new_order)
            rebuilt.push_back(fields[idx]); // 0-based expected
        fields = std::move(rebuilt);
    }
}

void CSVParser::reorderColumns(std::map<int, std::vector<std::string>> &table, const std::vector<int>& from, const std::vector<int>& to)
{
    if (from.size() != to.size())
        throw std::invalid_argument("[ERROR] reorderColumns: from and to must have equal length");
    if (table.empty()) return;
    int col_count = static_cast<int>(table.begin()->second.size());

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
    for (auto& [row_num, fields] : table) {
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
    }
}

void CSVParser::deleteColumn(std::map<int, std::vector<std::string>> &table, size_t column_index)
{
    std::cout << "Deleting column " << column_index << "\n";
    if (table[1].size() < column_index + 1) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table[1].size() << " columns";
        throw std::runtime_error(oss.str());
    } 

    for (auto& [row_num, field]: table) {
        field.erase(field.begin() + column_index);
    }
}

void CSVParser::deleteColumns(std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns_list)
{
    if (table[1].size() < *std::max_element(columns_list.begin(), columns_list.end()) + 1) {
        std::ostringstream oss;
        oss << "Out of range for merge: row has only " << table[1].size() << " columns, but need at least " << *std::max_element(columns_list.begin(), columns_list.end()) + 1;
        throw std::runtime_error(oss.str());
    }

    for (auto& [row_num, field] : table) {
        for (const auto& element : columns_list) {
            field.erase(field.begin() + element);
        }
    }
}

std::map<int, std::vector<std::string>> CSVParser::extractTable(const std::map<int, std::vector<std::string>> &table, const std::vector<int> &columns_list)
{
    std::map<int, std::vector<std::string>> extracted;
    

    // std::cout << "\nExtracting project data.\n";
    for (auto& [row_num, fields] : table) {
        for (const auto& element : columns_list) {
            auto it = fields.begin() + element;
            extracted[row_num].push_back(*it);
        }
    }
    return extracted;
}



void CSVParser::export_csv(const std::map<int, std::vector<std::string>> &table, const std::string &filename) const
{
    std::ofstream out(filename);
    if (!out.is_open())
        throw std::runtime_error("Cannot open output file: " + filename);

    for (const auto& [row_num, fields] : table) {
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
    }

    if (out.fail())
        throw std::runtime_error("Write error on file: " + filename);

    std::cout << "[DEBUG] Exported " << table.size() << " rows to: " << filename << "\n";
}

std::optional<dataformat::Row> CSVParser::next_row(int start_col) {
    std::string line;
    while (std::getline(file_, line)) {
        ++line_num_;
        if (!line.empty() && line.back() == '\r') line.pop_back(); // strip CRLF
        if (line.empty()) continue; // blank line — skip, don't return

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

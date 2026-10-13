#include <iostream>
#include <streambuf>
#include <cstddef>
#include <map>
#include <string>
#include <numeric>
#include <vector>

class IndentStreambuf : public std::streambuf {
public:
    IndentStreambuf(std::streambuf* dest, std::string indent)
        : dest_(dest), indent_(std::move(indent)), atLineStart_(true) {}

protected:
    int overflow(int ch) override {
        if (ch == EOF) return !EOF;

        if (atLineStart_) {
            dest_->sputn(indent_.data(), indent_.size());
            atLineStart_ = false;
        }

        if (ch == '\n') atLineStart_ = true;

        return dest_->sputc(static_cast<char>(ch));
    }

private:
    std::streambuf* dest_;
    std::string indent_;
    bool atLineStart_;
};

class IndentGuard {
public:
    IndentGuard(std::ostream& os, std::string indent)
        : os_(os), old_(os.rdbuf()), buf_(os.rdbuf(), std::move(indent)) {
        os_.rdbuf(&buf_);
    }
    ~IndentGuard() { os_.rdbuf(old_); }

    IndentGuard(const IndentGuard&) = delete;
    IndentGuard& operator=(const IndentGuard&) = delete;

private:
    std::ostream& os_;
    std::streambuf* old_;
    IndentStreambuf buf_;
};

struct ConstantColumnsCheck {
    std::vector<std::string> reference_row; // first non-empty row found
    std::vector<std::string> errors;        // empty = every row matched it
};

struct ColumnAggregate {
    std::string value;
    std::size_t count = 0;
    double sum = 0.0;
};

// Groups rows by a zero-based column and sums the numeric values in another
// zero-based column. Empty sum cells contribute zero; invalid values throw.
// Results retain the order in which each group first appears in the table.
std::vector<ColumnAggregate> AggregateByColumn(
    const std::map<int, std::vector<std::string>>& table,
    int group_column,
    int sum_column);
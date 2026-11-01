// openings.cpp — the opening names (loading the table, the lookup) and the book (Position::Key gives the keys).
#include "openings.h"
#include <sstream>
#include <algorithm>

int OpeningNames::load(const std::string &text) {
    std::istringstream lines(text);
    std::string line;
    int count = 0;
    while (std::getline(lines, line)) {
        const std::size_t tab1 = line.find('\t');
        const std::size_t tab2 = line.find('\t', tab1 + 1);
        if (tab1 == std::string::npos || tab2 == std::string::npos)
            continue;                                   // not a table line
        const std::size_t tab3 = line.find('\t', tab2 + 1);    // the moves: a fourth column, if there
        const std::string name = line.substr(tab2 + 1, tab3 == std::string::npos ? std::string::npos : tab3 - tab2 - 1);
        const std::string moves = tab3 == std::string::npos ? "" : line.substr(tab3 + 1);
        m_names[line.substr(0, tab1)] = {line.substr(tab1 + 1, tab2 - tab1 - 1), name, moves};
        ++count;
    }
    return count;
}

std::optional<OpeningName> OpeningNames::find(const std::string &key) const {
    const auto found = m_names.find(key);
    if (found == m_names.end())
        return std::nullopt;
    return found->second;
}

// FNV-1a: start from the offset basis; for each byte, XOR it in, then multiply by the prime. The tool does the same.
std::uint64_t OpeningBook::hash(const std::string &key) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

int OpeningBook::load(const std::string &data) {
    m_entries.clear();
    auto get = [&data](std::size_t at, int bytes) {   // little-endian: the lowest byte first
        std::uint64_t value = 0;
        for (int i = bytes - 1; i >= 0; --i)
            value = value << 8 | static_cast<unsigned char>(data[at + i]);
        return value;
    };
    for (std::size_t at = 0; at + 14 <= data.size(); at += 14)   // 14 bytes a move: key 8, move 2, games 4
        m_entries.push_back({get(at, 8), std::uint16_t(get(at + 8, 2)), std::uint32_t(get(at + 10, 4))});
    return int(m_entries.size());
}

std::vector<BookMove> OpeningBook::moves(const std::string &key) const {
    const std::uint64_t wanted = hash(key);
    // The first entry whose key isn't smaller than the wanted one: a binary search, the entries being sorted.
    auto smaller = [](const Entry &entry, std::uint64_t k) { return entry.key < k; };
    auto square = [](int sq) { return std::string{char('a' + sq % 8), char('1' + sq / 8)}; };
    std::vector<BookMove> result;
    for (auto it = std::lower_bound(m_entries.begin(), m_entries.end(), wanted, smaller);
         it != m_entries.end() && it->key == wanted; ++it) {
        std::string uci = square(it->move & 63) + square((it->move >> 6) & 63);   // from | to << 6 | promotion << 12
        if (it->move >> 12)
            uci += " nbrq"[it->move >> 12];
        result.push_back({uci, int(it->games)});     // a count fits an int easily (at most the games read: ~1 M)
    }
    return result;   // most played first: the file's order
}

int OpeningBook::games(const std::string &key) const {
    int total = 0;
    for (const BookMove &m : moves(key))
        total += m.games;
    return total;
}

// openings.h — the opening names (plain C++): a position → its ECO code and name. The table comes from
// data/openings.tsv (lichess chess-openings, CC0; see data/README.md), one line per named position.
#pragma once
#include "position.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>


struct OpeningName {
    std::string eco;    // "B20"
    std::string name;   // "Sicilian Defense"
    std::string moves;  // how to get there from the start, UCI, space-separated: "e2e4 c7c5" ("" = not given)
};


class OpeningNames {
public:
    int load(const std::string &text);                          // the table's lines: key \t eco \t name; returns how many
    std::optional<OpeningName> find(const std::string &key) const;   // nullopt: not a named position
    const std::unordered_map<std::string, OpeningName> &all() const { return m_names; }   // every named position

private:
    std::unordered_map<std::string, OpeningName> m_names;
};
struct BookMove {
    std::string uci;    // "e2e4"
    int games;          // how often it was played: the weight
};

// The opening book (data/book.bin; the format in data/README.md): what people play in a position, most played first.
class OpeningBook {
public:
    int load(const std::string &data);                           // the file's bytes; returns how many moves
    std::vector<BookMove> moves(const std::string &key) const;   // key as Position::Key; empty: not in the book
    int games(const std::string &key) const;                     // the games that reached it (its moves' sum); 0 = not in the book
    static std::uint64_t hash(const std::string &key);           // FNV-1a, 64 bits: the book's key for a position

private:
    struct Entry { std::uint64_t key; std::uint16_t move; std::uint32_t games; };
    std::vector<Entry> m_entries;   // sorted by key, as in the file
};

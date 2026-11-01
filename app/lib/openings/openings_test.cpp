// openings_test.cpp — the test gate for the opening names. Exit status 0 = all pass (ctest reads it).
#include "openings.h"
#include "move.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    int failures = 0;
    auto check = [&failures](const std::string &name, bool ok) {
        std::cout << (ok ? "PASS " : "FAIL ") << "openings: " << name << "\n";
        if (!ok) ++failures;
    };
    Position pos;
    const Position::Chess start = pos.FenInput("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1").value();

    // The key: the FEN without the move counters.
    check("key of the start", pos.Key(start) == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");

    // A small table.
    OpeningNames small;
    const int count = small.load("k1\tA00\tFirst Opening\nk2\tB01\tSecond Opening: Sharp Line\n");
    check("two lines loaded", count == 2);
    const auto second = small.find("k2");
    check("found: eco and name", second && second->eco == "B01" && second->name == "Second Opening: Sharp Line");
    check("not named: nullopt", !small.find("k3"));
    OpeningNames withMoves;
    withMoves.load("k9\tC50\tItalian Game\te2e4 e7e5 g1f3 b8c6 f1c4\n");
    const auto italian = withMoves.find("k9");
    check("four columns: the name and the moves apart",
          italian && italian->name == "Italian Game" && italian->moves == "e2e4 e7e5 g1f3 b8c6 f1c4");
    check("three columns: no moves", second && second->moves.empty());

    // The real table (the path from CMake): 1. e4 c5 is the Sicilian.
    std::ifstream file(OPENINGS_TSV);
    std::stringstream text;
    text << file.rdbuf();
    OpeningNames names;
    check("the real table loads", names.load(text.str()) > 3000);
    Moves mover;
    Position::Chess status = start;
    for (const char *san : {"e4", "c5"}) {
        auto move = pos.SanToMove(status, san);
        status = mover.make(status, *move);
    }
    const auto sicilian = names.find(pos.Key(status));
    check("1. e4 c5: the Sicilian", sicilian && sicilian->name == "Sicilian Defense" && sicilian->eco == "B20"
                                        && sicilian->moves == "e2e4 c7c5");

    // --- The book ---------------------------------------------------------------------------------------------
    check("hash: FNV-1a of nothing is the offset basis", OpeningBook::hash("") == 14695981039346656037ull);

    // One entry made by hand: key "x", e7e8q (52 | 60 << 6 | 4 << 12), 7 games; written little-endian, 14 bytes.
    std::string bytes;
    auto put = [&bytes](std::uint64_t value, int count) {
        for (int i = 0; i < count; ++i)
            bytes += char((value >> (8 * i)) & 0xff);
    };
    put(OpeningBook::hash("x"), 8);
    put(52 | 60 << 6 | 4 << 12, 2);
    put(7, 4);
    OpeningBook tiny;
    check("one entry loaded", tiny.load(bytes) == 1);
    const auto promo = tiny.moves("x");
    check("the move decoded: promotion and games", promo.size() == 1 && promo[0].uci == "e7e8q" && promo[0].games == 7);
    check("not in the book: empty", tiny.moves("y").empty());

    // The real book: from the start, 1. e4 is played most, and 1. d4 is in it too.
    std::ifstream bookFile(OPENINGS_BOOK, std::ios::binary);
    std::stringstream bookBytes;
    bookBytes << bookFile.rdbuf();
    OpeningBook book;
    check("the real book loads", book.load(bookBytes.str()) > 40000);
    const auto first = book.moves(pos.Key(start));
    bool hasD4 = false;
    for (const BookMove &m : first)
        hasD4 = hasD4 || m.uci == "d2d4";
    check("from the start: e4 first, d4 too", !first.empty() && first[0].uci == "e2e4" && hasD4);

    // Games through a position: every game starts at the start; fewer reach 1. e4 c5, none an unknown position.
    const int allGames = book.games(pos.Key(start));
    const int sicilianGames = book.games(pos.Key(status));   // status: after 1. e4 c5 (above)
    check("games: the start holds them all", allGames > 0 && allGames >= sicilianGames);
    check("games: the Sicilian, between 10 and 30 % of all",
          sicilianGames * 10 > allGames && sicilianGames * 10 < 3 * allGames);

    check("games: not in the book: 0", tiny.games("y") == 0);

    return failures == 0 ? 0 : 1;
}

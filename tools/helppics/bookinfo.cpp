// bookinfo "<opening name>" [uci ...]: after the moves, every book move with its share of the games ("A book move:
// N % …"), the game's name after it, and whether it stays inside the opening (Game::staysInside's rule); then every
// other legal move that leaves (named or not). The practice pictures' numbers and names come from here.
#include <fstream>
#include <iostream>
#include <sstream>
#include "move.h"
#include "openings.h"
#include "position.h"

static std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return 1;
    OpeningNames names;
    OpeningBook book;
    names.load(slurp(DATA_DIR "/openings.tsv"));
    book.load(slurp(DATA_DIR "/book.bin"));
    const std::string chosen = argv[1];
    Position model;
    Moves mover;
    std::vector<Position::Chess> path{*model.FenInput(Position::StartFen)};
    for (int i = 2; i < argc; ++i) {
        auto m = model.UciToMove(path.back(), argv[i]);
        if (!m) { std::cerr << "not legal: " << argv[i] << "\n"; return 1; }
        path.push_back(mover.make(path.back(), *m));
    }
    const Position::Chess &now = path.back();
    auto nameOf = [&](const std::vector<Position::Chess> &p) {
        for (auto it = p.rbegin(); it != p.rend(); ++it)
            if (auto n = names.find(model.Key(*it)))
                return n->eco + " " + n->name;
        return std::string("(no name)");
    };
    std::cout << "now: " << nameOf(path) << "\n";
    int total = 0;
    const auto bookMoves = book.moves(model.Key(now));
    for (const auto &b : bookMoves) total += b.games;
    for (const auto &m : model.LegalMoves(now).moves) {
        const std::string uci = model.MoveToUci(m);
        int games = 0;
        for (const auto &b : bookMoves) if (b.uci == uci) games = b.games;
        Position::Move copy = m;
        auto after = path;
        after.push_back(mover.make(now, copy));
        const bool named = names.find(model.Key(after.back())).has_value();
        std::string name = nameOf(after);
        const std::string bare = name.substr(name.find(' ') + 1);
        const bool inside = (games > 0 || named) && (bare == chosen || bare.rfind(chosen + ":", 0) == 0
                                                     || bare.rfind(chosen + ",", 0) == 0);
        if (games == 0 && !named)
            continue;                                  // unknown moves: all "not in the book", leaving
        std::cout << model.MoveToSan(now, m) << "\t" << (games ? std::to_string(int(100.0 * games / std::max(total, 1) + 0.5)) + " %"
                                                              : "-") << "\t" << (inside ? "inside" : "LEAVES")
                  << "\t" << name << "\n";
    }
}

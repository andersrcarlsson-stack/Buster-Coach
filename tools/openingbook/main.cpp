// openingbook — prepares Buster Coach's opening data (design: tutorial/openings_design.md).
//
//   openingbook names a.tsv b.tsv ... > names.tsv
//       lichess chess-openings (eco, name, pgn) → one line per position: key, eco, name, the line's moves in UCI
//       (space-separated: how to get there from the start; opening practice plays it). The key is the position's
//       FEN without the move counters (as the app computes it). Several names for one position (transpositions):
//       the one reached in the fewest moves.
//
//   zstdcat lichess_db_standard_rated_2026-09.pgn.zst | openingbook count [games] > counts.tsv
//       lichess games (PGN on stdin) → per position and move: how often, and how the mover scored. Only games where
//       both players are 2000+ and the time control isn't bullet or correspondence; the first 20 plies. Stops after
//       [games] qualifying games (default 1,000,000). Positions seen fewer than 5 times are dropped. One line per
//       position and move: key hash (hex), its first ply, the move (UCI), games, the mover's points × 2.
//
//   openingbook pack counts.tsv book.bin [plies] [min games] [min share %] [min score %]
//       The dial (defaults 16, 50, 3, 35): positions up to [plies] seen in [min games]+ games; per position only moves
//       played in [min share]+ % of its games and scoring [min score]+ % for the mover. The book: 14 bytes per move,
//       little-endian, sorted by key: key (8, keyHash of positionKey), move (2, moveCode), games (4: the real count, so
//       positions compare: a position's games = the sum of its moves').
//
//   openingbook probe book.bin "<fen>"
//       The book's moves in a position, with their share: a check of the book (and the reader the app needs).
//
//   openingbook depth counts.tsv < games.pgn
//       How far games stay on well-trodden ground: the first ply whose move counts.tsv saw fewer than N times.

#include "position.h"
#include "move.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// The position's key: the model's own (the app looks positions up the same way).
std::string positionKey(Position& pos, const Position::Chess& status) {
    return pos.Key(status);
}

// "1. e4 e5 2. Nf3" → its position, or an error (the SAN that didn't match).
bool playLine(Position& pos, const std::string& pgn, Position::Chess& status, int& plies, std::string& error,
              std::string& uci) {
    status = pos.FenInput(Position::StartFen).value();
    plies = 0;
    uci.clear();
    Moves mover;
    std::istringstream words(pgn);
    std::string word;
    while (words >> word) {
        if (std::isdigit(static_cast<unsigned char>(word[0])) && word.back() == '.')
            continue;                                    // a move number: "1." or "12..."
        std::optional<Position::Move> move = pos.SanToMove(status, word);
        if (!move) {
            error = word;
            return false;
        }
        uci += (uci.empty() ? "" : " ") + pos.MoveToUci(*move);
        status = mover.make(status, *move);
        ++plies;
    }
    return true;
}

// FNV-1a, 64 bits: the book's position key (the app computes the same from positionKey).
std::uint64_t keyHash(const std::string& key) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

// A header's value: [WhiteElo "2105"] → 2105.
std::string headerValue(const std::string& line) {
    const std::size_t a = line.find('"'), b = line.rfind('"');
    return (a == std::string::npos || b <= a) ? "" : line.substr(a + 1, b - a - 1);
}

// Blitz or slower (lichess: base + 40 × increment ≥ 180 s); "-" (correspondence) doesn't count.
bool slowEnough(const std::string& tc) {
    const std::size_t plus = tc.find('+');
    if (plus == std::string::npos)
        return false;
    return std::stoi(tc.substr(0, plus)) + 40 * std::stoi(tc.substr(plus + 1)) >= 180;
}

// A move in 2 bytes: from | to << 6 | promotion << 12 (squares a1 = 0 … h8 = 63; promotion 0 none, 1 n, 2 b, 3 r, 4 q).
std::uint16_t moveCode(const std::string& uci) {
    auto square = [&](int i) { return (uci[i] - 'a') + 8 * (uci[i + 1] - '1'); };
    const int promo = uci.size() > 4 ? int(std::string(" nbrq").find(uci[4])) : 0;
    return std::uint16_t(square(0) | square(2) << 6 | promo << 12);
}

std::string moveUci(std::uint16_t code) {
    auto square = [](int sq) { return std::string{char('a' + sq % 8), char('1' + sq / 8)}; };
    std::string uci = square(code & 63) + square((code >> 6) & 63);
    if (code >> 12)
        uci += " nbrq"[code >> 12];
    return uci;
}

int count(int argc, char** argv) {
    const long wantedGames = argc > 2 ? std::stol(argv[2]) : 1000000;
    const int maxPlies = 20, minGames = 5;
    struct Tally { std::uint16_t move; std::uint32_t games = 0, points2 = 0; };   // move: from | to << 6 | promo << 12
    struct Seen { int ply; std::vector<Tally> moves; };
    std::unordered_map<std::uint64_t, Seen> book;
    Position pos;
    Moves mover;
    const Position::Chess start = pos.FenInput(Position::StartFen).value();
    long read = 0, used = 0, broken = 0;
    int whiteElo = 0, blackElo = 0;
    bool timeOk = false;
    std::string line;
    while (used < wantedGames && std::getline(std::cin, line)) {
        if (line.rfind("[Event ", 0) == 0) {
            whiteElo = blackElo = 0;
            timeOk = false;
        } else if (line.rfind("[WhiteElo ", 0) == 0) {
            whiteElo = std::atoi(headerValue(line).c_str());
        } else if (line.rfind("[BlackElo ", 0) == 0) {
            blackElo = std::atoi(headerValue(line).c_str());
        } else if (line.rfind("[TimeControl ", 0) == 0) {
            timeOk = slowEnough(headerValue(line));
        } else if (!line.empty() && line[0] != '[') {    // the moves (lichess: one line per game)
            ++read;
            if (whiteElo < 2000 || blackElo < 2000 || !timeOk)
                continue;
            // The result is the movetext's last word.
            const std::size_t lastSpace = line.find_last_of(' ');
            const std::string result = line.substr(lastSpace + 1);
            int whitePoints2 = 1;                        // 1/2-1/2 (and "*", rare)
            if (result == "1-0") whitePoints2 = 2;
            else if (result == "0-1") whitePoints2 = 0;
            ++used;
            Position::Chess status = start;
            std::istringstream words(line);
            std::string word;
            int ply = 0, depthInComment = 0;
            while (ply < maxPlies && words >> word) {
                if (word[0] == '{') ++depthInComment;
                if (depthInComment > 0) {
                    if (word.back() == '}') --depthInComment;
                    continue;                            // { [%clk 0:03:00] } and other comments
                }
                if (std::isdigit(static_cast<unsigned char>(word[0])))
                    continue;                            // "1." "1..." and the result
                std::optional<Position::Move> move = pos.SanToMove(status, word);   // make() takes it non-const
                if (!move) {
                    ++broken;
                    break;
                }
                Seen& seen = book.try_emplace(keyHash(positionKey(pos, status)), Seen{ply, {}}).first->second;
                const std::uint16_t code = moveCode(pos.MoveToUci(*move));
                auto tally = std::find_if(seen.moves.begin(), seen.moves.end(),
                                          [&](const Tally& t) { return t.move == code; });
                if (tally == seen.moves.end())
                    tally = seen.moves.insert(seen.moves.end(), Tally{code});
                ++tally->games;
                tally->points2 += (ply % 2 == 0) ? whitePoints2 : 2 - whitePoints2;
                status = mover.make(status, *move);
                ++ply;
            }
            if (used % 100000 == 0)
                std::cerr << used << " games used of " << read << " read, " << book.size() << " positions\n";
        }
    }
    long kept = 0;
    for (const auto& [hash, seen] : book) {
        std::uint32_t total = 0;
        for (const Tally& t : seen.moves) total += t.games;
        if (total < minGames)
            continue;
        ++kept;
        for (const Tally& t : seen.moves)
            std::cout << std::hex << hash << std::dec << '\t' << seen.ply << '\t' << moveUci(t.move) << '\t' << t.games << '\t'
                      << t.points2 << '\n';
    }
    std::cerr << used << " games used of " << read << " read, " << broken << " broken off, " << book.size()
              << " positions, " << kept << " seen " << minGames << "+ times\n";
    return 0;
}

int pack(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: openingbook pack counts.tsv book.bin [plies] [min games] [min share %] [min score %]\n";
        return 2;
    }
    const int maxPlies = argc > 4 ? std::stoi(argv[4]) : 16;
    const unsigned minGames = argc > 5 ? std::stoul(argv[5]) : 50;
    const double minShare = (argc > 6 ? std::stod(argv[6]) : 3) / 100;
    const double minScore = (argc > 7 ? std::stod(argv[7]) : 35) / 100;
    struct Tally { std::uint16_t move; std::uint32_t games, points2; };
    std::map<std::uint64_t, std::vector<Tally>> positions;    // sorted by key: the book's order
    std::ifstream in(argv[2]);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream cols(line);
        std::string hex, uci;
        int ply;
        std::uint32_t games, points2;
        if (!(cols >> hex >> ply >> uci >> games >> points2) || ply >= maxPlies)
            continue;
        positions[std::stoull(hex, nullptr, 16)].push_back({moveCode(uci), games, points2});
    }
    std::ofstream out(argv[3], std::ios::binary);
    long entries = 0, kept = 0;
    auto put = [&](std::uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i)
            out.put(char((value >> (8 * i)) & 0xff));
    };
    for (auto& [key, moves] : positions) {
        std::uint32_t total = 0;
        for (const Tally& t : moves)
            total += t.games;
        if (total < minGames)
            continue;
        ++kept;
        std::sort(moves.begin(), moves.end(), [](const Tally& a, const Tally& b) { return a.games > b.games; });
        for (const Tally& t : moves) {
            if (t.games < minShare * total || t.points2 < minScore * 2 * t.games)
                continue;
            put(key, 8);
            put(t.move, 2);
            put(t.games, 4);
            ++entries;
        }
    }
    std::cerr << kept << " positions, " << entries << " moves, " << entries * 14 << " bytes\n";
    return 0;
}

int probe(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: openingbook probe book.bin \"<fen>\"\n";
        return 2;
    }
    Position pos;
    const std::optional<Position::Chess> status = pos.FenInput(argv[3]);
    if (!status) {
        std::cerr << "not a FEN: " << argv[3] << "\n";
        return 2;
    }
    const std::uint64_t key = keyHash(positionKey(pos, *status));
    std::ifstream in(argv[2], std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto get = [&](std::size_t at, int bytes) {
        std::uint64_t value = 0;
        for (int i = bytes - 1; i >= 0; --i)
            value = value << 8 | data[at + i];
        return value;
    };
    std::vector<std::pair<std::string, unsigned>> moves;
    unsigned total = 0;
    for (std::size_t at = 0; at + 14 <= data.size(); at += 14) {   // a straight scan: fine for a check
        if (get(at, 8) != key)
            continue;
        const unsigned games = unsigned(get(at + 10, 4));
        moves.push_back({moveUci(std::uint16_t(get(at + 8, 2))), games});
        total += games;
    }
    std::cout << positionKey(pos, *status) << "\n";
    if (moves.empty())
        std::cout << "  not in the book\n";
    for (const auto& [uci, games] : moves)
        std::cout << "  " << uci << "  " << (100 * games + total / 2) / total << " %\n";
    return 0;
}

int names(int argc, char** argv) {
    struct Name { std::string eco, name, uci; int plies; };
    std::map<std::string, Name> byKey;
    Position pos;
    int lines = 0, failed = 0, duplicates = 0;
    for (int i = 2; i < argc; ++i) {
        std::ifstream file(argv[i]);
        if (!file) {
            std::cerr << "can't read " << argv[i] << "\n";
            return 1;
        }
        std::string line;
        std::getline(file, line);                        // the header: eco, name, pgn
        while (std::getline(file, line)) {
            std::istringstream cols(line);
            std::string eco, name, pgn;
            if (!std::getline(cols, eco, '\t') || !std::getline(cols, name, '\t') || !std::getline(cols, pgn))
                continue;
            ++lines;
            Position::Chess status;
            int plies = 0;
            std::string error, uci;
            if (!playLine(pos, pgn, status, plies, error, uci)) {
                std::cerr << "no move '" << error << "' in " << eco << " " << name << ": " << pgn << "\n";
                ++failed;
                continue;
            }
            const std::string key = positionKey(pos, status);
            auto found = byKey.find(key);
            if (found != byKey.end()) {
                ++duplicates;
                if (found->second.plies <= plies)
                    continue;                            // the shorter line keeps its name
            }
            byKey[key] = {eco, name, uci, plies};
        }
    }
    for (const auto& [key, n] : byKey)
        std::cout << key << '\t' << n.eco << '\t' << n.name << '\t' << n.uci << '\n';
    std::cerr << lines << " lines, " << failed << " not playable, " << duplicates << " transpositions, "
              << byKey.size() << " positions\n";
    return failed == 0 ? 0 : 1;
}


// How far real games stay on well-trodden ground: per game (PGN on stdin), the first ply whose move was played
// fewer than N times in counts.tsv's games (2000+, the first 20 plies), for N = 20, 50, 200, 1000.
int depth(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: openingbook depth counts.tsv < games.pgn\n";
        return 2;
    }
    std::unordered_map<std::uint64_t, std::uint32_t> seen;      // hash ^ move code spread → games
    auto slot = [](std::uint64_t hash, std::uint16_t code) { return hash ^ (std::uint64_t(code) * 0x9E3779B97F4A7C15ull); };
    {
        std::ifstream in(argv[2]);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream cols(line);
            std::uint64_t hash;
            int ply;
            std::string uci;
            std::uint32_t games, points2;
            if (cols >> std::hex >> hash >> std::dec >> ply >> uci >> games >> points2)
                seen[slot(hash, moveCode(uci))] = games;
        }
    }
    const int limits[] = {20, 50, 200, 1000};
    std::vector<int> left[4];                                    // per limit: the ply each game left (20 = still in)
    Position pos;
    Moves mover;
    const Position::Chess start = pos.FenInput(Position::StartFen).value();
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty() || line[0] == '[')
            continue;
        Position::Chess status = start;
        std::istringstream words(line);
        std::string word;
        int ply = 0, depthInComment = 0;
        int out[4] = {20, 20, 20, 20};
        while (ply < 20 && words >> word) {
            if (word[0] == '{') ++depthInComment;
            if (depthInComment > 0) {
                if (word.back() == '}') --depthInComment;
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(word[0])))
                continue;
            std::optional<Position::Move> move = pos.SanToMove(status, word);
            if (!move)
                break;
            const auto it = seen.find(slot(keyHash(positionKey(pos, status)), moveCode(pos.MoveToUci(*move))));
            const std::uint32_t games = it == seen.end() ? 0 : it->second;
            for (int i = 0; i < 4; ++i)
                if (out[i] == 20 && games < std::uint32_t(limits[i]))
                    out[i] = ply;
            status = mover.make(status, *move);
            ++ply;
        }
        for (int i = 0; i < 4; ++i)
            left[i].push_back(out[i]);
    }
    std::cout << "games " << left[0].size() << "; the move that left (ply, 0-based; 20 = still in after 20 plies)\n";
    for (int i = 0; i < 4; ++i) {
        std::vector<int>& v = left[i];
        std::sort(v.begin(), v.end());
        double sum = 0;
        int in10 = 0, in14 = 0, in20 = 0;
        for (int p : v) {
            sum += p;
            in10 += p >= 10;
            in14 += p >= 14;
            in20 += p >= 20;
        }
        const double n = double(v.size());
        std::cout << "  move seen < " << limits[i] << " times: median ply " << v[v.size() / 2] << ", mean "
                  << sum / n << "; still in after 10 plies " << 100 * in10 / n << " %, after 14 " << 100 * in14 / n
                  << " %, after 20 " << 100 * in20 / n << " %\n";
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::string command = argc > 1 ? argv[1] : "";
    if (command == "names")
        return names(argc, argv);
    if (command == "count")
        return count(argc, argv);
    if (command == "pack")
        return pack(argc, argv);
    if (command == "probe")
        return probe(argc, argv);
    if (command == "depth")
        return depth(argc, argv);
    std::cerr << "usage: openingbook names a.tsv b.tsv ... > names.tsv\n"
                 "       zstdcat games.pgn.zst | openingbook count [games] > counts.tsv\n"
                 "       openingbook pack counts.tsv book.bin [plies] [min games] [min share %] [min score %]\n"
                 "       openingbook probe book.bin \"<fen>\"\n"
                 "       openingbook depth counts.tsv < games.pgn\n";
    return 2;
}

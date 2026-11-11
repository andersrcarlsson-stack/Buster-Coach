// model_test.cpp — the test gate for the board model. Exit status 0 = all pass (ctest reads it).

#include "position.h"
#include "move.h"
#include <string>

struct PerftCase {
    std::string name;
    std::string fen;
    int depth;
    long long expected;
};

struct MakeCase {
    std::string name;
    std::string fen;
    int from;
    int to;
    std::string expected_fen;
};

int main() {

    // --- Perft: the move generator ---------------------------------------------------------------
    const PerftCase cases[] = {
        {"Start",    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               4,  197281},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   3,   97862},
        {"Pos 3",    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                              4,   43238},
        {"Pos 4",    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",        3,    9467},
        {"Pos 5",    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",               3,   62379},
    };

    Position pos;
    int failures = 0;
    for (const PerftCase& c : cases) {
        long long nodes = pos.Test(pos.FenInput(c.fen).value(), c.depth);
        bool ok = (nodes == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << c.name << " d" << c.depth
                  << ": " << nodes << " (expected " << c.expected << ")\n";
        if (!ok) ++failures;
    }

    // --- make: the new position (counters, castling) ---------------------------------------------
    const MakeCase make_cases[] = {
        {"O-O keeps counting",   "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 5 1",                         4,  6, "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 6 1"},
        {"O-O-O keeps counting", "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 5 1",                        60, 58, "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 6 2"},
        {"quiet move counts",    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",      6, 21, "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1"},
        {"pawn move resets",     "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 5 1",     12, 28, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"},
        {"capture resets",       "4k3/8/8/3p4/8/2N5/8/4K3 w - - 7 20",                           18, 35, "4k3/8/8/3N4/8/8/8/4K3 b - - 0 20"},
    };

    Moves mover;
    for (const MakeCase& c : make_cases) {
        Position::Chess status = pos.FenInput(c.fen).value();
        std::string got = "(move not legal)";
        for (Position::Move move : pos.LegalMoves(status).moves) {
            if (move.from_square == c.from && move.to_square == c.to)
                got = pos.FenOutput(mover.make(status, move));
        }
        bool ok = (got == c.expected_fen);
        std::cout << (ok ? "PASS " : "FAIL ") << c.name << ": " << got
                  << (ok ? "" : "  (expected " + c.expected_fen + ")") << "\n";
        if (!ok) ++failures;
    }

    // --- MoveToUci -------------------------------------------------------------------------------
    struct UciOutCase { std::string name; Position::Move move; std::string expected; };
    const UciOutCase uci_out_cases[] = {
        {"quiet",         Position::Move{.from_square = 12, .to_square = 28},                        "e2e4"},
        {"corner",        Position::Move{.from_square = 0,  .to_square = 63},                        "a1h8"},
        {"white promo Q", Position::Move{.from_square = 52, .to_square = 60, .promotion_piece = 10}, "e7e8q"},
        {"black promo N", Position::Move{.from_square = 11, .to_square = 3,  .promotion_piece = 3},  "d2d1n"},
    };
    for (const UciOutCase& c : uci_out_cases) {
        std::string got = pos.MoveToUci(c.move);
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "uci out " << c.name << ": " << got
                  << (ok ? "" : "  (expected " + c.expected + ")") << "\n";
        if (!ok) ++failures;
    }

    // --- Key: the FEN without its move counters ---------------------------------------------------
    {
        const Position::Chess startPos = pos.FenInput(Position::StartFen).value();
        const bool ok = pos.Key(startPos) == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"
                        && pos.Key(pos.FenInput("4k3/8/8/8/8/8/8/4K3 b - - 37 90").value()) == "4k3/8/8/8/8/8/8/4K3 b - -";
        std::cout << (ok ? "PASS " : "FAIL ") << "key: the FEN without the move counters\n";
        if (!ok) ++failures;
    }

    // --- RepetitionKey: the en passant square only when en passant is really possible --------------------
    {
        // After 1. e4: e3 is set, but no black pawn can take: the same position as without it.
        const bool none = pos.RepetitionKey(pos.FenInput(
            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1").value())
            == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq -";
        // A black pawn on d4 can take on e3: the square stays.
        const bool real = pos.RepetitionKey(pos.FenInput(
            "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 3").value())
            == "rnbqkbnr/ppp1pppp/8/8/3pP3/8/PPPP1PPP/RNBQKBNR b KQkq e3";
        // The pawn on d4 is pinned (rook a4, king h4 on the 4th rank; taking empties the rank): not legal, so gone.
        const bool pinned = pos.RepetitionKey(pos.FenInput(
            "8/8/8/8/R2pP2k/8/8/4K3 b - e3 0 1").value()) == "8/8/8/8/R2pP2k/8/8/4K3 b - -";
        const bool ok = none && real && pinned;
        std::cout << (ok ? "PASS " : "FAIL ") << "repetition key: e.p. only when possible (" << none << real << pinned
                  << ")\n";
        if (!ok) ++failures;
    }

    // --- UciToMove -------------------------------------------------------------------------------
    struct UciInCase { std::string name; std::string fen; std::string uci; bool legal; };
    const UciInCase uci_in_cases[] = {
        {"legal pawn push",   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4",   true},
        {"illegal move",      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e5",   false},
        {"castling",          "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                     "e1g1",   true},
        {"promotion",         "8/4P3/8/8/8/8/k7/4K3 w - - 0 1",                           "e7e8q",  true},
        {"bestmove (none)",   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "(none)", false},
    };
    for (const UciInCase& c : uci_in_cases) {
        std::optional<Position::Move> move = pos.UciToMove(pos.FenInput(c.fen).value(), c.uci);
        bool ok = (move.has_value() == c.legal);
        if (ok && move)                                   // found: it must convert back to the same text
            ok = (pos.MoveToUci(*move) == c.uci);
        std::cout << (ok ? "PASS " : "FAIL ") << "uci in " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- State: mate, stalemate, 50 moves, dead positions ----------------------------------------
    struct StateCase { std::string name; std::string fen; Position::GameState expected; };
    const StateCase state_cases[] = {
        {"start: ongoing",      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",      Position::GameState::Ongoing},
        {"fool's mate",         "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3", Position::GameState::Checkmate},
        {"mate with Black",     "rnbqkbnr/ppppp2p/5p2/6pQ/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 3", Position::GameState::Checkmate},
        {"stalemate",           "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1",                                Position::GameState::Stalemate},
        {"clock 99: ongoing",   "4k3/8/8/8/8/8/8/R3K3 w - - 99 80",                              Position::GameState::Ongoing},
        {"clock 100: 50 moves", "4k3/8/8/8/8/8/8/R3K3 w - - 100 80",                             Position::GameState::FiftyMoves},
        {"mate beats 50 moves", "R5k1/5ppp/8/8/8/8/8/6K1 b - - 100 80",                          Position::GameState::Checkmate},
        {"K v K: dead",         "8/8/4k3/8/8/4K3/8/8 w - - 0 1",                                 Position::GameState::InsufficientMaterial},
        {"KB v K: dead",        "8/8/4k3/8/8/4K3/8/5B2 w - - 0 1",                               Position::GameState::InsufficientMaterial},
        {"KN v K: dead",        "8/8/4k3/2n5/8/4K3/8/8 w - - 0 1",                               Position::GameState::InsufficientMaterial},
        {"KB v KB same colour", "5b2/8/4k3/8/8/4K3/8/2B5 w - - 0 1",                             Position::GameState::InsufficientMaterial},
        {"KBB same colour v K", "8/8/4k3/8/8/4K3/8/2B1B3 w - - 0 1",                             Position::GameState::InsufficientMaterial},
        {"KB v KB opposite",    "5b2/8/4k3/8/8/4K3/8/5B2 w - - 0 1",                             Position::GameState::Ongoing},
        {"KN v KN: mate exists","8/8/4k3/2n5/8/4K3/8/1N6 w - - 0 1",                             Position::GameState::Ongoing},
        {"KNN v K: mate exists","8/8/4k3/8/8/4K3/8/1N4N1 w - - 0 1",                             Position::GameState::Ongoing},
        {"KP v K: can promote", "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1",                               Position::GameState::Ongoing},
        };
    for (const StateCase& c : state_cases) {
        bool ok = (pos.State(pos.FenInput(c.fen).value()) == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "state " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- CanMate: the flag rule ------------------------------------------------------------------
    struct MateCase { std::string name; std::string fen; bool white; bool expected; };
    const MateCase mate_cases[] = {
        {"lone king",          "8/8/4k3/8/8/4K3/8/7q w - - 0 1",     true,  false},
        {"K+B v K+Q",          "8/8/4k3/8/8/4K3/8/2B4q w - - 0 1",   true,  false},
        {"K+N v K+Q",          "8/8/4k3/8/8/4K3/8/1N5q w - - 0 1",   true,  false},
        {"K+NN (with help)",   "8/8/4k3/8/8/4K3/8/1N4Nq w - - 0 1",  true,  true},
        {"K+P",                "8/8/4k3/8/8/4K3/4P3/7q w - - 0 1",   true,  true},
        {"K+R",                "8/8/4k3/8/8/4K3/8/R6q w - - 0 1",    true,  true},
        {"K+BB",               "8/8/4k3/8/8/4K3/8/2B2B1q w - - 0 1", true,  true},
        {"K+BB one colour",    "8/8/4k3/8/8/4K3/8/2B1B3 w - - 0 1",  true,  false},
        {"Black K+N v K+R",    "8/8/4k3/2n5/8/4K3/8/R7 w - - 0 1",   false, true},
        {"K+N v K+N",          "8/8/4k3/2n5/8/4K3/8/1N6 w - - 0 1",  true,  true},
        {"K+B v K+B same",     "8/8/4k3/b7/8/4K3/8/2B5 w - - 0 1",   true,  false},
        {"K+B v K+B opposite", "8/8/4k3/1b6/8/4K3/8/2B5 w - - 0 1",  true,  true},
        {"K+B v K+N",          "8/8/4k3/2n5/8/4K3/8/2B5 w - - 0 1",  true,  true},
        {"K+B v K+R",          "8/8/4k3/8/8/4K3/8/2B4r w - - 0 1",   true,  false},
    };
    for (const MateCase& c : mate_cases) {
        bool ok = (pos.CanMate(pos.FenInput(c.fen).value(), c.white) == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "can mate: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- PassTurn: the other side to move (for "what does the opponent threaten?") ---------------
    struct PassCase { std::string name; std::string fen; std::string expected; };   // "" = none
    const PassCase pass_cases[] = {
        {"start position",  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"},
        {"en passant gone", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
                            "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1"},
        {"in check: none",  "rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2",
                            ""},
    };
    for (const PassCase& c : pass_cases) {
        const auto passed = pos.PassTurn(pos.FenInput(c.fen).value());
        const std::string got = passed ? pos.FenOutput(*passed) : "";
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "pass turn: " << c.name << ": " << got << "\n";
        if (!ok) ++failures;
    }

    // --- Attackers: who attacks a square (geometry only, attack_design.md) -----------------------
    struct AttackCase { std::string name; std::string fen; std::string square; bool white; std::string expected; };
    const std::string recapture = "r1bq1rk1/pppp1ppp/2n2n2/1B2p3/4P3/2bP1N2/PPP2PPP/R1BQ1RK1 w - - 0 7";
    const std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const AttackCase attack_cases[] = {   // expected: square names, alphabetical, "" = none
        {"bishop c3, by White",       recapture, "c3", true,  "b2"},
        {"bishop c3, by Black: none", recapture, "c3", false, ""},
        {"pawn e4, by Black",         recapture, "e4", false, "f6"},
        {"pawn e4, by White (def.)",  recapture, "e4", true,  "d3"},
        {"empty e3, start",           start,     "e3", true,  "d2 f2"},
        {"empty f3, start",           start,     "f3", true,  "e2 g1 g2"},
        {"no x-ray (Q behind R)",     "4k3/8/8/3q4/8/8/3R4/3QK3 w - - 0 1", "d5", true,  "d2"},
        {"a pinned knight counts",    "4k3/4r3/8/8/8/8/4N3/4K3 w - - 0 1",  "f4", true,  "e2"},
        {"pawn: diagonal",            "4k3/8/8/8/3p4/8/8/4K3 w - - 0 1",    "e3", false, "d4"},
        {"pawn: not the push",        "4k3/8/8/8/3p4/8/8/4K3 w - - 0 1",    "d3", false, ""},
        {"the king counts",           "4k3/8/8/8/3p4/8/8/4K3 w - - 0 1",    "e2", true,  "e1"},
    };
    auto squareIndex = [](const std::string& s) { return (s[1] - '1') * 8 + (s[0] - 'a'); };   // "e4" -> 28
    auto squareName = [](int sq) { return std::string{char('a' + sq % 8), char('1' + sq / 8)}; };
    for (const AttackCase& c : attack_cases) {
        std::vector<std::string> names;
        for (int sq : pos.Attackers(pos.FenInput(c.fen).value(), squareIndex(c.square), c.white))
            names.push_back(squareName(sq));
        std::sort(names.begin(), names.end());     // any order from Attackers: compare sorted
        std::string got;
        for (const std::string& n : names)
            got += (got.empty() ? "" : " ") + n;
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "attackers: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- Hanging: en prise (attacked, and undefended or attacked by a cheaper piece; never the king) ----------
    struct HangCase { std::string name; std::string fen; bool white; std::string expected; };
    const HangCase hang_cases[] = {   // expected: square names, alphabetical, "" = none
        {"your game: Black's Bc3",         recapture, false, "c3"},
        {"your game: White, nothing",      recapture, true,  ""},
        {"defended N, attacked by a pawn", "4k3/8/5p2/4n3/3P4/8/8/4K3 w - - 0 1",  false, "e5"},
        {"defended N, attacked by a B",    "4k3/8/2b5/8/4N3/3P4/8/4K3 w - - 0 1",  true,  ""},
        {"undefended N, attacked by a B",  "4k3/8/2b5/8/4N3/8/8/4K3 w - - 0 1",    true,  "e4"},
        {"the king takes a loose piece",   "4k3/8/8/8/8/8/3n4/4K3 w - - 0 1",      false, "d2"},
        {"a king in check isn't hanging",  "4k3/8/8/8/8/8/8/r3K3 w - - 0 1",       true,  ""},
    };
    for (const HangCase& c : hang_cases) {
        std::vector<std::string> names;
        for (int sq : pos.Hanging(pos.FenInput(c.fen).value(), c.white))
            names.push_back(squareName(sq));
        std::sort(names.begin(), names.end());
        std::string got;
        for (const std::string& n : names)
            got += (got.empty() ? "" : " ") + n;
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "hanging: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- MaterialAlongLine: the balance per ply (the start side's view), "x" = the ply captured ------------
    struct LineCase { std::string name; std::string fen; std::vector<std::string> moves; std::string expected; };
    const LineCase line_cases[] = {
        {"a pawn won and lost", start, {"e2e4", "d7d5", "e4d5", "d8d5"}, "0 0 1x 0x"},
        {"your game: Bxc3 bxc3",
         "r1bq1rk1/pppp1ppp/2n2n2/1B2p3/1b2P3/2NP1N2/PPP2PPP/R1BQ1RK1 b - - 0 6", {"b4c3", "b2c3"}, "3x 0x"},
        {"a free knight",     "4k3/8/8/3n4/8/8/8/3QK3 w - - 0 1", {"d1d5", "e8e7"}, "3x 3"},
        {"a promotion",       "8/4P1k1/8/8/8/8/8/4K3 w - - 0 1",   {"e7e8q"},        "8"},
        {"an illegal ply ends the line", start, {"e2e4", "e2e4"}, "0"},
    };
    for (const LineCase& c : line_cases) {
        std::string got;
        for (const Position::MaterialStep& s : pos.MaterialAlongLine(pos.FenInput(c.fen).value(), c.moves))
            got += (got.empty() ? "" : " ") + std::to_string(s.balance) + (s.capture ? "x" : "");
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "material: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- StableGain: where a line's material gain lands ("amount@ply"; ply -1 = nowhere) --------------------
    struct GainCase { std::string name; std::string fen; std::vector<std::string> moves; std::string expected; };
    const GainCase gain_cases[] = {
        {"won and lost: nothing",     start, {"e2e4", "d7d5", "e4d5", "d8d5"}, "0@-1"},
        {"your game: an equal trade",
         "r1bq1rk1/pppp1ppp/2n2n2/1B2p3/1b2P3/2NP1N2/PPP2PPP/R1BQ1RK1 b - - 0 6", {"b4c3", "b2c3"}, "0@-1"},
        {"a free knight",             "4k3/8/8/3n4/8/8/8/3QK3 w - - 0 1", {"d1d5", "e8e7"}, "3@0"},
        {"a pawn up at the end",      start, {"e2e4", "d7d5", "e4d5", "g8f6"}, "1@2"},
        {"a trailing capture: unseen", "4k3/8/8/3n4/8/8/8/3QK3 w - - 0 1", {"d1d5"}, "0@-1"},
        {"a promotion",               "8/4P1k1/8/8/8/8/8/4K3 w - - 0 1", {"e7e8q", "g7f6"}, "8@0"},
        // Anders' game (2026-11-10): 13.Bxd6 wins the knight on f6 at ply 6 (16.Rxf6); the even rook trade after it
        // (16...Rf8 17.Rxf8 Rxf8) mustn't move the date to ply 9 (past the coach's 6 plies: "a quiet move").
        {"a gain, then an even trade",
         "r3kb1r/1p2q2p/pn1ppnp1/2p5/5B2/2N2N2/PPPQ1PPP/2K1RB1R w kq - 0 13",
         {"f4d6", "e7d6", "d2d6", "f8d6", "e1e6", "e8d7", "e6f6", "a8f8", "f6f8", "h8f8", "f1e2"}, "2@6"},
    };
    for (const GainCase& c : gain_cases) {
        const Position::Gain g = Position::StableGain(pos.MaterialAlongLine(pos.FenInput(c.fen).value(), c.moves));
        const std::string got = std::to_string(g.amount) + "@" + std::to_string(g.ply);
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "gain: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- FirstExchange: the captures a line starts ("balance: yours / theirs", piece letters, "-" = none) ----------
    struct ExchangeCase { std::string name; std::string fen; std::vector<std::string> moves; std::string expected; };
    const ExchangeCase exchange_cases[] = {
        {"your game: Nxd4, even",
         "r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3",
         {"c6d4", "f3d4", "e5d4", "d1d4", "d8f6", "d4f6"}, "0: PN / NP"},
        {"your game: Bxc3 bxc3",
         "r1bq1rk1/pppp1ppp/2n2n2/1B2p3/1b2P3/2NP1N2/PPP2PPP/R1BQ1RK1 b - - 0 6", {"b4c3", "b2c3"}, "0: N / B"},
        {"a free knight",        "4k3/8/8/3n4/8/8/8/3QK3 w - - 0 1", {"d1d5", "e8e7"}, "3: N / -"},
        {"not a capture",        start, {"e2e4", "d7d5", "e4d5", "d8d5"}, "0: - / -"},
    };
    auto letters = [](const std::vector<int>& codes) {
        std::string s;
        for (int code : codes)
            s += "PNBRQK"[(code + 1) / 2 - 1];
        return s.empty() ? std::string("-") : s;
    };
    for (const ExchangeCase& c : exchange_cases) {
        const Position::Exchange e =
            Position::FirstExchange(pos.MaterialAlongLine(pos.FenInput(c.fen).value(), c.moves));
        const std::string got = std::to_string(e.balance) + ": " + letters(e.yours) + " / " + letters(e.theirs);
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "exchange: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- FirstSettled: where a line first settles after the reply (a sacrifice: a dip below 0) --------------------
    const ExchangeCase settled_cases[] = {
        {"Anders' Bxe4: the bishop for 2 pawns", "r1k5/1p1n3p/p1n5/3N1p2/4p3/3B4/PPP2PPP/2KRR3 w - - 0 23",
         {"d3e4", "f5e4", "e1e4", "d7f8", "e4e8"}, "-1: PP / B"},
        {"a knight put en prise",  "4k3/8/4p3/8/1N6/8/8/4K3 w - - 0 1", {"b4d5", "e6d5", "e1e2"}, "-3: - / N"},
        {"a free knight",          "4k3/8/8/3n4/8/8/8/3QK3 w - - 0 1", {"d1d5", "e8e7"}, "3: N / -"},
        {"an even trade settles level",
         "r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3",
         {"c6d4", "f3d4", "e5d4", "d1d4", "d8f6", "d4f6"}, "0: PN / NP"},
        {"one ply: nothing settles", start, {"e2e4"}, "0: - / -"},
    };
    for (const ExchangeCase& c : settled_cases) {
        const Position::Exchange e =
            Position::FirstSettled(pos.MaterialAlongLine(pos.FenInput(c.fen).value(), c.moves));
        const std::string got = std::to_string(e.balance) + ": " + letters(e.yours) + " / " + letters(e.theirs);
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "settled: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- NewThreats: the opponent's pieces hanging after the move that weren't before (why good, kind 2) -----------
    struct ThreatCase { std::string name; std::string fen; std::string move; std::string expected; };
    const ThreatCase threat_cases[] = {   // expected: square names, alphabetical, "" = none
        {"a fork: queen and rook",     "8/8/3q1r2/8/8/8/3N4/4K2k w - - 0 1", "d2e4", "d6 f6"},
        {"king and rook: the rook",    "r3k3/8/8/1N6/8/8/8/4K3 w - - 0 1",   "b5c7", "a8"},
        {"a discovered attack",        "4k3/8/8/3b4/8/8/3N4/3RK3 w - - 0 1", "d2b3", "d5"},
        {"already hanging: no credit", "4k3/8/8/8/3b4/8/8/3RK3 w - - 0 1",   "e1e2", ""},
        {"a quiet move: nothing",      start,                                "e2e4", ""},
    };
    for (const ThreatCase& c : threat_cases) {
        const Position::Chess status = pos.FenInput(c.fen).value();
        std::vector<std::string> names;
        for (int sq : pos.NewThreats(status, pos.UciToMove(status, c.move).value()))
            names.push_back(squareName(sq));
        std::sort(names.begin(), names.end());
        std::string got;
        for (const std::string& n : names)
            got += (got.empty() ? "" : " ") + n;
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "new threats: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- ThreatAnswer: how your move answers a quiet threat (why_design.md, "stopping a quiet threat") ----------
    {
        struct AnswerCase { std::string name; std::string fen; std::string yours; std::string threat;
                            Position::How how; int piece; };
        const AnswerCase answer_cases[] = {
            // ...d5 would hit the knight on c4 (a pawn attacks it: hanging)
            {"the knight moves away",  "4k3/3p4/8/8/2N5/8/8/4K3 w - - 0 1",  "c4e5", "d7d5", Position::How::MovesAway, 4},
            {"the bishop takes d7",    "4k3/3p4/8/8/2N5/7B/8/4K3 w - - 0 1", "h3d7", "d7d5", Position::How::Takes, 1},
            {"Nd6+ prevents ...d5",    "4k3/3p4/8/8/2N5/8/8/4K3 w - - 0 1",  "c4d6", "d7d5", Position::How::Prevents, 0},
            {"a king move: not stopped", "4k3/3p4/8/8/2N5/8/8/4K3 w - - 0 1", "e1d2", "d7d5", Position::How::None, 0},
            // ...Ng4 would hit the undefended bishop on e3 (a knight: not cheaper, so a defender helps)
            {"Kd2 defends the bishop", "4k3/8/5n2/8/8/4B3/8/4K3 w - - 0 1",  "e1d2", "f6g4", Position::How::Defends, 6},
            {"h3: ...Ng4 costs the knight", "4k3/8/5n2/8/8/4B3/7P/4K3 w - - 0 1", "h2h3", "f6g4", Position::How::CostsIt, 3},
            // ...Nc2 would fork the rooks on a1 and e1: one rook leaving breaks the fork (the other then moves)
            {"a fork: one rook away breaks it", "7k/8/8/8/3n4/8/8/R3R2K w - - 0 1", "a1a5", "d4c2", Position::How::MovesAway, 8},
            {"a fork: a king move doesn't",     "7k/8/8/8/3n4/8/8/R3R2K w - - 0 1", "h1g2", "d4c2", Position::How::None, 0},
            // ...d5 (defended by e6) would fork the knight on c4 and the bishop on e4
            {"the pawn fork: the bishop leaves", "4k3/3p4/4p3/8/2N1B3/8/8/4K3 w - - 0 1", "e4d3", "d7d5", Position::How::MovesAway, 6},
            {"the pawn fork: a king move doesn't", "4k3/3p4/4p3/8/2N1B3/8/8/4K3 w - - 0 1", "e1d2", "d7d5", Position::How::None, 0},
            {"a threat that attacks nothing", "4k3/3p4/8/8/2N5/8/8/4K3 w - - 0 1", "c4e5", "e8e7", Position::How::None, 0},
        };
        for (const AnswerCase& c : answer_cases) {
            const Position::Chess status = pos.FenInput(c.fen).value();
            const Position::Answer got = pos.ThreatAnswer(status, pos.UciToMove(status, c.yours).value(), c.threat);
            const bool ok = got.how == c.how && got.piece == c.piece;
            std::cout << (ok ? "PASS " : "FAIL ") << "threat answer: " << c.name
                      << (ok ? "" : "  (got " + std::to_string(int(got.how)) + "/" + std::to_string(got.piece) + ")") << "\n";
            if (!ok) ++failures;
        }
    }

    // --- MovePrinciple: a visible opening principle in a move (why_design.md, kind 4) --------------------------
    struct PrincipleCase { std::string name; std::string fen; std::string move; std::string expected; };
    const std::string afterE4E5 = "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2";
    const PrincipleCase principle_cases[] = {
        {"e4: the centre",           start,     "e2e4", "centre"},
        {"d3: not the centre",       start,     "d2d3", "none"},
        {"Nf3: develops",            start,     "g1f3", "develops"},
        {"Bc4: develops",            afterE4E5, "f1c4", "develops"},
        {"O-O: castles",             "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1", "castles"},
        {"a3: nothing",              start,     "a2a3", "none"},
        {"Ng5: not from its start",  "rnbqkbnr/ppp1pppp/8/8/4p3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3", "f3g5", "none"},
    };
    for (const PrincipleCase& c : principle_cases) {
        static const char* names[] = {"none", "castles", "develops", "centre"};   // in the enum's order
        const Position::Chess status = pos.FenInput(c.fen).value();
        const std::string got = names[int(pos.MovePrinciple(status, pos.UciToMove(status, c.move).value()))];
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "principle: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- FEN in and out --------------------------------------------------------------------------
    struct FenCase { std::string name; std::string fen; std::string expected; };
    const FenCase fen_cases[] = {
        {"full FEN round trip", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"4 fields (EPD style)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
                                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
    };
    for (const FenCase& c : fen_cases) {
        std::string got = pos.FenOutput(pos.FenInput(c.fen).value());
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "fen " << c.name << ": " << got << "\n";
        if (!ok) ++failures;
    }

    // --- FEN syntax errors -----------------------------------------------------------------------
    const std::string invalid_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNX w KQkq - 0 1",   // unknown piece letter
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP w KQkq - 0 1",            // only 7 ranks
        "rnbqkbnr/ppppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  // 9 squares in a rank
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",   // side to move 'x'
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkx - 0 1",   // bad castling field
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e5 0 1",  // e.p. square not on rank 3/6
        "hello",
        "",
    };
    for (const std::string& fen : invalid_fens) {
        bool ok = !pos.FenInput(fen).has_value();       // must be rejected
        std::cout << (ok ? "PASS " : "FAIL ") << "invalid fen rejected: \"" << fen << "\"\n";
        if (!ok) ++failures;
    }

    // --- IsValid: possible positions -------------------------------------------------------------
    struct ValidCase { std::string name; std::string fen; bool valid; };
    const ValidCase valid_cases[] = {
        {"start position",            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               true},
        {"Kiwipete",                  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   true},
        {"K+R v K, castling right",   "4k3/8/8/8/8/8/8/4K2R w K - 0 1",                                         true},
        {"no kings",                  "8/8/8/8/8/8/8/8 w - - 0 1",                                              false},
        {"no black king",             "8/8/8/8/8/8/8/4K3 w - - 0 1",                                            false},
        {"two white kings",           "4k3/8/8/8/8/8/8/4KK2 w - - 0 1",                                         false},
        {"white pawn on rank 8",      "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",                                         false},
        {"black pawn on rank 1",      "4k3/8/8/8/8/8/8/p3K3 w - - 0 1",                                         false},
        {"side not to move in check", "4k3/8/8/8/8/8/8/4K2r b - - 0 1",                                         false},
        {"castling right, no rook",   "4k3/8/8/8/8/8/8/4K3 w K - 0 1",                                          false},
        {"castling right, king moved","4k3/8/8/8/8/8/8/R4K2 w Q - 0 1",                                         false},
        {"e.p. after 1.e4 e5",          "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2",        true},
        {"e.p. no pawn in front",       "4k3/8/8/8/8/8/8/4K3 w - e6 0 1",                                       false},
        {"e.p. wrong rank for side",    "4k3/8/8/4p3/8/8/8/4K3 b - e6 0 1",                                     false},
        {"e.p. origin square occupied", "4k3/4p3/8/4p3/8/8/8/4K3 w - e6 0 1",                                   false},
    };
    for (const ValidCase& c : valid_cases) {
        bool ok = (pos.IsValid(pos.FenInput(c.fen).value()) == c.valid);
        std::cout << (ok ? "PASS " : "FAIL ") << "valid? " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- in_check without a king (M8): the board editor shows kingless boards; no king = not in check ------------
    {
        Moves checker;
        const bool ok = !checker.in_check(pos.FenInput("4r3/8/8/8/8/8/8/8 w - - 0 1").value(), 0, 0);
        std::cout << (ok ? "PASS " : "FAIL ") << "in_check: no king\n";
        if (!ok) ++failures;
    }

    // --- Validate: the first reason a position is impossible (the board editor's line) ----------------------------
    struct ReasonCase { std::string name; std::string fen; std::string expected; };
    const ReasonCase reason_cases[] = {
        {"start position",             "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "None"},
        {"no kings: White first",      "8/8/8/8/8/8/8/8 w - - 0 1",                                "WhiteKings"},
        {"no black king",              "8/8/8/8/8/8/8/4K3 w - - 0 1",                              "BlackKings"},
        {"white pawn on rank 8",       "P3k3/8/8/8/8/8/8/4K3 w - - 0 1",                           "PawnOnBackRank"},
        {"side not to move in check",  "4k3/8/8/8/8/8/8/4K2r b - - 0 1",                           "NotToMoveInCheck"},
        {"castling right, no rook",    "4k3/8/8/8/8/8/8/4K3 w K - 0 1",                            "Castling"},
        {"e.p. no pawn in front",      "4k3/8/8/8/8/8/8/4K3 w - e6 0 1",                           "EnPassant"},
    };
    static const char *reason_names[] = {"None", "WhiteKings", "BlackKings", "PawnOnBackRank", "NotToMoveInCheck",
                                         "Castling", "EnPassant"};   // in the enum's order
    for (const ReasonCase& c : reason_cases) {
        const std::string got = reason_names[static_cast<int>(pos.Validate(pos.FenInput(c.fen).value()))];
        bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "reason: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    // --- CapturedPiece ---------------------------------------------------------------------------
    // CapturedPiece: which piece code (1-12) a move takes, 0 for none.
    struct CaptureCase { std::string name; std::string fen; std::string uci; int expected; };
    const CaptureCase captures[] = {
        {"quiet move",        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        "g1f3", 0},
        {"double push",       "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        "d2d4", 0},  // e.p. flag set, no capture
        {"pawn takes pawn",   "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",   "e4d5", 1},  // black pawn
        {"queen takes queen", "4k3/8/8/8/8/8/3q4/3QK3 w - - 0 1",                                "d1d2", 9},  // black queen
        {"e.p. by White",     "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",    "e5d6", 1},  // the pawn on d5
        {"e.p. by Black",     "rnbqkbnr/pppp1ppp/8/8/3Pp3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 3",     "e4d3", 2},  // the pawn on d4
    };
    for (const CaptureCase& c : captures) {
        const Position::Chess status = pos.FenInput(c.fen).value();
        const auto move = pos.UciToMove(status, c.uci);
        const bool ok = move && pos.CapturedPiece(status, *move) == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "capture " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- MoveToSan -------------------------------------------------------------------------------
    // MoveToSan: the move in standard algebraic notation.
    struct SanCase { std::string name; std::string fen; std::string uci; std::string expected; };
    const SanCase sans[] = {
        {"pawn push",      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        "e2e4",  "e4"},
        {"knight",         "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",        "g1f3",  "Nf3"},
        {"pawn capture",   "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",   "e4d5",  "exd5"},
        {"piece capture",  "rnbqkbnr/ppp1pppp/8/3p4/8/2N5/PPPPPPPP/R1BQKBNR w KQkq - 0 2",    "c3d5",  "Nxd5"},
        {"castle short",   "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                            "e1g1",  "O-O"},
        {"castle long",    "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                            "e1c1",  "O-O-O"},
        {"promotion",      "8/4P3/8/8/8/8/k7/4K3 w - - 0 1",                                  "e7e8q", "e8=Q"},
        {"check",          "4k3/8/8/8/8/8/8/R3K3 w - - 0 1",                                  "a1a8",  "Ra8+"},
        {"mate",           "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1",                               "a1a8",  "Ra8#"},
        {"by file",        "4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1",                                "b1d2",  "Nbd2"},
        {"by rank",        "4k3/8/8/8/R7/8/8/R3K3 w - - 0 1",                                 "a1a3",  "R1a3"},
        {"by file + rank", "4k3/8/8/8/8/Q7/8/Q1Q4K w - - 0 1",                                "a1b2",  "Qa1b2"},
        {"pinned rival",   "4k3/8/8/8/8/8/8/1N2KN1r w - - 0 1",                               "b1d2",  "Nd2"},   // f1 knight is pinned by the h1 rook
    };
    for (const SanCase& c : sans) {
        const Position::Chess status = pos.FenInput(c.fen).value();
        const auto move = pos.UciToMove(status, c.uci);
        const std::string got = move ? pos.MoveToSan(status, *move) : "(illegal)";
        const bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "san " << c.name << ": " << got << "\n";
        if (!ok) ++failures;
    }

    // --- SanToMove -------------------------------------------------------------------------------
    // SanToMove: SAN as written in PGN files (marks, zeros in castling) → the legal move, or none.
    struct SanInCase { std::string name; std::string fen; std::string san; std::string expected; };
    const SanInCase sanIns[] = {
        {"pawn push",         start,                                                             "e4",    "e2e4"},
        {"knight",            start,                                                             "Nf3",   "g1f3"},
        {"pawn capture",      "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",   "exd5",  "e4d5"},
        {"castle short",      "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                            "O-O",   "e1g1"},
        {"castle long",       "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                            "O-O-O", "e1c1"},
        {"castle with zeros", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                            "0-0",   "e1g1"},
        {"promotion",         "8/4P3/8/8/8/8/k7/4K3 w - - 0 1",                                  "e8=Q",  "e7e8q"},
        {"check given",       "4k3/8/8/8/8/8/8/R3K3 w - - 0 1",                                  "Ra8+",  "a1a8"},
        {"check missing",     "4k3/8/8/8/8/8/8/R3K3 w - - 0 1",                                  "Ra8",   "a1a8"},
        {"mate",              "6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1",                               "Ra8#",  "a1a8"},
        {"annotation",        start,                                                             "Nf3!?", "g1f3"},
        {"by file",           "4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1",                                "Nbd2",  "b1d2"},
        {"pinned rival",      "4k3/8/8/8/8/8/8/1N2KN1r w - - 0 1",                               "Nd2",   "b1d2"},
        {"ambiguous",         "4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1",                                "Nd2",   "(none)"},
        {"illegal",           start,                                                             "Ke3",   "(none)"},
        {"empty",             start,                                                             "",      "(none)"},
    };
    for (const SanInCase& c : sanIns) {
        const Position::Chess status = pos.FenInput(c.fen).value();
        const auto move = pos.SanToMove(status, c.san);
        const std::string got = move ? pos.MoveToUci(*move) : "(none)";
        const bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "san in " << c.name << ": " << got << "\n";
        if (!ok) ++failures;
    }

    return failures == 0 ? 0 : 1;
}

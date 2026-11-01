// uci_test.cpp — the test gate for UCI parsing. Exit status 0 = all pass (ctest reads it).

#include "uci_info.h"
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <map>

int main() {
    int failures = 0;

    // --- parseInfo: score and depth --------------------------------------------------------------
    // One row per line: should it give a score, and which?
    struct InfoCase {
        std::string name;
        std::string line;
        bool hasScore;       // false: parseInfo must return nullopt
        int depth;
        bool mate;
        int score;
    };
    const InfoCase cases[] = {
        {"Buster cp",       "info depth 8 seldepth 14 score cp -35 nodes 12345 nps 1000000 hashfull 3 time 12 pv e7e5 g1f3",
                                                                       true,  8, false, -35},
        {"mate for us",     "info depth 12 score mate 3 pv d1h5 g7g6",  true, 12, true,    3},
        {"mated",           "info depth 12 score mate -2 pv e1e2",      true, 12, true,   -2},
        {"bound",           "info depth 9 score cp 20 lowerbound pv e2e4", true, 9, false, 20},
        {"string",          "info string score cp 5",                   false, 0, false,   0},
        {"score, then text","info depth 5 score cp 10 string hello",    true,  5, false,  10},
        {"currmove",        "info depth 10 currmove e2e4 currmovenumber 1", false, 0, false, 0},
        {"no score",        "info depth 3",                             false, 0, false,   0},
        {"bestmove",        "bestmove e2e4",                            false, 0, false,   0},
        {"empty",           "",                                         false, 0, false,   0},
    };
    for (const InfoCase &c : cases) {
        const auto info = parseInfo(c.line);
        const bool ok = c.hasScore
            ? (info && info->depth == c.depth && info->mate == c.mate && info->score == c.score)
            : !info;
        std::cout << (ok ? "PASS " : "FAIL ") << "info: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- parseInfo: the pv's first move ----------------------------------------------------------
    // The pv's first move: the engine's best move so far ("" without a pv).
    struct PvCase { std::string name; std::string line; std::string move; };
    const PvCase pvs[] = {
        {"Buster pv", "info depth 15 seldepth 22 score cp -49 nodes 1521867 nps 1999825 hashfull 53 time 761 pv e7e5 b1c3 b8c6", "e7e5"},
        {"mate pv",   "info depth 12 score mate 3 pv d1h5 g7g6",  "d1h5"},
        {"promotion", "info depth 7 score cp 900 pv e7e8q e1e2",  "e7e8q"},
        {"no pv",     "info depth 5 score cp 10 string hello",    ""},
    };
    for (const PvCase &c : pvs) {
        const auto info = parseInfo(c.line);
        const bool ok = info && info->pvMove == c.move;
        std::cout << (ok ? "PASS " : "FAIL ") << "pv: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- parseInfo: the whole line -----------------------------------------------------------------
    struct LineCase { std::string name; std::string line; std::vector<std::string> pv; };
    const LineCase lines[] = {
        {"Buster line", "info depth 15 seldepth 22 score cp -49 nodes 1521867 time 761 pv e7e5 b1c3 b8c6",
                        {"e7e5", "b1c3", "b8c6"}},
        {"one move",    "info depth 3 score cp 5 pv e2e4",            {"e2e4"}},
        {"empty pv",    "info depth 40 score mate 1 pv ",             {}},     // Buster at the mate edge
        {"no pv",       "info depth 5 score cp 10 string hello",      {}},
    };
    for (const LineCase &c : lines) {
        const auto info = parseInfo(c.line);
        const bool ok = info && info->pv == c.pv;   // vectors compare element by element
        std::cout << (ok ? "PASS " : "FAIL ") << "line: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- parseInfo: search statistics ------------------------------------------------------------
    // The search statistics (for the engine's geek line). Any order; missing = 0.
    struct StatsCase { std::string name; std::string line; long long nodes; long long nps; long long time; };
    const StatsCase stats[] = {
        {"Buster line", "info depth 15 seldepth 22 score cp -49 nodes 1521867 nps 1999825 hashfull 53 time 761 pv e7e5 b1c3",
                        1521867, 1999825, 761},
        {"other order", "info depth 9 time 120 nodes 5000 score cp 3 nps 41666 pv e2e4", 5000, 41666, 120},
        {"no stats",    "info depth 5 score cp 10 pv e2e4", 0, 0, 0},
    };
    for (const StatsCase &c : stats) {
        const auto info = parseInfo(c.line);
        const bool ok = info && info->nodes == c.nodes && info->nps == c.nps && info->time == c.time;
        std::cout << (ok ? "PASS " : "FAIL ") << "stats: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- winChance and chanceWords ---------------------------------------------------------------
    // Win chance: compare with a tolerance (floating point).
    struct ChanceCase { std::string name; UciInfo score; double expected; };
    const ChanceCase chances[] = {
        {"cp 0",      {6, false,    0}, 0.5},
        {"cp +400",   {6, false,  400}, 10.0 / 11.0},   // 1 / (1 + 10^-1)
        {"cp -400",   {6, false, -400},  1.0 / 11.0},
        {"mate 3",    {6, true,     3}, 1.0},
        {"mated -2",  {6, true,    -2}, 0.0},
    };
    for (const ChanceCase &c : chances) {
        const bool ok = std::abs(winChance(c.score) - c.expected) < 1e-9;
        std::cout << (ok ? "PASS " : "FAIL ") << "chance: " << c.name << "\n";
        if (!ok) ++failures;
    }

    struct WordsCase { double chance; std::string expected; };
    const WordsCase words[] = {
        {0.50, "Equal"}, {0.57, "You are slightly better"}, {0.43, "You are slightly worse"},
        {0.95, "You are winning"}, {0.05, "You are losing"},
    };
    for (const WordsCase &c : words) {
        const bool ok = chanceWords(c.chance) == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "words: " << c.chance << " -> " << c.expected << "\n";
        if (!ok) ++failures;
    }

    // --- classifyMove ----------------------------------------------------------------------------
    // classifyMove: the correct move, else Bad on a big drop in win chance, 2+ pawns lost, or walking into mate.
    struct QualityCase { std::string name; UciInfo best; UciInfo move; bool correct; MoveQuality expected; };
    const QualityCase qualities[] = {     // UciInfo: {depth, mate, score}, the human's view
        {"the correct move",       {10, false,   40}, {10, false,   40}, true,  MoveQuality::Correct},
        {"correct, scored lower",  {10, false,  100}, {10, false,  -60}, true,  MoveQuality::Correct},
        {"as good, not the one",   {10, false,   40}, {10, false,   40}, false, MoveQuality::Playable},
        {"80 cp worse",            {10, false,   40}, {10, false,  -40}, false, MoveQuality::Playable},
        {"190 cp worse, 26 pts",   {10, false,   40}, {10, false, -150}, false, MoveQuality::Bad},
        {"misses a mate",          {10, true,     3}, {10, false,  250}, false, MoveQuality::Bad},
        {"lost anyway: 50 cp",     {10, false, -800}, {10, false, -850}, false, MoveQuality::Playable},
        {"hangs the queen behind", {10, false, -360}, {10, false, -980}, false, MoveQuality::Bad},   // Anders' game
        {"loses a piece ahead",    {10, false,  800}, {10, false,  500}, false, MoveQuality::Bad},
        {"a pawn less, ahead",     {10, false,  800}, {10, false,  650}, false, MoveQuality::Playable},
        {"walks into mate",        {10, false, -500}, {10, true,    -2}, false, MoveQuality::Bad},
        {"misses a mate, still far ahead", {10, true, 2}, {10, false, 900}, false, MoveQuality::Bad},   // V4
        {"a slower mate is no mistake",    {10, true, 2}, {10, true,    5}, false, MoveQuality::Playable},
        {"misses mate in 1 (exact)",       {99, true, 1}, {10, false, 1500}, false, MoveQuality::Bad},
    };
    for (const QualityCase &c : qualities) {
        const bool ok = classifyMove(c.best, c.move, c.correct) == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "quality: " << c.name << "\n";
        if (!ok) ++failures;
    }

    // --- commonReply: the red moves' dominant reply (why_design.md, kind 3) --------------------------------------
    struct ReplyCase { std::string name; std::vector<std::string> replies; std::string expected; };   // "" = none
    const std::vector<std::string> nine(9, "c6f3"), five{"e5e4", "d8h4", "e5e4", "g8f6", "c5f2"};
    std::vector<std::string> mixed = nine;
    mixed.insert(mixed.end(), five.begin(), five.end());   // 14 red moves, 9 of them punished by ...Bxf3
    const ReplyCase replyCases[] = {
        {"9 of 14",              mixed,                                        "c6f3 9"},
        {"only 2: too few",      {"c6f3", "c6f3"},                             ""},
        {"3 of 7: not half",     {"a", "a", "a", "b", "c", "d", "e"},          ""},
        {"3 of 6: exactly half", {"a", "a", "a", "b", "c", "d"},               "a 3"},
        {"no red moves",         {},                                           ""},
    };
    for (const ReplyCase &c : replyCases) {
        const std::optional<CommonReply> r = commonReply(c.replies);
        const std::string got = r ? r->move + " " + std::to_string(r->count) : "";
        const bool ok = got == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "common reply: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }
    
    // --- pickMove: the opponent's mistakes (release_design.md 14) ------------------------------------------------
    // Scores from the mover's view (cp). Buster plays e2e4; the coach likes c2c4 better; f2f3 is a blunder.
    const std::map<std::string, UciInfo> table = {
        {"a2a3", {6, false, -10}}, {"c2c4", {6, false, 45}}, {"d2d4", {6, false, 25}},
        {"e2e4", {6, false, 30}}, {"f2f3", {6, false, -400}}, {"g1f3", {6, false, 20}}};
    struct PickCase { std::string name; std::string engine; Fallibility level; double dice; std::string move;
                      bool mistake; };
    const PickCase picks[] = {
        {"loose: the dice on its own move",  "e2e4", {1.0, 0.5},  0.10, "e2e4", false},
        {"loose: a weaker move, playable",   "e2e4", {1.0, 0.5},  0.30, "a2a3", false},
        {"loose: better counts as good",     "e2e4", {1.0, 0.5},  0.50, "c2c4", false},
        {"loose: the blunder, a mistake",    "e2e4", {1.0, 0.5},  0.79, "f2f3", true},
        {"loose: the last in the table",     "e2e4", {1.0, 0.5},  0.95, "g1f3", false},
        {"tight: mostly its own move",       "e2e4", {4.0, 0.02}, 0.50, "e2e4", false},
        {"tight: a near-equal move",         "e2e4", {4.0, 0.02}, 0.90, "d2d4", false},
        {"tight: the last near-equal move",  "e2e4", {4.0, 0.02}, 0.99, "g1f3", false},
        {"no cap: always its own move",      "e2e4", {1.0, 0.0},  0.79, "e2e4", false},
        {"not in the table: its own move",   "h2h4", {1.0, 0.5},  0.79, "h2h4", false},
    };
    // Decided positions: the engine's own move stands, whatever the dice (a won ending must still be won).
    const std::map<std::string, UciInfo> won = {
        {"a1a7", {6, false, 590}}, {"e5d6", {6, false, 600}}, {"e5f5", {6, false, 600}}, {"h1h8", {6, true, 2}}};
    const std::map<std::string, UciInfo> lost = {{"e8d8", {6, false, -700}}, {"e8f8", {6, false, -720}}};
    for (const PickCase &c : picks) {
        const Pick p = pickMove(c.engine, table, c.level, c.dice);
        const bool ok = p.move == c.move && p.mistake == c.mistake;
        std::cout << (ok ? "PASS " : "FAIL ") << "pick: " << c.name << ": " << p.move
                  << (p.mistake ? " (mistake)" : "") << "\n";
        if (!ok) ++failures;
    }
    struct DecidedCase { std::string name; const std::map<std::string, UciInfo> &table; std::string engine; };
    const DecidedCase decided[] = {
        {"decided: a found mate is played",  won,  "h1h8"},
        {"decided: won (+6), its own plan",  won,  "e5d6"},
        {"decided: lost (-7), its own move", lost, "e8d8"},
    };
    for (const DecidedCase &c : decided) {
        const Pick p = pickMove(c.engine, c.table, {1.0, 0.45}, 0.99);   // level 1's loose pick, the dice at the end
        const bool ok = p.move == c.engine && !p.mistake;
        std::cout << (ok ? "PASS " : "FAIL ") << "pick: " << c.name << ": " << p.move << "\n";
        if (!ok) ++failures;
    }
    // The blunder chance (release_design.md 14, the calibration): with probability `blunder` a move losing 30+ points
    // (here only f2f3), else the variety pick on the rest of the dice; no blunder on offer: the variety pick.
    struct BlunderCase { std::string name; Fallibility level; double dice; std::string move; bool mistake; };
    const BlunderCase blunders[] = {
        {"blunder: inside its chance, the big loss",  {1.0, 0.02, 0.10}, 0.05, "f2f3", true},
        {"blunder: outside it, the variety pick",     {1.0, 0.02, 0.10}, 0.50, "c2c4", false},
        {"blunder: no chance, its own move",          {1.0, 0.0, 0.0},  0.05, "e2e4", false},
    };
    for (const BlunderCase &c : blunders) {
        const Pick p = pickMove("e2e4", table, c.level, c.dice);
        const bool ok = p.move == c.move && p.mistake == c.mistake;
        std::cout << (ok ? "PASS " : "FAIL ") << "pick: " << c.name << ": " << p.move << "\n";
        if (!ok) ++failures;
    }
    const std::map<std::string, UciInfo> calm = {{"d2d4", {6, false, 25}}, {"e2e4", {6, false, 30}}};
    const Pick noBlunder = pickMove("e2e4", calm, {1.0, 0.02, 0.10}, 0.05);
    const bool noBlunderOk = noBlunder.move == "e2e4" && !noBlunder.mistake;
    std::cout << (noBlunderOk ? "PASS " : "FAIL ") << "pick: blunder: none on offer, the variety pick\n";
    if (!noBlunderOk) ++failures;
    const Pick none = pickMove("e2e4", {}, {1.0, 0.5}, 0.79);
    const bool emptyOk = none.move == "e2e4" && !none.mistake;
    std::cout << (emptyOk ? "PASS " : "FAIL ") << "pick: an empty table: its own move\n";
    if (!emptyOk) ++failures;

    // --- hashSetting: at least 128 MB, within the engine's range, never lowered (engine_design.md) ----------------
    struct HashCase { std::string name; std::string line; std::string expected; };   // "none" = nothing to send
    const HashCase hashCases[] = {
        {"Buster: 128 already",     "option name Hash type spin default 128 min 1 max 4096",      "none"},
        {"Stockfish: 16 -> 128",    "option name Hash type spin default 16 min 1 max 33554432",   "128"},
        {"Blunder: 64 -> 128",      "option name Hash type spin default 64 min 1 max 4096",       "128"},
        {"a small max: 64",         "option name Hash type spin default 16 min 1 max 64",         "64"},
        {"a bigger default: kept",  "option name Hash type spin default 256 min 1 max 4096",      "none"},
        {"not Hash: Threads",       "option name Threads type spin default 1 min 1 max 1024",     "none"},
        {"a longer name: not Hash", "option name HashSize type spin default 16 min 1 max 1024",   "none"},
        {"not an option line",      "id name Buster 7.0.2",                                       "none"},
    };
    for (const HashCase &c : hashCases) {
        const std::optional<int> h = hashSetting(c.line);
        const std::string got = h ? std::to_string(*h) : "none";
        const bool ok = got == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "hash: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }


    // --- judgeCoach / judgeOpponent: the engine tests (engine_design.md; the probes of 2026-10-22) -----------------
    auto fitness = [](const TestResult &r) {
        static const char *names[] = {"Good", "Warn", "No"};
        return std::string(names[static_cast<int>(r.fitness)]) + (r.reason.empty() ? "" : ": " + r.reason);
    };
    struct CoachCase { std::string name; bool answered; long long ms; std::size_t pv; std::string expected; };
    const CoachCase coachCases[] = {
        {"Buster",              true,  260, 12, "Good"},
        {"Stockfish",           true,   50, 10, "Good"},
        {"Euwe: just in time",  true,  960, 12, "Good"},
        {"Blunder 7.2: slow",   true, 3340, 12, "Warn: coaching will be slow: 3.3 s per search, the colours settle late"},
        {"short lines",         true,   50,  7, "Warn: its lines are short (7 plies): the explanations will be thinner"},
        {"Blunder 5: silent",   false,   0,  0, "No: no answer within 5 s at depth 12"},
        {"Blunder 6: no pv",    true,   10,  0, "No: it gives no lines (no pv): there's nothing to explain"},
        {"lines too short",     true,   50,  5, "No: its lines are too short (5 plies) to explain anything"},
    };
    for (const CoachCase &c : coachCases) {
        const std::string got = fitness(judgeCoach(c.answered, c.ms, c.pv));
        const bool ok = got == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "coach test: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }
    struct OpponentCase { std::string name; bool nodes, movetime, clock; std::string expected; };
    const OpponentCase opponentCases[] = {
        {"Buster",               true,  true,  true, "Good"},
        {"Blunder 5: clock only", false, false, true,
         "No: it only searches by the clock; untimed games ask for nodes or a fixed time"},
        {"no clock",             true,  true, false, "No: it doesn't answer a search by the clock (timed games)"},
    };
    for (const OpponentCase &c : opponentCases) {
        const std::string got = fitness(judgeOpponent(c.nodes, c.movetime, c.clock));
        const bool ok = got == c.expected;
        std::cout << (ok ? "PASS " : "FAIL ") << "opponent test: " << c.name << ": [" << got << "]\n";
        if (!ok) ++failures;
    }

    return failures == 0 ? 0 : 1;
}

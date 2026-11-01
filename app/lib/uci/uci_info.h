// uci_info.h — reading UCI "info" lines. Plain C++ (no Qt), like the board model,
// so it can sit behind a test.

#pragma once
#include <optional>
#include <string>
#include <vector>
#include <map>

// What we use from an "info" line.
struct UciInfo {
    int depth = 0;
    bool mate = false;   // true: "score mate N" (N moves to mate); false: "score cp N" (centipawns)
    int score = 0;       // from the side to move's point of view in the searched position
    std::string pvMove {};   // the pv's first move = the engine's best move so far ("" if no pv)
    std::vector<std::string> pv {};   // the whole line: the moves after "pv" (empty if none)
    long long nodes = 0;     // nodes searched so far (0 = not given)
    long long nps = 0;       // nodes per second
    long long time = 0;      // ms searched so far
};

// Parse one engine line. nullopt unless it's an "info" line with a score (not e.g. "info string ...").
std::optional<UciInfo> parseInfo(const std::string &line);

// The side's chance of winning, 0.0 .. 1.0, from a score in its own view.
// cp: 1 / (1 + 10^(-cp/400)) (0 cp = 0.5); mate for us = 1.0, mate against us = 0.0.
double winChance(const UciInfo &score);

// The win chance in words, for the player ("You are slightly better", "Equal", ...).
std::string chanceWords(double chance);

// A move's quality for the coach: how much win chance it loses compared to the best move.
enum class MoveQuality { Correct, Playable, Bad };

// The correct move (exactly one: the coach decides which) is Correct, whatever its score. Any other move is
// Bad if it loses 15+ points of win chance compared with the best move, OR (the material backstop, since win
// chance hardly moves when far behind or ahead) it is 2+ pawns worse, OR it walks into a mate the best move
// avoids, OR it misses a forced mate the best move has (a slower mate is fine). Otherwise Playable.
MoveQuality classifyMove(const UciInfo &best, const UciInfo &move, bool correct);

// The opponent's mistakes (release_design.md 14): a level's settings. loyalty: the weight of the engine's own move;
// cap: the variety pick's biggest loss, in win-chance points (0.0 .. 1.0) against the engine's move (0 = none);
// blunder: the chance per move of a BIG blunder (a move losing 30+ points: humans hang pieces, a search seldom does).
struct Fallibility { double loyalty; double cap; double blunder = 0.0; };

// The move the opponent plays, and whether it's a mistake (Bad by classifyMove, against the engine's own move).
struct Pick { std::string move; bool mistake; };

// Pick the opponent's move. table: every legal move's score from the MOVER's view (the coach's first pass).
// First the blunder chance: dice below `blunder` picks evenly among the moves losing 30+ points (a mistake).
// Otherwise the variety pick on the rest of the dice: the engine's move = loyalty; another move within the cap = 1 (as good or better) falling to 0 at the cap;
// beyond it, 0. dice (0.0 .. 1.0, from the caller: repeatable tests) picks one in proportion: the engine's move
// first, then the table's order. The engine's move, never a mistake, with no cap, an empty table, the engine's move
// missing from it, or a DECIDED position: its move is a found mate, or 95 %+ for either side (then the table's
// shallow scores can't tell progress from shuffling, and a random pick would undo the engine's plan: a won K+R v K
// left unmated, autoplay 2026-10-26). Mistakes belong to the undecided game.
Pick pickMove(const std::string &engineMove, const std::map<std::string, UciInfo> &table, Fallibility level,
              double dice);

// The red moves' common reply (why_design.md, kind 3): the most frequent reply among them, if it punishes at least 3
// red moves AND at least half of them. replies: one per red move (its punishing reply). nullopt = no common reply.
struct CommonReply { std::string move; int count; };
std::optional<CommonReply> commonReply(const std::vector<std::string> &replies);

// The Hash Buster Coach asks of an engine (engine_design.md): at least `wanted` MB, within the engine's own min..max,
// never lower than its default. line: an engine's "option name Hash type spin default D min A max B" line.
// nullopt = nothing to send (not the Hash option, or the default is already enough).
std::optional<int> hashSetting(const std::string &line, int wanted = 128);

// Engine tests (engine_design.md): can this engine coach, can it be an opponent? The verdict and why, in words.
enum class Fitness { Good, Warn, No };   // ✓ suitable, ⚠ allowed with a reason, ✗ refused with a reason
struct TestResult { Fitness fitness; std::string reason; };   // reason "" when Good

// The coach test: one "go depth 12" search. answered: a bestmove within 5 s; ms: how long it took; pvLength: the
// last info line's pv, in plies. No answer, no pv, or a pv under 6 plies: No. Over 1 s, or a pv of 6-7: Warn.
TestResult judgeCoach(bool answered, long long ms, std::size_t pvLength);

// The opponent test: did it answer "go nodes", "go movetime" and "go wtime ..." (each within a few seconds)?
TestResult judgeOpponent(bool nodes, bool movetime, bool clock);

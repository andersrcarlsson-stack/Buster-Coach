// uci_info.h — reading UCI "info" lines. Plain C++ (no Qt), like the board model,
// so it can sit behind a test.

#pragma once
#include <algorithm>
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

// The opponent's mistakes (release_design.md 14, two kinds of human mistakes, 2026-11-04): a level's settings, as
// chances per move. lapse: it plays from the "hope" view (didn't check the reply: can hang a piece; the weak levels
// only); oversight: from a short look (every capture seen, a quiet resource not); best: the share of the oversights
// that play the look's best-looking move (a stronger player trusts it more), the rest choosing among the near-best;
// margin: how close to the best-looking move a near-best one must look, in win-chance points (a lapse's: 5 points).
// cap (mistakes_design.md 8, 2026-11-07): the safety net, 0 = none. A pick may lose at most this much win chance by the
// truth against the engine's own move, and with any cap also at most `capPawns` pawns, never walking into a mate the
// engine's move avoids; a move beyond it is never picked (the dice picks again among the rest).
struct Fallibility { double lapse = 0.0; double oversight = 0.0; double margin = 0.15; double best = 0.0;
                     double cap = 0.0; };
inline constexpr int capPawns = 300;   // centipawns: the cap's material backstop (caps up to 0.40)
// Looser caps (levels 2-4, check round 3: mistakes_design.md) loosen the backstop with them: 7.5 pawns per 1.0 of cap
// (0.50: 3.75 pawns, 0.80: 6), never under capPawns.
inline int capPawnsFor(double cap) { return std::max(capPawns, static_cast<int>(cap * 750)); }

// The shortlist (mistakes_design.md 2, 7, 10): the moves a view puts first, the only ones looked at again and given the
// truth. In the view's order of win chance (equals in the map's order): the first `least` always, then any within
// `margin` of the best-looking, at most `most` in all.
struct ShortlistRule { double margin; int least; int most; };
inline constexpr ShortlistRule lookShortlist{0.20, 3, 8};   // an oversight's: wider than the pick's margin (15)
inline constexpr ShortlistRule hopeShortlist{0.05, 1, 8};   // a lapse's: its pick's own margin (5)
std::vector<std::string> shortlist(const std::map<std::string, UciInfo> &view, ShortlistRule rule);

// The move the opponent plays, and whether it's a mistake (Bad by classifyMove, against the engine's own move).
struct Pick { std::string move; bool mistake; };

// Pick the opponent's move: only mistakes a player of the level could make (a move that LOOKS fine, punished beyond
// what it sees). deep: every legal move's true score (the coach, depth 8); hope, look: the same moves as the opponent
// sees them in a lapse and in an oversight (empty = that view isn't used). All from the MOVER's view.
// dice (0.0 .. 1.0, from the caller: repeatable tests): below `lapse` a lapse, then up to lapse + oversight an
// oversight (its first `best` share playing the look's best-looking move), else the engine's move. Otherwise the rest
// of the dice (stretched back to 0..1) picks among that view's moves that look fine: within its margin of the
// best-looking one, weighted 1 at the best falling to 0 at the margin, in the view's order (the first best-looking one
// when only the best counts). Whether that's a mistake, and how big, the deep scores decide; mistakes are
// never chosen by their size. The engine's move also with an empty view, the engine's move missing from the deep
// table, or a DECIDED position (its move is a found mate, or 95 %+ for either side: a random pick would undo the
// engine's plan, a won K+R v K left unmated, autoplay 2026-10-26). A move the deep table doesn't know is never picked;
// nor a move beyond the level's cap (see Fallibility).
Pick pickMove(const std::string &engineMove, const std::map<std::string, UciInfo> &deep,
              const std::map<std::string, UciInfo> &hope, const std::map<std::string, UciInfo> &look,
              Fallibility level, double dice);

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

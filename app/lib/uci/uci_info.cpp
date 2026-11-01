// uci_info.cpp — reading UCI info lines, and turning scores into win chances, words and verdicts.
//
// Sections:
//   1. Parsing an info line
//   2. Win chance and its words
//   3. A move's quality: correct / playable / bad

#include "uci_info.h"
#include <sstream>
#include <cmath>
#include <map>
#include <algorithm>

// =================================================================================================
// 1. PARSING AN INFO LINE
// =================================================================================================
// Read the line word by word: react to the keywords we need, skip everything else.
std::optional<UciInfo> parseInfo(const std::string &line) {
    std::istringstream in(line);
    std::string word;
    if (!(in >> word) || word != "info")
        return std::nullopt;                  // not an info line
    UciInfo info;
    bool haveScore = false;
    while (in >> word) {
        if (word == "depth") {
            in >> info.depth;
        } else if (word == "nodes") {
            in >> info.nodes;
        } else if (word == "nps") {
            in >> info.nps;
        } else if (word == "time") {
            in >> info.time;
        } else if (word == "score") {
            std::string kind;
            in >> kind >> info.score;         // "cp -35" or "mate 3"
            info.mate = (kind == "mate");
            haveScore = true;
        } else if (word == "pv") {
            while (in >> word)                // pv comes last: everything after it is the line
                info.pv.push_back(word);
            if (!info.pv.empty())
                info.pvMove = info.pv.front();   // the first move = the best move so far
            break;
                         // only moves follow: nothing more we need (for now)
        } else if (word == "string") {
            break;                            // the rest of the line is free text for humans
        }
    }
    if (!haveScore)
        return std::nullopt;                  // e.g. "info currmove e2e4": no score
    return info;
}

// =================================================================================================
// 2. WIN CHANCE AND ITS WORDS
// =================================================================================================
// 1 / (1 + 10^(-cp/400)); mate counts as a certain win or loss.
double winChance(const UciInfo &score) {
    if (score.mate)
        return score.score > 0 ? 1.0 : 0.0;
    return 1.0 / (1.0 + std::pow(10.0, -score.score / 400.0));
}

// Thresholds in win chance (so mate and cp are treated alike). In centipawns they are
// about 50, 150 and 380: slightly / clearly better / winning.
std::string chanceWords(double chance) {
    if (chance >= 0.90) return "You are winning";
    if (chance >= 0.70) return "You are clearly better";
    if (chance >= 0.57) return "You are slightly better";
    if (chance >  0.43) return "Equal";
    if (chance >  0.30) return "You are slightly worse";
    if (chance >  0.10) return "You are clearly worse";
    return "You are losing";
}

// =================================================================================================
// 3. A MOVE'S QUALITY: correct / playable / bad
// =================================================================================================
// The one correct move first; any other by its drop in win chance compared with the best.
MoveQuality classifyMove(const UciInfo &best, const UciInfo &move, bool correct) {
    if (correct)
        return MoveQuality::Correct;                       // the one correct move, whatever its score so far
    if (winChance(best) - winChance(move) >= 0.15)
        return MoveQuality::Bad;                           // 15+ points of win chance lost
    // The material backstop: far behind or far ahead, win chance hardly moves, but throwing material away
    // is still bad (the lesson holds in every position).
    if (!best.mate && !move.mate && best.score - move.score >= 200)
        return MoveQuality::Bad;                           // 2+ pawns worse than the best move
    if (move.mate && move.score < 0 && !(best.mate && best.score < 0))
        return MoveQuality::Bad;                           // walks into a mate the best move avoids
    if (best.mate && best.score > 0 && !(move.mate && move.score > 0))
        return MoveQuality::Bad;                           // misses a forced mate (V4: still winning is not enough)
    return MoveQuality::Playable;
}

// The opponent's mistakes (release_design.md 14). Weights from the loss against the engine's own move, then the
// dice walks through them: the engine's move first, then the table's (sorted) order.
Pick pickMove(const std::string &engineMove, const std::map<std::string, UciInfo> &table, Fallibility level,
              double dice) {
    const auto own = table.find(engineMove);
    if ((level.cap <= 0.0 && level.blunder <= 0.0) || own == table.end())
        return {engineMove, false};                      // no mistakes at this level, or nothing to compare with
    const double base = winChance(own->second);
    if (base >= 0.95 || base <= 0.05)
        return {engineMove, false};                      // decided (a found mate, or 95 %+ either way): its own plan

    // The blunder chance: the first `blunder` of the dice picks among the moves losing 30+ points (evenly); the rest
    // of the dice, stretched back to 0..1, goes to the variety pick. No big loss on offer: the variety pick.
    if (dice < level.blunder) {
        std::vector<std::string> big;
        for (const auto &[move, score] : table)
            if (base - winChance(score) >= 0.30)
                big.push_back(move);
        const double stretched = dice / level.blunder;
        if (!big.empty())
            return {big[std::min(std::size_t(stretched * big.size()), big.size() - 1)], true};
        dice = stretched;
    } else {
        dice = (dice - level.blunder) / (1.0 - level.blunder);
    }

    std::vector<std::pair<std::string, double>> weights {{engineMove, level.loyalty}};
    for (const auto &[move, score] : table) {
        if (move == engineMove || level.cap <= 0.0)
            continue;                                    // no cap: no variety, its own move
        const double loss = base - winChance(score);     // win-chance points lost against the engine's move
        if (loss < level.cap)
            weights.push_back({move, loss <= 0.0 ? 1.0 : 1.0 - loss / level.cap});
    }
    double total = 0.0;
    for (const auto &[move, weight] : weights)
        total += weight;
    double target = dice * total;                        // where the dice lands on the line of weights
    for (const auto &[move, weight] : weights) {
        if (target < weight) {
            const bool mistake = classifyMove(own->second, table.at(move), false) == MoveQuality::Bad;
            return {move, mistake};
        }
        target -= weight;
    }
    return {engineMove, false};                          // dice = 1.0 exactly (or rounding): the engine's move
}

std::optional<CommonReply> commonReply(const std::vector<std::string> &replies) {
    std::map<std::string, int> counts;                  // reply -> how many red moves it punishes
    for (const std::string &reply : replies)
        ++counts[reply];                                 // [] inserts 0 first, then counts
    std::optional<CommonReply> best;
    for (const auto &[move, count] : counts)
        if (!best || count > best->count)                // the most frequent (a tie: the first in the map's order)
            best = CommonReply{move, count};
    if (!best || best->count < 3 || 2 * best->count < int(replies.size()))
        return std::nullopt;                             // too few, or not at least half of the red moves
    return best;
}
std::optional<int> hashSetting(const std::string &line, int wanted) {
    std::istringstream in(line);
    std::vector<std::string> words;
    for (std::string word; in >> word;)
        words.push_back(word);
    // Exactly the option named "Hash": the name is the word(s) between "name" and "type", so "Hash" alone, then "type".
    if (words.size() < 4 || words[0] != "option" || words[1] != "name" || words[2] != "Hash" || words[3] != "type")
        return std::nullopt;
    auto number = [&words](const std::string &key, int fallback) {   // the number after "default", "min", "max"
        for (std::size_t i = 0; i + 1 < words.size(); ++i)
            if (words[i] == key)
                return std::stoi(words[i + 1]);
        return fallback;
    };
    const int value = std::clamp(wanted, number("min", 1), number("max", wanted));   // within the engine's range
    if (value <= number("default", 0))
        return std::nullopt;                                         // the default is already enough: never lowered
    return value;
}
TestResult judgeCoach(bool answered, long long ms, std::size_t pvLength) {
    const std::string plies = std::to_string(pvLength) + " plies";
    // No: the coach would be silent (no passes finish, or no lines to explain).
    if (!answered)
        return {Fitness::No, "no answer within 5 s at depth 12"};
    if (pvLength == 0)
        return {Fitness::No, "it gives no lines (no pv): there's nothing to explain"};
    if (pvLength < 6)
        return {Fitness::No, "its lines are too short (" + plies + ") to explain anything"};
    // Warn: it coaches, but worse. Slowness first: the coach runs ~30 such searches per pass.
    if (ms > 1000) {
        const std::string seconds = std::to_string(ms / 1000) + "." + std::to_string(ms % 1000 / 100);   // 3340 -> "3.3"
        return {Fitness::Warn, "coaching will be slow: " + seconds + " s per search, the colours settle late"};
    }
    if (pvLength < 8)
        return {Fitness::Warn, "its lines are short (" + plies + "): the explanations will be thinner"};
    return {Fitness::Good, ""};
}

TestResult judgeOpponent(bool nodes, bool movetime, bool clock) {
    if (!clock)
        return {Fitness::No, "it doesn't answer a search by the clock (timed games)"};
    if (!nodes || !movetime)   // the untimed levels search by nodes, full strength by a fixed time
        return {Fitness::No, "it only searches by the clock; untimed games ask for nodes or a fixed time"};
    return {Fitness::Good, ""};
}

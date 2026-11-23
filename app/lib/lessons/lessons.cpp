// lessons.cpp — the teaching levels' lesson moves (lessons.h, teaching_levels_design.md).

#include "lessons.h"
#include "move.h"
#include <algorithm>
#include <vector>

Lesson lessonFor(int level) {
    switch (level) {
    case 1:  return Lesson::FreePieces;
    case 2:  return Lesson::LoosePieces;
    case 3:  return Lesson::Threats;
    default: return Lesson::None;
    }
}

int lessonGap(double dice) {
    return 6 + std::clamp(static_cast<int>(dice * 5), 0, 4);   // 6 .. 10
}

namespace {

int pieceType(int code) { return (code + 1) / 2; }   // 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king

int targetSquare(const std::string &uci) {             // "e2e4" -> e4 (a1 = 0)
    return (uci[3] - '1') * 8 + (uci[2] - 'a');
}

bool contains(const std::vector<int> &squares, int square) {
    return std::find(squares.begin(), squares.end(), square) != squares.end();
}

// The squares in `after` that aren't in `before`: newly hanging.
std::vector<int> newlyHanging(const std::vector<int> &before, const std::vector<int> &after) {
    std::vector<int> fresh;
    for (int square : after)
        if (!contains(before, square))
            fresh.push_back(square);
    return fresh;
}

}   // namespace

std::optional<std::string> freePieceMove(Position &model, const Position::Chess &position,
                                         const std::map<std::string, UciInfo> &deep, const std::string &engineMove,
                                         double dice) {
    const auto own = deep.find(engineMove);
    if (own == deep.end() || own->second.mate)
        return std::nullopt;                            // no yardstick, or a mate on the board: no gifts then
    const bool moverWhite = !position.pit;
    const std::vector<int> before = model.Hanging(position, moverWhite);
    std::vector<std::string> gifts;
    for (const auto &[uci, info] : deep) {
        if (uci == engineMove || info.mate || info.pv.empty() || info.pv[0].size() < 4)
            continue;
        const int loss = own->second.score - info.score;
        if (loss < 70 || loss > 450)
            continue;                                   // not a real gift, or too big a one
        std::optional<Position::Move> move = model.UciToMove(position, uci);
        if (!move)
            continue;
        Moves mover;
        const Position::Chess after = mover.make(position, *move);
        const std::vector<int> fresh = newlyHanging(before, model.Hanging(after, moverWhite));
        if (fresh.empty())
            continue;
        // Only pawns, knights and bishops are given, and only undefended ones: free means nobody takes back (a piece
        // merely attacked by a cheaper one is an exchange, not a gift; 2026-11-23, Anders: "is it really free?").
        bool small = true;
        for (int square : fresh)
            small = small && pieceType(after.mailbox[square]) <= 3 && model.Attackers(after, square, moverWhite).empty();
        if (small && contains(fresh, targetSquare(info.pv[0])))   // and the truth's reply takes one of them
            gifts.push_back(uci);
    }
    if (gifts.empty())
        return std::nullopt;
    const std::size_t i = std::min(gifts.size() - 1, static_cast<std::size_t>(dice * gifts.size()));
    return gifts[i];
}

std::optional<std::string> takeLooseMove(Position &model, const Position::Chess &position,
                                         const std::map<std::string, UciInfo> &deep, const std::string &engineMove) {
    const auto own = deep.find(engineMove);
    if (own == deep.end())
        return std::nullopt;
    const bool playerWhite = position.pit;              // the opponent (to move) is Black when pit: the player White
    const std::vector<int> loose = model.Hanging(position, playerWhite);
    if (loose.empty())
        return std::nullopt;
    std::optional<std::string> best;
    int bestScore = 0;
    for (const auto &[uci, info] : deep) {
        std::optional<Position::Move> move = model.UciToMove(position, uci);
        if (!move || !contains(loose, move->to_square) || model.CapturedPiece(position, *move) == 0)
            continue;
        if (info.mate && info.score < 0)
            continue;                                   // walks into a mate
        if (!info.mate && !own->second.mate && info.score < own->second.score - 50)
            continue;                                   // clearly worse than the engine's own move
        if (!info.mate && own->second.mate && own->second.score > 0)
            continue;                                   // the engine mates: that comes first
        std::vector<std::string> line{uci};
        line.insert(line.end(), info.pv.begin(), info.pv.end());
        if (Position::StableGain(model.MaterialAlongLine(position, line)).amount <= 0)
            continue;                                   // not really won: the line gets it back
        const int score = info.mate ? 100000 - info.score : info.score;
        if (!best || score > bestScore) {
            best = uci;
            bestScore = score;
        }
    }
    return best;
}

std::optional<std::string> threatMove(Position &model, const Position::Chess &position,
                                      const std::map<std::string, UciInfo> &deep, const std::string &engineMove) {
    const auto own = deep.find(engineMove);
    if (own == deep.end() || own->second.mate)
        return std::nullopt;
    const bool playerWhite = position.pit;
    const std::vector<int> before = model.Hanging(position, playerWhite);
    std::optional<std::string> best;
    int bestScore = 0;
    for (const auto &[uci, info] : deep) {
        if (info.mate || info.score < own->second.score - 40)
            continue;                                   // a sensible move only
        std::optional<Position::Move> move = model.UciToMove(position, uci);
        if (!move || model.CapturedPiece(position, *move) != 0)
            continue;                                   // a quiet move: a capture is something else
        Moves mover;
        const Position::Chess after = mover.make(position, *move);
        if (newlyHanging(before, model.Hanging(after, playerWhite)).empty())
            continue;                                   // attacks nothing new
        if (!best || info.score > bestScore) {
            best = uci;
            bestScore = info.score;
        }
    }
    return best;
}

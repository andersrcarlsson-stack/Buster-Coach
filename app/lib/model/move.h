// move.h — Moves: the move generator (raw moves, check, legality, making a move) and small helpers.

#pragma once
#include "position.h"

class Moves { // Declaration of the Move class
public:
    Position::MoveList raw_moves(const Position::Chess& status); // Declaration of the MoveGen method
    void slide_hopper(std::vector<std::vector<int>>&, const Position::Chess&, Position::MoveList& ,int&, int&); // Declaration of MoveGen metohd
    Position::Chess position_maker(Position::Chess status, Position::Move& move); // Declaration of MoveGen method
    Position::MoveList ilegal_weeder(const Position::Chess& status, Position::MoveList moves);
    bool in_check(const Position::Chess& status, int Q_side, int K_side);
    Position::Chess make(const Position::Chess& status, Position::Move& move);

    Position::Move move_writer (int from, int to, int promo, bool capture, bool castle, bool en_pass) {
        Position::Move move; // Move structure

        move.from_square = from;
        move.to_square = to;
        move.promotion_piece = promo;
        move.is_capture = capture;
        move.is_castling = castle;
        move.is_en_passant = en_pass;

        return move;
    }

    std::vector<std::vector<int>> slider(std::vector<int> full_range, int x) {

        std::vector<std::vector<int>> low_high {};
        std::vector<int> result;

        std::copy_if(full_range.begin(), full_range.end(), std::back_inserter(result), [x](int val) { return val < x; }); // lower part
        low_high.push_back(result); // will be vector[0] - low part

        result.clear();
        std::copy_if(full_range.begin(), full_range.end(), std::back_inserter(result), [x](int val) { return val > x; }); // higher part
        low_high.push_back(result); // will be vector[1] - high part

        return low_high;
    }
};

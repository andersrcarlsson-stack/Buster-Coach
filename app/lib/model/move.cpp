// move.cpp — the move generator: raw moves, check, the legal-move filter, and making a move.
//
// Sections:
//   1. Raw (pseudo-legal) moves, the slider helper
//   2. A quick position for the legality test (no flags, side to move unchanged)
//   3. Check, and weeding out illegal moves
//   4. Making a move: the full new position (flags, castling rights, e.p., counters)

#include "move.h"
#include "constants.h"
#include <ranges>

// =================================================================================================
// 1. RAW (PSEUDO-LEGAL) MOVES, THE SLIDER HELPER
// =================================================================================================
Position::MoveList Moves::raw_moves(const Position::Chess& status) { // Definition of the MoveGen method
    Moves c_moves; // Create an instance of the Moves class
    Position::MoveList move_list; // Create an instance of the MoveList struct to hold the generated moves
    // Move generation logic will be implemented here in the future

    // scan mailbox{} once - at each square - switch/case the action
    int piece {0}, own_piece_switch {0}; // own_piece_switch is generic 1-6 .... not to mix up with type
    for (int square = 0; square < 64; ++square) {

        piece = status.mailbox[square]; // piece: 1 - 12 full set
        own_piece_switch = (piece / 2 + piece % 2) * (!status.pit ^ bool(piece % 2)); // just pieces that is "in turn" to move

        switch (own_piece_switch) {

            case 0: // empty square - take next
                break;

            case 1: // Pawn
                if (!status.pit) { // this is white pawn moves - for pawns, it does matter
                    if (square > 7 and square < 16 ) { // white pawn moves from start pos - can double push
                        if (status.mailbox[square + 8] == 0 and status.mailbox[square + 16] == 0) { // double push possible
                            move_list.moves.push_back(c_moves.move_writer(square, square + 16, 0, false, false, true)); // add move to move_list
                        }
                    }
                    // white single push moves
                    if (status.mailbox[square + 8] == 0) { // single push possible
                        if (square > 47)
                            for (int i = 4; i < 12; i += 2) {
                            move_list.moves.push_back(c_moves.move_writer(square, square + 8, i, false, false, false)); // add 4 moves to move_list
                        }
                        else move_list.moves.push_back(c_moves.move_writer(square, square + 8, 0, false, false, false)); // add move to move_list
                    }
                    // white pawn capture and capture promotion moves
                    for (int to : whitePawnCaptures[square]) {
                        if (((bool(status.mailbox[to] % 2) != bool(piece % 2))) and status.mailbox[to]) { // this is an enemy square - pawn can capture
                        if (square > 47)
                            for (int i = 4; i < 12; i += 2) {
                            move_list.moves.push_back(c_moves.move_writer(square, to, i, true, false, false)); // add 4 moves to move_list
                        }
                        else move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
                        }
                        if (to == status.en_passant_target) { // can capture en_passant
                            move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, true)); // add move to move_list
                        }
                    }
                }
                else { // this is black pawn moves - for pawns, it does matter
                    if (square > 47 and square < 56 ) { // black pawn moves from start pos - can double push
                        if (status.mailbox[square - 8] == 0 and status.mailbox[square - 16] == 0) { // double push possible
                            move_list.moves.push_back(c_moves.move_writer(square, square - 16, 0, false, false, true)); // add move to move_list
                        }
                    }
                    // black single push moves
                    if (status.mailbox[square - 8] == 0) { // single push possible
                        if (square < 16)
                            for (int i = 3; i < 11; i += 2) {
                            move_list.moves.push_back(c_moves.move_writer(square, square - 8, i, false, false, false)); // add 4 moves to move_list
                        }
                        else move_list.moves.push_back(c_moves.move_writer(square, square - 8, 0, false, false, false)); // add move to move_list
                    }
                    // black pawn capture and capture promotion moves
                    for (int to : blackPawnCaptures[square]) {
                        if (((bool(status.mailbox[to] % 2) != bool(piece % 2))) and status.mailbox[to]) { // this is an enemy square - pawn can capture
                            if (square < 16)
                            for (int i = 3; i < 11; i += 2) {
                            move_list.moves.push_back(c_moves.move_writer(square, to, i, true, false, false)); // add move to move_list
                        }
                        else move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
                        }
                        if (to == status.en_passant_target) { // can capture en_passant
                            move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, true)); // add move to move_list
                        }
                    }
                } break;

            case 2: // Knight
                for (int to : knightMoves[square]) {
                    if (status.mailbox[to] == 0) { // this is an empty square - Knights can always jump here
                        move_list.moves.push_back(c_moves.move_writer(square, to, 0, false, false, false)); // add move to move_list
                        continue;
                    }
                    if ((bool(status.mailbox[to] % 2) != bool(piece % 2))) { // this is an enemy square - Knights can always capture
                        move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
                        continue;
                    }
                } break;

            case 3: // Bishop
                for (int dia : squareToDiagonals[square]) {
                    std::vector<std::vector<int>> two_vectors = c_moves.slider(diagonals[dia], square);
                    slide_hopper(two_vectors, status, move_list, square, piece);
                } break;

            case 4: // Rook
                for (int r_f : square_to_rank_file[square]) {
                    std::vector<std::vector<int>> two_vectors = c_moves.slider(ranks_and_files[r_f], square);
                    slide_hopper(two_vectors, status, move_list, square, piece);
                } break;

            case 5: // Queen
                for (int dia : squareToDiagonals[square]) {
                    std::vector<std::vector<int>> two_vectors = c_moves.slider(diagonals[dia], square);
                    slide_hopper(two_vectors, status, move_list, square, piece);
                }
                for (int r_f : square_to_rank_file[square]) {
                    std::vector<std::vector<int>> two_vectors = c_moves.slider(ranks_and_files[r_f], square);
                    slide_hopper(two_vectors, status, move_list, square, piece);
                } break;

            case 6: // King
                for (int to : king_moves[square]) {
                    if (status.mailbox[to] == 0) { // this is an empty square - Kings can always jump here
                        move_list.moves.push_back(c_moves.move_writer(square, to, 0, false, false, false)); // add move to move_list
                        continue;
                    }
                    if ((bool(status.mailbox[to] % 2) != bool(piece % 2))) { // this is an enemy square - Kings can always capture
                        move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
                        continue;
                    }
                } break;
        }
    }

    // castling moves
    if (!status.pit) { // white move

        if (status.mailbox[5] == 0 and status.mailbox[6] == 0 and status.K) { // white Kingside castle maybe possible
            if (!in_check(status, 0, 1)) move_list.moves.push_back(c_moves.move_writer(4, 6, 0, false, true, false)); // add move to move_list
        } if (status.mailbox[3] == 0 and status.mailbox[2] == 0 and status.mailbox[1] == 0 and status.Q) { // white Queenside castle maybe possible
            if (!in_check(status, 1, 0)) move_list.moves.push_back(c_moves.move_writer(4, 2, 0, false, true, false)); // add move to move_list
        }
    } else { // black move

        if (status.mailbox[61] == 0 and status.mailbox[62] == 0 and status.k) { // black Kingside castle maybe possible
            if (!in_check(status, 0, 1)) move_list.moves.push_back(c_moves.move_writer(60, 62, 0, false, true, false)); // add move to move_list
        } if (status.mailbox[59] == 0 and status.mailbox[58] == 0  and status.mailbox[57] == 0 and status.q) { // black Queenside castle maybe possible
            if (!in_check(status, 1, 0)) move_list.moves.push_back(c_moves.move_writer(60, 58, 0, false, true, false)); // add move to move_list
        }
    }

    return move_list;
}

void Moves::slide_hopper(std::vector<std::vector<int>>& two_vectors, const Position::Chess& status, Position::MoveList& move_list, int& square, int& piece) {
    Moves c_moves;
    for (int to : two_vectors[0] | std::views::reverse) { // lower end - smaller than square
        if (status.mailbox[to] == 0) {
            move_list.moves.push_back(c_moves.move_writer(square, to, 0, false, false, false)); // add move to move_list
            continue;
        } else if ((bool(status.mailbox[to] % 2) != bool(piece % 2))) { // this is an enemy square - capture and break out
            move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
            break;;
        } else break; // only option if not 0 and not enemy - own piece blocking - break out!
    }
    for (int to : two_vectors[1]) { // upper end - greater than square
        if (status.mailbox[to] == 0) {
            move_list.moves.push_back(c_moves.move_writer(square, to, 0, false, false, false)); // add move to move_list
            continue;
        } else if ((bool(status.mailbox[to] % 2) != bool(piece % 2))) { // this is an enemy square - capture and break out
            move_list.moves.push_back(c_moves.move_writer(square, to, 0, true, false, false)); // add move to move_list
            break;;
        } else break; // only option left if not 0 and not enemy - own piece blocking - break out!
    }
}

// =================================================================================================
// 2. A QUICK POSITION FOR THE LEGALITY TEST (no flags, side to move unchanged)
// =================================================================================================
Position::Chess Moves::position_maker(Position::Chess updated_positon, Position::Move& move) { // Definition of the MoveGen method | COPY !

    updated_positon.mailbox[move.to_square] = updated_positon.mailbox[move.from_square];
    updated_positon.mailbox[move.from_square] = 0;

    // check if en pass flag & capture flag is set -> delete (set 0) also the en pass pusher
    if (move.is_en_passant) { // en_pass - we will need the direction
        int up_down = ((move.to_square - move.from_square) > 0) - ((move.to_square - move.from_square) < 0); // white move up_down = 1, black = -1
        if (move.is_capture) updated_positon.mailbox[move.to_square - (8 * up_down)] = 0; // clear also the pushing pawn that set en_pass target
    }

    // do not toggle pit ################# for this kind of position make ...

    return updated_positon;
}

// =================================================================================================
// 3. CHECK, AND WEEDING OUT ILLEGAL MOVES
// =================================================================================================
// Is the side to move's king attacked? With Q_side / K_side = 1 (castling): also the square(s) the king passes,
// king - Q_side .. king + K_side. The attack test itself is Position::Attackers: one attack rule in the model.
bool Moves::in_check(const Position::Chess& status, int Q_side, int K_side) {
    const int king_type = status.pit ? 11 : 12;          // the side to move's king (Black odd)
    const auto it = std::find(status.mailbox.begin(), status.mailbox.end(), king_type);
    if (it == status.mailbox.end())
        return false;                                     // no king: not a real position (IsValid rejects it)
    const int king = std::distance(status.mailbox.begin(), it);
    const bool enemy_white = status.pit;                  // Black to move: the enemy is White
    Position model;
    for (int square = king - Q_side; square <= king + K_side; ++square)
        if (!model.Attackers(status, square, enemy_white).empty())
            return true;
    return false;
}

Position::MoveList Moves::ilegal_weeder(const Position::Chess& status, Position::MoveList move_list) {

    Position::MoveList legal_moves {}; // make a new move list based on positions that does not expose the king
    for (Position::Move move : move_list.moves) {
        if (!in_check(position_maker (status, move), 0, 0)) legal_moves.moves.push_back(move);
    }

    return legal_moves;
}

// =================================================================================================
// 4. MAKING A MOVE: the full new position (flags, castling rights, e.p., counters)
// =================================================================================================
Position::Chess Moves::make(const Position::Chess& status, Position::Move& move) {
    Position::Chess new_position = status;
    // make new position with move ... all flags etc ...

    // base for new position - ordinary move
    new_position.mailbox[move.to_square] = new_position.mailbox[move.from_square];
    new_position.mailbox[move.from_square] = 0;
    new_position.en_passant_target = -1; // clear any set en_pass target

    // it a promosion flag is set - the to_square is switched for the promotion piece
    if (move.promotion_piece) new_position.mailbox[move.to_square] = move.promotion_piece;

    // check if en pass flag & capture flag is set -> delete (set 0) also the en pass pusher
    if (move.is_en_passant) { // en_pass - we will need the direction
        int up_down = ((move.to_square - move.from_square) > 0) - ((move.to_square - move.from_square) < 0); // white move up_down = 1, black = -1

        if (move.is_capture) new_position.mailbox[move.to_square - (8 * up_down)] = 0; // clear also the pushing pawn that set en_pass target
        else new_position.en_passant_target = move.from_square + (8 * up_down); // if not capture above - then flag means that a target needs to be set
    }

    if (move.is_castling) {
        if (move.to_square == 6) { // white kingside
            new_position.mailbox[5] = 8;
            new_position.mailbox[7] = 0;
        } else if (move.to_square == 2) { // white queenside
            new_position.mailbox[3] = 8;
            new_position.mailbox[0] = 0;
        } else if (move.to_square == 62) { // black kingside
            new_position.mailbox[61] = 7;
            new_position.mailbox[63] = 0;
        } else if (move.to_square == 58) { // black queenside
            new_position.mailbox[59] = 7;
            new_position.mailbox[56] = 0;
        }
    }

    // update castling flags based on king and rook whereabouts
    new_position.K &= (new_position.mailbox[4] == 12) & (new_position.mailbox[7] == 8);
    new_position.Q &= (new_position.mailbox[4] == 12) & (new_position.mailbox[0] == 8);
    new_position.k &= (new_position.mailbox[60] == 11) & (new_position.mailbox[63] == 7);
    new_position.q &= (new_position.mailbox[60] == 11) & (new_position.mailbox[56] == 7);

    // halfmove clock
    ++ new_position.halfmove_clock;
    if (move.is_capture or status.mailbox[move.from_square] == 2 or status.mailbox[move.from_square] == 1) new_position.halfmove_clock = 0;

    // fullmove clock
    if (status.pit) ++ new_position.fullmove_number;

    new_position.pit ^= 1; // toggle player in turn

    return new_position;
}

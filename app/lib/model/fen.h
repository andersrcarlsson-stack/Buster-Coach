// fen.h — FEN: parsing a FEN into a position, and writing one back.

#pragma once
#include "position.h"
#include <map>

class FEN { // Declaration of the FEN class
public:
    std::optional<Position::Chess>  FentoPos ( // Declaration of the FentoPos method
                            std::string piece_placement_data,
                            std::string active_color,
                            std::string castling_availability,
                            std::string en_passant_target_square,
                            std::string halfmove_clock,
                            std::string fullmove_number);

    std::string PosToFEN (const Position::Chess& status); // Declaration of the PosToFEN method
};

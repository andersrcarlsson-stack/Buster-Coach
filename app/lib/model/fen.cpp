// fen.cpp — FEN text <-> the mailbox position.
//
// Sections:
//   1. FEN -> position
//   2. Position -> FEN

#include "fen.h"

// =================================================================================================
// 1. FEN -> POSITION
// =================================================================================================
std::optional<Position::Chess> FEN::FentoPos ( // Definition of the FentoPos method
                            std::string piece_placement_data,
                            std::string active_color,
                            std::string castling_availability,
                            std::string en_passant_target_square,
                            std::string halfmove_clock,
                            std::string fullmove_number) {
    Position::Chess status; // Create an instance of the Chess struct to hold the position data
    size_t found;
    int number_of_sub = 0;
    std::array<std::string, 8> ppd_sub;
    std::array<std::string, 16> castle_codes = {"K", "Q", "k", "q", "KQ", "Kk", "Kq", "Qk", "Qq", "kq", "KQk", "KQq", "Kkq", "Qkq", "KQkq", "-"};
    std::array<std::string, 17> en_passant_codes = {"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "-"};
    std::array<int, 16> en_passant_squares = {16, 17, 18, 19, 20, 21, 22, 23, 40, 41, 42, 43, 44, 45, 46, 47};
    bool castle_NOK {true}, en_passant_NOK {true};
    std::string temp_string;
    char piece_data, castling_data;
    std::map<char, int> piece_map = { // Map of piece characters to their corresponding integer values
        {'p', 1}, {'n', 3}, {'b', 5}, {'r', 7}, {'q', 9}, {'k', 11}, // Lowercase odd pieces for black
        {'P', 2}, {'N', 4}, {'B', 6}, {'R', 8}, {'Q', 10}, {'K', 12} // Uppercase even pieces for white
    };

    // check for legitimate characters
    found = piece_placement_data.find_first_not_of("prbnqkPRNBQK/12345678");
    if (found!=std::string::npos) return std::nullopt;

    found = active_color.find_first_not_of("wb");
    if (found!=std::string::npos) return std::nullopt;

    found = castling_availability.find_first_not_of("kqKQ-");
    if (found!=std::string::npos) return std::nullopt;

    found = en_passant_target_square.find_first_not_of("abcdefgh36-");
    if (found!=std::string::npos) return std::nullopt;

    found = halfmove_clock.find_first_not_of("0123456789");
    if (found!=std::string::npos) return std::nullopt;

    found = fullmove_number.find_first_not_of("0123456789");
    if (found!=std::string::npos) return std::nullopt;

    // Piece Placement decoding and correct format
    for (size_t i = 0; i < piece_placement_data.size(); ++i) {
        temp_string = piece_placement_data.at(i);
        if (temp_string == "/" and i == 0) return std::nullopt;

        if (temp_string == "/") {
            ++ number_of_sub;
            if (number_of_sub > 7 or ppd_sub[number_of_sub - 1].size() == 0 ) return std::nullopt;

            continue;
        }
        ppd_sub[number_of_sub].append(temp_string);
    }
    if (number_of_sub != 7) return std::nullopt;

    int index {63}, gap {0}, rank_count {0};

    for (int i = 0; i < 8; ++i) { // Loop through each rank of the chessboard
        gap = 0; rank_count = 0;
        for (int j = ppd_sub[i].size() - 1; j >= 0; --j) { // Loop through each character in the rank string
            piece_data = ppd_sub[i].at(j); // Get the character representing the piece or empty squares
            try { // Try to map the character to a piece value using the piece_map
                status.mailbox[index] = piece_map.at(piece_data); // Map the character to a piece value and store it in the mailbox
                ++ rank_count;
            } catch (const std::out_of_range&) { // If the character is not found in the piece_map, it must be a digit representing empty squares
                    gap = piece_data - '0';
                    index -= (gap - 1);
            }
            if (gap > 0) { // If there are empty squares, fill them in the mailbox with 0
                for (int fill = index; fill < index + gap; ++fill) status.mailbox[fill] = 0;
            }
            rank_count += gap; gap = 0; index -= 1;
        }
        if (rank_count != 8) return std::nullopt;
    }

    // Active Color decoding
    if (active_color.size() != 1) return std::nullopt;

    if (active_color == "w") status.pit = false;
    else status.pit = true;

    // Castling Availability decoding
    for (int i = 0; i < 16; ++i) castle_NOK &= (castling_availability == castle_codes[i]);

    if (castle_NOK) return std::nullopt;

    for (size_t i = 0; i < castling_availability.size(); ++i) {
        castling_data = castling_availability.at(i); // Get the character representing the castling rights
        switch(castling_data) {
            case 'K':
            status.K = true;
            break;
            case 'Q':
            status.Q = true;
            break;
            case 'k':
            status.k = true;
            break;
            case 'q':
            status.q = true;
            break;
            case '-':
            break;
        }
    }

    // En Passant Target Square decoding
    for (int i = 0; i < 17; ++i) en_passant_NOK &= (en_passant_target_square == en_passant_codes[i]);

    if (en_passant_NOK) return std::nullopt;

    if (en_passant_target_square != "-") {
        for (int i = 0; i < 16; ++ i) {
            if (en_passant_target_square == en_passant_codes[i]) status.en_passant_target = en_passant_squares[i];
        }
    }

    // Halfmove Clock decoding
    status.halfmove_clock = std::stoi(halfmove_clock);

    // Fullmove Number decoding
    status.fullmove_number = std::stoi(fullmove_number);

    return status;
}

// =================================================================================================
// 2. POSITION -> FEN
// =================================================================================================
std::string FEN::PosToFEN (const Position::Chess& status) { // Definition of the PosToFEN method
    std::ostringstream oss;
    std::map<int, char> map_piece = { // Map of piece integer values to their corresponding characters
        {1, 'p'}, {3, 'n'}, {5, 'b'}, {7, 'r'}, {9, 'q'}, {11, 'k'}, // Odd Lowercase pieces for black
        {2, 'P'}, {4, 'N'}, {6, 'B'}, {8, 'R'}, {10, 'Q'}, {12, 'K'} // Even Uppercase pieces for white
    };

    // Piece Placement encoding
    for (int rank = 7; rank >= 0; --rank) { // Loop through each rank of the chessboard from 8 to 1
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) { // Loop through each file of the chessboard from a to h
            int index = rank * 8 + file;
            int piece_value = status.mailbox[index]; // Get the piece value from the mailbox for the current square
            if (piece_value == 0) {
                ++empty_count;
            } else { // If there are empty squares before this piece, append the count to the FEN string
                if (empty_count > 0) {
                    oss << empty_count;
                    empty_count = 0;
                }

                char piece_char = map_piece.at(piece_value); // Map the piece value to its corresponding character using the map_piece

                oss << piece_char; // Append the piece character to the FEN string
            }
        }
        if (empty_count > 0) { // If there are empty squares at the end of the rank, append the count to the FEN string
            oss << empty_count;
        }
        if (rank > 0) { // If not the last rank, append a '/' to separate ranks in the FEN string
            oss << '/';
        }
    }

    // Active Color encoding
    oss << ' ' << (status.pit ? 'b' : 'w');

    // Castling Availability encoding
    oss << ' ';
    if (status.K) oss << 'K';
    if (status.Q) oss << 'Q';
    if (status.k) oss << 'k';
    if (status.q) oss << 'q';
    if (!status.K && !status.Q && !status.k && !status.q) oss << '-';

    // En Passant Target Square encoding
    oss << ' ';
    if (status.en_passant_target != -1) {
        int ep_index = status.en_passant_target;
        char file_char = 'a' + (ep_index % 8);
        char rank_char = '1' + (ep_index / 8);
        oss << file_char << rank_char;
    } else {
        oss << '-';
    }

    // Halfmove Clock encoding
    oss << ' ' << status.halfmove_clock;

    // Fullmove Number encoding
    oss << ' ' << status.fullmove_number;

    return oss.str();
}

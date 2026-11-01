// position.h — the board model: a mailbox position (a1 = 0, pieces 1-12, Black odd), moves, game states,
// and the functions the GUI uses (FEN, UCI, SAN, legal moves, state, validity, captures).

#pragma once
#include <iostream>
#include <sstream>
#include <algorithm>
#include <array>
#include <vector>
#include <optional>

class Position { // Declaration of the Position class
public:
    // The standard start position as FEN: the one copy (the game, the editor, PGN, practice, …).
    static constexpr const char *StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    struct Chess { // struct to hold Chess Position
        std::array <int, 64> mailbox {}; // array to hold the mailbox
        bool pit {false}; // player in turn - 0 white to move (false), 1 black to move (true)
        bool K {false}; // White King Side Castle Rights true/false
        bool Q {false}; // White Queen Side Castle Rights true/false
        bool k {false}; // Black King Side Castle Rights true/false
        bool q {false}; // Black Queen Side Castle Rights true/false
        int en_passant_target {-1}; // en passant target square (0-63) or -1 if none
        int halfmove_clock {0}; // halfmove clock for the fifty-move rule
        int fullmove_number {1}; // fullmove number starting from 1
    };

    struct Move { // struct to hold Move Information
        int from_square {-1}; // from square (0-63) or -1 if none
        int to_square {-1}; // to square (0-63) or -1 if none
        int promotion_piece {0}; // promotion piece (1-12) or 0 if none
        bool is_capture {false}; // true if the move is a capture, false otherwise
        bool is_castling {false}; // true if the move is castling, false otherwise
        bool is_en_passant {false}; // true if the move is en passant, false otherwise
    };

    enum class GameState { Ongoing, Checkmate, Stalemate, FiftyMoves, Repetition,  // Repetition: set by the game (needs history)
                           InsufficientMaterial };   // a dead position: no legal moves can lead to mate (FIDE)

    struct MoveList { // struct to hold a list of Moves
        std::vector <Move> moves {}; // array to hold the moves
    };

    std::optional<Chess> FenInput(const std::string& fen);   // nullopt if the FEN isn't valid
    std::string FenOutput(const Chess& status); // Declaration of the FenOutput method
    // The position as a key: its FEN without the two move counters (the same position at any move number). One copy
    // for the repetition history, the opening names and the book.
    std::string Key(const Chess& status);
    // The key for repetitions (FIDE 9.2.3): the same, but the en passant square only if an en passant capture is
    // really legal. After most double pushes none is, and the position is the same as one without that square.
    std::string RepetitionKey(const Chess& status);
    MoveList LegalMoves(const Chess& status) const; // Declaration of the LegalMoves method
    int long long Test(const Chess& status, int depth); // Declaration of Test
    std::string MoveToUci(const Move& move) const; // e.g. "e2e4", "e7e8q"
    std::string MoveToSan(const Chess& status, const Move& move) const; // Standard algebraic notation, e.g. "Nf3", "exd5", "O-O", "e8=Q+", "Nbd2". The position is BEFORE the move.
    std::optional<Move> UciToMove(const Chess& status, const std::string& uci); // nullopt if not legal
    // SAN as found in PGN files ("Nf3", "exd5", "O-O", "e8=Q", "Nf3!?", "0-0") → the legal move. Marks (+ # ! ?) are
    // ignored. nullopt if no legal move matches, or if the SAN is ambiguous ("Nd2" when two knights can go there).
    std::optional<Move> SanToMove(const Chess& status, const std::string& san) const;
    // The piece this move captures (its code, 1-12), or 0 for none. En passant: the pawn beside the target square.
    int CapturedPiece(const Chess& status, const Move& move) const;
    GameState State(const Chess& status);
    // Why a position can't occur in a real game: the FIRST problem found, in this order (the board editor shows it).
    enum class Invalid { None, WhiteKings, BlackKings, PawnOnBackRank, NotToMoveInCheck, Castling, EnPassant };
    Invalid Validate(const Chess& status) const;
    bool IsValid(const Chess& status); // Can this position occur in a real game? (Parsing a FEN only checks its syntax.)
    // The same position with the other side to move and no en passant square: "what if I passed?"
    // For the opponent's threat. None if the side to move is in check (the result would be impossible).
    std::optional<Chess> PassTurn(const Chess& status);
    // The squares of that colour's pieces that attack `square`. Geometry only, like a check test: pinned pieces
    // count, no x-rays, pawns diagonally forward. Defenders: ask with the colour of the piece standing there.
    std::vector<int> Attackers(const Chess& status, int square, bool white) const;
    // That side's hanging pieces ("en prise", attack_design.md): not the king; attacked by the enemy and either
    // undefended or attacked by a cheaper piece. Their squares, in any order.
    std::vector<int> Hanging(const Chess& status, bool white) const;
    // the side to move at the start (P1 N3 B3 R5 Q9; a promotion counts), whether the ply captured, and what (the
    // piece's code, 0 = nothing). An illegal move ends the line.
    struct MaterialStep { int balance; bool capture; int taken; };
    std::vector<MaterialStep> MaterialAlongLine(const Chess& status, const std::vector<std::string>& moves);
    // Where a line's material gain lands (why_design.md): the balance at the last STABLE step (the next ply isn't a
    // capture; the last ply only if it isn't one), and the ply where the balance last changed before it (-1 = none).
    struct Gain { int amount; int ply; };
    static Gain StableGain(const std::vector<MaterialStep>& steps);
    // The exchange a line starts (why good, the even exchange): the first ply's capture and every capture straight
    // after it, up to the first ply that takes nothing. The balance after it, and the pieces taken (codes, in the
    // order taken): yours on your plies, theirs on the opponent's. Not a capture: balance 0, nothing taken.
    struct Exchange { int balance; std::vector<int> yours; std::vector<int> theirs; };
    static Exchange FirstExchange(const std::vector<MaterialStep>& steps);
    // Where a line first settles after the opponent's reply (why_design.md, a sacrifice, 2026-10-29): the first ply
    // from 1 on whose next ply isn't a capture (the last ply: if it isn't one itself); the balance there, and the
    // pieces each side took until then (yours: taken by you). A dip below 0 = the line first gives material.
    // Nothing settles (a line of under two plies, or captures to its end): balance 0, nothing taken.
    static Exchange FirstSettled(const std::vector<MaterialStep>& steps);
    // The opponent's pieces hanging AFTER this move that weren't before (why_design.md, kind 2 "creates a threat"):
    // newly attacked only, discovered attacks included. Their squares, in any order.
    std::vector<int> NewThreats(const Chess& status, const Move& move) const;
    // How your move answers the opponent's quiet threat (why_design.md, "stopping a quiet threat", 2026-10-28).
    // `threat` (UCI) is the opponent's move as if it were to move now (PassTurn's position), and it would leave your
    // pieces hanging there. Takes: your move captures the threat's piece. Prevents: the threat move isn't legal any
    // more. Then the threat move is played after yours: still attacking, but its own piece hangs there and is the only
    // attacker → CostsIt (you take it); fewer of your pieces newly hanging than it would hit now (none, or a fork
    // broken to a single attack) → MovesAway (an attacked piece left), Defends (an attacked piece got another
    // defender), or Stops (some other way). Else None, also when the threat attacks nothing. `piece`: the code of the
    // piece named (taken, moved, defended, lost), 0 for none.
    enum class How { None, Takes, Prevents, MovesAway, Defends, Stops, CostsIt };
    struct Answer { How how; int piece; };
    Answer ThreatAnswer(const Chess& status, const Move& yours, const std::string& threat);
    // A visible opening principle in a move (why_design.md, kind 4): castling; developing a knight or bishop from its
    // starting square; a pawn to the centre (d4 e4 d5 e5).
    enum class Principle { None, Castles, Develops, Centre };
    Principle MovePrinciple(const Chess& status, const Move& move) const;
    // A dead position (FIDE): no pawns, rooks or queens, and either at most one minor piece in all,
    // or only bishops, all on squares of one colour. No sequence of legal moves can lead to mate.
    bool IsDeadPosition(const Chess& status) const;
    // Can this side still mate at all, the opponent helping (FIDE 6.9)? For the flag rule: a flag fall is a draw if the
    // opponent can't. As lichess decides it: a pawn, rook or queen, two knights, or knight and bishop: yes. One knight:
    // only if the opponent has a piece besides king and queens. Bishops only: only if bishops (either side's) stand on
    // both colours, or the opponent has a knight or a pawn. A lone king: no.
    bool CanMate(const Chess& status, bool white) const;
};

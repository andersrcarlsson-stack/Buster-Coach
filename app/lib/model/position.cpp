// position.cpp — the board model's public face: FEN and UCI text, game state, legal moves, validity,
// captures and SAN.
//
// Sections:
//   1. Text in and out: FEN, UCI
//   2. Game state: mate, stalemate, the 50-move rule, dead positions; can a side still mate?
//   3. Legal moves, perft
//   4. Validity, captured pieces, SAN

#include "position.h"
#include "fen.h"
#include "move.h"
#include "test.h"
#include "constants.h"   // the attack tables (knight, king, pawn captures, diagonals, ranks and files)
#include <ranges>        // std::views::reverse


// =================================================================================================
// 1. TEXT IN AND OUT: FEN, UCI
// =================================================================================================
std::optional<Position::Chess> Position::FenInput(const std::string& fen) { // Definition of the FenInput method
    std::istringstream iss(fen); // Create an input string stream to parse the FEN string
    std::string token;
    std::array<std::string, 6> fen_parts; // Array to hold the six parts of the FEN string
    int index = 0;
    while (iss >> token && index < 6) { // Parse the FEN string into its components
        fen_parts[index++] = token;
    }
    // EPD-style FENs leave out the two counters: use the standard defaults.
    if (fen_parts[4].empty()) fen_parts[4] = "0";
    if (fen_parts[5].empty()) fen_parts[5] = "1";
    FEN c_fen_parser; // Create an instance of the FEN class to parse the FEN components

    return c_fen_parser.FentoPos(fen_parts[0], fen_parts[1], fen_parts[2], fen_parts[3], fen_parts[4], fen_parts[5]);
}

std::string Position::MoveToUci(const Move& move) const {
    auto square = [](int sq) {                       // 0..63 (a1 = 0) -> "a1".."h8"
        return std::string{char('a' + sq % 8), char('1' + sq / 8)};
    };
    std::string uci = square(move.from_square) + square(move.to_square);
    if (move.promotion_piece) {
        const int type = move.promotion_piece / 2 + move.promotion_piece % 2;  // your formula: 2=N 3=B 4=R 5=Q
        uci += " pnbrqk"[type];                      // UCI uses lowercase for both colours
    }
    return uci;
}

std::optional<Position::Move> Position::UciToMove(const Chess& status, const std::string& uci) {
    for (const Move& move : LegalMoves(status).moves) {
        if (MoveToUci(move) == uci)
            return move;                 // a Move converts to an optional<Move> automatically
    }
    return std::nullopt;                 // no legal move matches
}

// SAN from a PGN file: compared with MoveToSan's own SAN for each legal move, both cleaned first. An ambiguous SAN
// ("Nd2" when MoveToSan says "Nbd2") matches nothing: nullopt, as it should.
std::optional<Position::Move> Position::SanToMove(const Chess& status, const std::string& san) const {
    auto clean = [](const std::string& text) {
        std::string out;
        for (char c : text) {
            if (c == '+' || c == '#' || c == '!' || c == '?')
                continue;                // check and mate marks, annotations: not part of the move
            out += (c == '0') ? 'O' : c; // castling written with zeros: 0-0, 0-0-0 (no square has a 0)
        }
        return out;
    };
    const std::string wanted = clean(san);
    if (wanted.empty())
        return std::nullopt;
    // The target square is the last one written ("Nbd2" → d2, "e8=Q" → e8); castling has none. MoveToSan is costly
    // (it generates moves itself), so only the moves landing on that square are asked.
    const std::size_t rank = wanted.find_last_of("12345678");
    const std::string target = (rank != std::string::npos && rank > 0) ? wanted.substr(rank - 1, 2) : "";
    for (const Move& move : LegalMoves(status).moves) {
        if (!target.empty() && MoveToUci(move).substr(2, 2) != target)
            continue;
        if (clean(MoveToSan(status, move)) == wanted)
            return move;
    }
    return std::nullopt;                 // no legal move matches (or the SAN is ambiguous)
}

std::string Position::FenOutput(const Chess& status) { // Definition of the FenOutput method
    std::ostringstream oss; // Create an output string stream to build the FEN string
    FEN c_fen_composer; // Create an instance of the FEN class to generate the FEN string
    oss << c_fen_composer.PosToFEN(status); // Generate the FEN string from the position data

    return oss.str();
}

// "rnbqkbnr/... w KQkq - 0 1" → "rnbqkbnr/... w KQkq -": the last two fields (the counters) cut off.
std::string Position::Key(const Chess& status) {
    const std::string fen = FenOutput(status);
    const std::size_t last = fen.rfind(' ');
    return fen.substr(0, fen.rfind(' ', last - 1));
}

// The Key with the en passant square only when a legal move takes there (a pinned pawn, or no pawn beside: none).
// (is_en_passant also marks the double pushes that create the square: the capture is the one landing ON it.)
std::string Position::RepetitionKey(const Chess& status) {
    const std::vector<Move> moves = LegalMoves(status).moves;
    const auto takes = [&status](const Move& m) { return m.is_en_passant && m.to_square == status.en_passant_target; };
    if (std::none_of(moves.begin(), moves.end(), takes)) {
        Chess same = status;
        same.en_passant_target = -1;
        return Key(same);
    }
    return Key(status);
}

// =================================================================================================
// 2. GAME STATE: mate, stalemate, 50 moves, dead positions; can a side still mate?
// =================================================================================================
Position::GameState Position::State(const Chess& status) {
    if (!LegalMoves(status).moves.empty()) { // moves exist: no mate/stalemate, so material or the clock can decide
        if (IsDeadPosition(status))
            return GameState::InsufficientMaterial;
        return status.halfmove_clock >= 100 ? GameState::FiftyMoves : GameState::Ongoing;
    }
    Moves checker;
    return checker.in_check(status, 0, 0) ? GameState::Checkmate : GameState::Stalemate;
}

bool Position::IsDeadPosition(const Chess& status) const {
    int knights = 0, darkBishops = 0, lightBishops = 0;
    for (int square = 0; square < 64; ++square) {
        switch ((status.mailbox[square] + 1) / 2) {   // piece type: 0 empty, 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
            case 1: case 4: case 5:
                return false;                         // a pawn, rook or queen: mate is still possible
            case 2:
                ++knights;
                break;
            case 3:
                if ((square / 8 + square % 8) % 2 == 0)   // rank + file even = dark (a1 = 0 is dark)
                    ++darkBishops;
                else
                    ++lightBishops;
                break;
        }
    }
    return knights + darkBishops + lightBishops <= 1                     // K v K, K + one minor v K
        || (knights == 0 && (darkBishops == 0 || lightBishops == 0));   // only bishops, all on one colour
}

bool Position::CanMate(const Chess& status, bool white) const {
    int knights = 0, bishops = 0;                      // this side's minors
    int theirKnights = 0, theirPawns = 0, theirBlockers = 0;   // blockers: the opponent's pieces but king and queens
    bool light = false, dark = false;                  // bishops on either colour of square, either side's
    for (int square = 0; square < 64; ++square) {
        const int code = status.mailbox[square];
        if (code == 0)
            continue;
        const int type = (code + 1) / 2;               // 1 pawn, 2 knight, 3 bishop, 4 rook, 5 queen, 6 king
        if (type == 3)
            ((square / 8 + square % 8) % 2 == 0 ? dark : light) = true;   // rank + file even = dark (a1 = 0)
        if ((code % 2 == 0) == white) {                // this side's piece (White = even)
            if (type == 1 || type == 4 || type == 5)
                return true;                           // a pawn, rook or queen: mate is possible
            knights += (type == 2);
            bishops += (type == 3);
        } else {
            theirKnights += (type == 2);
            theirPawns += (type == 1);
            theirBlockers += (type != 5 && type != 6);
        }
    }
    if (knights >= 2 || (knights == 1 && bishops >= 1))
        return true;                                   // K+NN (with help), K+B+N
    if (knights == 1)
        return theirBlockers > 0;                      // a knight's check can't be blocked: a piece that hems the
                                                       // king in is enough (a queen beside it would take the knight)
    if (bishops >= 1)
        return (light && dark) || theirKnights > 0 || theirPawns > 0;   // bishops on one colour only: a blocker
                                                                         // that can't interpose (a rook could)
    return false;                                      // a lone king
}

// =================================================================================================
// 3. LEGAL MOVES, PERFT
// =================================================================================================
Position::MoveList Position::LegalMoves(const Chess& status) const { // Definition of the LegalMoves method
    Position::MoveList semi_legal_moves; // Create an instance of the MoveList struct to hold the semi_legal moves
    Position::MoveList legal_moves;
    Moves c_move_generator; // Create an instance of the Moves class to generate moves
    semi_legal_moves = c_move_generator.raw_moves(status); // Generate the raw moves for the given position
    legal_moves = c_move_generator.ilegal_weeder(status, semi_legal_moves); // weed out the ilegal moves - deliver the legal moves

    return legal_moves;
}

int long long Position::Test(const Chess& status, int depth) {
    TestHarness nodes;
    int long long number_of_nodes;
    number_of_nodes = nodes.perft(status, depth);

    return number_of_nodes;
}

// =================================================================================================
// 4. VALIDITY, CAPTURED PIECES, SAN
// =================================================================================================
Position::Invalid Position::Validate(const Chess& status) const {
    // 1. Exactly one king of each colour.
    if (std::count(status.mailbox.begin(), status.mailbox.end(), 12) != 1)
        return Invalid::WhiteKings;
    if (std::count(status.mailbox.begin(), status.mailbox.end(), 11) != 1)
        return Invalid::BlackKings;

    // 2. No pawns on rank 1 or rank 8 (squares 0-7 and 56-63).
    for (int file = 0; file < 8; ++file) {
        for (int square : {file, 56 + file}) {
            const int piece = status.mailbox[square];
            if (piece == 1 || piece == 2)
                return Invalid::PawnOnBackRank;
        }
    }

    // 3. The side NOT to move must not be in check (it would have left its own king attacked).
    Chess other_side = status;            // a copy; status itself is const and stays as it is
    other_side.pit = !other_side.pit;     // "pretend it's the other side's turn"
    Moves checker;
    if (checker.in_check(other_side, 0, 0))
        return Invalid::NotToMoveInCheck;

    // 4. A castling right needs the king and that rook on their start squares.
    //    (White: king 12 on e1 = 4, rooks 8 on h1 = 7 / a1 = 0; Black: king 11 on e8 = 60, rooks 7 on h8 = 63 / a8 = 56.)
    if ((status.K && !(status.mailbox[4]  == 12 && status.mailbox[7]  == 8))
        || (status.Q && !(status.mailbox[4]  == 12 && status.mailbox[0]  == 8))
        || (status.k && !(status.mailbox[60] == 11 && status.mailbox[63] == 7))
        || (status.q && !(status.mailbox[60] == 11 && status.mailbox[56] == 7)))
        return Invalid::Castling;

    // 5. An en-passant square needs the pawn that just double-pushed past it.
    if (status.en_passant_target != -1) {
        const int ep = status.en_passant_target;
        const bool white_to_move = !status.pit;
        const int pawn_sq   = white_to_move ? ep - 8 : ep + 8;   // where the pushed pawn stands now
        const int origin_sq = white_to_move ? ep + 8 : ep - 8;   // where it came from
        const int pawn_code = white_to_move ? 1 : 2;             // the opponent's pawn
        const bool right_rank = white_to_move ? (ep >= 40 && ep <= 47)    // rank 6
                                              : (ep >= 16 && ep <= 23);   // rank 3
        if (!right_rank || status.mailbox[pawn_sq] != pawn_code
            || status.mailbox[ep] != 0 || status.mailbox[origin_sq] != 0)
            return Invalid::EnPassant;
    }

    return Invalid::None;
}

bool Position::IsValid(const Chess& status) {
    return Validate(status) == Invalid::None;   // the same checks; the editor wants the reason, the rest only yes / no
}

std::optional<Position::Chess> Position::PassTurn(const Chess& status) {
    Chess passed = status;               // a copy: the caller's position stays as it is
    passed.pit = !passed.pit;            // the other side to move
    passed.en_passant_target = -1;       // a pass is no double push: nothing to take en passant
    if (!IsValid(passed))                // the side that passed is in check: impossible
        return std::nullopt;
    return passed;                       // a Chess becomes an optional<Chess> by itself
}

// The model's one attack rule, from the square outward: every piece of that colour that attacks the square
// (defenders: ask with the colour of the piece standing there). Moves::in_check is built on it.
std::vector<int> Position::Attackers(const Chess& status, int square, bool white) const {
    std::vector<int> found;
    Moves moves;                                                 // for its slider() helper
    auto ofColour = [&](int sq) { const int p = status.mailbox[sq]; return p != 0 && (p % 2 == 0) == white; };
    auto type = [&](int sq) { const int p = status.mailbox[sq]; return p / 2 + p % 2; };   // 1 P 2 N 3 B 4 R 5 Q 6 K

    // Sliders: walk out from the square; the first piece blocks the rest (no x-rays).
    auto firstPiece = [&](auto&& range, int typeA, int typeB) {
        for (int from : range) {
            if (status.mailbox[from] == 0)
                continue;                                        // empty: keep going
            if (ofColour(from) && (type(from) == typeA || type(from) == typeB))
                found.push_back(from);
            return;                                              // any piece ends this direction
        }
    };
    for (int d : squareToDiagonals[square]) {                    // bishops and queens
        const auto halves = moves.slider(diagonals[d], square);
        firstPiece(halves[0] | std::views::reverse, 3, 5);       // the lower half, walked from the square down
        firstPiece(halves[1], 3, 5);
    }
    for (int rf : square_to_rank_file[square]) {                 // rooks and queens
        const auto halves = moves.slider(ranks_and_files[rf], square);
        firstPiece(halves[0] | std::views::reverse, 4, 5);
        firstPiece(halves[1], 4, 5);
    }

    // Jumpers and pawns: one step each.
    for (int from : knightMoves[square])
        if (ofColour(from) && type(from) == 2)
            found.push_back(from);
    for (int from : king_moves[square])
        if (ofColour(from) && type(from) == 6)
            found.push_back(from);
    // Pawns by the reversed table: a White pawn attacks the square if a BLACK pawn standing there could take it.
    for (int from : (white ? blackPawnCaptures : whitePawnCaptures)[square])
        if (ofColour(from) && type(from) == 1)
            found.push_back(from);
    return found;
}

std::vector<int> Position::Hanging(const Chess& status, bool white) const {
    static const int value[] = {0, 1, 3, 3, 5, 9, 100};   // by type (code + 1) / 2: -, P N B R Q K (never "cheaper")
    std::vector<int> hanging;
    for (int sq = 0; sq < 64; ++sq) {
        const int piece = status.mailbox[sq];
        if (piece == 0 || (piece % 2 == 0) != white || (piece + 1) / 2 == 6)
            continue;                                      // empty, the other side's, or the king
        const std::vector<int> attackers = Attackers(status, sq, !white);
        if (attackers.empty())
            continue;                                      // not attacked: safe
        const bool defended = !Attackers(status, sq, white).empty();
        const bool cheaper = std::any_of(attackers.begin(), attackers.end(), [&](int from) {
            return value[(status.mailbox[from] + 1) / 2] < value[(piece + 1) / 2];
        });
        if (!defended || cheaper)
            hanging.push_back(sq);
    }
    return hanging;
}

std::vector<Position::MaterialStep> Position::MaterialAlongLine(const Chess& status, const std::vector<std::string>& moves) {
    static const int value[] = {0, 1, 3, 3, 5, 9, 0};   // by type (code + 1) / 2: -, P N B R Q K
    std::vector<MaterialStep> steps;
    Chess pos = status;                                  // a copy: the line is played on it
    Moves mover;
    int balance = 0;
    for (const std::string& uci : moves) {
        std::optional<Move> move = UciToMove(pos, uci);
        if (!move)
            break;                                       // not legal here: the line ends
        const int sign = (pos.pit == status.pit) ? 1 : -1;   // the start side's ply, or the other side's
        const int taken = CapturedPiece(pos, *move);     // 0 = no capture (en passant included)
        int gain = value[(taken + 1) / 2];
        if (move->promotion_piece != 0)
            gain += value[(move->promotion_piece + 1) / 2] - 1;   // the pawn becomes the new piece
        balance += sign * gain;
        steps.push_back({balance, taken != 0, taken});
        pos = mover.make(pos, *move);
    }
    return steps;
}

Position::Gain Position::StableGain(const std::vector<MaterialStep>& steps) {
    const int n = int(steps.size());
    // The last stable step: its next ply isn't a capture. The last ply counts only if it isn't a capture itself
    // (its recapture may lie beyond the line's end). -1 = only the start is stable (balance 0).
    int stable = -1;
    for (int i = 0; i < n; ++i) {
        const bool unsettled = (i + 1 < n) ? steps[i + 1].capture : steps[i].capture;
        if (!unsettled)
            stable = i;
    }
    Gain gain{0, -1};
    if (stable >= 0) {
        gain.amount = steps[stable].balance;
        // The gain is dated where the balance was reached and then HELD: back over the stable steps that already
        // show it, so an even trade after the gain (a rook for a rook) doesn't move the date (2026-11-11, Anders'
        // 13.Bxd6: the knight won at ply 6, the rooks traded at 7-9; dated 9 it was past the coach's 6 plies).
        for (int i = stable - 1; i >= 0; --i) {
            const bool unsettled = steps[i + 1].capture;
            if (unsettled)
                continue;
            if (steps[i].balance != gain.amount)
                break;
            stable = i;
        }
        for (int i = stable; i >= 0; --i) {              // back to where the balance last changed
            const int before = (i > 0) ? steps[i - 1].balance : 0;
            if (steps[i].balance != before) {
                gain.ply = i;
                break;
            }
        }
    }
    return gain;
}
Position::Exchange Position::FirstExchange(const std::vector<MaterialStep>& steps) {
    Exchange exchange{0, {}, {}};
    for (int i = 0; i < int(steps.size()) && steps[i].capture; ++i) {   // the chain: captures from the first ply on
        (i % 2 == 0 ? exchange.yours : exchange.theirs).push_back(steps[i].taken);   // even plies are yours
        exchange.balance = steps[i].balance;
    }
    return exchange;
}

Position::Exchange Position::FirstSettled(const std::vector<MaterialStep>& steps) {
    const int n = int(steps.size());
    for (int i = 1; i < n; ++i) {
        const bool unsettled = (i + 1 < n) ? steps[i + 1].capture : steps[i].capture;
        if (unsettled)
            continue;
        Exchange settled{steps[i].balance, {}, {}};
        for (int k = 0; k <= i; ++k)
            if (steps[k].capture)
                (k % 2 == 0 ? settled.yours : settled.theirs).push_back(steps[k].taken);   // even plies are yours
        return settled;
    }
    return {0, {}, {}};
}

std::vector<int> Position::NewThreats(const Chess& status, const Move& move) const {
    const bool opponent_white = status.pit;                    // the mover is the side to move: Black → White
    const std::vector<int> before = Hanging(status, opponent_white);
    Moves mover;
    Move m = move;                                             // a copy: make() takes a non-const Move&
    const Chess after = mover.make(status, m);
    std::vector<int> fresh;                                    // hanging now, but not before
    for (int square : Hanging(after, opponent_white))
        if (std::find(before.begin(), before.end(), square) == before.end())
            fresh.push_back(square);
    return fresh;
}

Position::Answer Position::ThreatAnswer(const Chess& status, const Move& yours, const std::string& threat) {
    const std::optional<Chess> passed = PassTurn(status);
    const std::optional<Move> t = passed ? UciToMove(*passed, threat) : std::nullopt;
    if (!t || NewThreats(*passed, *t).empty())
        return {How::None, 0};                                 // no threat move, or it attacks nothing of yours
    const std::vector<int> targets = NewThreats(*passed, *t);  // your pieces it would leave hanging
    const bool white = !status.pit;                            // you: the side to move
    if (yours.to_square == t->from_square)
        return {How::Takes, status.mailbox[t->from_square]};
    Moves mover;
    Move m = yours;                                            // a copy: make() takes a non-const Move&
    const Chess after = mover.make(status, m);
    const std::optional<Move> again = UciToMove(after, threat);
    if (!again)
        return {How::Prevents, 0};
    Move a = *again;
    const Chess played = mover.make(after, a);
    const std::vector<int> hungBefore = Hanging(status, white);
    std::vector<int> fresh;                                    // yours hanging after both moves, but not before
    for (int square : Hanging(played, white))
        if (std::find(hungBefore.begin(), hungBefore.end(), square) == hungBefore.end())
            fresh.push_back(square);
    // The threat move would still attack, but its own piece hangs where it lands and is the only attacker: you take it.
    // The strongest answer, so asked first.
    const std::vector<int> theirs = Hanging(played, !white);
    bool costs = !fresh.empty() && std::find(theirs.begin(), theirs.end(), again->to_square) != theirs.end();
    for (int square : fresh)
        costs = costs && Attackers(played, square, !white) == std::vector<int>{again->to_square};
    if (costs)
        return {How::CostsIt, after.mailbox[again->from_square]};
    // Answered: fewer of your pieces hang than the threat would have hit (a fork broken to a single attack, which you
    // then answer by moving; the coach only asks this for moves that aren't red, so the engine agrees it's dealt with).
    if (fresh.size() < targets.size()) {
        if (std::find(targets.begin(), targets.end(), yours.from_square) != targets.end())
            return {How::MovesAway, status.mailbox[yours.from_square]};
        for (int square : targets)
            if (after.mailbox[square] == status.mailbox[square]
                && Attackers(after, square, white).size() > Attackers(status, square, white).size())
                return {How::Defends, status.mailbox[square]};
        return {How::Stops, 0};
    }
    return {How::None, 0};
}

Position::Principle Position::MovePrinciple(const Chess& status, const Move& move) const {
    if (move.is_castling)
        return Principle::Castles;
    const int type = (status.mailbox[move.from_square] + 1) / 2;   // 1 P, 2 N, 3 B, ...
    // The minor pieces' starting squares: knights b1 g1 b8 g8, bishops c1 f1 c8 f8 (a1 = 0).
    static constexpr int starts[] = {1, 6, 57, 62, 2, 5, 58, 61};
    if ((type == 2 || type == 3) && std::find(std::begin(starts), std::end(starts), move.from_square) != std::end(starts))
        return Principle::Develops;
    if (type == 1 && (move.to_square == 27 || move.to_square == 28 || move.to_square == 35 || move.to_square == 36))
        return Principle::Centre;                               // d4 e4 d5 e5
    return Principle::None;
}

int Position::CapturedPiece(const Chess& status, const Move& move) const {
    if (!move.is_capture)
        return 0;                                // also a double push: is_en_passant is set, is_capture isn't
    if (move.is_en_passant)
        return status.pit ? 2 : 1;               // e.p.: the side to move takes the other side's pawn (Black odd)
    return status.mailbox[move.to_square];       // whatever stood on the target square
}

std::string Position::MoveToSan(const Chess& status, const Move& move) const {
    static const std::string letters = " PNBRQK";                  // by type: (code + 1) / 2, both colours
    const int type = (status.mailbox[move.from_square] + 1) / 2;    // 1 = pawn ... 6 = king
    const std::string uci = MoveToUci(move);                        // e.g. "e4d5"
    const std::string to = uci.substr(2, 2);                        // the target square, e.g. "d5"
    const std::string x = move.is_capture ? "x" : "";

    std::string san;
    if (move.is_castling) {
        san = move.to_square > move.from_square ? "O-O" : "O-O-O";  // kingside: the king moves towards h
    } else if (type == 1) {
        san = move.is_capture ? uci.substr(0, 1) + x + to : to;     // a pawn capture names its file
        if (move.promotion_piece != 0)
            san += std::string("=") + letters[(move.promotion_piece + 1) / 2];   // e.g. "e8=Q"
    } else {
        // Disambiguation: can another identical piece (same code) also reach the target square?
        bool rival = false, sameFile = false, sameRank = false;
        for (const Move& other : LegalMoves(status).moves) {
            if (other.to_square == move.to_square && other.from_square != move.from_square
                && status.mailbox[other.from_square] == status.mailbox[move.from_square]) {
                rival = true;
                if (other.from_square % 8 == move.from_square % 8) sameFile = true;
                if (other.from_square / 8 == move.from_square / 8) sameRank = true;
            }
        }
        std::string from;
        if (rival) {
            if (!sameFile)      from = uci.substr(0, 1);   // the file is enough: "Nbd2"
            else if (!sameRank) from = uci.substr(1, 1);   // the rank is enough: "R1a3"
            else                from = uci.substr(0, 2);   // both are needed: "Qa1b2"
        }
        san = letters[type] + from + x + to;
    }

    // Check or mate: look at the position AFTER the move (the opponent is to move there).
    Moves mover;
    Move m = move;                                  // make() takes a non-const Move&: work on a copy
    const Chess after = mover.make(status, m);
    if (mover.in_check(after, 0, 0))
        san += LegalMoves(after).moves.empty() ? "#" : "+";
    return san;
}

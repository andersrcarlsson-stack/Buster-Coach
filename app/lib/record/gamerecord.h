// gamerecord.h — the game record: every move as it was played, with the coaching at that moment. Taken-back moves
// are KEPT, marked (a mistake taken back is still a mistake made: puzzle and review material). Plain C++ (no Qt).
#pragma once
#include <string>
#include <vector>
#include "uci_info.h"

// One move, as played.
struct MoveEntry {
    std::string uci;                     // "g1f3"
    std::string san;                     // "Nf3"
    bool human = false;                  // played by the human (else the opponent)
    bool takenBack = false;              // played, then taken back
    int ply = 0;                         // its place in the game when played: 0 = the first move (set by addMove)
    // The coaching at the moment of the move (the human's moves only):
    bool scored = false;                 // false: the coach had no score for it yet (a very quick move)
    MoveQuality quality = MoveQuality::Playable;
    bool onlyMove = false;               // "!": the only good move (movelist_design.md 3; yours only)
    UciInfo score {};                    // the human's view
    std::string bestMove {};             // the coach's correct move at that moment
    std::string line {};                 // the strongest line after it, in SAN
    int hints = 0;                       // hints used on this move
    bool threatAsked = false;            // you asked for the threat (T) on this move (shown by itself: not asked)
    std::vector<std::string> notes {};   // the coach's comments, e.g. "Buster missed 2... Qh4#" (an older name)
    std::string yourNote {};             // your own note on this move: the move list's Notes column (movelist_design.md 5)
    double after = -1.0;                 // your win chance after this move, 0..1 (the graph); -1 = not known (yet)
};

// The game as a whole (pgn_design.md): who, when, under what conditions, how it ended. The PGN tags come from here, so
// the values are kept in PGN's own forms.
struct GameInfo {
    std::string startFen {};             // "" = the standard start position (else a set-up position: SetUp/FEN tags)
    std::string white {}, black {};      // "You", "Stockfish 16"
    std::string date {};                 // "2026.10.25"
    std::string time {};                 // "14:32" (the file's name; the UTCTime-like tag)
    std::string timeControl = "-";       // "300+3" (seconds + increment); "-" = untimed
    int level = 0;                       // the opponent's strength, 1..10
    std::string result = "*";            // "1-0", "0-1", "1/2-1/2", or "*" (not finished)
    std::string ending {};               // how it ended, in words: "checkmate", "White resigned", "abandoned", …
    std::string eco {}, opening {};      // "B90", "Sicilian Defense: Najdorf Variation" ("" = no name)
};

class GameRecord {
public:
    void start();                                  // a new game: an empty record
    GameInfo &info() { return m_info; }                  // the game's data: filled in by the app as it learns it
    const GameInfo &info() const { return m_info; }
    void addMove(const MoveEntry &move);           // a move was played
    void takeBack(int plies);                      // the last `plies` moves still in the game are marked taken back
    void noteLastHumanMove(const std::string &note);   // a note on your newest move still in the game
    void setYourNote(int live, const std::string &note);   // your note on the game's move number `live` as it stands
                                                           // (0 = the first; taken-back moves not counted)
    void setLastAfter(double chance);              // the newest move still in the game gets its value (the opponent's
                                                   // arrives a moment after its move)
    std::vector<double> curve() const;             // the values of the moves in the game, in order; -1 = unknown
    int liveCount() const;                         // the moves in the game as it stands (not taken back)
    std::vector<const MoveEntry *> liveMoves() const;   // those moves themselves, in order (pointers into the record)
    const std::vector<MoveEntry> &moves() const { return m_moves; }   // all, in the order played
    std::vector<int> mistakes() const;             // indices of the human's bad moves, taken back ones included

private:
    std::vector<MoveEntry> m_moves;
    GameInfo m_info;
};

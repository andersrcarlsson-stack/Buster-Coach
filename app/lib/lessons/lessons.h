// lessons.h — the teaching levels' lesson moves (teaching_levels_design.md): which of the opponent's moves teaches
// the level's habit. Plain C++ (no Qt), like the model and the pick, so it sits behind a test.
//
// Level 1, free pieces: now and then the opponent leaves a piece free on purpose.
// Level 2, your loose pieces: the opponent never misses a piece of yours left hanging.
// Level 3, threats: the opponent makes a simple, direct attack now and then, and (as at level 2) takes the piece if
// it isn't answered.
//
// Every function takes the opponent's position (the opponent to move) and the coach's table of its moves as they
// really are (the pick's `deep` table: move -> score from the MOVER's view, its pv starting with the reply). Only moves
// in that table are ever chosen: they are the moves the opponent's own look put first, so a lesson stays a move a weak
// player could make. nullopt: no such move this turn (the lesson waits).

#pragma once
#include <map>
#include <optional>
#include <string>
#include "position.h"
#include "uci_info.h"

// The lesson a level teaches (lessons on): 0 = none (levels 4 and up).
enum class Lesson { None, FreePieces, LoosePieces, Threats };
Lesson lessonFor(int level);

// The rhythm: the opponent moves until the next lesson, drawn from 6 to 10 (dice 0.0 .. 1.0, from the caller).
int lessonGap(double dice);

// Level 1: a move that leaves a pawn, knight or bishop of the mover's free: newly hanging and undefended after it, the
// truth's reply takes it, and the move costs 0.7 to 4.5 pawns against the engine's own (a real gift, never a rook or queen). The
// dice chooses among such moves.
std::optional<std::string> freePieceMove(Position &model, const Position::Chess &position,
                                         const std::map<std::string, UciInfo> &deep, const std::string &engineMove,
                                         double dice);

// Levels 2 and 3: the best capture of a piece of the opponent's opponent (the player's) that hangs now, if the truth's
// line keeps the material and the capture is at most half a pawn worse than the engine's own move.
std::optional<std::string> takeLooseMove(Position &model, const Position::Chess &position,
                                         const std::map<std::string, UciInfo> &deep, const std::string &engineMove);

// Level 3: a quiet move (no capture) after which a piece of the player's hangs that didn't before (a direct attack),
// at most 0.4 pawns worse than the engine's own. The best of them by the truth.
std::optional<std::string> threatMove(Position &model, const Position::Chess &position,
                                      const std::map<std::string, UciInfo> &deep, const std::string &engineMove);

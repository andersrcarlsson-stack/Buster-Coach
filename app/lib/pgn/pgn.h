// pgn.h — games as PGN text (pgn_design.md): out now, in later. Plain C++ (no Qt).
#pragma once
#include <string>
#include <vector>
#include "gamerecord.h"

std::string writePgn(const GameRecord &record);   // the whole game as PGN: the tags, the moves, the result

// One game read from PGN text: as a record (its info from the tags, its moves), and why it stops early, if it does.
struct PgnGame {
    GameRecord record;
    std::string problem;   // "" = read completely; else why the moves stop there ("move 12: no legal Nxe5")
};

std::vector<PgnGame> readPgn(const std::string &text);   // every game in the text, in order

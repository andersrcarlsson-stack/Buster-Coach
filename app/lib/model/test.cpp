// test.cpp — perft: count the positions reachable in `depth` plies (the move generator's check).

#include "test.h"
#include "move.h"

int long long TestHarness::perft(const Position::Chess& status, int depth) {
    if (depth == 0) return 1; // a leaf: one position

    Position pos;
    Moves mover;
    int long long nodes {0};

    for (Position::Move move : pos.LegalMoves(status).moves) {
        nodes += perft(mover.make(status, move), depth - 1);
    }

    return nodes;
}

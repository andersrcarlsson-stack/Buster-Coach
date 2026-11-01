// moves <fen>: every legal move as "uci san", then "pass <fen>" (the other side to move)
#include <iostream>
#include "position.h"
int main(int, char *argv[]) {
    Position p;
    auto c = p.FenInput(argv[1]);
    if (!c) return 1;
    for (const auto &m : p.LegalMoves(*c).moves) std::cout << p.MoveToUci(m) << " " << p.MoveToSan(*c, m) << "\n";
    if (auto q = p.PassTurn(*c)) std::cout << "pass " << p.FenOutput(*q) << "\n";
}

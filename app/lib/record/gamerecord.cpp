// gamerecord.cpp — keeping the record.
#include "gamerecord.h"
#include <algorithm>

void GameRecord::start() {
    m_moves.clear();
    m_info = GameInfo{};                 // a new game: everything back to its defaults (result "*", untimed)
}

void GameRecord::addMove(const MoveEntry &move) {
    m_moves.push_back(move);
    m_moves.back().ply = liveCount() - 1;     // the moves in the game before it (it counts itself now)
}

// Mark the newest `plies` moves that are still in the game; moves taken back earlier are skipped.
void GameRecord::takeBack(int plies) {
    for (auto it = m_moves.rbegin(); it != m_moves.rend() && plies > 0; ++it) {
        if (!it->takenBack) {
            it->takenBack = true;
            --plies;
        }
    }
}

int GameRecord::liveCount() const {
    return int(std::count_if(m_moves.begin(), m_moves.end(),
                             [](const MoveEntry &m) { return !m.takenBack; }));
}

std::vector<int> GameRecord::mistakes() const {
    std::vector<int> result;
    for (int i = 0; i < int(m_moves.size()); ++i)
        if (m_moves[i].human && m_moves[i].scored && m_moves[i].quality == MoveQuality::Bad)
            result.push_back(i);
    return result;
}

// Walk back from the newest move to the human's newest move still in the game; none found: nothing happens.
void GameRecord::noteLastHumanMove(const std::string &note) {
    for (auto it = m_moves.rbegin(); it != m_moves.rend(); ++it) {
        if (it->human && !it->takenBack) {
            it->notes.push_back(note);
            return;
        }
    }
}
// The newest move still in the game (either side's); none: nothing happens. As noteLastHumanMove, without the "human".
void GameRecord::setLastAfter(double chance) {
    for (auto it = m_moves.rbegin(); it != m_moves.rend(); ++it) {
        if (!it->takenBack) {
            it->after = chance;
            return;
        }
    }
}

void GameRecord::setYourNote(int live, const std::string &note) {
    for (MoveEntry &move : m_moves)
        if (!move.takenBack && live-- == 0) {
            move.yourNote = note;
            return;
        }
}

std::vector<const MoveEntry *> GameRecord::liveMoves() const {
    std::vector<const MoveEntry *> live;
    for (const MoveEntry &move : m_moves)
        if (!move.takenBack)                  // the game as it stands: taken-back moves aren't part of its story
            live.push_back(&move);
    return live;
}

std::vector<double> GameRecord::curve() const {
    std::vector<double> values;
    for (const MoveEntry *move : liveMoves())
        values.push_back(move->after);
    return values;
}

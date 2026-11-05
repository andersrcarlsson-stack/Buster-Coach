// mastercoach.cpp — one deep search of the human's position per turn.

#include "mastercoach.h"
#include "engine.h"

// Shallower lines are no better than the general coach's, and they jump around (bar, attention).
static const int minDepth = 10;
// Unless depth 10 is slow to come (an engine's search blowing up in some position): after 3 s, from depth 6 on.
static const int slowAfterMs = 3000;
static const int slowMinDepth = 6;

MasterCoach::MasterCoach(Engine *engine, QObject *parent)
    : QObject(parent), m_engine(engine)
{
    connect(m_engine, &Engine::infoReceived, this, &MasterCoach::onInfo);
    connect(m_engine, &Engine::bestMove, this, &MasterCoach::onBestMove);
    connect(m_engine, &Engine::ready, this, &MasterCoach::onEngineReady);
    m_slowTimer.setSingleShot(true);
    connect(&m_slowTimer, &QTimer::timeout, this, [this]() {
        m_slow = true;
        if (m_best.pvMove.empty() && !m_latest.pvMove.empty()) {   // not spoken yet, but it has a line: speak now
            m_best = m_latest;
            emit bestMoveChanged(m_best);
        }
    });
}

// The human is to move: search this position until the turn ends.
void MasterCoach::onHumanTurnStarted(const QString &startFen, const QStringList &moves) {
    cancel();                    // an old search may still be running
    m_best = UciInfo();          // a new position: nothing known yet
    m_pending = "position fen " + startFen;
    if (!moves.isEmpty())
        m_pending += " moves " + moves.join(' ');
    if (m_engineReady)
        startSearch();           // otherwise onEngineReady starts it
}

void MasterCoach::startSearch() {
    m_engine->send(m_pending);
    m_engine->send("go infinite");   // no limit: runs until "stop" (Buster: up to depth 40)
    m_pending.clear();
    m_searching = true;
    m_slow = false;
    m_latest = UciInfo();
    m_slowTimer.start(slowAfterMs);
}

// Each depth sends an info line; its pv starts with the best move so far. The human is to move
// in the searched position, so the score is already in the human's view.
void MasterCoach::onInfo(const UciInfo &info) {
    if (m_staleBestMoves > 0 || !m_searching || info.pvMove.empty() || info.depth < slowMinDepth)
        return;                  // a stopped search's last words, no move in the line, or too shallow
    m_latest = info;
    if (info.depth < minDepth && !m_slow)
        return;                  // not deep enough yet (unless depth 10 is slow to come)
    m_best = info;
    emit bestMoveChanged(m_best);
}

// After "stop" (or at Buster's depth 40) the search ends with a bestmove. Nothing to do with it:
// the info lines already told us the move.
void MasterCoach::onBestMove(const QString &) {
    if (m_staleBestMoves > 0) {
        --m_staleBestMoves;
        return;
    }
    m_searching = false;
}

void MasterCoach::cancel() {
    m_pending.clear();
    m_best = UciInfo();   // the turn is over: no best move to offer any more (the hint greys out)
    m_slowTimer.stop();
    if (m_searching) {
        m_engine->send("stop");
        ++m_staleBestMoves;
        m_searching = false;
    }
}

void MasterCoach::onEngineReady() {
    m_engineReady = true;
    if (!m_pending.isEmpty())
        startSearch();
}

void MasterCoach::engineRestarted() {
    m_engineReady = false;      // a pending search waits for the new process's "ready" (onEngineReady starts it)
    m_searching = false;
    m_slowTimer.stop();
    m_staleBestMoves = 0;
}

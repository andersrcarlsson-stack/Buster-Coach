// mastercoach.h — the master coach: a third engine that searches only for the best move, as deep as
// the player's thinking time allows ("go infinite" when the human's turn starts, "stop" when it ends).

#pragma once
#include <QObject>
#include <QTimer>
#include <QStringList>
#include "uci_info.h"

class Engine;   // only a pointer here

class MasterCoach : public QObject
{
    Q_OBJECT

signals:
    // A new info line with a best move: deeper, or a different move. Score in the HUMAN's view.
    void bestMoveChanged(const UciInfo &best);

public:
    explicit MasterCoach(Engine *engine, QObject *parent = nullptr);

    // The best move so far in this turn (pvMove "" = nothing yet).
    const UciInfo &best() const { return m_best; }

public slots:
    // From Game: the human is to move. (The signal has a third argument, the position: not needed here.)
    void onHumanTurnStarted(const QString &startFen, const QStringList &moves);
    void cancel();   // stop a running search; forget a waiting one
    void engineRestarted();   // the master's engine was restarted: forget the old process, wait for its "ready"

private:
    Engine *m_engine;            // not owned: the main window owns it
    UciInfo m_best;              // the latest info line with a pv in this turn
    QString m_pending;           // a "position ..." command waiting to be searched ("" = none)
    bool m_searching = false;    // between "go infinite" and its bestmove
    int m_staleBestMoves = 0;    // bestmoves still to come from stopped searches: skip them
    bool m_engineReady = false;  // the uci/isready handshake is done
    void startSearch();          // send m_pending + "go infinite"
    // A slow search (2026-11-05: Buster 7.0.3 needs many seconds for depth 10 in some endgames, and the hint waited):
    // after slowAfterMs the master speaks from slowMinDepth on. m_latest: the deepest usable line so far.
    QTimer m_slowTimer;
    bool m_slow = false;
    UciInfo m_latest;
    void onInfo(const UciInfo &info);
    void onBestMove(const QString &);
    void onEngineReady();
};

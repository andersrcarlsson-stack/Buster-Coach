// enginetester.h — runs an engine file through the engine tests (engine_design.md): is it a UCI engine, can it coach,
// can it be an opponent? Each search runs in a fresh process (an engine stuck in one test can't spoil the next), and
// every step has a time limit: a silent engine ends as "No", never as a hang. Signals only, never a waiting loop.
#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include "uci_info.h"

class Engine;

class EngineTester : public QObject
{
    Q_OBJECT

signals:
    void progress(const QString &step);   // what it's doing now, for the status line ("coach search (depth 12)")
    // The results. name: the engine's "id name" ("" = not a UCI engine; coach.reason says why).
    void finished(const QString &name, const TestResult &coach, const TestResult &opponent);

public:
    explicit EngineTester(QObject *parent = nullptr);
    void start(const QString &path);      // runs every test, one after another, then emits finished
    void cancel();                        // stop a running test: no finished signal comes

private:
    enum class Step { Coach, Nodes, Movetime, Clock };   // in the order they run
    void run(Step step);                  // a fresh engine process for this step: the handshake, then the search
    void stepDone(bool answered);         // this step's search ended: a bestmove (true), or the time limit (false)

    QString m_path;
    QString m_name;                       // from "id name"
    Step m_step = Step::Coach;
    Engine *m_engine = nullptr;           // this step's engine; nullptr between steps
    QTimer m_timeout;                     // this step's time limit
    QElapsedTimer m_clock;                // how long the search took
    std::size_t m_pvLength = 0;           // the latest pv seen, in plies
    bool m_ready = false;                 // the handshake finished (if not: not a UCI engine)
    TestResult m_coach{Fitness::No, ""};
    bool m_nodes = false;                 // the opponent steps' answers
    bool m_movetime = false;
};

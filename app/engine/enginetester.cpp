// enginetester.cpp — the engine tests, run on a real engine (see enginetester.h; the judging is judgeCoach /
// judgeOpponent in lib/uci).

#include "enginetester.h"
#include "engine.h"

// The coach test's position: a middlegame (engine_design.md), searched as the coach does: "go depth 12".
static const char *coachFen = "r1bq1rk1/pp3ppp/2n2n2/3p4/1b1P4/2NB1N2/PP3PPP/R1BQ1RK1 w - - 0 10";

EngineTester::EngineTester(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() { stepDone(false); });   // too slow, or never ready
}

void EngineTester::start(const QString &path) {
    m_path = path;
    m_name.clear();
    run(Step::Coach);
}

void EngineTester::cancel() {
    if (!m_engine)
        return;                                       // nothing running
    m_timeout.stop();
    disconnect(m_engine, nullptr, this, nullptr);
    m_engine->deleteLater();
    m_engine = nullptr;                               // a late signal now finds nothing to do (stepDone's guard)
}

void EngineTester::run(Step step) {
    static const char *names[] = {"coach search (depth 12)", "opponent: nodes", "opponent: fixed time",
                                  "opponent: clock"};
    static const char *searches[] = {"go depth 12", "go nodes 20000", "go movetime 500",
                                     "go wtime 60000 btime 60000 winc 0 binc 0"};
    static const int limits[] = {5000, 3000, 3000, 8000};   // ms, the handshake included (a clock search may
                                                            // take a few seconds of a 1-minute clock)
    m_step = step;
    m_pvLength = 0;
    m_ready = false;
    emit progress(names[static_cast<int>(step)]);
    m_engine = new Engine(this);
    connect(m_engine, &Engine::lineReceived, this, [this](const QString &line) {
        if (line.startsWith("id name "))
            m_name = line.mid(8);
    });
    connect(m_engine, &Engine::infoReceived, this, [this](const UciInfo &info) {
        if (!info.pv.empty())
            m_pvLength = info.pv.size();             // the latest line with a pv
    });
    connect(m_engine, &Engine::ready, this, [this]() {
        m_ready = true;
        m_engine->send(m_step == Step::Coach ? QString("position fen ") + coachFen
                                             : QString("position startpos moves e2e4"));
        m_clock.start();
        m_engine->send(searches[static_cast<int>(m_step)]);
    });
    connect(m_engine, &Engine::bestMove, this, [this](const QString &move) {
        stepDone(move != "null" && move != "(none)" && move != "0000");   // not a move: no answer (Lishex: "null")
    });
    connect(m_engine, &Engine::error, this, [this]() { stepDone(false); });   // couldn't start, or crashed
    m_timeout.start(limits[static_cast<int>(step)]);
    m_engine->start(m_path);
}

void EngineTester::stepDone(bool answered) {
    if (!m_engine)
        return;                                       // this step has already ended (a late second signal)
    m_timeout.stop();
    disconnect(m_engine, nullptr, this, nullptr);     // nothing more from this engine, it's going
    m_engine->deleteLater();                          // deleted once its signal has returned; it says "quit"
    m_engine = nullptr;
    const long long ms = m_clock.elapsed();
    switch (m_step) {
    case Step::Coach:
        if (!m_ready) {                               // it never finished "uci"/"isready": not a UCI engine
            emit finished(QString(), {Fitness::No, "it didn't answer as a UCI engine"},
                          {Fitness::No, "it didn't answer as a UCI engine"});
            return;
        }
        m_coach = judgeCoach(answered, ms, m_pvLength);
        run(Step::Nodes);
        break;
    case Step::Nodes:
        m_nodes = answered;
        run(Step::Movetime);
        break;
    case Step::Movetime:
        m_movetime = answered;
        run(Step::Clock);
        break;
    case Step::Clock:
        emit finished(m_name, m_coach, judgeOpponent(m_nodes, m_movetime, answered));
        break;
    }
}

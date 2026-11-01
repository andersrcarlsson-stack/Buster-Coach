// logs.cpp — the debug views: the engine log and the coach log (docks, listed in the View menu, hidden at start).
//
// Sections:
//   1. The engine log dock and the View menu
//   2. The coach log dock and what it shows
//   3. What the engine log shows

#include "mainwindow.h"
#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include "boardwidget.h"
#include "engine.h"
#include "mastercoach.h"
#include "game.h"

// =================================================================================================================
// 1. THE ENGINE LOG DOCK AND THE VIEW MENU
// =================================================================================================================
void MainWindow::setupEngineLog()
{
    // The engine log in a dock panel (right side; the user can drag, float or resize it).
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    auto *dock = new QDockWidget("Engine", this);
    dock->setObjectName("engineLog");        // saveState/restoreState tell docks apart by this name
    dock->setWidget(m_log);                  // the dock takes ownership of the log
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // View menu: the logs are for debugging, so they start hidden (vision: player-first).
    m_viewMenu = menuBar()->addMenu("&View");
    QAction *logAction = dock->toggleViewAction();
    logAction->setText("&Engine log");
    m_viewMenu->addAction(logAction);
    dock->hide();
}

// =================================================================================================================
// 2. THE COACH LOG DOCK AND WHAT IT SHOWS
// =================================================================================================================
void MainWindow::setupCoachLog()
{
    m_coachLog = new QPlainTextEdit;
    m_coachLog->setReadOnly(true);
    auto *coachDock = new QDockWidget("Coach", this);
    coachDock->setObjectName("coachLog");
    coachDock->setWidget(m_coachLog);
    addDockWidget(Qt::RightDockWidgetArea, coachDock);
    QAction *coachLogAction = coachDock->toggleViewAction();
    coachLogAction->setText("&Coach log");
    m_viewMenu->addAction(coachLogAction);
    coachDock->hide();  // debug view: off at start, like the engine log

    // --- What the coach log shows ----------------------------------------------------------------------------------
    connect(m_coachEngine, &Engine::lineReceived, m_coachLog, [this](const QString &line) {
        if (!line.startsWith("info"))           // the info lines are parsed; showing them all floods the log
            m_coachLog->appendPlainText(line);
    });
    connect(m_coachEngine, &Engine::commandSent, m_coachLog, [this](const QString &cmd) {
        m_coachLog->appendPlainText("> " + cmd);
    });
    connect(m_coach, &Coach::moveScored, m_coachLog, [this](const QString &move, const UciInfo &s) {
        m_coachLog->appendPlainText(QString("coach: %1 → %2 %3 (depth %4)   %5")
                                    .arg(move).arg(s.mate ? "mate" : "cp").arg(s.score).arg(s.depth)
                                    .arg(m_coach->lineText(move)) + "   " + m_coach->whyText(move, m_qualities.value(move) == MoveQuality::Bad, m_correctMove));
    });
    connect(m_coach, &Coach::analysisDone, m_coachLog, [this]() {
        m_coachLog->appendPlainText(QString("coach: all moves scored (%1 in the table)").arg(m_coach->scores().size()));
    });
    connect(m_coach, &Coach::threatSearched, m_coachLog, [this](const UciInfo &p) {
        m_coachLog->appendPlainText(QString("coach: THREAT %1 → you %2 %3 (depth %4)")
                                    .arg(QString::fromStdString(p.pvMove))
                                    .arg(p.mate ? "mate" : "cp").arg(p.score).arg(p.depth));
    });
    connect(m_coach, &Coach::threatJudged, m_coachLog, [this]() {
        m_coachLog->appendPlainText("coach: THREAT judged: " + m_coach->threatText());
    });
    connect(m_masterEngine, &Engine::commandSent, m_coachLog, [this](const QString &cmd) {
        m_coachLog->appendPlainText("master> " + cmd);
    });
    connect(m_master, &MasterCoach::bestMoveChanged, m_coachLog, [this](const UciInfo &b) {
        m_coachLog->appendPlainText(QString("MASTER: %1 → %2 %3 (depth %4)")
                                    .arg(QString::fromStdString(b.pvMove)).arg(b.mate ? "mate" : "cp")
                                    .arg(b.score).arg(b.depth));
    });
    // Hanging pieces each turn (attack_design.md, step 3: watch the en prise rule in real games).
    connect(m_game, &Game::humanTurnStarted, m_coachLog,
            [this](const QString &, const QStringList &, const Position::Chess &position) {
        Position model;                                // the rules (Hanging is const: it only reads the position)
        auto list = [&](bool white) {                  // "Bc3 Nf6", or "-"
            QStringList names;
            for (int sq : model.Hanging(position, white))
                names << QString("%1%2%3").arg(QChar("?PNBRQK"[(position.mailbox[sq] + 1) / 2]))
                                          .arg(QChar('a' + sq % 8)).arg(sq / 8 + 1);
            return names.isEmpty() ? QString("-") : names.join(' ');
        };
        const bool youAreWhite = !position.pit;        // your turn: you're the side to move     
        m_coachLog->appendPlainText("HANGING: yours " + list(youAreWhite) + " · " + m_engine->shortName() + "'s " + list(!youAreWhite));
    });

}

// =================================================================================================================
// 3. WHAT THE ENGINE LOG SHOWS: both sides of the conversation, plus the user's moves
// =================================================================================================================
void MainWindow::setupEngineLogContents()
{
    // Direct signal -> slot connections (no lambda): QString in, QString out.
    connect(m_engine, &Engine::lineReceived, m_log, &QPlainTextEdit::appendPlainText);
    connect(m_board, &BoardWidget::moveMade, m_log, &QPlainTextEdit::appendPlainText);

    // A lambda, because the text gets a "> " prefix. The log as context object: if the log dies, the connection
    // goes with it.
    connect(m_engine, &Engine::commandSent, m_log, [this](const QString &cmd) {
        m_log->appendPlainText("> " + cmd);
    });
    connect(m_engine, &Engine::bestMove, m_log, [this](const QString &move) {
        m_log->appendPlainText(m_engine->shortName() + " plays " + move);
    });
    connect(m_engine, &Engine::infoReceived, m_log, [this](const UciInfo &info) {
        m_log->appendPlainText(QString("eval: %1 %2 (depth %3)")
                               .arg(info.mate ? "mate" : "cp").arg(info.score).arg(info.depth));
    });

    connect(m_engine, &Engine::error, this, [this](const QString &message) {
        QString text = message;
        if (!m_opponentPath.isEmpty() && m_opponentPath == bundledEngine())   // the built-in Buster (release_design.md 4)
            text += "\n\nThe built-in Buster needs a processor from 2013 or later (one with BMI2), and this computer's "
                    "may be older. Choose another UCI engine, for example Stockfish, under Engine → Engines….";
        QMessageBox::critical(this, "Engine", text);
    });
}

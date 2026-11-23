// lessons.cpp — MainWindow: the teaching levels (teaching_levels_design.md, 2026-11-23).
//
// Levels 1-3 teach a habit when Coaching → Lessons is on: free pieces (1), your loose pieces (2), threats (3). The
// opponent's lesson moves are Game's (lib/lessons); here, the window's part:
//   1. The switch: remembered, passed on to Game and the opponent's strip
//   2. Forced Full: while the lessons run, coaching is Full (lessons) and locked; your own level comes back after
//   3. The lesson card: a nudge before your move (the habit, not the answer), a word after it (caught or missed)

#include "mainwindow.h"
#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include "boardwidget.h"
#include "engine.h"
#include "game.h"
#include "playerstrip.h"

namespace {

QString pieceName(int code) {
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};
    return names[(code + 1) / 2];
}

QString squareName(int square) {                    // 28 -> "e4" (a1 = 0)
    return QString("%1%2").arg(QChar('a' + square % 8)).arg(square / 8 + 1);
}

}   // namespace

// =================================================================================================================
// 1. THE SWITCH
// =================================================================================================================
// Called after setupLevels: the level's switches are in place, so forcing Full can save them first.
void MainWindow::setupLessons()
{
    m_lessonsAction->setChecked(QSettings().value("coaching/lessons", true).toBool());   // a first start: on
    m_game->setLessons(m_lessonsAction->isChecked());
    m_opponentStrip->setLessons(m_lessonsAction->isChecked());
    connect(m_lessonsAction, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue("coaching/lessons", on);
        m_game->setLessons(on);
        m_opponentStrip->setLessons(on);
        updateLessons();
        updateCoachLine();
    });
    connect(m_opponentStrip, &PlayerStrip::levelChanged, this, [this]() {
        updateLessons();
        updateCoachLine();
    });

    // --- The lesson card's state: what the position offers now, and the word after your last move --------------
    connect(m_game, &Game::humanTurnStarted, this,
            [this](const QString &, const QStringList &moves, const Position::Chess &position) {
        m_lessonAfter.clear();
        // Exactly one ply after your move (Buster's reply): otherwise a takeback or a new game came in between.
        if (lessonsActive() && !m_lessonYourMove.isEmpty() && moves.size() == m_lessonPlies + 1) {
            const QString reply = moves.last();
            const int yourFrom = BoardWidget::uciSquare(m_lessonYourMove, 0);
            const int yourTo = BoardWidget::uciSquare(m_lessonYourMove, 2);
            const int replyTo = BoardWidget::uciSquare(reply, 2);
            const QString buster = m_engine->shortName();
            const QList<Position::Chess> &positions = m_game->positions();
            const Position::Chess beforeReply = positions.size() >= 2 ? positions[positions.size() - 2] : position;
            const int replyTook = beforeReply.mailbox[replyTo];   // your piece the reply took (0 = none)
            // Your move took something and the reply took back on that square: a trade you started, not a loss
            // (2026-11-23, Bxf6 Qxf6 against ...h6: "Buster took your bishop.").
            const bool traded = m_lessonWasPosition.mailbox[yourTo] != 0 && replyTo == yourTo;
            // The word after follows the nudge that was shown (the levels are cumulative: level 3 has all three).
            if (!m_lessonWasFree.isEmpty()) {       // free pieces: did you take it?
                m_lessonAfter = m_lessonWasFree.contains(yourTo)
                    ? QString("You took the free %1.").arg(pieceName(m_lessonWasPosition.mailbox[yourTo]))
                    : QString("The %1 on %2 was free.").arg(pieceName(m_lessonWasPosition.mailbox[m_lessonWasFree[0]]),
                                                              squareName(m_lessonWasFree[0]));
            } else if (!m_lessonWasLoose.isEmpty()) {   // your loose piece: taken, or kept safe?
                const int was = m_lessonWasLoose[0];
                const QString name = pieceName(m_lessonWasPosition.mailbox[was]);
                const int now = (yourFrom == was) ? yourTo : was;   // you moved it: it stands there now
                const std::vector<int> stillLoose = Position().Hanging(position, m_game->humanIsWhite());
                if (traded && now == yourTo)
                    m_lessonAfter = QString("You traded your %1 instead of losing it.").arg(name);
                else if (replyTo == now && replyTook != 0)
                    m_lessonAfter = QString("%1 took your %2: it was loose.").arg(buster, name);
                else if (std::find(stillLoose.begin(), stillLoose.end(), now) == stillLoose.end())
                    m_lessonAfter = QString("You kept your %1 safe.").arg(name);
                else   // not taken, but it hangs still (Buster attacked it again; the lesson audit, 2026-11-23)
                    m_lessonAfter = QString("Your %1 is still loose.").arg(name);
            } else if (!m_lessonWasThreat.isEmpty()) {   // the threat: carried out, or answered?
                if (reply == m_lessonWasThreat)
                    m_lessonAfter = QString("%1 carried out its threat: %2.").arg(buster, m_game->sanMoves().last());
                else if (replyTook != 0 && !traded)
                    m_lessonAfter = QString("%1 took your %2.").arg(buster, pieceName(replyTook));
                else
                    m_lessonAfter = QString("You answered %1's threat.").arg(buster);
            }
            if (!m_lessonAfter.isEmpty())
                m_record.noteLastHumanMove(m_lessonAfter.toStdString());   // the saved game keeps it (the review)
        }
        m_lessonYourMove.clear();
        m_lessonPosition = position;
        m_lessonFree.clear();                       // a new turn: what the lesson saw starts afresh
        m_lessonLoose.clear();
        m_lessonThreat.clear();
        m_lessonPassMate = false;
        // Buster's last move, if it took something: the piece that took stands there. Taking it back is a trade, not a
        // free piece (2026-11-23, "Something is free." on the queen after ...Qxb3).
        m_lessonRecapture = -1;
        m_lessonLeftLoose.clear();
        const QList<Position::Chess> &positions = m_game->positions();
        if (positions.size() >= 2) {                // your pieces loose as you left them (before Buster's move)
            const std::vector<int> loose = Position().Hanging(positions[positions.size() - 2], m_game->humanIsWhite());
            m_lessonLeftLoose = QList<int>(loose.begin(), loose.end());
        }
        if (!moves.isEmpty() && positions.size() >= 2) {
            const int to = BoardWidget::uciSquare(moves.last(), 2);
            if (positions[positions.size() - 2].mailbox[to] != 0)
                m_lessonRecapture = to;
        }
        updateLessonNow();
    });
    connect(m_coach, &Coach::analysisDone, this, &MainWindow::updateLessonNow);   // a pass: the verdicts are new
    connect(m_coach, &Coach::threatSearched, this, [this](const UciInfo &pass) {   // each pass, before its judgment
        m_lessonPassMate = pass.mate && pass.score < 0;   // your view: Buster mates if you pass
    });
    connect(m_coach, &Coach::threatJudged, this, &MainWindow::updateLessonNow);   // after m_threat is set
    // Your move: what the lesson saw is kept for the word after (humanTurnEnded comes first and clears the rest).
    connect(m_game, &Game::humanTurnEnded, this, [this]() {
        m_lessonWasFree = m_lessonFree;
        m_lessonWasLoose = m_lessonLoose;
        m_lessonWasThreat = m_lessonThreat;
        m_lessonWasPosition = m_lessonPosition;
        m_lessonFree.clear();
        m_lessonLoose.clear();
        m_lessonThreat.clear();
        m_lessonAfter.clear();
    });
    connect(m_game, &Game::movePlayed, this, [this](const QString &uci, const QString &, bool human) {
        if (!human)
            return;
        m_lessonYourMove = uci;
        m_lessonPlies = int(m_game->positions().size()) - 1;   // [0] is the start: plies played
    });

    updateLessons();                                // at start: a saved level 1-3 with lessons on forces Full now
}

bool MainWindow::lessonsActive() const
{
    return m_lessonsAction && m_lessonsAction->isChecked() && m_game->level() >= 1 && m_game->level() <= 3;
}

QList<QAction *> MainWindow::levelSwitches() const
{
    return {m_guidanceAction, m_evalBarAction, m_moveColoursAction, m_aidsAction,
            m_hintsSwitch, m_commentsAction, m_warningsAction};   // = levelPresets' columns, in order
}

// =================================================================================================================
// 2. FORCED FULL: lessons need the coach (teaching_levels_design.md). Your own level and switches are kept in
//    QSettings while forced (lessons/own*), so a quit in a lesson game gives them back at the next start too.
// =================================================================================================================
void MainWindow::updateLessons()
{
    const bool active = lessonsActive();
    if (active != m_lessonsForced) {
        const QList<QAction *> switches = levelSwitches();
        QSettings settings;
        if (active) {
            if (!settings.contains("lessons/ownSwitches")) {   // kept from a quit while forced: those are yours
                QVariantList own;
                for (QAction *action : switches)
                    own << action->isChecked();
                settings.setValue("lessons/ownSwitches", own);
                settings.setValue("lessons/ownLevel", m_level);
            }
            m_lessonsForced = true;
            m_level = 3;
            m_levelActions[3]->setChecked(true);
            applyLevel(3);
            // Full, but the warnings wait for you: the lesson's nudge names the habit, and the threat's answer (T) or
            // the "!" marks would give it away by themselves.
            m_warningsAction->setChecked(false);
        } else {
            m_lessonsForced = false;
            const int own = settings.value("lessons/ownLevel", m_level).toInt();
            const QVariantList saved = settings.value("lessons/ownSwitches").toList();
            m_level = own;
            if (own >= 0 && own < m_levelActions.size()) {
                m_levelActions[own]->setChecked(true);
                applyLevel(own);
            } else {
                m_level = -1;                       // Custom: none ticked, your own switches
                if (QAction *ticked = m_levelGroup->checkedAction())
                    ticked->setChecked(false);
                if (saved.size() == switches.size())
                    for (int i = 0; i < switches.size(); ++i)
                        switches[i]->setChecked(saved[i].toBool());
            }
            settings.remove("lessons/ownSwitches");
            settings.remove("lessons/ownLevel");
        }
    }
    m_levelActions[3]->setText(m_lessonsForced ? "Full coaching (lessons)" : levelNames[3]);
    updateMode();
    updateGameRow();                                // the ladder and the switches lock while forced
}

// =================================================================================================================
// 3. THE LESSON CARD
// =================================================================================================================
// What the lesson sees now. Free (level 1): a Buster piece the geometry calls hanging, with NO defender (attacked by a
// cheaper piece only is an exchange: 2026-11-23, Bxe7+ against a rook the king and knight defend), AND the coach's best
// move takes it (the green box): really free, not a trap. Loose (level 2): the real threat's target, one of your hanging pieces.
// Threat (level 3): the real threat itself. All from the coach's latest pass.
// Levels 1-2: once seen, kept for the turn (2026-11-23, Anders: "Check your pieces." was just a flash): a deeper pass
// may judge the threat again and drop it, but the habit's nudge, once given, stays until you move. Level 3: live (below).
void MainWindow::updateLessonNow()
{
    if (lessonsActive() && m_yourTurn) {
        const bool real = m_threatJudged && !m_threat.isEmpty();
        // Cumulative (2026-11-23, Anders): level 2 has the nudges of levels 1-2, level 3 all three; the most basic habit
        // first (lessonWords).
        const int level = m_game->level();
        for (auto it = m_qualities.cbegin(); it != m_qualities.cend(); ++it) {   // 1: something free (kept)
            const int to = BoardWidget::uciSquare(it.key(), 2);
            if (it.value() == MoveQuality::Correct && m_busterHanging.contains(to) && !m_lessonFree.contains(to)
                && to != m_lessonRecapture                  // taking back is a trade, not a free piece
                && Position().Attackers(m_lessonPosition, to, !m_game->humanIsWhite()).empty())
                m_lessonFree << to;
        }
        // 2: the real threat falls on a piece of yours that hangs. At level 3 only one you LEFT loose (it hung before
        // Buster's move too); a piece Buster's move attacked is level 3's question. Kept once seen.
        const int target = real ? BoardWidget::uciSquare(m_threat, 2) : -1;
        const bool leftLoose = real && m_hanging.contains(target) && (level == 2 || m_lessonLeftLoose.contains(target));
        if (level >= 2 && leftLoose && !m_lessonLoose.contains(target))
            m_lessonLoose << target;
        // 3: any other real threat that is a capture: Buster's next move takes a piece of yours (the lesson's own kind;
        // a quiet first move, a plan like ...h4 then ...Bxg5, is too deep for the question; T still shows it; 2026-11-23,
        // Anders), or a mate (…f4#: the lesson audit, 2026-11-23). Follows the latest judgment, not kept: T answers the question, so the two must agree (2026-11-23,
        // Anders: "What does Buster want?" stayed, T said "No direct threat.").
        const bool takes = real && (m_lessonPosition.mailbox[target] != 0 || m_lessonPassMate);
        if (level >= 3)
            m_lessonThreat = (takes && !leftLoose) ? m_threat : QString();
    }
    updateCoachLine();
}

// The card's words: what happened after your last move, then the nudge for this one (the habit, never the answer).
QString MainWindow::lessonWords() const
{
    QStringList words;
    if (!m_lessonAfter.isEmpty())
        words << m_lessonAfter;
    if (!m_lessonFree.isEmpty())
        words << "Something is free.";
    else if (!m_lessonLoose.isEmpty())
        words << "Check your pieces.";
    else if (!m_lessonThreat.isEmpty())
        words << QString("What does %1 want?").arg(m_engine->shortName());
    return words.join(' ');
}

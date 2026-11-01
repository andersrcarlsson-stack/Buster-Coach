// hint_threat.cpp — what you ask for: the hint (H) and the threat (T).
//
// Sections:
//   1. The hint: the master's move in two steps
//   2. The threat: what Buster would play if you passed
// 3. Your hanging pieces: the red "!"

#include "mainwindow.h"
#include <QAction>
#include <QMenu>
#include "engine.h"
#include "boardwidget.h"
#include "game.h"
#include "mastercoach.h"

// =================================================================================================================
// 1. THE HINT: the master's move in two steps (press 1: the piece; press 2: + its target)
// =================================================================================================================
void MainWindow::setupHint()
{
    m_coachingMenu->addSeparator();                  // the switches above, what you ask for below (H, T)
    m_hintAction = m_coachingMenu->addAction("&Hint");
    m_hintAction->setToolTip("A hint (H): first the piece to move, then where to. Hints are counted.");
    m_hintAction->setShortcut(Qt::Key_H);
    connect(m_hintAction, &QAction::triggered, m_board, [this]() {
        ++m_hintsUsed;                               // the action is greyed out when there's nothing to give,
                                                     // so every trigger is a real hint
        m_hintStep = std::min(m_hintStep + 1, 2);    // press 1: the piece, press 2: the move, then no more
        updateHint();
    });

    // The hint lives for one turn: reset when the turn ends (move, takeback, new game).
    connect(m_game, &Game::humanTurnEnded, m_board, [this]() {
        m_hintStep = 0;
        updateHint();                                // step 0: clears the board's hint
    });
    // It follows the master: a new best move moves the gold at the same step.
    connect(m_master, &MasterCoach::bestMoveChanged, this, &MainWindow::updateHint);
    // Connected BEFORE the threat's viewChanged listener (setupThreat), which reads m_viewing.
    connect(m_game, &Game::viewChanged, m_board, [this](int ply, int) {
        m_viewing = (ply != -1);                     // -1 = live
        updateHint();
    });

    // Selecting another piece than the hinted one ends the hint (reset: H again starts over, and counts).
    connect(m_board, &BoardWidget::pieceSelected, m_board, [this](int square) {
        const QString move = hintMove();             // V2: in practice the opening's move, not the master's
        if (m_hintStep == 0 || move.isEmpty())
            return;                                  // no hint showing
        if (square != BoardWidget::uciSquare(move, 0)) {
            m_hintStep = 0;
            updateHint();                            // clears the gold, and makes H available again
        }
    });

    updateHint();   // the starting state: greyed out until the master has spoken

    connect(m_hintsSwitch, &QAction::toggled, m_board, [this](bool on) {
        if (!on)
            m_hintStep = 0;                          // switched off: a hint on the board goes too
        updateHint();                                // and H greys out (or comes back)
    });
}

// The move the hint shows. Opening practice, while the game is inside the opening: H follows the opening, not the
// engine (the line's next move, then the book's most played move that stays inside). Out of it, and in normal games:
// the master's move. One place for both updateHint and the "another piece picked up" check (V2).
QString MainWindow::hintMove() const
{
    if (!m_practiceInside.isEmpty()) {
        const QString line = m_game->nextLineMove();
        if (!line.isEmpty())
            return line;
        if (!m_practiceBest.isEmpty())
            return m_practiceBest;
    }
    return QString::fromStdString(m_master->best().pvMove);
}

void MainWindow::updateHint()
{
    const QString move = hintMove();
    m_hintAction->setEnabled(m_hintsSwitch->isChecked() && !move.isEmpty() && m_hintStep < 2 && !m_viewing);
    if (move.isEmpty() || m_hintStep == 0) {
        m_board->setHint(-1, -1);                    // nothing to show
        return;
    }
    const int from = BoardWidget::uciSquare(move, 0);   // "g1f3" -> model squares (a1 = 0)
    const int to   = BoardWidget::uciSquare(move, 2);
    m_board->setHint(from, m_hintStep == 2 ? to : -1);
}

// =================================================================================================================
// 2. THE THREAT (T): what Buster would play if you passed (learning_design.md). Not a hint: not counted.
// =================================================================================================================
// A question, like H (2026-10-29; it was a toggle): T shows the threat (the flash on the board, the words on the top
// card), again and again; the flash goes when you pick up a piece, with Esc/End, or at the turn's end; the words stay
// all turn. "Warnings by themselves" shows it once a turn by itself (not counted as asked).
void MainWindow::setupThreat()
{
    m_threatAction = m_coachingMenu->addAction("Show &threat");
    m_threatAction->setShortcut(Qt::Key_T);
    connect(m_threatAction, &QAction::triggered, this, [this]() {
        m_threatAskedByYou = true;                   // asked: for the record, and the answer "No direct threat."
        setThreatShown(true);
    });
    connect(m_coach, &Coach::threatJudged, m_board, [this](const QString &move) {
        const bool first = !m_threatJudged;          // the turn's first judgment
        m_threat = move;     // every pass judges again: the deeper judgment replaces the earlier one
        m_threatJudged = true;
        if (first && m_warningsAction->isChecked() && m_aidsAction->isChecked())
            setThreatShown(true);                    // shown by itself, ONCE a turn: put away, it stays away
        updateThreat();
    });
    // Picking up a piece: the board belongs to your move now (its box, the linger flash). T brings the threat back.
    connect(m_board, &BoardWidget::pieceSelected, this, [this]() { setThreatShown(false); });
    connect(m_game, &Game::humanTurnEnded, m_board, [this]() {
        m_threat.clear();                            // the threat lives for one turn, like the hint
        m_threatJudged = false;
        setThreatShown(false);   // also when it wasn't showing: T greys out until the next judgment
    });
    connect(m_game, &Game::viewChanged, this, &MainWindow::updateThreat);   // after the hint's: m_viewing is set
    connect(m_aidsAction, &QAction::toggled, this, &MainWindow::updateThreat);
    updateThreat();   // the starting state: greyed out until the coach has judged
}

// The flash on the board on or off. On also brings the threat's words to the top card for the rest of the turn.
void MainWindow::setThreatShown(bool on)
{
    m_threatShown = on;
    if (on)
        m_threatWords = true;
    updateThreat();
    updateCoachLine();
}

void MainWindow::updateThreat()
{
    m_threatAction->setEnabled(m_aidsAction->isChecked() && m_threatJudged && !m_viewing);
    m_board->setThreat(m_threatShown && m_aidsAction->isChecked() ? m_threat : QString());
    updateHangingMarks();   // the "!" leaves the threat's target alone while the flash shows
}


// =================================================================================================================
// 3. YOUR HANGING PIECES: the red "!" (attack_design.md). Shown with "Warnings by themselves" and Thinking aids.
// =================================================================================================================
void MainWindow::setupHanging()
{
    connect(m_game, &Game::humanTurnStarted, m_board,
            [this](const QString &, const QStringList &, const Position::Chess &position) {
        const std::vector<int> hanging = Position().Hanging(position, !position.pit);   // you're the side to move
        m_hanging = QList<int>(hanging.begin(), hanging.end());
        const std::vector<int> buster = Position().Hanging(position, position.pit);
        m_busterHanging = QList<int>(buster.begin(), buster.end());
        m_turnPosition = position;       
        updateHangingMarks();
    });
    connect(m_game, &Game::humanTurnEnded, m_board, [this]() {
        m_hanging.clear();                           // a new position: the old marks mean nothing here
        m_busterHanging.clear();
        m_turnPosition = Position::Chess();          // no turn of yours now: no check words (no king = none)
        updateHangingMarks();
    });
    connect(m_warningsAction, &QAction::toggled, this, &MainWindow::updateHangingMarks);
}

void MainWindow::updateHangingMarks()
{
    QList<int> marks;
    // No "!" once the coach has judged there is no real threat (2026-10-29): nothing of yours can be taken profitably
    // now, so the marks would be false alarms (e.g. a knight "attacked by a pawn" whose capture opens a discovered
    // attack). Before the first judgment, and with a real threat, the geometry's marks show as before.
    const bool safeNow = m_threatJudged && m_threat.isEmpty();
    if (m_warningsAction->isChecked() && m_aidsAction->isChecked() && !safeNow) {
        marks = m_hanging;
        if (m_threatShown && m_threat.size() >= 4)
            marks.removeAll(BoardWidget::uciSquare(m_threat, 2));   // the flash says it already
    }
    m_board->setHangingMarks(marks);
}

// The reason in words, for the coach line, short enough for one line. "" if the piece on the square isn't hanging.
// Whose piece it is shows in the attacker: "the" = Buster's attacks yours, "your" = yours attacks Buster's.
//   "Queen d4: attacked by the f3 knight, not defended."
//   "Knight e5: attacked by your d4 pawn, a cheaper piece."   (defended, but that doesn't help)
// Yours, once the coach has judged there is no real threat: "…, but taking it doesn't pay now (no real threat)."

QString MainWindow::hangingWords(int square) const
{
    const bool yours = m_hanging.contains(square);
    if (!yours && !m_busterHanging.contains(square))
        return QString();
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    auto name = [this](int sq) { return QString(names[(m_turnPosition.mailbox[sq] + 1) / 2]); };
    auto at = [](int sq) { return QString("%1%2").arg(QChar('a' + sq % 8)).arg(sq / 8 + 1); };
    const bool white = m_turnPosition.mailbox[square] % 2 == 0;   // the hanging piece's colour
    Position model;
    QStringList attackers;
    for (int from : model.Attackers(m_turnPosition, square, !white))
        attackers << QString(yours ? "the " : "your ") + at(from) + " " + name(from);
    const bool defended = !model.Attackers(m_turnPosition, square, white).empty();
    QString piece = name(square);
    piece[0] = piece[0].toUpper();
    const bool safeNow = yours && m_threatJudged && m_threat.isEmpty();   // the engine: nothing of yours drops now
    return piece + " " + at(square) + ": attacked by " + attackers.join(" and ")
         + (defended ? ", a cheaper piece" : ", not defended")
         + (safeNow ? ", but taking it doesn't pay now (no real threat)." : ".");
}

// In check: who gives it, by the attack maps ("" = not in check). A double check: only the king can answer.
QString MainWindow::checkWords() const
{
    const bool white = !m_turnPosition.pit;          // you: the side to move
    const auto king = std::find(m_turnPosition.mailbox.begin(), m_turnPosition.mailbox.end(), white ? 12 : 11);
    if (king == m_turnPosition.mailbox.end())
        return QString();                            // no position (not your turn)
    const int square = int(king - m_turnPosition.mailbox.begin());
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    QStringList checkers;
    for (int from : Position().Attackers(m_turnPosition, square, !white))
        checkers << QString("the ") + names[(m_turnPosition.mailbox[from] + 1) / 2] + " on "
                    + QChar('a' + from % 8) + QString::number(from / 8 + 1);
    if (checkers.isEmpty())
        return QString();
    if (checkers.size() == 1)
        return "Now: check from " + checkers[0] + ".";
    return "Now: double check (" + checkers.join(" and ") + "): the king must move.";
}

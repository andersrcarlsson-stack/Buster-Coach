// coaching.cpp — the coaching around the board: the box colours, the game record, the coach line, "Buster missed
// it", the play-through, the eval bar.
//
// Sections:
//   1. Feedback: the bar's words, the box colours
//   2. The game record
//   3. The coach line: lines, the threat's words, notes
//   4. "Buster missed it"
//   5. The play-through (Space)
//   6. The eval bar: fill, preview frame, after your move

#include "mainwindow.h"
#include <QAction>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QDateTime>
#include <algorithm>
#include "engine.h"
#include "boardwidget.h"
#include "evalbar.h"
#include "game.h"
#include "mastercoach.h"
#include "confidencedot.h"
#include "wingraph.h"

// The verdict colours: the move's box on the board, and its card in the coach line.
static QColor verdictColour(MoveQuality quality) {
    switch (quality) {
    case MoveQuality::Correct:  return QColor(60, 180, 75);    // green
    case MoveQuality::Playable: return QColor(230, 160, 30);   // amber
    case MoveQuality::Bad:      break;
    }
    return QColor(200, 40, 40);                                 // red
}


// =================================================================================================================
// 1. FEEDBACK: the bar's words, the box colours
// =================================================================================================================
void MainWindow::setupFeedback() {
    // --- The bar's words (status bar) ------------------------------------------------------------------------------
    // A label IN the status bar. Unlike showMessage() (temporary, wiped by menu tips), a widget in the status bar
    // comes back after a temporary message.
    m_confidence = new ConfidenceDot;       // the dot first, then the words it qualifies
    statusBar()->addWidget(m_confidence);
    // Shown with Move colours (the verdicts it qualifies) AND the Evaluation bar (it sits with the eval words): Full.
    auto showDot = [this]() {
        m_confidence->setVisible(m_moveColoursAction->isChecked() && m_evalBarAction->isChecked());
    };
    connect(m_moveColoursAction, &QAction::toggled, m_confidence, showDot);
    connect(m_evalBarAction, &QAction::toggled, m_confidence, showDot);
    showDot();   // the start; the remembered switches are restored later, and their toggled signals update it
    m_evalWords = new QLabel;
    statusBar()->addWidget(m_evalWords);    // the status bar takes ownership
    connect(m_evalBarAction, &QAction::toggled, m_evalWords, &QWidget::setVisible);

    // --- Move colours: the box colours from the coaches ------------------------------------------------------------
    connect(m_aidsAction, &QAction::toggled, this, &MainWindow::updateMoveColours);
    connect(m_coach, &Coach::analysisDone, this, &MainWindow::updateMoveColours);
    connect(m_master, &MasterCoach::bestMoveChanged, this, &MainWindow::updateMoveColours);
    connect(m_moveColoursAction, &QAction::toggled, this, &MainWindow::updateMoveColours);   // on: at once; off: cleared
    connect(m_marksAction, &QAction::toggled, this, &MainWindow::updateMoveColours);
    // V1 (the likely cause of G7): the master named a move the coach hadn't scored yet; it's scored next, out of turn.
    // Its score comes before the pass ends: redraw then, or the correct move has no colour until the pass is done.
    connect(m_coach, &Coach::moveScored, this, [this](const QString &move) {
        if (move == QString::fromStdString(m_master->best().pvMove))
            updateMoveColours();
    });
    connect(m_game, &Game::humanTurnStarted, m_board, [this]() {
        m_board->setTargetColours({});      // a new position: the old colours mean nothing here
        m_board->setTargetMarks({});
        m_board->setBookMoves({});
        m_board->setPieceVerdicts({});
        m_board->setTargetReplies({});
        m_badReplies.clear();               // a new position: last turn's refutations mean nothing here
        m_qualities.clear();
        m_correctMove.clear();
    });

    // --- The confidence dot: how settled the verdicts are (coach_design.md "Coach Confidence: a dot") -------------
    // Connected AFTER updateMoveColours' analysisDone connection (above), so m_qualities already holds this pass's
    // verdicts when we compare.
    connect(m_coach, &Coach::analysisDone, m_confidence, [this]() {
        ++m_passesDone;
        if (m_passesDone > 1 && m_qualities == m_lastPassQualities)
            m_stablePasses = std::min(m_stablePasses + 1, 3);   // no verdict changed in this pass
        else
            m_stablePasses = 0;                                 // the first pass, or a change: start over
        m_lastPassQualities = m_qualities;
        m_confidence->setStable(m_passesDone >= 5 ? 3 : m_stablePasses);   // all five passes done: settled
    });
    connect(m_game, &Game::humanTurnStarted, m_confidence, [this]() {
        m_stablePasses = 0;                                     // a new turn: yellow again
        m_passesDone = 0;
        m_lastPassQualities.clear();
        m_confidence->setStable(0);
    });
}

// Every scored move classified against the best one. What the board shows depends on the switches; the
// snapshots (m_qualities, m_badReplies, m_correctMove) are kept ALWAYS: the record and the notes need them.
void MainWindow::updateMoveColours() {
    double best = -1.0;          // the best win chance in the general coach's table ...
    QString coachBest;           // ... and its move
    for (auto it = m_coach->scores().cbegin(); it != m_coach->scores().cend(); ++it) {
        if (winChance(it.value()) > best) {
            best = winChance(it.value());
            coachBest = it.key();
        }
    }
    // The correct move: the master's, or the general coach's best until the master has spoken.
    const QString masterMove = QString::fromStdString(m_master->best().pvMove);
    const QString correct = masterMove.isEmpty() ? coachBest : masterMove;
    QHash<QString, QColor> colours;
    QHash<QString, int> marks;
    QHash<int, int> verdicts;               // per piece (from-square): how many moves aren't bad
    QHash<QString, QString> replies;        // move -> its strongest reply, for the linger flash
    QSet<QString> counted;                  // from + to: a promotion's four pieces are ONE move to one square
    for (auto it = m_coach->scores().cbegin(); it != m_coach->scores().cend(); ++it) {
        const MoveQuality quality = classifyMove(m_coach->scores().value(coachBest), it.value(), it.key() == correct);
        m_qualities.insert(it.key(), quality);
        const int from = BoardWidget::uciSquare(it.key(), 0);
        const bool counts = quality != MoveQuality::Bad && !counted.contains(it.key().left(4));
        if (counts)
            counted.insert(it.key().left(4));
        verdicts[from] += counts ? 1 : 0;   // [] inserts 0 first: only bad moves = 0
        if (quality == MoveQuality::Bad)    // the flash answers "why is this bad?" (the line covers the rest)
            replies.insert(it.key(), QString::fromStdString(it.value().pvMove));
        switch (quality) {
        case MoveQuality::Correct:  marks.insert(it.key(), 1); break;
        case MoveQuality::Playable: marks.insert(it.key(), 0); break;
        case MoveQuality::Bad:      marks.insert(it.key(), -1); break;
        }
        colours.insert(it.key(), verdictColour(quality));

    }
    const bool showColours = m_moveColoursAction->isChecked();
    const bool showAids = m_aidsAction->isChecked();
    m_board->setTargetColours(showColours ? colours : QHash<QString, QColor>());
    m_board->setTargetMarks(showColours && m_marksAction->isChecked() ? marks : QHash<QString, int>());
    QSet<QString> book, leaving;            // the position's book moves (the box's book sign); in practice, while the
    Position model;                         // game is inside the opening, the ones that leave it (drawn hollow)
    for (const BookMove &m : m_book.moves(model.Key(m_game->positions().last()))) {
        const QString uci = QString::fromStdString(m.uci);
        book.insert(uci);
        if (!m_practiceInside.isEmpty() && !m_practiceInside.contains(uci))
            leaving.insert(uci);
    }
    m_board->setBookMoves(showColours ? book : QSet<QString>(), showColours ? leaving : QSet<QString>());   
    // A piece of the side to move with no legal move has nothing good either: 0, not "not known" (with guidance off
    // it can still be picked up, and lingering on it should say so).
    const Position::Chess &now = m_game->positions().last();
    QSet<int> canMove;
    for (const Position::Move &m : model.LegalMoves(now).moves)
        canMove.insert(m.from_square);
    for (int square = 0; square < 64; ++square) {
        const int code = now.mailbox[square];
        if (code != 0 && (code % 2 == 1) == now.pit && !canMove.contains(square))
            verdicts.insert(square, 0);
    }
    m_board->setPieceVerdicts(showAids ? verdicts : QHash<int, int>());
    m_board->setTargetReplies(showAids ? replies : QHash<QString, QString>());
    m_badReplies = replies;
    // Kind 3's second source: the red moves' common reply (commonReply, lib/uci), handed to the coach for its whys.
    std::vector<std::string> redReplies;
    for (const QString &reply : replies)          // a QHash's range-for visits the values: one reply per red move
        redReplies.push_back(reply.toStdString());
    const std::optional<CommonReply> common = commonReply(redReplies);
    m_coach->setCommonReply(common ? QString::fromStdString(common->move) : QString(), common ? common->count : 0);
    m_correctMove = correct;
}

// =================================================================================================================
// 2. THE GAME RECORD: every move as played (taken-back moves kept, marked), with your coaching snapshot
// =================================================================================================================
void MainWindow::setupRecord() {
    // The game's data (pgn_design.md), in PGN's own forms: who, when, under what conditions. At a new game, and when
    // the opponent's engine is ready with nothing played yet: the app's very first game starts there, without a
    // newGameStarted, and only then is the engine's name known.
    auto fillInfo = [this]() {
        GameInfo &info = m_record.info();
        const QString standard = Position::StartFen;
        info.startFen = m_game->startFen() == standard ? "" : m_game->startFen().toStdString();
        const std::string opponent = m_engine->name().toStdString();   // its full "id name": "Stockfish 16"
        info.white = m_game->humanIsWhite() ? "You" : opponent;
        info.black = m_game->humanIsWhite() ? opponent : "You";
        const QDateTime now = QDateTime::currentDateTime();
        info.date = now.toString("yyyy.MM.dd").toStdString();
        info.time = now.toString("HH:mm").toStdString();
        info.timeControl = m_game->timeControlPgn().toStdString();
        info.level = m_game->level();
    };
    connect(m_game, &Game::newGameStarted, this, [this, fillInfo]() {
        saveAbandoned();                             // the game being left, while the record still holds it
        m_record.start();
        fillInfo();
        updateGraph();
    });
    connect(m_engine, &Engine::ready, this, [this, fillInfo]() {
        if (m_record.liveCount() == 0)
            fillInfo();                              // nothing played yet: (re)fill, the name included
    });
    connect(m_game, &Game::gameOver, this, [this](const QString &text, const QString &result) {
        m_record.info().result = result.toStdString();
        m_record.info().ending = text.toStdString();
    });

    connect(m_game, &Game::movesTakenBack, this, [this](int plies) {
        m_record.takeBack(plies);
        m_record.info().result = "*";                // a takeback after the end reopens the game
        m_record.info().ending.clear();
        updateGraph();
    });
    connect(m_game, &Game::humanTurnStarted, this, [this]() {
        m_hintsAtTurn = m_hintsUsed;
        m_threatAskedByYou = false;
    });
    // "Asked" (T by hand, not the automatic showing) is set in setupThreat (hint_threat.cpp).
    connect(m_game, &Game::movePlayed, this, [this](const QString &uci, const QString &san, bool human) {
        MoveEntry move;
        move.uci = uci.toStdString();
        move.san = san.toStdString();
        move.human = human;
        if (human) {                     // what the coach knew at that moment (its table survives cancel)
            move.scored = m_qualities.contains(uci);
            move.quality = m_qualities.value(uci, MoveQuality::Playable);
            move.onlyMove = isOnlyMove(uci);
            move.score = m_coach->scores().value(uci);
            move.bestMove = m_correctMove.toStdString();
            move.line = m_coach->lineText(uci).toStdString();
            move.hints = m_hintsUsed - m_hintsAtTurn;
            move.threatAsked = m_threatAskedByYou;
            // The graph (graph_design.md): your move's value is its score; the opponent's move before it, if the
            // master hadn't spoken yet, gets the coach's best score (the position's value as the passes saw it).
            if (m_coach->scores().contains(uci))
                move.after = winChance(m_coach->scores().value(uci));
            const std::vector<double> curve = m_record.curve();
            if (!curve.empty() && curve.back() < 0 && !m_coach->scores().isEmpty())
                m_record.setLastAfter(bestChance());
            m_curveWaiting = false;                  // too late for the master's word now
        } else if (m_game->lastPickMistake()) {      // the opponent's mistake on purpose: marked as yours are
            move.scored = true;
            move.quality = MoveQuality::Bad;
            move.bestMove = m_game->lastEngineMove().toStdString();
        }
        m_record.addMove(move);
        updateGraph();
    });

    // The opponent's move gets its value from the master's first word (depth 10+) in the new position: the same signal
    // "... missed it" waits for. A turn start also comes after a takeback or a resume: the value is fresh then too.
    connect(m_game, &Game::humanTurnStarted, this, [this]() { m_curveWaiting = true; });
    connect(m_master, &MasterCoach::bestMoveChanged, this, [this](const UciInfo &now) {
        if (!m_curveWaiting)
            return;
        m_curveWaiting = false;
        m_record.setLastAfter(winChance(now));      // the master's score is in your view already
        updateGraph();
    });
}

// The "!" (movelist_design.md 3): your move was the only good one. The coach's best, every other move red (counted by
// target square: a promotion's four pieces are one move), the first pass done (it scores every legal move; the later
// passes only the candidates), and not a simple recapture (the opponent just took on the square your move goes to).
// A forced move (nothing else legal) has nothing to find: no "!".
bool MainWindow::isOnlyMove(const QString &uci) const {
    if (m_passesDone < 1 || m_qualities.value(uci, MoveQuality::Playable) != MoveQuality::Correct)
        return false;
    QSet<QString> good, all;                         // from + to: the moves' squares
    for (auto it = m_qualities.cbegin(); it != m_qualities.cend(); ++it) {
        all.insert(it.key().left(4));
        if (it.value() != MoveQuality::Bad)
            good.insert(it.key().left(4));
    }
    if (good.size() != 1 || all.size() < 2)
        return false;
    const std::vector<const MoveEntry *> moves = m_record.liveMoves();   // your move isn't in the record yet
    if (!moves.empty() && !moves.back()->human) {
        const MoveEntry &last = *moves.back();
        const bool took = last.san.find('x') != std::string::npos;
        if (took && last.uci.substr(2, 2) == uci.mid(2, 2).toStdString())
            return false;                            // a simple recapture
    }
    return true;
}

// =================================================================================================================
// 3. THE COACH LINE: under the move list. The strongest line after the move you linger on (level 2); otherwise
//    the threat's words (if shown this turn) and this turn's note.
// =================================================================================================================
void MainWindow::setupCoachLine() {
    m_coachLine = new QWidget;                      // a column of cards, one per row; an empty row is hidden
    m_coachLine->setToolTip("The coach's notes: the threat or the check now, about the last move, a hanging piece, why "
                            "the move you point at is good or bad, and its strongest line. Linger on a piece or a "
                            "move to fill them; T asks for the threat.");
    auto *cards = new QVBoxLayout(m_coachLine);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setSpacing(4);
    for (QLabel *&card : m_cards) {                 // a reference to each pointer in the array: we fill them in
        card = new QLabel;
        card->setWordWrap(true);                    // a long line wraps instead of widening the panel
        // Its width comes from the panel, never from the text: a wrapped label's size hint grows with its text,
        // and the panel (min 260, no max) would grow with it, nudging the board. Ignored = "take what you get".
        card->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        card->hide();
        cards->addWidget(card);
    }
    cards->addStretch();                            // the cards at the top, the free space below them
    m_coachLine->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_coachLine->setMinimumHeight(5 * m_coachLine->fontMetrics().height());   // five rows kept free: no jumping
    m_panelLayout->insertWidget(2, m_coachLine);    // after the move list's row (index 1), before the buttons

    // Row 2, the square: lingering on a hanging piece, yours or Buster's (Thinking aids). -1 = the linger ended.
    connect(m_board, &BoardWidget::lingerChanged, m_coachLine, [this](int square) {
        const bool asking = square != -1 && m_aidsAction->isChecked() && !m_viewing;
        m_rowSquare = asking ? hangingWords(square) : QString();
        // Row 3 already at the first level: the why is cheap (counting along the line), the long line waits for
        // level 2. The box's move is the move to the square we linger on.
        m_rowMove.clear();
        if (asking && !m_boxMoveNow.isEmpty()) {
            const QString key = m_coach->scores().contains(m_boxMoveNow) ? m_boxMoveNow : m_boxMoveNow + "q";
            m_rowMove = withBook(key, m_coach->whyText(key, m_qualities.value(key) == MoveQuality::Bad, m_correctMove));
        }
        updateCoachLine();
    });
    // Rows 3 and 4, the move: lingering long on a move: its why, then the strongest line. "" = the linger ended.
    connect(m_board, &BoardWidget::lingeredOnMove, m_coachLine, [this](const QString &move) {
        m_rowMove.clear();
        m_rowLine.clear();
        if (!move.isEmpty() && m_aidsAction->isChecked()) {
            const QString key = m_coach->scores().contains(move) ? move : move + "q";   // a promotion: the queen's
            m_rowMove = withBook(key, m_coach->whyText(key, m_qualities.value(key) == MoveQuality::Bad, m_correctMove));
            const QString line = m_coach->lineText(key);
            if (!line.isEmpty())
                m_rowLine = "Strongest line: " + line;
        }
        updateCoachLine();
    });
    connect(m_game, &Game::humanTurnEnded, m_coachLine, [this]() {
        m_coachNote.clear();                        // a new position: everything in the coach line goes
        m_threatWords = false;
        m_rowSquare.clear();
        m_rowMove.clear();
        m_rowLine.clear();
        updateCoachLine();
    });

    // Row 1, the position: the threat's words from their first showing on (T, or by itself: setThreatShown), all
    // turn, also when the flash is put away. Each new judgment (a deeper pass) may change them.
    connect(m_coach, &Coach::threatJudged, this, &MainWindow::updateCoachLine);
    connect(m_game, &Game::humanTurnStarted, this, &MainWindow::updateCoachLine);   // in check: the words at once
    connect(m_aidsAction, &QAction::toggled, this, &MainWindow::updateCoachLine);
}

// The coach line, composed: every event sets its own row and calls this; nobody writes the label directly, so
// nothing overwrites anything. The top card is the position now (the threat's words, or the check: back from the
// status bar since 2026-10-29); the cards below come and go with the turn and the linger.
void MainWindow::updateCoachLine() {
    // The threat's words, from its first showing on: a real threat by itself; "No direct threat." (or "can take, not
    // worth stopping") only as the answer when YOU pressed T: as a card every turn it would be noise.
    const bool real = !m_threat.isEmpty();          // the latest judgment (threatJudged: "" = not real)
    QString threatWords = (m_threatWords && m_aidsAction->isChecked() && (real || m_threatAskedByYou))
                              ? m_coach->threatText() : QString();
    if (threatWords.isEmpty() && m_aidsAction->isChecked())
        threatWords = checkWords();                 // in check there's no threat question: who gives check instead
    const QString rows[5] = {threatWords, m_coachNote, m_rowSquare, m_rowMove, m_rowLine};   // in the cards' order
    // Each card's left edge (Looks D): a little brighter in the dark theme. The line is the evidence: grey, quiet.
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    // The move's card takes the move's verdict colour, as its box on the board, but only while Move colours shows
    // the boxes' colours (in Guided they're off: the card mustn't give the verdict away).
    QString moveEdge = "gray";
    const QString key = m_coach->scores().contains(m_boxMoveNow) ? m_boxMoveNow : m_boxMoveNow + "q";   // a promotion
    if (m_moveColoursAction->isChecked() && m_qualities.contains(key))
        moveEdge = verdictColour(m_qualities.value(key)).name();   // "#3cb44b": a colour as style-sheet text
    const QString danger = dark ? "#ff7070" : "#c62828";
    const QString edges[5] = {danger,                         // the position (threat, check): red, danger
                              dark ? "#8ab4f8" : "#2a6ab0",   // the note: blue
                              danger,                         // the hanging square: red, as the "!"
                              moveEdge,                       // the move: its verdict colour, as its box
                              "gray"};                        // the strongest line
    for (int i = 0; i < 5; ++i) {
        m_cards[i]->setText(rows[i]);
        m_cards[i]->setVisible(!rows[i].isEmpty());
        // A half-transparent grey background: it darkens light and lightens dark, right in both themes.
        m_cards[i]->setStyleSheet(QString("QLabel { border-left: 4px solid %1; background: rgba(128, 128, 128, 12%);"
                                          " padding: 2px 6px;%2 }").arg(edges[i], i == 4 ? " color: gray;" : ""));
    }

}

// =================================================================================================================
// 4. "BUSTER MISSED IT": your red move went unpunished. Judged by the OUTCOME (the master's first word in the new
//    position), not by matching moves: Buster may have found another, equally strong punishment.
// =================================================================================================================
void MainWindow::setupMissed() {
    connect(m_board, &BoardWidget::moveMade, m_coachLine, [this](const QString &move) {
        m_refutation = m_badReplies.value(move);                         // "" unless the move was red
        m_refutationText = m_refutation.isEmpty() ? QString() : m_coach->lineText(move, 1);   // just the reply
        m_predicted = m_coach->scores().value(move);                     // the human's view, after the refutation
    });
    connect(m_game, &Game::humanTurnStarted, m_coachLine, [this](const QString &, const QStringList &moves) {
        // Exactly two moves later (yours, then the opponent's): otherwise a takeback or new game started this turn.
        m_checkMissed = !m_refutation.isEmpty() && moves.size() == m_movesAtTurn + 2 && moves.last() != m_refutation;
        m_refutation.clear();
        m_movesAtTurn = moves.size();
    });
    connect(m_master, &MasterCoach::bestMoveChanged, m_coachLine, [this](const UciInfo &now) {
        if (!m_checkMissed)
            return;
        m_checkMissed = false;                      // the master's FIRST word (depth 10+) decides, once
        const bool better = winChance(now) - winChance(m_predicted) >= 0.10    // clearly better than predicted:
                            || (!now.mate && !m_predicted.mate && now.score - m_predicted.score >= 200);   // the backstop
        if (better) {
            const QString note = m_engine->shortName() + " missed " + m_refutationText + ". You got away with it.";
            m_record.noteLastHumanMove(note.toStdString());   // the record keeps it at every level (the review)
            if (m_commentsAction->isChecked()) {                  // shown during the game only if notes are on
                m_coachNote = note;
                updateCoachLine();
            }
        }
    });

    // The opponent's mistake on purpose (release_design.md 14): a quiet nudge at Guided and Full, never
    // the move itself (H still gives it). Play and Tournament: nothing; the saved game marks it at every level.
    connect(m_game, &Game::movePlayed, m_coachLine, [this](const QString &, const QString &, bool human) {
        if (human || !m_game->lastPickMistake() || !m_commentsAction->isChecked() || !m_guidanceAction->isChecked())
            return;                                 // as its mark in the move list: Comments after moves AND Guidance
        m_coachNote = m_engine->shortName() + " slipped. There's something here.";
        updateCoachLine();
    });
}

// =================================================================================================================
// 5. THE PLAY-THROUGH: Space while the box is on a move shows it; each further Space one more ply of its line
// =================================================================================================================
void MainWindow::setupPlayThrough() {
    connect(m_board, &BoardWidget::boxMoveChanged, this, [this](const QString &uci) {
        m_boxMoveNow = uci;
    });
    auto *playAction = new QAction("Play the line", this);
    playAction->setShortcut(Qt::Key_Space);
    addAction(playAction);                          // a shortcut only works if a widget holds the action
    connect(playAction, &QAction::triggered, this, [this]() {
        if (m_playIndex != -1) {                    // a play-through is showing: Space steps on, up to the line's end
            if (m_playIndex + 1 < m_playSteps.size()) {
                ++m_playIndex;
                m_board->setPosition(m_playSteps[m_playIndex].position);
                m_board->setLastMove(m_playSteps[m_playIndex].from, m_playSteps[m_playIndex].to);
            }
            return;                                 // at the end the line stays; Esc or End goes back to the game
        }
        if (m_boxMoveNow.isEmpty() || !m_aidsAction->isChecked())
            return;                                 // no box on a move, or thinking aids off
        const QString key = m_coach->scores().contains(m_boxMoveNow) ? m_boxMoveNow : m_boxMoveNow + "q";
        m_playSteps = m_coach->lineSteps(key);
        if (m_playSteps.isEmpty())
            return;
        m_board->setViewing(true);                  // read-only while it plays (this drops the selection)
        m_board->setPreview(true);
        m_playIndex = 0;                            // step 0, your move, shown at once; each further Space one more ply
        m_board->setPosition(m_playSteps[0].position);
        m_board->setLastMove(m_playSteps[0].from, m_playSteps[0].to);
    });
    // Anything else happening to the game stops the play-through (takeback, new game, a move, navigation).
    connect(m_game, &Game::humanTurnEnded, this, &MainWindow::stopPlay);
    connect(m_game, &Game::viewChanged, this, &MainWindow::stopPlay);
}

void MainWindow::stopPlay() {
    if (m_playIndex == -1)
        return;
    m_playIndex = -1;
    m_board->setPreview(false);
}

// =================================================================================================================
// 6. THE EVAL BAR: fill, preview frame, after your move
// =================================================================================================================
// The best win chance among the coach's scored moves (= the position's evaluation for the player).
double MainWindow::bestChance() const {
    double best = 0.0;
    for (const UciInfo &s : m_coach->scores())
        best = std::max(best, winChance(s));
    return best;
}

void MainWindow::setupEvalBar() {
    // The position's evaluation for the player, as a win chance: the master's score (the deepest look), or the
    // general coach's best until the master has spoken.
    connect(m_coach, &Coach::analysisDone, m_evalBar, [this]() {
        if (!m_master->best().pvMove.empty())
            return;                                 // the master has spoken: its score is the better one
        m_evalBar->setWinChance(bestChance());
        m_evalWords->setText(QString::fromStdString(chanceWords(bestChance())));
    });
    connect(m_master, &MasterCoach::bestMoveChanged, m_evalBar, [this](const UciInfo &best) {
        // The human is to move there: already the human's view. A mate: "M3" instead of a percentage.
        m_evalBar->setWinChance(winChance(best), best.mate ? QString("M%1").arg(std::abs(best.score)) : QString());
        m_evalWords->setText(QString::fromStdString(chanceWords(winChance(best))));
    });

    // Eval bar v2: the frame previews the move under the box ("if you play this").
    connect(m_board, &BoardWidget::boxMoveChanged, m_evalBar, [this](const QString &uci) {
        const QHash<QString, UciInfo> &scores = m_coach->scores();
        const QString key = scores.contains(uci) ? uci : uci + "q";   // a promotion: the queen's score
        if (uci.isEmpty() || !scores.contains(key) || !m_guidanceAction->isChecked()
            || !m_moveColoursAction->isChecked()) {
            m_evalBar->clearPreview();  // no box, not scored (yet), guidance off (level 0 reveals nothing), or no
                                        // move colours: the frame's colour is a verdict too (Full only)
            return;
        }
        const double chance = winChance(scores.value(key));
        m_evalBar->setPreview(chance, bestChance() - chance);
    });

    // Right after the human's move: the score of the move actually played, if the coach has it.
    connect(m_board, &BoardWidget::moveMade, m_evalBar, [this](const QString &move) {
        if (!m_coach->scores().contains(move))
            return;                                 // not scored yet (a very quick move): keep the bar
        const double chance = winChance(m_coach->scores().value(move));
        m_evalBar->setWinChance(chance);
        m_evalWords->setText(QString::fromStdString(chanceWords(chance)) + " (after your move)");
    });

    // The game is over: the bar shows the outcome for you (full = you won, empty = you lost, half = a draw).
    // Queued: the game ends INSIDE a move's handling, and the "after your move" connection above runs after it.
    connect(m_game, &Game::gameOver, m_evalBar, [this](const QString &, const QString &result) {
        if (result == "*")
            return;                                 // not a finished game
        m_evalBar->clearPreview();
        if (result == "1/2-1/2")
            m_evalBar->setWinChance(0.5, "DRAW");
        else if ((result == "1-0") == m_game->humanIsWhite())
            m_evalBar->setWinChance(1.0, "WON");
        else
            m_evalBar->setWinChance(0.0, "LOST");
    }, Qt::QueuedConnection);
}

// =================================================================================================================
// THE WIN-CHANCE GRAPH (graph_design.md): between the move list and the coach line, fed from the record
// =================================================================================================================
void MainWindow::setupGraph() {
    m_graph = new WinGraph;
    m_graph->setToolTip("Your win chance after every move of this game; red marks your mistakes (moves the coach "
                        "rates red: they lose a lot). Click a point to see that position.");
    m_panelLayout->insertWidget(2, m_graph);        // after the move list's row (1), before the coach line (now 3)
    // Shown with the eval bar (graph_design.md): the levels and Custom switch it through the same switch.
    m_graph->setVisible(m_evalBarAction->isChecked());
    connect(m_evalBarAction, &QAction::toggled, m_graph, &QWidget::setVisible);
    // The amber line follows the move the board shows: an earlier one while browsing, the newest when live (-1).
    connect(m_game, &Game::viewChanged, m_graph, [this](int ply) { m_graph->setShownPly(ply); });
    // A click on the curve: that move on the board, as a click in the move list does.
    connect(m_graph, &WinGraph::plyClicked, m_game, &Game::viewPly);
}

void MainWindow::updateGraph() {
    QList<double> values;
    QList<bool> red;
    for (const MoveEntry *move : m_record.liveMoves()) {   // the game as it stands
        values << move->after;
        red << (move->human && move->scored && move->quality == MoveQuality::Bad);
    }
    m_graph->setCurve(values, red);
    updateMoveCells();                               // the move list's marks come from the record too
}

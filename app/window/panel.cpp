// panel.cpp — the side panel and the status bar: buttons, player strips, the move list, navigation, the status
// text, the mode status, captured pieces.
//
// Sections:
//   1. Side panel: buttons, player strips, strength slider, geek line, clocks
//   2. Move list: rows, viewing, clicks
//   3. Navigation: actions (keys + buttons), the column beside the move list
//   4. Status bar: result row, status text
//   5. Mode status
//   6. Captured pieces

#include "mainwindow.h"
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QSettings>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QActionGroup>
#include <QFile>
#include <QStyle>
#include "engine.h"
#include "evalbar.h"
#include "game.h"
#include "playerstrip.h"
#include "confidencedot.h"
#include "armedbutton.h"
#include "boardwidget.h"
#include "mastercoach.h"
#include <functional>

// The Notes column's typing box (movelist_design.md 5): one line, at most 30 characters. It also tells the window when
// typing begins and ends, so the table waits meanwhile (a rebuild would throw the typing away). The end: the box being
// destroyed, however the typing ended (Enter, Esc, a click elsewhere: the last one sends no closeEditor signal).
class NoteDelegate : public QStyledItemDelegate {
public:
    NoteDelegate(std::function<void()> opened, std::function<void()> closed, QObject *parent)
        : QStyledItemDelegate(parent), m_opened(std::move(opened)), m_closed(std::move(closed)) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
        auto *editor = new QLineEdit(parent);
        editor->setMaxLength(30);
        m_opened();
        connect(editor, &QObject::destroyed, this, m_closed, Qt::QueuedConnection);   // queued: the table is done
        return editor;
    }
private:
    std::function<void()> m_opened, m_closed;
};

// =================================================================================================================
// 1. SIDE PANEL: buttons, player strips, strength slider, geek line, clocks
// =================================================================================================================
void MainWindow::setupSidePanel()
{
    // --- Buttons ---------------------------------------------------------------------------------------------------
    // Each SHOWS an existing menu action, so menu and button share text, state and behaviour. Greying out an
    // action greys out both.
    // The game row (buttons_design.md), above the old row: Pause, Play now, Offer draw, Resign. The ones that end a
    // game arm first (armedbutton.h).
    auto *gameRow = new QHBoxLayout;
    m_pauseButton = new QPushButton("Pause");
    m_playNowButton = new QPushButton("Play now");
    m_drawButton = new ArmedButton("Offer draw", "Draw? ✓", "Click again within 3 s to offer a draw.");
    m_resignButton = new ArmedButton("Resign", "Resign? ✓", "Click again within 3 s to resign.");
    m_pauseButton->setToolTip("Pause the game: the clocks stop, and in a timed game the board is hidden. Not in "
                              "Tournament.");
    m_playNowButton->setToolTip("Make the opponent move now, with the best move it has found so far.");
    m_drawButton->setToolTip("Offer a draw (click twice). The opponent accepts if it isn't better.");
    m_resignButton->setToolTip("Resign the game (click twice).");
    for (QPushButton *button : {m_pauseButton, m_playNowButton, static_cast<QPushButton *>(m_drawButton),
                                static_cast<QPushButton *>(m_resignButton)}) {
        // The width comes from the panel, never from the text ("Pause"/"Resume", the armed question): equal shares
        // of the row, so a changing text can't widen the panel and nudge the board (as the coach line).
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        gameRow->addWidget(button);
    }
    m_panelLayout->addLayout(gameRow);
    auto *buttonRow = new QHBoxLayout;          // a layout without a widget: it goes inside the panel's column
    // New ♔ / New ♚ arm first (buttons_design.md): a new game abandons the one in progress. They trigger their menu
    // action when they fire (the menu and Ctrl+N fire directly), and keep its king icon (it follows the theme).
    m_newWhiteButton = new ArmedButton("New", "New? ✓", "Click again within 3 s to abandon this game.");
    m_newBlackButton = new ArmedButton("New", "New? ✓", "Click again within 3 s to abandon this game.");
    m_newWhiteButton->setToolTip("A new game as White (click twice while a game is in progress).");
    m_newBlackButton->setToolTip("A new game as Black (click twice while a game is in progress).");
    for (auto [button, action] : {std::pair{m_newWhiteButton, m_newWhite}, {m_newBlackButton, m_newBlack}}) {
        button->setIcon(action->icon());
        connect(action, &QAction::changed, button, [button, action]() { button->setIcon(action->icon()); });
        connect(button, &ArmedButton::fired, action, &QAction::trigger);
        buttonRow->addWidget(button);
    }
    for (QAction *action : {m_takebackAction, m_guidanceAction, m_hintAction}) {
        auto *button = new QToolButton;
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);   // icon AND text (default: only the icon)
        button->setDefaultAction(action);
        buttonRow->addWidget(button);
    }
    m_panelLayout->addLayout(buttonRow);        // layouts nest: a row inside the column
    m_armHint = new QLabel;                     // the armed button's hint, red and small; empty otherwise
    m_armHint->setStyleSheet("color: #c62828;");
    m_panelLayout->addWidget(m_armHint);

    // --- Player strips ---------------------------------------------------------------------------------------------
    // The opponent at the top, you at the bottom (where your pieces are).
    m_opponentStrip = new PlayerStrip;
    m_playerStrip = new PlayerStrip;
    m_panelLayout->insertWidget(0, m_opponentStrip);   // index 0: above the buttons
    // The move list: below the opponent's strip, above the buttons; placed with the navigation column (section 3).
    m_moveList = new QTableWidget(0, 4);                // number | White | Black | your notes (movelist_design.md)
    m_moveList->horizontalHeader()->hide();
    m_moveList->verticalHeader()->hide();
    m_moveList->setShowGrid(false);
    m_moveList->setAlternatingRowColors(true);         // every other row shaded, no lines
    m_moveList->setEditTriggers(QAbstractItemView::DoubleClicked);   // only the notes can be typed in (their flags)
    m_moveList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_moveList->setFocusPolicy(Qt::NoFocus);           // never the keyboard: ← → stay the app's navigation keys
    m_moveList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);   // the numbers: narrow
    const int moveWidth = fontMetrics().horizontalAdvance("Qxd8=Q+! ✎") + 12;                  // a long move, a mark
    for (int column : {1, 2}) {                                                                // White, Black: as
        m_moveList->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);      // wide as a long move
        m_moveList->setColumnWidth(column, moveWidth);
    }
    m_moveList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);            // your notes: the rest
    m_moveList->setItemDelegateForColumn(3, new NoteDelegate(
        [this]() { ++m_noteEditors; },
        [this]() {                                      // typing ended: a rebuild owed meanwhile comes now
            if (--m_noteEditors == 0 && m_moveListPending)
                updateMoveList();
        }, m_moveList));
    m_moveList->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);          // the text + a little air
    m_moveList->setToolTip("The game so far. Click a move to see that position; the game goes on meanwhile (End or "
                           "Esc brings you back).");
    m_panelLayout->addWidget(m_playerStrip);           // at the very bottom
    auto showSides = [this](bool humanIsWhite) {
        m_opponentStrip->setPlayer(m_engine->name(), !humanIsWhite);
        m_playerStrip->setPlayer("You", humanIsWhite);
        m_evalBar->setPlayerWhite(humanIsWhite);
    };
    connect(m_game, &Game::sidesChanged, m_opponentStrip, showSides);
    showSides(true);                                   // at start, before any restart: Game's default is White
    // The opponent's name arrives with its engine's handshake, and changes with the engine (engine setup).
    connect(m_engine, &Engine::ready, m_opponentStrip, [this]() {
        m_opponentStrip->setPlayer(m_engine->name(), m_opponentStrip->isWhite());
    });

    // --- Strength slider (remembered between runs) -----------------------------------------------------------------
    connect(m_opponentStrip, &PlayerStrip::levelChanged, m_game, &Game::setLevel);
    // Every change is written; the start reads it (a first run: level 5).
    connect(m_opponentStrip, &PlayerStrip::levelChanged, m_opponentStrip, [](int level) {
        QSettings().setValue("strength/level", level);
    });
    // Up to 2026-11-04 the slider had 10 levels, 10 = full strength; level 10 is now ≈ 2400 and full strength 11. A
    // saved 10 from then still means full strength: moved up once (the "strength/levels" mark says it's done).
    if (QSettings().value("strength/levels", 10).toInt() < 11) {
        if (QSettings().value("strength/level", 5).toInt() == 10)
            QSettings().setValue("strength/level", 11);
        QSettings().setValue("strength/levels", 11);
    }
    const int level = QSettings().value("strength/level", 5).toInt();
    m_opponentStrip->showStrength(level);
    m_game->setLevel(level);   // showStrength only signals a CHANGE: a saved 1 is the slider's own start value

    // --- The geek line ---------------------------------------------------------------------------------------------
    // Buster's search as it runs. No score (it would reveal the evaluation).
    connect(m_engine, &Engine::infoReceived, m_opponentStrip, [this](const UciInfo &i) {
        auto big = [](long long n) {                  // 1521867 -> "1.5M", 20000 -> "20k"
            if (n >= 1000000) return QString::number(n / 1e6, 'f', 1) + "M";
            if (n >= 1000)    return QString::number(n / 1e3, 'f', 0) + "k";
            return QString::number(n);
        };
        const QString time = (i.time < 1000) ? QString("%1 ms").arg(i.time)                       // "12 ms"
                                             : QString("%1 s").arg(i.time / 1000.0, 0, 'f', 1);   // "8.4 s"
        m_opponentStrip->setStats(QString("depth %1 · %2 nodes · %3 nps · %4")
                                  .arg(i.depth).arg(big(i.nodes)).arg(big(i.nps)).arg(time));
    });
    // A new search: clear the old numbers first (some searches send no info line at all: G11).
    connect(m_engine, &Engine::commandSent, m_opponentStrip, [this](const QString &cmd) {
        if (cmd.startsWith("go"))
            m_opponentStrip->setStats(QString());
    });

    // --- Clocks: each reading goes to the strip of that colour -----------------------------------------------------
    connect(m_game, &Game::clockChanged, m_playerStrip,
            [this](bool white, bool timed, qint64 ms, bool running, qint64 startMs) {
        PlayerStrip *strip = (m_playerStrip->isWhite() == white) ? m_playerStrip : m_opponentStrip;
        strip->setClock(timed ? QString::fromStdString(formatClock(ms)) : QString(), running, clockAlert(ms, startMs));
    });
}

// =================================================================================================================
// 2. MOVE LIST: rows, viewing, clicks
// =================================================================================================================
void MainWindow::setupMoveList()
{
    // --- Rows: rebuilt from Game on every change (updateMoveList); your notes saved as they're typed ----------------
    connect(m_game, &Game::sanMovesChanged, this, &MainWindow::updateMoveList);   // its arguments: read from Game
    connect(m_moveList, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        const int live = item->column() == 3 ? yourMoveInRow(item->row()) : -1;
        if (live < 0)
            return;                                     // not a note (the cells are filled with signals blocked)
        m_record.setYourNote(live, item->text().trimmed().toStdString());
        saveNotes();
    });
    // The marks and comments follow the coaching switches (yours: Comments after moves; the opponent's: also Guidance).
    connect(m_commentsAction, &QAction::toggled, this, &MainWindow::updateMoveCells);
    connect(m_guidanceAction, &QAction::toggled, this, &MainWindow::updateMoveCells);

    // --- Viewing an earlier position: row highlight, greyed eval ---------------------------------------------------
    // Highlight the viewed move's row. Live (-1) or the start (0): no highlight.
    connect(m_game, &Game::viewChanged, m_moveList, [this](int ply, int) {
        if (ply <= 0) {
            m_moveList->clearSelection();
            return;
        }
        const int cell = ply - 1 + (m_game->blackFirst() ? 1 : 0);   // the move's place, counting a "…"
        m_moveList->setCurrentCell(cell / 2, 1 + cell % 2);
    });
    // While viewing an earlier position, the eval feedback (about the live game) is greyed out. A loaded game's moves
    // each have their value (the file's), so its bar stays lit (saving.cpp: showLoadedMove).
    connect(m_game, &Game::viewChanged, m_evalBar, [this](int ply, int) {
        const bool live = (ply == -1) || m_loaded;
        m_evalBar->setEnabled(live);
        m_evalWords->setEnabled(live);
        m_confidence->setEnabled(live);
        m_evalBar->update();             // repaint with the new look
    });

    // --- Click a move ----------------------------------------------------------------------------------------------
    // A click on a move views the position after exactly that move; a number, the "…", a note or the result row:
    // nothing.
    connect(m_moveList, &QTableWidget::cellClicked, m_game, [this](int row, int column) {
        const int ply = 2 * row + column - (m_game->blackFirst() ? 1 : 0);   // cell 2*row + column - 1, plus one
        if ((column == 1 || column == 2) && ply >= 1)
            m_game->viewPly(ply);                    // viewPly clamps a ply beyond the end to live
    });
}

// The move list's rows (movelist_design.md): the number, White's move, Black's move, your note; the result last. A
// move's place among the cells counts a "…" first when Black began: cell = ply - 1 + bf; row cell / 2. While a note
// is being typed the table waits (a rebuild would throw the typing away): after Enter or Esc it catches up.
void MainWindow::updateMoveList() {
    if (m_noteEditors > 0) {
        m_moveListPending = true;
        return;
    }
    m_moveListPending = false;
    const QSignalBlocker quiet(m_moveList);             // filling the cells isn't typing a note (itemChanged)
    const QStringList &san = m_game->sanMoves();
    const int number = m_game->firstNumber();
    const int bf = m_game->blackFirst() ? 1 : 0;
    m_moveList->setRowCount(0);                         // every row away (the result row too)
    m_moveList->setRowCount((san.size() + bf + 1) / 2);
    for (int row = 0; row < m_moveList->rowCount(); ++row) {
        auto *numberCell = new QTableWidgetItem(QString::number(number + row));
        numberCell->setTextAlignment(Qt::AlignCenter);
        numberCell->setForeground(QColor(130, 130, 130));   // grey: the moves stand out
        numberCell->setFlags(Qt::ItemIsEnabled);             // shown, not selectable
        m_moveList->setItem(row, 0, numberCell);
        auto *noteCell = new QTableWidgetItem;              // filled from the record (updateMoveCells)
        noteCell->setFlags(Qt::ItemIsEnabled);
        m_moveList->setItem(row, 3, noteCell);
    }
    if (bf == 1 && !san.isEmpty()) {
        auto *dots = new QTableWidgetItem("…");             // Black began: White's first cell is empty
        dots->setFlags(Qt::ItemIsEnabled);
        m_moveList->setItem(0, 1, dots);
    }
    for (int i = 0; i < san.size(); ++i)
        m_moveList->setItem((i + bf) / 2, 1 + (i + bf) % 2, new QTableWidgetItem(san[i]));
    if (!m_resultText.isEmpty()) {                      // the game is over: its end in words, across the table
        const int row = m_moveList->rowCount();
        m_moveList->insertRow(row);
        m_moveList->setSpan(row, 0, 1, 4);
        auto *result = new QTableWidgetItem(m_resultText);
        result->setTextAlignment(Qt::AlignCenter);
        result->setForeground(QColor(130, 130, 130));
        result->setFlags(Qt::NoItemFlags);              // not clickable
        m_moveList->setItem(row, 0, result);
    }
    m_moveList->scrollToBottom();
    updateMoveCells();                                  // the new cells: marks, comments, notes
}

// The cells from the record (movelist_design.md 2-5): a red "?" on a mistake, a green "!" on your only good move, the
// coach's comments as a move's tooltip ("Best was …", "Buster missed …"), your notes in the Notes column. The marks
// and comments follow the coaching during the game: yours with Comments after moves, the opponent's (its mistakes on
// purpose) with Guidance too. Once the game is over, and in a loaded game: all of them.
void MainWindow::updateMoveCells() {
    const QSignalBlocker quiet(m_moveList);             // not typing (itemChanged)
    const bool all = m_gameOver || m_loaded;
    const int bf = m_game->blackFirst() ? 1 : 0;
    const std::vector<const MoveEntry *> moves = m_record.liveMoves();
    Position model;
    for (int i = 0; i < int(moves.size()); ++i) {
        QTableWidgetItem *cell = m_moveList->item((i + bf) / 2, 1 + (i + bf) % 2);
        if (!cell)
            continue;                                   // the table is a move behind the record: it follows
        const MoveEntry &move = *moves[i];
        const bool shown = all || (m_commentsAction->isChecked() && (move.human || m_guidanceAction->isChecked()));
        const bool bad = shown && move.scored && move.quality == MoveQuality::Bad;
        const bool only = shown && move.onlyMove;      // your only good move (or a "!" in a loaded file)
        cell->setText(QString::fromStdString(move.san) + (bad ? "?" : only ? "!" : ""));
        const QColor colour = bad ? QColor(211, 47, 47) : QColor(46, 158, 68);   // red, green: the box colours
        cell->setData(Qt::ForegroundRole, bad || only ? QVariant(colour) : QVariant());   // none: the palette's
        QStringList comments;                           // the coach's comments: the move's tooltip
        if (bad && !move.bestMove.empty() && move.bestMove != move.uci && i < m_game->positions().size()) {
            const Position::Chess &before = m_game->positions()[i];   // [i]: the position before move i
            if (const std::optional<Position::Move> best = model.UciToMove(before, move.bestMove))
                comments << "Best was " + QString::fromStdString(model.MoveToSan(before, *best)) + ".";
        }
        if (shown)
            for (const std::string &comment : move.notes)
                comments << QString::fromStdString(comment);
        cell->setToolTip(comments.join("\n"));
    }
    for (int row = 0; row < m_moveList->rowCount(); ++row) {   // your notes: on your move in each row
        QTableWidgetItem *note = m_moveList->item(row, 3);
        if (!note)
            continue;                                   // the result row
        const int live = yourMoveInRow(row);
        const QString text = live >= 0 ? QString::fromStdString(moves[live]->yourNote) : QString();
        note->setText(text);
        note->setToolTip(text);                         // the whole note, when the column cuts it short
        note->setFlags(live >= 0 && notesAllowed() ? Qt::ItemIsEnabled | Qt::ItemIsEditable : Qt::ItemIsEnabled);
    }
}

// Your move in a row of the move list: its index among the record's live moves, or -1 (none yet, or the opponent's
// only: a row holds one move of each side, so at most one of yours).
int MainWindow::yourMoveInRow(int row) const {
    const std::vector<const MoveEntry *> moves = m_record.liveMoves();
    const int bf = m_game->blackFirst() ? 1 : 0;
    for (int i : {2 * row - bf, 2 * row + 1 - bf})
        if (i >= 0 && i < int(moves.size()) && moves[i]->human)
            return i;
    return -1;
}

// Notes can be typed (movelist_design.md 5): not in Tournament while the game is on (the rules allow only the moves);
// a loaded game only when its file can take them back (one game, one of ours: m_loadedFile).
bool MainWindow::notesAllowed() const {
    if (m_loaded)
        return !m_loadedFile.isEmpty();
    return m_level != 0 || m_gameOver;
}

// =================================================================================================================
// 3. NAVIGATION: actions (keys + buttons), the column beside the move list
// =================================================================================================================
void MainWindow::setupNavigation()
{
    auto *backAction = new QAction("◀", this);
    backAction->setToolTip("Back (←)");
    backAction->setShortcut(Qt::Key_Left);
    addAction(backAction);                     // an action's shortcut only works if a widget holds the action
    connect(backAction, &QAction::triggered, m_game, &Game::viewBack);

    auto *forwardAction = new QAction("▶", this);
    forwardAction->setToolTip("Forward (→)");
    forwardAction->setShortcut(Qt::Key_Right);
    addAction(forwardAction);
    connect(forwardAction, &QAction::triggered, m_game, &Game::viewForward);

    auto *startAction = new QAction("|◀", this);
    startAction->setToolTip("Start position (Home)");
    startAction->setShortcut(Qt::Key_Home);
    addAction(startAction);
    connect(startAction, &QAction::triggered, m_game, &Game::viewStart);

    m_endAction = new QAction("▶|", this);
    m_endAction->setToolTip("Back to the game (End, Esc)");
    m_endAction->setShortcuts({QKeySequence(Qt::Key_End), QKeySequence(Qt::Key_Escape)});   // two keys, one action
    addAction(m_endAction);
    connect(m_endAction, &QAction::triggered, m_game, &Game::viewEnd);
    // Esc / End also puts the threat away: "back to the plain board". (Not a second Esc shortcut on the threat
    // action: two actions with one shortcut = "ambiguous", and Qt runs neither.)
    connect(m_endAction, &QAction::triggered, this, [this]() { setThreatShown(false); });

    // --- The navigation column beside the move list ----------------------------------------------------------------
    // The move list with the navigation buttons in a column on its right (saves a row of height).
    auto *navColumn = new QVBoxLayout;
    for (QAction *action : {startAction, backAction, forwardAction, m_endAction}) {
        auto *button = new QToolButton;
        button->setDefaultAction(action);
        navColumn->addWidget(button);
    }
    navColumn->addStretch();                       // the buttons at the top, next to the first moves
    auto *listRow = new QHBoxLayout;
    listRow->addWidget(m_moveList, 1);             // the list takes the width
    listRow->addLayout(navColumn);
    m_panelLayout->insertLayout(1, listRow, 1);    // after the opponent's strip; it takes all spare height
}

// =================================================================================================================
// 4. STATUS BAR: result row, status text
// =================================================================================================================
void MainWindow::setupStatusBar()
{
    // The result as the move list's last row. A takeback or a new game rebuilds the list without it.
    connect(m_game, &Game::gameOver, m_moveList, [this](const QString &text) {
        m_resultText = text;                         // a row of its own, across the table (updateMoveList)
        updateMoveList();
    });

    // The status text: left in the status bar, before the eval words.
    auto *statusText = new QLabel;
    statusBar()->insertWidget(0, statusText);      // index 0: always the leftmost
    connect(m_game, &Game::statusChanged, statusText, &QLabel::setText);
    connect(m_game, &Game::gameOver, m_evalWords, &QLabel::clear, Qt::QueuedConnection);

    // The game-end banner over the board (release_design.md 6): the game's words, the result and the game's length,
    // edged in the outcome's colour for you (the eval bar's blue for a win).
    connect(m_game, &Game::gameOver, m_board, [this](const QString &text, const QString &result) {
        if (result == "*")
            return;                                 // not a finished game
        const bool draw = (result == "1/2-1/2");
        const bool won = !draw && ((result == "1-0") == m_game->humanIsWhite());
        const QColor tone = draw ? QColor(140, 140, 140) : won ? QColor(70, 130, 180) : QColor(190, 70, 70);
        const int plies = int(m_game->positions().size()) - 1;                  // [0] is the start position
        const int moves = (plies + (m_game->blackFirst() ? 1 : 0) + 1) / 2;    // full moves, as the move list counts them
        QString detail = draw ? "½-½" : result;
        if (plies > 0)                              // a pasted, already finished position: no moves to count
            detail += QString(" after %1 move%2").arg(moves).arg(moves == 1 ? "" : "s");
        m_board->setBanner(text, detail, tone);
    });

    // ... and gone again: a new game (also a loaded one), a takeback, or browsing back to an earlier move.
    auto clearBanner = [this]() { m_board->setBanner(QString()); };
    connect(m_game, &Game::newGameStarted, m_board, clearBanner);
    connect(m_game, &Game::movesTakenBack, m_board, clearBanner);   // the signal's (int plies) isn't needed
    connect(m_game, &Game::viewChanged, m_board, [this](int ply) {
        if (ply != -1)                              // -1 = the live position: the banner may stay there
            m_board->setBanner(QString());
    });
}

// =================================================================================================================
// 5. MODE STATUS: right side of the status bar, always visible. Built from the current settings.
// =================================================================================================================
void MainWindow::setupModeStatus()
{
    m_modeStatus = new QLabel;
    m_modeStatus->setToolTip("The coaching level and what is switched on, and the clock setting.");
    statusBar()->addPermanentWidget(m_modeStatus);   // right side; temporary messages never cover it
    connect(m_guidanceAction, &QAction::toggled, this, &MainWindow::updateMode);
    connect(m_evalBarAction, &QAction::toggled, this, &MainWindow::updateMode);
    connect(m_moveColoursAction, &QAction::toggled, this, &MainWindow::updateMode);
    // Made AFTER the hint's own trigger connection (setupHint), so the count is already up when this runs.
    connect(m_hintAction, &QAction::triggered, this, &MainWindow::updateMode);
    // A new game (sidesChanged comes with every restart): the count starts over.
    connect(m_game, &Game::sidesChanged, m_modeStatus, [this]() {
        m_hintsUsed = 0;
        updateMode();
    });
    connect(m_game, &Game::timeControlChanged, this, &MainWindow::updateMode);

    // Connected to each level action AFTER the lambda that sets m_level (createMenus), so the status reads the new
    // level. (Not the group's triggered: the group connected to the actions first, inside addAction.)
    for (QAction *action : m_levelActions)
        connect(action, &QAction::triggered, this, &MainWindow::updateMode);

    updateMode();  // the text at start
}

void MainWindow::updateMode()
{
    QStringList parts{m_level >= 0 ? QString(levelNames[m_level]) : QString("Custom"), m_game->timeControlText()};
    if (m_practiceOn)
        parts.prepend("Opening practice");     // first: what kind of game this is
    parts << (m_guidanceAction->isChecked() ? "Guidance" : "No guidance");
    if (m_evalBarAction->isChecked())
        parts << "Eval bar";
    if (m_moveColoursAction->isChecked())
        parts << "Move colours";
    if (m_hintsUsed > 0)
        parts << QString("%1 hint%2").arg(m_hintsUsed).arg(m_hintsUsed == 1 ? "" : "s");
    m_modeStatus->setText(parts.join(" · "));
}

// =================================================================================================================
// 6. CAPTURED PIECES: into the strips; odd codes are black pieces, taken by White
// =================================================================================================================
void MainWindow::setupCaptures()
{
    connect(m_game, &Game::capturesChanged, m_playerStrip, [this](const QList<int> &captured) {
        static const int value[] = {0, 1, 3, 3, 5, 9, 0};       // by type: (code + 1) / 2 = P N B R Q K
        QList<int> byWhite, byBlack;                            // the pieces each side has taken
        int whitePoints = 0, blackPoints = 0;
        for (int c : captured) {
            if (c % 2 == 1) { byWhite << c; whitePoints += value[(c + 1) / 2]; }   // odd = a black piece
            else            { byBlack << c; blackPoints += value[(c + 1) / 2]; }
        }
        for (PlayerStrip *strip : {m_opponentStrip, m_playerStrip}) {
            if (strip->isWhite())
                strip->setCaptured(byWhite, whitePoints - blackPoints);
            else
                strip->setCaptured(byBlack, blackPoints - whitePoints);
        }
    });
}

// =================================================================================================================
// THE GAME ROW'S STATE (buttons_design.md): enabled while a game is in progress; the arming's hint line
// =================================================================================================================
void MainWindow::setupGameRow() {
    // After setupRecord: the record has counted the move when these run (connection order).
    connect(m_game, &Game::newGameStarted, this, [this]() {
        m_gameOver = false;
        m_tournamentGame = false;               // a new game: the rules come from the level again
        m_illegalAttempts = 0;
        m_paused = false;                       // a new game while paused: it simply starts
        m_drawPending = false;
        m_drawRestUntil = 0;    
        m_board->setVeil(false);
        m_resultText.clear();                   // the move list: no result row
        updateMoveList();
        updateGameRow();
    });
    connect(m_game, &Game::gameOver, this, [this]() {
        m_gameOver = true;
        updateMoveCells();                      // the game is over: every mark
        updateGameRow();
    });
    connect(m_game, &Game::movePlayed, this, &MainWindow::updateGameRow);
    connect(m_game, &Game::movesTakenBack, this, [this]() {
        m_gameOver = false;                     // a takeback after the end: the game goes on
        m_resultText.clear();
        updateMoveList();                       // no result row; back to the coaching's marks
        m_paused = false;                       // a takeback while paused: play goes on (takeback restarts it)
        m_board->setVeil(false);
        updateGameRow();
    });
    for (ArmedButton *button : {m_drawButton, m_resignButton, m_newWhiteButton, m_newBlackButton})
        connect(button, &ArmedButton::armedChanged, m_armHint, [this, button](bool armed) {
            m_armHint->setStyleSheet("color: #c62828;");   // red: a question
            m_armHint->setText(armed ? button->hint() : QString());
        });
    connect(m_resignButton, &ArmedButton::fired, m_game, &Game::resign);
    updateGameRow();
    // Offer draw (buttons_design.md): on your turn; the opponent answers from the master coach's score, waiting
    // for its first word this turn if it hasn't spoken yet. Your move meanwhile withdraws the offer.
    connect(m_game, &Game::humanTurnStarted, this, [this]() {
        m_yourTurn = true;
        updateGameRow();
    });
    connect(m_game, &Game::humanTurnEnded, this, [this]() {
        m_yourTurn = false;
        if (m_drawPending) {
            m_drawPending = false;                  // you played on: the offer is gone
            m_armHint->clear();
        }
        updateGameRow();
    });
    connect(m_drawButton, &ArmedButton::fired, this, [this]() {
        if (!m_master->best().pvMove.empty()) {     // the master has spoken this turn (depth 10+)
            answerDraw(m_master->best());
            return;
        }
        m_drawPending = true;
        m_armHint->setStyleSheet(QString());
        m_armHint->setText(m_engine->shortName() + " is considering your offer…");
        updateGameRow();
    });
    connect(m_master, &MasterCoach::bestMoveChanged, this, [this](const UciInfo &now) {
        if (m_drawPending)
            answerDraw(now);
    });
    // Pause / Resume: the game rests as for the editor (both coaches stop); a veil in timed games only.
    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        m_paused = !m_paused;
        if (m_paused) {
            m_game->pause();
            m_board->setVeil(m_game->isTimed());
        } else {
            m_board->setVeil(false);
            m_game->resume();
        }
        updateGameRow();
    });
    // Play now: the opponent plays at once. Enabled while it thinks: the status changes with every turn.
    connect(m_playNowButton, &QPushButton::clicked, m_game, &Game::playNow);
    connect(m_game, &Game::statusChanged, this, &MainWindow::updateGameRow);

    // Touch-move (Tournament): another piece was clicked while one is touched. Said for 4 s in the status bar.
    connect(m_board, &BoardWidget::touchRefused, this, [this](int touched) {
        static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
        const int code = m_game->positions().last().mailbox[touched];
        const QString square = QString(QChar('a' + touched % 8)) + QString::number(touched / 8 + 1);
        statusBar()->showMessage(QString("Touch-move: you must move the %1 on %2.").arg(names[(code + 1) / 2])
                                 .arg(square), 4000);
    });

    // Illegal attempts (Tournament, release_design.md 8): the first gives the opponent a minute, the second loses.
    connect(m_board, &BoardWidget::illegalAttempt, this, [this]() {
        if (m_level != 0)
            return;                                 // the other levels: the violet mark is all
        if (++m_illegalAttempts == 1) {
            m_game->addOpponentTime(60000);
            statusBar()->showMessage("Illegal move: " + m_engine->shortName()
                                     + " gets a minute. One more loses the game.", 6000);
        } else {
            m_game->lose("A second illegal move: " + m_engine->shortName() + " wins.");
        }
    });
}

void MainWindow::updateGameRow() {
    const bool inProgress = !m_gameOver && m_record.liveCount() > 0;   // a move played, not over
    m_resignButton->setEnabled(inProgress && !m_paused);
    m_resignButton->setArming(inProgress);
    // New asks while a game is in progress, and also before the first move when the game began from a set-up
    // position (pasted, or from the editor): that position would be lost. Nothing to lose otherwise: at once.
    const bool setUp = !m_gameOver && m_game->startFen() != Position::StartFen;
    m_newWhiteButton->setArming(inProgress || setUp);
    m_newBlackButton->setArming(inProgress || setUp);
    m_pauseButton->setEnabled(inProgress && (m_level != 0 || m_paused));   // no Pause in Tournament (level 0), but
                                                                            // Resume always: never a way to lock yourself out
    m_pauseButton->setText(m_paused ? "Resume" : "Pause");
    // Tournament (release_design.md 8): from the first move to the end, no way out and no going back.
    const bool locked = inProgress && m_level == 0;
    if (locked)
        m_tournamentGame = true;                    // remembered until a new game: resigning doesn't open a way back
    m_takebackAction->setEnabled(!m_tournamentGame);
    m_levelGroup->setEnabled(!locked);              // a group enables or disables all its actions at once
    for (QAction *option : {m_guidanceAction, m_evalBarAction, m_moveColoursAction, m_aidsAction, m_hintsSwitch,
                            m_commentsAction, m_warningsAction})
        option->setEnabled(!locked);
    m_opponentStrip->setStrengthEnabled(!locked);
    m_untimedAction->setEnabled(m_level != 0);      // Tournament needs a clock: the other time controls stay free
    m_board->setTouchMove(m_level == 0);            // touch-move: Tournament, from the first touch on
    m_playNowButton->setEnabled(!m_gameOver && !m_paused && m_game->isThinking());
    m_drawButton->setEnabled(inProgress && !m_paused && m_yourTurn && !m_drawPending
                             && m_record.liveCount() >= m_drawRestUntil);
    m_drawButton->setArming(inProgress);
}

// The opponent's own view decides: its win chance (1 - yours, the master's score is in your view) at most 52 %, so
// equal or worse for it: it accepts. Better: it declines, and the button rests for your next 5 moves.
void MainWindow::answerDraw(const UciInfo &master) {
    m_drawPending = false;
    m_armHint->setStyleSheet(QString());
    if (1.0 - winChance(master) <= 0.52) {
        m_armHint->setText(m_engine->shortName() + " accepts: draw.");
        m_game->agreeDraw();
    } else {
        m_armHint->setText(m_engine->shortName() + " declines.");
        m_drawRestUntil = m_record.liveCount() + 10;   // 5 of your moves = 10 plies
    }
    updateGameRow();
}

// The opening names: the table from the resources (data/openings.tsv: lichess chess-openings, CC0).
void MainWindow::setupOpenings() {
    QFile file(":/data/openings.tsv");
    if (!file.open(QIODevice::ReadOnly))
        return;                                // not built in: no names (the strip stays empty)
    m_openings.load(file.readAll().toStdString());
    m_game->setNames(&m_openings);
    QFile bookFile(":/data/book.bin");
    if (bookFile.open(QIODevice::ReadOnly)) {
        m_book.load(bookFile.readAll().toStdString());
        m_game->setBook(&m_book);
    }
    // The strip: the name, and ⏩ at its right end while a practised line can be skipped.
    m_openingStrip = new QWidget;
    auto *stripRow = new QHBoxLayout(m_openingStrip);
    stripRow->setContentsMargins(0, 0, 0, 0);
    m_openingLabel = new QLabel;
    m_openingLabel->setMinimumHeight(m_openingLabel->fontMetrics().height());   // its row is there even when empty
    m_openingLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // a long name never widens the panel
    stripRow->addWidget(m_openingLabel, 1);
    m_skipButton = new QToolButton;
    m_skipButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));   // the style's own ⏩: no font needed
    m_skipButton->setToolTip("Skip to the end of the practised line");
    m_skipButton->setAutoRaise(true);          // flat until the mouse is over it
    m_skipButton->hide();                      // practice only
    stripRow->addWidget(m_skipButton);
    connect(m_skipButton, &QToolButton::clicked, m_game, &Game::skipLine);
    m_panelLayout->insertWidget(1, m_openingStrip);   // after the opponent's strip (0), above the move list
    m_openingStrip->setVisible(m_guidanceAction->isChecked());   // as the switch is now ...
    connect(m_guidanceAction, &QAction::toggled, m_openingStrip, &QWidget::setVisible);   // ... and when it changes
    // Every change to the moves: a move, a takeback, a new game (the list is rebuilt the same way).
    connect(m_game, &Game::sanMovesChanged, this, &MainWindow::updateOpening);
}

// The game's name: the last named position on its path (a game out of book keeps the name it had).
void MainWindow::updateOpening() {
    const QList<Position::Chess> &positions = m_game->positions();
    Position model;
    // Out of book (no book moves in the live position): the name greyed. A disabled label is drawn in the palette's
    // Disabled colours, so the grey is right in light and dark alike.
    m_openingLabel->setEnabled(!m_book.moves(model.Key(positions.last())).empty());
    m_skipButton->setVisible(m_game->canSkipLine());   // practice, still on the line before its end
    for (auto it = positions.rbegin(); it != positions.rend(); ++it) {   // from the newest position back
        if (const auto named = m_openings.find(model.Key(*it))) {
            m_openingLabel->setText(QString::fromStdString(named->name));
            m_openingLabel->setToolTip(QString::fromStdString(named->eco + " " + named->name));
            m_record.info().eco = named->eco;          // the record keeps the game's name (the PGN's ECO/Opening)
            m_record.info().opening = named->name;
            return;
        }
    }
    m_openingLabel->clear();                   // no named position yet (or a set-up position)
    m_openingLabel->setToolTip(QString());
}

// A move from the book gets its share of the games in this position (openings_design.md "A book move in the
// coaching"): after the why, or alone with the move's name when there is no why yet. In practice, while the game is
// inside the opening, also whether the move stays inside it or leaves it (and into what).
QString MainWindow::withBook(const QString &move, const QString &why) {
    const Position::Chess &now = m_game->positions().last();
    Position model;
    int total = 0, games = 0;
    for (const BookMove &m : m_book.moves(model.Key(now))) {
        total += m.games;
        if (QString::fromStdString(m.uci) == move)
            games = m.games;
    }
    QString words;                                 // the book's sentence, without its full stop ("" = nothing to add)
    if (games > 0)
        words = QString("A book move: %1 % of players choose it here").arg(qRound(100.0 * games / total));
    if (m_game->isPractice() && !m_practiceInside.isEmpty()) {
        const QString chosen = shortOpening(m_game->practiceName());
        if (m_practiceInside.contains(move)) {
            if (games > 0)
                words += ", inside the " + chosen;
        } else {
            words += (games > 0 ? ", but it leaves the " : "Not in the book: it leaves the ") + chosen;
            const QString into = m_game->nameAfter(move.toStdString());
            if (!into.isEmpty())
                words += " (into " + shortOpening(into) + ")";
        }
    }
    if (words.isEmpty())
        return why;                                // nothing about the book: the why alone
    if (!why.isEmpty())
        return why + " " + words + ".";
    const auto m = model.UciToMove(now, move.toStdString());
    const QString name = m ? QString::fromStdString(model.MoveToSan(now, *m)) : move;
    words[0] = words[0].toLower();                 // after the move's name: "Nf3: a book move …"
    return name + ": " + words + ".";
}

// "Sicilian Defense: Najdorf Variation, English Attack" → "English Attack": the most specific part of a name.
QString MainWindow::shortOpening(const QString &name) {
    const int cut = std::max(name.lastIndexOf(':'), name.lastIndexOf(','));
    return cut == -1 ? name : name.mid(cut + 1).trimmed();
}


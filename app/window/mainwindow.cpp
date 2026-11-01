// mainwindow.cpp — the constructor (the order of the setup steps), the window, the engines, the start, shutdown.
//
// Sections:
//   1. The constructor: the setup steps, in order
//   2. Shutdown: the engines and the game first
//   3. The window: board area, side panel, status bar font, size
//   4. Engines, coaches and the game
//   5. Start: the engines, the window state

#include "mainwindow.h"
#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QStackedWidget>
#include <QAction>
#include <QMenuBar>
#include <QScreen>
#include "boardarea.h"
#include "boardwidget.h"
#include "engine.h"
#include "evalbar.h"
#include "game.h"
#include "mastercoach.h"
#include "editorpanel.h"
#include "enginepanel.h"
#include "practicepanel.h"
#include "gamespanel.h"
#include "playerstrip.h"


// =================================================================================================================
// 1. THE CONSTRUCTOR: the setup steps, in order
// =================================================================================================================
// The order matters: several signals have more than one listener, and Qt calls them in the order they were
// connected (e.g. the hint sets m_viewing before the threat reads it; a level sets m_level before the mode status
// shows it). And the remembered switches are restored only after everything that reacts to them is connected.
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    buildWindow();              // the window: board area, side panel, status bar font, size
    createEngines();            // engines, coaches and the game; the coaches follow the human's turns
    createMenus();              // menus: Game, Time, Coaching (levels and switches)
    setupHint();                // the hint (H)
    setupThreat();              // the threat (T)
    setupHanging();             // your hanging pieces: the red "!"
    setupFeedback();            // the bar's words, the box colours
    setupSidePanel();           // buttons, player strips, strength slider, geek line, clocks
    setupEngineLog();           // the engine log dock and the View menu
    setupRecord();              // the game record
    setupSaving();              // saving games: autosave, Save game…, Forget last game
    setupGameRow();             // the game buttons' state
    setupMoveList();            // move list: rows, viewing, clicks
    setupCoachLog();            // the coach log dock
    setupNavigation();          // navigation: actions, the column beside the move list
    setupCoachLine();           // the coach line under the move list; the threat's words
    setupGraph();               // the win-chance graph: between the move list and the coach line
    setupOpenings();            // the opening names: the strip above the move list
    setupMissed();              // "Buster missed it"
    setupPlayThrough();         // the play-through (Space)
    setupStatusBar();           // status bar: result row, status text
    setupEvalBar();             // eval bar: fill, preview frame, after your move
    setupModeStatus();          // mode status
    restoreSwitches();          // the remembered Coaching switches: AFTER everything that reacts to them
    setupLevels();              // the levels' presets; Custom on a hand change
    setupEngineLogContents();   // what the engine log shows
    setupCaptures();            // captured pieces
    setupEditor();              // the board editor: in and out of its page
    setupEngines();             // the engine setup: in and out of its page
    setupPractice();            // opening practice: in and out of its page
    setupHelp();                // the Help menu, last; the welcome page on a first run
    start();                    // the engines start (after all listeners are connected); the window state
}

// =================================================================================================================
// 2. SHUTDOWN: the engines and the game first
// =================================================================================================================
// Engine's destructor waits (up to a second) for its process to quit, and an engine may still print a line then:
// it reaches the logs (and Game). So they go FIRST, while every member and widget still exists: Game before its
// engine, each coach before its engine. Otherwise QObject would delete them later, with the other children, after
// MainWindow's own members are already gone.
MainWindow::~MainWindow() {
    delete m_game;
    delete m_master;
    delete m_coach;
    delete m_engine;
    delete m_coachEngine;
    delete m_masterEngine;
}

// =================================================================================================================
// 3. THE WINDOW: board area, side panel, status bar font, size
// =================================================================================================================
// Everything is new-ed with an owner (the window, a layout's widget, a dock), so it is deleted automatically when
// the window dies.
void MainWindow::buildWindow() {
    setWindowTitle(QCoreApplication::applicationName());   // "Buster Coach": set once, in main
    // The board area: the board kept square, the eval bar glued to its left side.
    m_evalBar = new EvalBar;
    m_evalBar->setToolTip("Your chance of winning, as the coach sees it: blue from the bottom is yours, the middle "
                          "line is equal. With a piece selected, the frame shows the chance if you play the move the "
                          "mouse points at. At the end of a game: WON, LOST or DRAW.");
    m_board = new BoardWidget;
    m_area = new BoardArea(m_board, m_evalBar);
    m_area->setMinimumSize(200, 200);          // the board never shrinks below this because of the panel

    // The side panel: a column on the right. A QFrame is a QWidget that can draw a border.
    auto *panel = new QFrame;
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setMinimumWidth(260);               // room for the strips, clocks and buttons
    m_panelLayout = new QVBoxLayout(panel);

    // The window's centre: board area (left) and side panel (right).
    auto *central = new QWidget;
    auto *row = new QHBoxLayout(central);
    row->addWidget(m_area);                    // stretch 0: the board area gets the width the panel leaves
    m_panelStack = new QStackedWidget;         // the side panel's pages, one shown at a time
    m_panelStack->addWidget(panel);            // page 0: play
    m_editor = new EditorPanel;
    m_panelStack->addWidget(m_editor);         // page 1: the board editor
    m_engines = new EnginePanel;
    m_panelStack->addWidget(m_engines);        // page 2: the engines
    m_practice = new PracticePanel;
    m_panelStack->addWidget(m_practice);       // page 3: opening practice
    m_gamesPanel = new GamesPanel;
    m_panelStack->addWidget(m_gamesPanel);     // page 4: open a game

    // The panel takes the spare width first, up to ~60 characters (the cards and the move list wrap less); beyond
    // that the board area gets the rest (layout_design.md "Sizes and spare space").
    m_panelStack->setMaximumWidth(60 * fontMetrics().averageCharWidth());
    row->addWidget(m_panelStack, 1);
    setCentralWidget(central);

    QFont statusFont = statusBar()->font();
    statusFont.setPointSize(statusFont.pointSize() + 3);   // 3 points larger than the default
    statusBar()->setFont(statusFont);                      // inherited by every label in the bar

    // The size and place it was left at; a first run (nothing saved): 80 % of the screen's free area, centred.
    if (!restoreGeometry(QSettings().value("window/geometry").toByteArray())) {
        const QRect free = screen()->availableGeometry();   // the screen minus the taskbars and panels
        resize(free.size() * 0.8);
        move(free.center() - rect().center());
    }
}

// The side panel's other pages (the board editor, the engines): the game rests while one is shown, and nothing that
// could touch it works (the menus, their shortcuts, the window's own keys); the bar's place stays empty.
void MainWindow::showPage(QWidget *page) {
    m_game->pause();
    menuBar()->setEnabled(false);                // its menus' shortcuts too (Ctrl+N, H, T, ...)
    for (QAction *action : actions())            // the window's own keys: Space, the arrows, Home, End/Esc
        action->setEnabled(false);
    m_area->setBarVisible(false);
    m_board->setBanner(QString());               // the game-end banner belongs to the game, not to the page's board
    m_panelStack->setCurrentWidget(page);
}

void MainWindow::showPlay() {
    m_panelStack->setCurrentIndex(0);            // 0 = play
    m_area->setBarVisible(m_evalBarAction->isChecked());
    for (QAction *action : actions())
        action->setEnabled(true);
    menuBar()->setEnabled(true);
}


// =================================================================================================================
// 4. ENGINES, COACHES AND THE GAME
// =================================================================================================================
void MainWindow::createEngines() {
    m_engine = new Engine(this);                         // the opponent. Its destructor sends "quit"
    m_coachEngine = new Engine(this);                    // a second, independent engine process: the coach
    m_coach = new Coach(m_coachEngine, this);
    m_masterEngine = new Engine(this);                   // the third engine (and the last): the master coach
    m_master = new MasterCoach(m_masterEngine, this);
    m_game = new Game(m_board, m_engine, this);

    // --- The coaches follow the human's turns ----------------------------------------------------------------------
    connect(m_game, &Game::humanTurnStarted, m_coach, &Coach::onHumanTurnStarted);
    connect(m_game, &Game::humanTurnEnded, m_coach, &Coach::cancel);
    connect(m_game, &Game::opponentTurnStarted, m_coach, &Coach::onOpponentTurnStarted);   // its moves, for its pick
    connect(m_coach, &Coach::opponentTableReady, m_game, [this]() { m_game->setOpponentTable(m_coach->opponentTable()); });
    connect(m_board, &BoardWidget::boxMoveChanged, m_coach, &Coach::setAttention);
    connect(m_game, &Game::humanTurnStarted, m_master, &MasterCoach::onHumanTurnStarted);
    connect(m_game, &Game::humanTurnEnded, m_master, &MasterCoach::cancel);
    connect(m_master, &MasterCoach::bestMoveChanged, m_coach, &Coach::setMasterMove);
    // The threat words name the opponent ("Stockfish threatens ..."): its engine tells its name in the handshake.
    connect(m_engine, &Engine::ready, m_coach, [this]() { m_coach->setOpponentName(m_engine->shortName()); });

    // A new game: the coaches' engines forget the old one too (ucinewgame clears their hash tables).
    connect(m_game, &Game::newGameStarted, m_coachEngine, [this]() { m_coachEngine->send("ucinewgame"); });
    connect(m_game, &Game::newGameStarted, m_masterEngine, [this]() { m_masterEngine->send("ucinewgame"); });
}

// =================================================================================================================
// 5. START: the engines, the window state
// =================================================================================================================
void MainWindow::start() {
    // Start the engines only after all listeners are connected, and shortly after the event loop runs: the delay
    // gives the window manager time to place the main window, so an error pop-up (a crashed engine) appears over it,
    // not on another screen (0 ms was not enough on a two-monitor desktop). No engine chosen, or its file gone (R1
    // fixed): the engine page instead, and the engines start on its OK.
    QTimer::singleShot(500, m_engine, [this]() {
        const QString opponent = savedEngine("engines/opponent");
        const QString coach = savedEngine("engines/coach");
        if (!QFileInfo(opponent).isExecutable() || !QFileInfo(coach).isExecutable()) {   // R1 gone: no fixed path
            m_engines->open(opponent.isEmpty() && coach.isEmpty()
                                ? "Welcome! Choose an engine to play against and one to coach you: Add… finds "
                                  "them on disk."
                                : "An engine can't be found (moved or deleted?): choose again.");
            showPage(m_engines);
            return;                                   // the engines start on OK (engines.cpp)
        }
        m_opponentPath = opponent;
        m_opponentStrip->setRatingsShown(m_opponentPath == bundledEngine());   // the ratings: measured with it only
        m_coachPath = coach;
        m_engine->start(m_opponentPath);
        m_coachEngine->start(m_coachPath);    // the coach: its own process
        m_masterEngine->start(m_coachPath);   // the master coach: the same file, another process
    });

    // The window as it was left: the docks (open or not, where), then saved again when the app quits.
    restoreState(QSettings().value("window/state").toByteArray());
    connect(qApp, &QApplication::aboutToQuit, this, [this]() {
        QSettings settings;
        settings.setValue("window/geometry", saveGeometry());
        settings.setValue("window/state", saveState());
    });
}

// A role's engine file as last chosen on the engine page; never chosen: the built-in Buster ("" = none of them).
QString MainWindow::savedEngine(const char *key) {
    const QString chosen = QSettings().value(key).toString();
    return chosen.isEmpty() ? bundledEngine() : chosen;
}

// The engine that ships with the app (release_design.md 12): Buster, in the app's own folder. "" = not there (a
// build without it, or removed).
QString MainWindow::bundledEngine() {
#ifdef Q_OS_WIN
    const QString path = QCoreApplication::applicationDirPath() + "/buster.exe";
#else
    const QString path = QCoreApplication::applicationDirPath() + "/buster";
#endif
    return QFileInfo(path).isExecutable() ? path : QString();
}




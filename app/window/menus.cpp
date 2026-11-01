// menus.cpp — the Game, Time and Coaching menus; the remembered Coaching switches; the levels' presets.
//
// Sections:
//   1. Menus: Game, Time, Coaching (levels, then the switches)
//   2. The remembered Coaching switches (QSettings)
//   3. The levels: presets, Custom on a hand change

#include "mainwindow.h"
#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QEvent>
#include "boardarea.h"
#include "boardwidget.h"
#include "game.h"
#include "theme.h"
#include "piecepainter.h"

// =================================================================================================================
// 1. MENUS: Game, Time, Coaching
// =================================================================================================================
void MainWindow::createMenus() {
    // --- Game menu -------------------------------------------------------------------------------------------------
    QMenu *gameMenu = menuBar()->addMenu("&Game");

    m_newWhite = gameMenu->addAction("New game as &White");
    m_newWhite->setShortcut(QKeySequence::New);              // Ctrl+N
    connect(m_newWhite, &QAction::triggered, m_game, [this]() { m_game->newGame(true); });

    m_newBlack = gameMenu->addAction("New game as &Black");
    connect(m_newBlack, &QAction::triggered, m_game, [this]() { m_game->newGame(false); });

    m_newWhite->setIconText("New");      // the short text a button shows (the menu keeps the long one)
    m_newBlack->setIconText("New");
    updateNewIcons();                    // the kings as icons: text glyphs swap colours on a dark palette

    QAction *copyFen = gameMenu->addAction("&Copy FEN");
    connect(copyFen, &QAction::triggered, m_game, [this]() {
        QGuiApplication::clipboard()->setText(m_game->currentFen());
    });

    QAction *pasteFen = gameMenu->addAction("&Paste FEN");
    connect(pasteFen, &QAction::triggered, m_game, [this]() {
        if (!m_game->newGameFromFen(QGuiApplication::clipboard()->text()))
            QMessageBox::warning(this, "Paste FEN", "The clipboard doesn't contain a valid FEN.");
    });

    m_setupAction = gameMenu->addAction("&Set up position…");   // the board editor (editor_design.md)
    m_practiceAction = gameMenu->addAction("&Practise an opening…");   // openings_design.md "Opening practice"

    m_openAction = gameMenu->addAction("&Open game…");           // pgn_design.md; wired in saving.cpp
    m_openAction->setShortcut(QKeySequence::Open);                // Ctrl+O

    m_playFromWhite = gameMenu->addAction("Play from here as &White");   // pgn_design.md step 6; saving.cpp
    m_playFromBlack = gameMenu->addAction("Play from here as &Black");

    m_saveAction = gameMenu->addAction("&Save game…");           // pgn_design.md; wired in saving.cpp
    m_saveAction->setShortcut(QKeySequence::Save);                // Ctrl+S
    m_forgetAction = gameMenu->addAction("&Forget last game");   // the newest autosaved game, to the trash

    m_takebackAction = gameMenu->addAction("&Take back");
    m_takebackAction->setToolTip("Take back your last move (Ctrl+Z). Not in Tournament.");   // the panel's button shows it
    m_takebackAction->setShortcut(QKeySequence::Undo);   // Ctrl+Z
    connect(m_takebackAction, &QAction::triggered, m_game, &Game::takeback);

    gameMenu->addSeparator();

    QAction *quitAction = gameMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);  // Ctrl+Q on Linux
    connect(quitAction, &QAction::triggered, this, &QWidget::close);


    // --- Engine menu (engine_design.md) ----------------------------------------------------------------------------
    QMenu *engineMenu = menuBar()->addMenu("&Engine");
    m_enginesAction = engineMenu->addAction("&Engines…");


    // --- Time menu -------------------------------------------------------------------------------------------------
    // The time control as presets, one checked. Applies at once before the first move, otherwise from the next
    // new game.
    QMenu *timeMenu = menuBar()->addMenu("&Time");
    auto *timeGroup = new QActionGroup(this);   // exclusive: one checked at a time (radio buttons)
    struct Preset { const char *text; int minutes; int increment; };   // minutes 0 = untimed
    const Preset presets[] = {{"&Untimed", 0, 0}, {"1 + 0", 1, 0}, {"3 + 2", 3, 2}, {"5 + 3", 5, 3},
                              {"10 + 5", 10, 5}, {"15 + 10", 15, 10}, {"30 + 0", 30, 0}};
    // The last choice is remembered (a first run: untimed, the decided default).
    const int savedMinutes = QSettings().value("time/minutes", 0).toInt();
    const int savedIncrement = QSettings().value("time/increment", 0).toInt();
    for (const Preset &p : presets) {
        QAction *action = timeMenu->addAction(p.text);
        action->setCheckable(true);
        timeGroup->addAction(action);
        if (p.minutes == 0)
            m_untimedAction = action;
        if (p.minutes == 15 && p.increment == 10)
            m_tournamentTime = action;
        if (p.minutes == savedMinutes && p.increment == savedIncrement) {
            action->setChecked(true);
            m_game->setTimeControl(p.minutes > 0, p.minutes * 60000LL, p.increment * 1000LL);   // no move yet: at once
        }
        connect(action, &QAction::triggered, m_game, [this, p]() {   // p by value: the loop's struct is gone later
            m_game->setTimeControl(p.minutes > 0, p.minutes * 60000LL, p.increment * 1000LL);
            QSettings settings;                    // one object for two values
            settings.setValue("time/minutes", p.minutes);
            settings.setValue("time/increment", p.increment);
        });
        if (p.minutes == 0)
            timeMenu->addSeparator();              // untimed, then the clocks
    }

    // --- Coaching menu: the levels ---------------------------------------------------------------------------------
    // How much the board helps the player. The level comes first (one choice sets all the help; coach_design.md
    // "Switches and levels"), then the individual switches for fine-tuning, then what you ask for (H, T).
    m_coachingMenu = menuBar()->addMenu("&Coaching");
    m_levelGroup = new QActionGroup(this);
    m_levelGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);   // none checked = Custom
    for (const char *name : levelNames) {
        QAction *action = m_coachingMenu->addAction(name);
        action->setCheckable(true);
        m_levelGroup->addAction(action);
        m_levelActions << action;
    }
    m_coachingMenu->addSeparator();                  // the levels above, the switches below
    m_level = QSettings().value("coaching/level", 3).toInt();   // -1 = Custom; a first run: Full coaching
    if (m_level >= 0 && m_level < m_levelActions.size())
        m_levelActions[m_level]->setChecked(true);
    for (int i = 0; i < m_levelActions.size(); ++i) {
        connect(m_levelActions[i], &QAction::triggered, this, [this, i]() {
            m_level = i;
            QSettings().setValue("coaching/level", i);
            if (i == 0 && m_untimedAction->isChecked())
                m_tournamentTime->trigger();          // Tournament needs a clock (release_design.md 8): 15 + 10
            updateGameRow();                          // AFTER m_level is set: the level decides Pause (and the locks)
        });
    }

    // --- Coaching menu: the switches -------------------------------------------------------------------------------
    m_guidanceAction = m_coachingMenu->addAction("&Guidance");
    m_guidanceAction->setToolTip("Guidance: only your movable pieces glow, the box appears only on legal moves, and "
                                 "the last move and check are marked.");
    m_guidanceAction->setCheckable(true);     // shows a tick; each click flips it
    m_guidanceAction->setChecked(true);       // on at start, matching the board's default
    connect(m_guidanceAction, &QAction::toggled, m_board, &BoardWidget::setGuidance);

    m_evalBarAction = m_coachingMenu->addAction("&Evaluation bar");
    m_evalBarAction->setCheckable(true);
    m_evalBarAction->setChecked(true);
    connect(m_evalBarAction, &QAction::toggled, m_area, &BoardArea::setBarVisible);

    m_moveColoursAction = m_coachingMenu->addAction("&Move colours");
    m_moveColoursAction->setCheckable(true);
    m_moveColoursAction->setChecked(true);

    m_aidsAction = m_coachingMenu->addAction("&Thinking aids");   // linger: verdicts, flash, line; Space; T
    m_aidsAction->setCheckable(true);
    m_aidsAction->setChecked(true);

    m_hintsSwitch = m_coachingMenu->addAction("Hin&ts allowed");   // the Hint action (H) needs it on
    m_hintsSwitch->setCheckable(true);
    m_hintsSwitch->setChecked(true);

    m_commentsAction = m_coachingMenu->addAction("&Comments after moves");   // "Buster missed it", later warnings
    m_commentsAction->setCheckable(true);
    m_commentsAction->setChecked(true);

    m_warningsAction = m_coachingMenu->addAction("&Warnings by themselves");   // the coach warns unasked: T ticked by itself, the "!" marks
    m_warningsAction->setCheckable(true);
    m_warningsAction->setChecked(true);
    // (All on at start = Full coaching, a first run's level. Remembered values are restored later.)

    // --- Settings menu: personal settings, not a level's (coach_design.md "Personal settings") -------------------
    QMenu *settingsMenu = menuBar()->addMenu("&Settings");
    m_marksAction = settingsMenu->addAction("Colour-blind &marks");   // ▲ ● ▼ marks on the box, besides the colours
    m_marksAction->setCheckable(true);
    m_marksAction->setChecked(false);          // off by default; remembered (restoreSwitches: key coaching/marks kept)

    // The linger time: how long the mouse must stay on a square before the coach answers (both levels).
    QMenu *lingerMenu = settingsMenu->addMenu("&Linger time");
    auto *lingerGroup = new QActionGroup(this);   // exclusive: one checked at a time
    const int savedLinger = QSettings().value("personal/lingerMs", 1500).toInt();   // a first run: 1.5 s
    for (int ms : {1000, 1500, 2000, 3000}) {
        QAction *action = lingerMenu->addAction(QString("%1 s").arg(ms / 1000.0));   // "1 s", "1.5 s", ...
        action->setCheckable(true);
        lingerGroup->addAction(action);
        if (ms == savedLinger) {
            action->setChecked(true);
            m_board->setLingerTime(ms);           // the remembered time, at start
        }
        connect(action, &QAction::triggered, m_board, [this, ms]() {   // ms by value: the loop variable is gone later
            m_board->setLingerTime(ms);
            QSettings().setValue("personal/lingerMs", ms);
        });
    }

    // The appearance: light or dark (theme.cpp). Applied at start in main(), before the window is built; here it
    // switches while the program runs (setPalette repaints every widget).
    QMenu *appearanceMenu = settingsMenu->addMenu("&Appearance");
    auto *themeGroup = new QActionGroup(this);   // exclusive, like the linger time
    const bool savedDark = QSettings().value("personal/theme", "light").toString() == "dark";
    for (bool dark : {false, true}) {
        QAction *action = appearanceMenu->addAction(dark ? "&Dark" : "&Light");
        action->setCheckable(true);
        action->setChecked(dark == savedDark);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, dark]() {
            applyTheme(dark);
            applyTitleBar(this, dark);
            QSettings().setValue("personal/theme", dark ? "dark" : "light");
        });
    }

    // Saving games by themselves (pgn_design.md): on by default, remembered; off for test sessions.
    m_autosaveAction = settingsMenu->addAction("Save games &automatically");
    m_autosaveAction->setCheckable(true);
    m_autosaveAction->setChecked(QSettings().value("personal/autosave", true).toBool());
    connect(m_autosaveAction, &QAction::toggled, this, [](bool on) { QSettings().setValue("personal/autosave", on); });

}

void MainWindow::updateNewIcons() {
    m_newWhite->setIcon(QIcon(piecePixmap(12, 32, panelRim(palette(), 12))));   // drawn at 32 px: sharp when scaled
    m_newBlack->setIcon(QIcon(piecePixmap(11, 32, panelRim(palette(), 11))));
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        updateNewIcons();
        updateCoachLine();                          // the cards' edge colours follow the theme
    }
}


// =================================================================================================================
// 2. THE REMEMBERED COACHING SWITCHES (QSettings)
// =================================================================================================================
// Called AFTER all the connections that react to the switches, so restoring one (setChecked -> toggled) also hides
// the bar, sets the guidance, updates the mode status ... A first run: the defaults set where the actions were made.
// (The strength level and the time control are remembered where they are created: panel.cpp and above.)
void MainWindow::restoreSwitches()
{
    const std::pair<QAction *, const char *> remembered[] = {
        {m_guidanceAction, "coaching/guidance"},       {m_evalBarAction, "coaching/evalBar"},
        {m_moveColoursAction, "coaching/moveColours"}, {m_marksAction, "coaching/marks"},
        {m_aidsAction, "coaching/aids"},               {m_hintsSwitch, "coaching/hints"},
        {m_commentsAction, "coaching/notes"},             {m_warningsAction, "coaching/threatEachTurn"},
    };
    for (const auto &[action, key] : remembered) {
        action->setChecked(QSettings().value(key, action->isChecked()).toBool());   // fallback: the default
        connect(action, &QAction::toggled, action, [key](bool on) { QSettings().setValue(key, on); });
    }
}

// =================================================================================================================
// 3. THE LEVELS: presets, Custom on a hand change
// =================================================================================================================
// One level sets its preset's switches (MainWindow::levelPresets, in mainwindow.h).
void MainWindow::applyLevel(int level)
{
    m_guidanceAction->setChecked(levelPresets[level][0]);
    m_evalBarAction->setChecked(levelPresets[level][1]);
    m_moveColoursAction->setChecked(levelPresets[level][2]);
    m_aidsAction->setChecked(levelPresets[level][3]);
    m_hintsSwitch->setChecked(levelPresets[level][4]);
    m_commentsAction->setChecked(levelPresets[level][5]);
    m_warningsAction->setChecked(levelPresets[level][6]);
}

// Called AFTER all connections, like restoreSwitches (the presets go through setChecked -> toggled too).
void MainWindow::setupLevels()
{
    for (int i = 0; i < m_levelActions.size(); ++i)
        connect(m_levelActions[i], &QAction::triggered, this, [this, i]() { applyLevel(i); });
    if (m_level >= 0)
        applyLevel(m_level);     // at start: the level's switches (Custom keeps the remembered ones)

    // A switch changed BY HAND (triggered: only user actions, never setChecked) away from the level's preset: the
    // level becomes Custom (no level ticked); the remembered switches count from now on. Back to a level: choose it.
    const QList<QAction *> levelSwitches = {m_guidanceAction, m_evalBarAction, m_moveColoursAction, m_aidsAction,
                                            m_hintsSwitch, m_commentsAction, m_warningsAction};   // = the columns, in order
    for (int s = 0; s < levelSwitches.size(); ++s) {
        connect(levelSwitches[s], &QAction::triggered, this, [this, s](bool on) {
            if (m_level < 0 || on == levelPresets[m_level][s])
                return;                               // Custom already, or it still matches the level
            m_level = -1;
            if (QAction *ticked = m_levelGroup->checkedAction())
                ticked->setChecked(false);            // ExclusiveOptional: none ticked = Custom
            QSettings().setValue("coaching/level", -1);
            updateMode();
            updateGameRow();
        });
    }
}

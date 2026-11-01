// mainwindow.h — the main window: builds the window and wires the parts together.
//
// BoardWidget and Engine don't know each other. Game connects them and owns the game flow; MainWindow creates the
// parts and connects their signals to its member functions and to lambdas. Many connections depend on their ORDER
// (noted where it matters), so the constructor runs the setup steps in a fixed order (mainwindow.cpp).
//
// The member functions live in several files, by area:
//   mainwindow.cpp   the constructor (the order), the window, the engines, the start, shutdown
//   menus.cpp        the Game, Time and Coaching menus; the remembered switches; the levels
//   hint_threat.cpp  the hint (H) and the threat (T)
//   coaching.cpp     the box colours, the game record, the coach line, "Buster missed it", the play-through,
//                    the eval bar
//   panel.cpp        the side panel, the move list, navigation, the status bar, the mode status, captures
//   logs.cpp         the engine log and the coach log
//   editor.cpp       the board editor: in and out of its page
//   engines.cpp      the engine setup: in and out of its page
//   practice.cpp     opening practice: in and out of its page
//   saving.cpp       saving games as PGN: autosave, Save game…, Forget last game
//   help.cpp         Help → How the coach works: the built-in pages in a window of their own




#pragma once
#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include "coach.h"        // Coach::LineStep (the play-through)
#include "gamerecord.h"   // GameRecord, a member
#include "openings.h"     // OpeningNames, a member
#include "uci_info.h"     // UciInfo, MoveQuality

class QAction;
class QActionGroup;
class QLabel;
class QTableWidget;
class QMenu;
class QPlainTextEdit;
class QVBoxLayout;
class BoardArea;
class BoardWidget;
class Engine;
class EvalBar;
class Game;
class MasterCoach;
class PlayerStrip;
class ConfidenceDot;
class EditorPanel;
class QStackedWidget;
class EnginePanel;
class PracticePanel;
class WinGraph;
class ArmedButton;
class QPushButton;
class QToolButton;
class GamesPanel;
struct GameListEntry;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // --- The coaching levels (coach_design.md "Switches and levels") ----------------------------------------------
    static constexpr const char *levelNames[4] = {"Tournament", "Play", "Guided", "Full coaching"};
    // The levels' presets, one column per switch: guidance, eval bar, move colours, thinking aids, hints allowed,
    // comments after moves, warnings by themselves. Colour-blind marks stay a personal preference.
    static constexpr bool levelPresets[4][7] = {
        {false, false, false, false, false, false, false},    // Tournament
        {false, false, false, false, true,  true,  false},    // Play
        {true,  true,  false, true,  true,  true,  false},    // Guided
        {true,  true,  true,  true,  true,  true,  true },    // Full coaching
    };

protected:
    // The palette changed (light / dark): the pictures drawn once are drawn again (the New buttons' kings).
    void changeEvent(QEvent *event) override;


private:
    // --- The setup steps, in the constructor's order ----------------------------------------------------------------
    void buildWindow();             // mainwindow.cpp
    void createEngines();
    void createMenus();             // menus.cpp
    void setupHint();               // hint_threat.cpp
    void setupThreat();
    void setupHanging();            // your hanging pieces: the red "!" (hint_threat.cpp)
    void setupFeedback();           // coaching.cpp
    void setupSidePanel();          // panel.cpp
    void setupEngineLog();          // logs.cpp
    void setupRecord();             // coaching.cpp
    void setupSaving();             // saving.cpp (after setupRecord: the record has the result first)
    void setupGameRow();            // the game buttons' state (panel.cpp; after the record: it counts the moves)
    void setupMoveList();           // panel.cpp
    void setupCoachLog();           // logs.cpp
    void setupNavigation();         // panel.cpp
    void setupCoachLine();          // coaching.cpp
    void setupGraph();              // the win-chance graph between the move list and the coach line (coaching.cpp)
    void setupOpenings();           // the opening names and the book; the strip above the move list (panel.cpp)
    void setupMissed();
    void setupPlayThrough();
    void setupStatusBar();          // panel.cpp
    void setupEvalBar();            // coaching.cpp
    void setupModeStatus();         // panel.cpp
    void restoreSwitches();         // menus.cpp
    void setupLevels();
    void setupEngineLogContents();  // logs.cpp
    void setupCaptures();           // panel.cpp
    void setupEditor();             // editor.cpp
    void setupEngines();            // engines.cpp
    void setupPractice();           // practice.cpp
    void setupHelp();               // help.cpp: the Help menu (the last one); the welcome page on the first run
    void start();                   // mainwindow.cpp

    // --- Updates, called from several connections -------------------------------------------------------------------
    QString hintMove() const;       // the move H shows: the master's, or the opening's in practice
    void updateHint();              // the hint on the board and H's state, from the hint step and the master
    void setThreatShown(bool on);   // the threat's flash on or off (on: its words for the turn too)
    void updateThreat();            // the threat on the board and T's state
    void updateHangingMarks();      // the "!" marks: your hanging pieces, minus the threat's target while its flash shows
    QString hangingWords(int square) const;   // "Your queen on d4 is attacked by ..." ("" = not hanging)
    QString checkWords() const;               // in check: who gives it ("Now: check from ..."; "" = not in check)
    void updateMoveColours();       // the box colours, marks, piece verdicts, linger replies; the turn's snapshots
    void updateCoachLine();         // the coach line: its rows joined (the position, the square, the move, the line)
    bool isOnlyMove(const QString &uci) const;   // your move gets a "!": the only good one (movelist_design.md 3)
    void updateMoveList();          // the move list's rows from Game (waits while a note is typed; movelist_design.md)
    void updateMoveCells();         // its cells from the record: the marks, the coach's comments, your notes
    int yourMoveInRow(int row) const;   // the live index of your move in a move list row; -1 = none (yet)
    bool notesAllowed() const;      // can a note be typed now? (not in Tournament mid-game; a loaded game: its file)
    void saveNotes();               // a note changed: after the game its file is written again (saving.cpp)
    void updateGraph();             // the graph from the record: the curve and your red moves
    void updateOpening();           // the opening strip: the last named position on the game's path
    QString withBook(const QString &move, const QString &why);   // the why, plus the book's share if it's a book move
    static QString shortOpening(const QString &name);   // the most specific part: "…: Najdorf Variation" → "Najdorf Variation"
    void updateGameRow();           // the game buttons: enabled by the game's state (buttons_design.md)
    void answerDraw(const UciInfo &master);   // the opponent answers your draw offer from the master's score
    void stopPlay();                // ends a running play-through
    double bestChance() const;      // the best win chance in the general coach's table
    void updateMode();              // the mode status text
    void applyLevel(int level);     // sets the switches to a level's preset
    void updateNewIcons();          // the New actions' kings: our own pieces, with the palette's rim
    void showPage(QWidget *page);   // a side-panel page instead of play: the game rests, menus and keys off
    void showPlay();                // back to play: menus, keys and the bar as before (the game: the caller decides)
    void showHelp();                // the help window (help.cpp), made on first use
    static QString savedEngine(const char *key);   // a role's engine file from QSettings (engines/opponent, engines/coach)
    static QString bundledEngine();                // the Buster beside the app ("" = not there)
    static QString gamesFolder();   // the autosave folder: ~/Documents/Buster Coach
    bool saveGame(const QString &path);   // the record as PGN into a file
    void autosave();                // this game into its file in the month's folder (10+ plies)
    void saveAbandoned();           // the game being left, if unfinished (result "*"): autosaved
    void openGame(const QString &path, int index = 0);   // a game from a PGN file onto the board, for browsing
    QList<GameListEntry> monthGames(const QString &month);   // a month folder's games as list lines, newest first
    QList<GameListEntry> fileGames(const QString &path);     // every game in one file as list lines
    void showLoadedMove(int ply);              // browsing a loaded game: the viewed move's value and comment
    void playFromHere(bool humanIsWhite);      // the loaded game goes on from the viewed move, as your game

    // --- The window's parts -----------------------------------------------------------------------------------------
    BoardWidget *m_board = nullptr;
    EvalBar *m_evalBar = nullptr;
    BoardArea *m_area = nullptr;
    QVBoxLayout *m_panelLayout = nullptr;      // the side panel's column
    QPushButton *m_pauseButton = nullptr;      // the game row (buttons_design.md)
    QPushButton *m_playNowButton = nullptr;
    ArmedButton *m_drawButton = nullptr;
    ArmedButton *m_resignButton = nullptr;
    ArmedButton *m_newWhiteButton = nullptr;   // New ♔ / New ♚: arm too (a new game abandons the current one)
    ArmedButton *m_newBlackButton = nullptr;
    QLabel *m_armHint = nullptr;               // under the buttons: "Click again within 3 s to resign."
    QStackedWidget *m_panelStack = nullptr;    // the side panel's pages: 0 play, 1 the board editor, 2 the engines
    EditorPanel *m_editor = nullptr;           // page 1
    EnginePanel *m_engines = nullptr;          // page 2
    PracticePanel *m_practice = nullptr;       // page 3
    GamesPanel *m_gamesPanel = nullptr;        // page 4: open a game
    PlayerStrip *m_opponentStrip = nullptr;
    PlayerStrip *m_playerStrip = nullptr;
    QTableWidget *m_moveList = nullptr;
    QWidget *m_coachLine = nullptr;            // under the move list: a column of cards (Looks D)
    QLabel *m_cards[5] = {};                   // its cards, top to bottom: the position, note, square, move, line
    WinGraph *m_graph = nullptr;               // the win-chance graph (graph_design.md)
    QLabel *m_evalWords = nullptr;             // status bar: the bar's words
    ConfidenceDot *m_confidence = nullptr;     // status bar, before the eval words: how settled the verdicts are
    QLabel *m_modeStatus = nullptr;            // status bar, right: level, time control, switches, hints
    QPlainTextEdit *m_log = nullptr;           // the engine log (a dock)
    QPlainTextEdit *m_coachLog = nullptr;      // the coach log (a dock)
    QMenu *m_coachingMenu = nullptr;
    QMenu *m_viewMenu = nullptr;

    // --- Engines, coaches, the game (children of the window; deleted first in the destructor) -----------------------
    Engine *m_engine = nullptr;                // the opponent
    Engine *m_coachEngine = nullptr;           // the general coach's engine
    Engine *m_masterEngine = nullptr;          // the master coach's engine (the third, and the last)
    QWidget *m_helpWindow = nullptr;           // the help window (help.cpp); made the first time it's asked for
    Coach *m_coach = nullptr;
    MasterCoach *m_master = nullptr;
    Game *m_game = nullptr;

    // --- Actions ----------------------------------------------------------------------------------------------------
    QAction *m_newWhite = nullptr;
    QAction *m_newBlack = nullptr;
    QAction *m_takebackAction = nullptr;
    QAction *m_setupAction = nullptr;          // Game → Set up position… (the board editor)
    QAction *m_practiceAction = nullptr;       // Game → Practise an opening…
    QAction *m_saveAction = nullptr;           // Game → Save game… (Ctrl+S)
    QAction *m_openAction = nullptr;           // Game → Open game… (Ctrl+O)
    QAction *m_playFromWhite = nullptr;        // Game → Play from here as White (a loaded game)
    QAction *m_playFromBlack = nullptr;        // … as Black
    QAction *m_forgetAction = nullptr;         // Game → Forget last game (the newest autosaved file to the trash)
    QAction *m_autosaveAction = nullptr;       // Settings → Save games automatically
    QAction *m_enginesAction = nullptr;        // Engine → Engines… (the engine setup)
    QActionGroup *m_levelGroup = nullptr;      // the levels: ExclusiveOptional, none ticked = Custom
    QAction *m_untimedAction = nullptr;        // the Time menu's "Untimed" (greyed at Tournament)
    QAction *m_tournamentTime = nullptr;       // the Time menu's "15 + 10": Tournament's clock when untimed
    QList<QAction *> m_levelActions;           // index = level: 0 Tournament .. 3 Full coaching
    QAction *m_guidanceAction = nullptr;       // the switches (Coaching menu)
    QAction *m_evalBarAction = nullptr;
    QAction *m_moveColoursAction = nullptr;
    QAction *m_marksAction = nullptr;
    QAction *m_aidsAction = nullptr;           // "Thinking aids": linger verdicts, flash, line, Space, T
    QAction *m_hintsSwitch = nullptr;          // "Hints allowed": H needs it
    QAction *m_commentsAction = nullptr;
    QAction *m_warningsAction = nullptr;
    QAction *m_hintAction = nullptr;           // what you ask for: H, T
    QAction *m_threatAction = nullptr;         // T: a question (shows the threat), not a toggle
    bool m_threatShown = false;                // the threat's flash is on the board now
    QAction *m_endAction = nullptr;            // back to the game (End, Esc)

    // --- State ------------------------------------------------------------------------------------------------------
    // The engines' files (engine_design.md): what each role runs now.
    QString m_opponentPath;
    QString m_coachPath;                       // both coaches: two processes of one file
    int m_level = 3;                           // the coaching level; -1 = Custom
    // The hint
    int m_hintsUsed = 0;                       // hints in this game (each step counts); shown in the mode status
    int m_hintStep = 0;                        // 0 = no hint, 1 = the piece, 2 = the piece + its target
    bool m_viewing = false;                    // an earlier position is shown: no hint or threat there
    // The threat
    QString m_threat;                          // this turn's judged threat: the move, "" = no direct threat
    bool m_threatJudged = false;               // the coach has judged this turn's threat (at least once)
    bool m_threatWords = false;                // the threat was shown this turn: its words stay until the turn ends
    bool m_threatAskedByYou = false;           // you pressed T this turn (for the record; automatic doesn't count)
    QList<int> m_hanging;                      // your hanging pieces this turn (en prise): their squares
    QList<int> m_busterHanging;                // Buster's hanging pieces this turn
    Position::Chess m_turnPosition;            // the position at the start of your turn (for the words)
    // This turn's coaching snapshots (the record, the notes)
    QHash<QString, QString> m_badReplies;      // red moves -> their refutation
    QHash<QString, MoveQuality> m_qualities;   // the verdicts, per move
    // The confidence dot (coach_design.md "Coach Confidence: a dot")
    int m_stablePasses = 0;                    // passes in a row that changed no verdict (0..3)
    int m_passesDone = 0;                      // the general coach's passes completed this turn
    QHash<QString, MoveQuality> m_lastPassQualities;   // the verdicts after the previous pass
    QString m_correctMove;                     // the correct move
    // The game record
    GameRecord m_record;
    QString m_gameFile;                        // this game's autosave file ("" = not saved yet)
    QString m_lastSaved;                       // the newest autosaved file (Forget last game)
    bool m_loaded = false;                     // the game on the board came from a file (not saved again)
    QString m_loadedFile;                      // a loaded game's file when notes go back into it: one game, ours
    int m_noteEditors = 0;                     // notes being typed in the move list (0 or 1): its rows wait
    bool m_moveListPending = false;            // ... and a rebuild is owed when the typing ends
    QString m_resultText;                      // the game's end in words: the move list's last row ("" = none)
    OpeningNames m_openings;                   // the opening names (data/openings.tsv), loaded once
    OpeningBook m_book;                        // the opening book (data/book.bin): the opponent's book moves
    QWidget *m_openingStrip = nullptr;         // the opening strip's row, above the move list: the name and ⏩
    QLabel *m_openingLabel = nullptr;          // the game's name
    QToolButton *m_skipButton = nullptr;       // ⏩: skip to the practised line's end (practice only)
    // Opening practice, your turn (computed at its start): the moves that keep the game inside the opening (empty = the
    // game is out of it), and the book's most played ones among them (the leaving card names them).
    QSet<QString> m_practiceInside;
    QStringList m_practiceAlternatives;
    QString m_practiceBest;                    // the most played of them (UCI): the hint after the line
    bool m_practiceLeft = false;               // you left the opening (told once; a takeback or a new game resets it)
    bool m_practiceOn = false;                 // the mode status says "Opening practice": the game is inside the opening
    bool m_curveWaiting = false;               // the opponent's last move waits for its value (the master's first word)
    bool m_gameOver = false;                   // the game has ended (the game row goes grey)
    bool m_tournamentGame = false;             // a move was played under Tournament: no takeback, even after the end
    int m_illegalAttempts = 0;                 // Tournament: illegal attempts this game (the second loses)
    bool m_paused = false;                     // Pause pressed (the button reads Resume)
    bool m_yourTurn = false;                   // your turn (a draw can only be offered then)
    bool m_drawPending = false;                // your offer waits for the master's first word
    int m_drawRestUntil = 0;                   // after a decline: no offer before the game has this many plies
    int m_hintsAtTurn = 0;                     // the game's hint count when your turn started
    // The coach line and "Buster missed it"
    QString m_coachNote;                       // a note for this turn ("Buster missed …")
    QString m_rowSquare;                       // coach line row 2: the square you linger on (a hanging piece)
    QString m_rowMove;                         // row 3: the move under the box (its why)
    QString m_rowLine;                         // row 4: the evidence ("Strongest line: ...")
    QString m_refutation;                      // your last move's refutation, if that move was red ("" = none)
    QString m_refutationText;                  // the same in SAN, for the note ("2... Bxf3")
    UciInfo m_predicted;                       // the coach's score for your move: where the refutation leaves you
    bool m_checkMissed = false;                // waiting for the master's first word in the new position
    int m_movesAtTurn = 0;                     // the game's move count when your turn started
    // The play-through
    QString m_boxMoveNow;                      // the move under the box right now ("" = none)
    QList<Coach::LineStep> m_playSteps;        // the line being played through
    int m_playIndex = -1;                      // the step shown now; -1 = no play-through running
};

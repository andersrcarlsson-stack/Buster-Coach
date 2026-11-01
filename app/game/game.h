// game.h — Game: the hub and the one source of truth for the game (position, moves, turns, clocks).

#pragma once
#include <QObject>
#include <QStringList>
#include "position.h"
#include "chessclock.h"
#include <QElapsedTimer>
#include <QTimer>
#include <map>
#include "openings.h"   // OpeningBook
#include "uci_info.h"

class BoardWidget;   // forward declarations: only pointers are used here,
class Engine;        // so the full class definitions aren't needed in the header

class Game : public QObject
{
    Q_OBJECT

public:
    // --- New games, FEN, takeback ----------------------------------------------------------------
    Game(BoardWidget *board, Engine *engine, QObject *parent = nullptr);
    // Start a fresh game from the start position, the human playing the given side.
    void newGame(bool humanIsWhite);
    // The current position as a FEN string (for Copy FEN).
    QString currentFen();
    // Start a game from this FEN, the human keeping their side. False if the FEN isn't valid.
    bool newGameFromFen(const QString &fen, bool humanIsWhite);   // you play this side (the board editor)
    bool newGameFromFen(const QString &fen) { return newGameFromFen(fen, m_humanIsWhite); }   // the side as now
    // A game from a file, for browsing (pgn_design.md "On load"): its moves replayed, nobody to move (it counts as
    // ended: no engine, no clock, no coach). False: the FEN can't be read.
    bool loadGame(const QString &startFen, const QStringList &moves, bool humanIsWhite);
    void continueFrom(int ply, bool humanIsWhite);   // Play from here: a loaded game goes on after its first `ply` moves
    int viewedPly() const { return m_viewPly == -1 ? int(m_moves.size()) : m_viewPly; }   // the position on the board
    void takeback(); // Take back the last move: replay the game from the start without it.
    // The board editor is open (editor_design.md): the game rests. Buster stops thinking, the clocks stop, the
    // coaches stop (humanTurnEnded), the board takes no moves. resume() picks it up exactly where it was.
    void pause();
    void resume();
    // The opponent's engine was restarted (another engine chosen, or a restart): forget the old process's search,
    // wait for the new one's "ready". A crashed engine is available again.
    void engineRestarted();
    void resign();                  // you resign: the game ends ("You resigned: Stockfish wins.")
    void lose(const QString &text);  // the game ends as your loss, in these words (resigning; a 2nd illegal move)
    void addOpponentTime(long long ms);   // a bonus on the opponent's clock (Tournament: your illegal move)
    // From the coach (via the window): the opponent's moves scored this turn, for its pick (release_design.md 14).
    void setOpponentTable(const std::map<std::string, UciInfo> &table);
    void playNow();                 // the opponent stops thinking and plays the move it has so far
    void agreeDraw();               // a draw offer accepted: the game ends ("Draw agreed.")

    // --- Navigation ------------------------------------------------------------------------------
    // Navigation: look at earlier positions (read-only). The game itself goes on untouched.
    void viewBack();
    void viewForward();
    void viewStart();
    void viewEnd();  // back to the live position
    const QStringList &sanMoves() const { return m_sanMoves; }   // the moves in SAN (the move list's rows)
    int firstNumber() const { return m_firstNumber; }            // the start position's move number
    bool blackFirst() const { return m_blackFirst; }   // the start position had Black to move (for the move list rows)
    bool isTimed() const { return m_timed; }   // a game with clocks (pause covers the board then)
    QString startFen() const { return m_startFen; }    // the position this game began from
    bool humanIsWhite() const { return m_humanIsWhite; }
    int level() const { return m_level; }
    // The opponent's last move: a mistake on purpose (release_design.md 14), and the engine's own move it replaced.
    bool lastPickMistake() const { return m_lastPickMistake; }
    QString lastEngineMove() const { return m_lastEngineMove; }
    QString timeControlPgn() const;                    // PGN's TimeControl: "300+3" (seconds), "-" = untimed
    bool isThinking() const { return m_engineThinking; }   // the opponent is searching (Play now can hurry it)

    void viewPly(int ply);  // show the position after this ply; the newest ply = live

    const QList<Position::Chess> &positions() const { return m_positions; }   // [0] = start, [i] = after ply i
    void setBook(const OpeningBook *book) { m_book = book; }   // the opening book (owned by MainWindow); nullptr = none

    // --- Opening practice (openings_design.md "Opening practice") -----------------------------------
    void newPracticeGame(const QString &line, const QString &name, bool humanIsWhite);   // the opponent follows the line
    void setNames(const OpeningNames *names) { m_names = names; }   // the opening names (owned by MainWindow)
    bool isPractice() const { return !m_practiceName.isEmpty(); }   // an opening practice game
    QString practiceName() const { return m_practiceName; }         // the chosen opening ("" = a normal game)
    bool canSkipLine() const { return !practiceMove().isEmpty(); }   // the game still follows the line, not at its end
    QString nextLineMove() const { return practiceMove(); }         // the line's next move, whoever's turn ("" = off it)
    void skipLine();                     // ⏩: both sides' remaining line moves at once; then the game goes on
    // The game's name after this move in the live position: the last named position on the path, the new one
    // included ("" = none on the way). Practice: does the move keep the game inside the chosen opening (the line's
    // own next move, or a name that is the chosen one or one of its branches)?
    QString nameAfter(const std::string &uci);
    bool staysInside(const std::string &uci);

    // --- Strength and time control ---------------------------------------------------------------
    // The opponent's strength: 1 (easiest) .. 10 (full strength). Used from the engine's next move on.
    void setLevel(int level) { m_level = level; }

    // The time control (from the Time menu): untimed, or minutes + increment. Applies at once if no move
    // has been made yet, otherwise from the next new game.
    void setTimeControl(bool timed, long long baseMs, long long incMs);

    QString timeControlText() const;   // this game's control: "Untimed", "5+3" (for the mode status)

signals:
    // The game has ended; text for the player, e.g. "Checkmate: you win!".
    void gameOver(const QString &text, const QString &result);   // the words, and PGN's result: "1-0", "0-1", "1/2-1/2"

    // The human's turn has started (for the coach): the game so far, and the current position.
    void humanTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position);

    // The human's turn is over (they moved, or the game was reset): the coach should stop.
    void humanTurnEnded();

    // A short text for the status bar: whose turn it is, what is happening.
    void statusChanged(const QString &text);

    // Who plays which colour (at every new game): for the player strips.
    void sidesChanged(bool humanIsWhite);

    // The pieces captured so far (codes 1-12; odd = black pieces, taken by White).
    void capturesChanged(const QList<int> &captured);

    // firstNumber/blackFirst describe the start position (a pasted FEN may start at "12...").
    void sanMovesChanged(const QStringList &san, int firstNumber, bool blackFirst);

    // The viewed ply changed: -1 = live, else 0 (start) .. total-1. total = plies played.
    void viewChanged(int ply, int total);

    // One side's clock, ~10 times a second. timed = false: an untimed game (no clock shown).
    // startMs: that side's starting time (for the low-time colours).
    void clockChanged(bool white, bool timed, qint64 ms, bool running, qint64 startMs);

    // This game's time control changed (a new game, or a choice before the first move).
    void timeControlChanged();

    // A new game begins (New game, a pasted FEN): the coaches' engines should forget the old one too.
    void newGameStarted();

    // A real move was played (not a replay during a takeback): for the game record.
    void movePlayed(const QString &uci, const QString &san, bool human);

    // A takeback removed this many plies (usually 2: Buster's move and yours): for the game record.
    void movesTakenBack(int plies);

    // The opponent's engine is asked for a move (not a book or practice move): the coach can score the opponent's
    // moves meanwhile, for its pick (release_design.md 14). Like humanTurnStarted, but for the other side.
    void opponentTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position);

private slots:
    void onUserMove(const QString &move); // from BoardWidget::moveMade
    void onEngineMove(const QString &move); // from Engine::bestMove
    void onEngineReady();  // from Engine::ready: the game can start
    void onEngineError();  // from Engine::error: the engine is gone

private:
    // --- The parts -------------------------------------------------------------------------------
    BoardWidget *m_board; // not owned: the main window owns the board
    Engine *m_engine; // not owned: the main window owns the engine

    // --- The game and the engine -----------------------------------------------------------------
    QStringList m_moves; // every move so far, in UCI format (UCI engines keep no game state)
    bool m_humanIsWhite = true;  // the human's side; the engine plays the other
    bool humanToMove() const;  // is it the human's turn in m_position?
    QString resultText(Position::GameState state) const;  // the result text for a finished game
    QString resultCode(Position::GameState state) const;   // PGN's result for a finished position
    void requestEngineMove();  // send position + go
    QString bookMove();                 // a move from the opening book for this position, by weight; "" = none
    QString practiceMove() const;       // the practice line's next move while the game follows it; "" = not (any more)
    QStringList m_practiceLine;         // opening practice: the chosen line's moves (UCI); empty = a normal game
    QString m_practiceName;             // opening practice: the chosen opening's name ("" = a normal game)
    const OpeningNames *m_names = nullptr;
    const OpeningBook *m_book = nullptr;
    bool m_engineThinking = false; // a search is running (set by requestEngineMove)
    int m_staleBestMoves = 0; // bestmoves still to come from cancelled searches: ignore them

    // --- The position ----------------------------------------------------------------------------
    // The position the game started from (standard start, or any FEN).
    QString m_startFen = Position::StartFen;
    Position m_model;
    Position::Chess m_position = m_model.FenInput(m_startFen.toStdString()).value();

    // --- Game flow helpers -----------------------------------------------------------------------
    bool applyUci(const QString &uci);   // play a UCI move on m_position; false if not legal
    void startPlay(); // whoever is to move begins (human: input on; engine: go)
    void restart(bool humanIsWhite);  // reset to m_startFen and begin (shared by both new-game functions)
    void cancelSearch();   // stop a running search; its bestmove will be ignored
    bool m_engineAvailable = true;  // false after the engine failed to start or crashed
    bool m_engineReady = false;     // the engine has finished its handshake (since its last (re)start)
    bool m_waitingToPlay = true;    // play starts at the engine's next "ready" (at first, and after a restart)
    bool m_ended = false;           // ended by agreement, not by the position (resigned; a draw accepted later)
    QStringList m_history;   // one key per position reached in this game (for repetition)
    QString positionKey();   // the current position for repetitions: FEN without the counters, e.p. only if possible
    Position::GameState gameState();  // the model's State() plus threefold repetition (from m_history)
    void resetToStart();   // position back to m_startFen, history restarted (restart + takeback)

    // --- The record per ply (rebuilt on takeback) ------------------------------------------------
    QList<int> m_captured;   // captured piece codes, in order (rebuilt like m_history)
    QStringList m_sanMoves;   // the same moves as m_moves, in SAN (rebuilt like m_history)
    int m_firstNumber = 1;      // the start position's move number (FEN field 6)
    bool m_blackFirst = false;  // the start position has Black to move
    QList<Position::Chess> m_positions;   // [0] = start, [i] = after ply i (rebuilt like m_history)
    QList<int> m_moveFrom, m_moveTo;      // the squares of ply i+1, for the last-move highlight

    // --- Viewing and the status text -------------------------------------------------------------
    int m_viewPly = -1;             // -1 = live; else the index into m_positions being shown
    void showView();                // put the live or the viewed position on the board
    QString m_liveStatus;               // the live game's status ("Your move", the result, ...)
    bool m_movedWhileViewing = false;   // Buster moved while an earlier position was shown
    void setStatus(const QString &text);   // remember the live status, then publish
    void publishStatus();               // emit the live status, or the "Viewing ..." text

    // --- Clocks ----------------------------------------------------------------------------------
    // Clocks. This game's time control (untimed by default) ...
    bool m_timed = false;
    long long m_baseMs = 300000;    // starting time per side
    long long m_incMs = 3000;       // increment per move
    // ... and the one chosen for the next game (the Time menu). Copied over by applyTimeControl().
    bool m_nextTimed = false;
    long long m_nextBaseMs = 300000;
    long long m_nextIncMs = 3000;
    void applyTimeControl();        // next -> this game: fresh clocks, full time
    ChessClock m_clock {m_baseMs, m_baseMs, m_incMs, m_incMs};
    QElapsedTimer m_time;           // a stopwatch started once: m_time.elapsed() is the clock's "now"
    QTimer m_tick;                  // refreshes the clock display; measures nothing
    void publishClocks();           // emit clockChanged for both sides

    void clockAfterMove();          // a real move was played: start or switch the clocks
    void flagFall(bool white);      // this side's time ran out: the game ends (a loss, or a draw)

    // --- Strength --------------------------------------------------------------------------------
    int m_level = 5;                // the opponent's strength (first run: the middle level)

    std::map<std::string, UciInfo> m_opponentTable;   // this turn's scored opponent moves (empty = not yet)
    QString m_heldMove;              // the engine's move, held until the table is there ("" = none held)
    bool m_tableRequested = false;   // this turn the coach was asked for the opponent's table (the level has a cap)
    bool m_lastPickMistake = false;  // the opponent's last move was a mistake on purpose (for the card and the PGN)
    QString m_lastEngineMove;        // the engine's own move behind the last pick (the PGN's "Best was")   
    void playPicked(const QString &engineMove);   // the pick, the courtesy pause, then playEngineMove


    // --- The courtesy pause ----------------------------------------------------------------------
    long long m_thinkStart = 0;     // when the engine was asked (m_time), for the courtesy pause
    int m_moveToken = 0;            // bumped by cancelSearch(): a move waiting out its pause is then dropped
    int m_searchCount = 0;           // counts the engine's searches: a 60 s cap timer stops only its own
    void playEngineMove(const QString &move);   // the engine's move, played after the courtesy pause
};

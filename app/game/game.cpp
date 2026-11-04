// game.cpp — the game: moves, turns, the engine's answers, new games, takeback, viewing, clocks.
//
// Sections:
//   1. Settings: the strength table, the courtesy pause
//   2. Construction
//   3. Moves: the human's, the engine's, playing a UCI move
//   4. Turns and the engine: who begins, results, asking for a move
//   5. New games, resets, takeback
//   6. Viewing earlier positions, and the status text
//   7. Clocks: display, starting/switching, flag fall, time control

#include "game.h"
#include "boardwidget.h"
#include "engine.h"
#include "move.h"
#include <QRandomGenerator>

// =================================================================================================
// 1. SETTINGS: the strength table, the courtesy pause
// =================================================================================================
// The strength levels, calibrated for the built-in Buster 7.0.2 (release_design.md 14: 5 rounds of autoplay, every
// game re-analysed by Stockfish, the mistakes matched to lichess rapid players of each rating; 2026-10-28).
// Nodes per level (index = level; ~x2.2 a level): the quality of the ordinary moves, and so the steps between levels.
// Level 11 = full strength: no node limit. Level 10 (≈ 2400) added 2026-11-04 (Anders: a step between 2200 and full
// strength): its node limit one more x2.2, to be confirmed by the calibration.
static constexpr int fullStrength = 11;
static const long long levelNodes[] = {0, 1500, 3300, 7000, 15000, 33000, 70000, 150000, 330000, 700000, 1500000};

// The opponent's mistakes per level (release_design.md 14, two kinds of human mistakes, 2026-11-04):
// {horizon, {lapse, oversight, margin, best}}. A lapse: it plays from the "hope" view (didn't check the reply: can hang
// a piece), levels 1-3 only (Anders: big blunders only there). An oversight: from a short look of `horizon` plies after
// its move (every capture seen, a quiet resource not): `best` of them play its best-looking move (the stronger, the more
// it trusts it), the rest choose among the moves within `margin` of it. Otherwise the engine's own move. The chances per
// move; mistakes only where the view hides a trap. Matched to lichess players of ~1000 ... 2400 (red moves per 100 in
// undecided positions; tools/pickcheck, 600 positions): START VALUES until the calibration games. None at full strength.
struct OpponentLevel { int horizon; Fallibility fallibility; };
static const OpponentLevel opponentLevels[] = {
    {0, {}},                                                                              // [0] unused
    {1, {0.22, 0.25, 0.15, 0.25}}, {1, {0.20, 0.22, 0.15, 0.25}}, {1, {0.02, 0.98, 0.15, 0.25}},   // 1000 1150 1300
    {1, {0.00, 1.00, 0.15, 0.25}}, {1, {0.00, 0.89, 0.15, 0.25}}, {1, {0.00, 0.95, 0.15, 0.50}},   // 1450 1600 1750
    {1, {0.00, 0.85, 0.15, 0.50}}, {1, {0.00, 0.73, 0.15, 0.50}}, {1, {0.00, 0.73, 0.15, 0.75}},   // 1900 2050 2200
    {1, {0.00, 0.55, 0.15, 0.75}},                                                                  // 2400
    {0, {}}};                                                                             // [11] full strength: none

// How deep the opponent follows the opening book, per level (plies from the standard start; the book holds 14): a
// weak opponent leaves the book early, as a weak player would. openings_design.md "The opponent plays from the book".
static const int bookPlies[] = {0, 2, 4, 6, 8, 10, 12, 14, 14, 14, 14, 14};

// Buster never answers faster than this: time to see its move coming. A first, fixed version of the
// "human mimic" idea (todo: per level, varying).
static const long long courtesyMs = 800;
static const long long virtualClockMs = 300000;   // untimed full strength: the clock the engine is told it has
static const int maxThinkMs = 60000;              // untimed full strength: the GUI stops a search at 60 s (the cap) 



// =================================================================================================
// 2. CONSTRUCTION
// =================================================================================================
Game::Game(BoardWidget *board, Engine *engine, QObject *parent)
    : QObject(parent), m_board(board), m_engine(engine) {
    connect(m_board, &BoardWidget::moveMade, this, &Game::onUserMove);
    connect(m_engine, &Engine::bestMove, this, &Game::onEngineMove);
    connect(m_engine, &Engine::ready, this, &Game::onEngineReady);
    connect(m_engine, &Engine::error, this, &Game::onEngineError);
    connect(this, &Game::gameOver, this, &Game::setStatus);   // the result becomes the live status
    connect(this, &Game::gameOver, this, [this]() { m_clock.stop(m_time.elapsed()); });   // the clocks stop

    m_board->setInputEnabled(false);  // no moves before the engine is ready (C5)
    m_board->setPosition(m_position); //  show the start position on the board
    resetToStart();                   // the ONE way to set up the start: history, positions, captures, SAN
    m_time.start();  // the stopwatch runs for the whole program
    connect(&m_tick, &QTimer::timeout, this, &Game::publishClocks);
    m_tick.start(100); // every 100 ms: refresh the display
}

// =================================================================================================
// 3. MOVES: the human's, the engine's, playing a UCI move
// =================================================================================================
// The user moved: if the game goes on, tell the engine the new position and let it think.
void Game::onUserMove(const QString &move) {
    if (!applyUci(move)) return;
    emit humanTurnEnded();
    m_moves << move;
    clockAfterMove();
    emit movePlayed(move, m_sanMoves.last(), true);
    m_board->setInputEnabled(false);
    const Position::GameState state = gameState();
    m_board->setGameState(state);
    if (state != Position::GameState::Ongoing) {
        emit gameOver(resultText(state), resultCode(state));
        return; // game over: don't ask the engine to move
    }
    requestEngineMove();
}

// The engine answered: record its move, show it, and check whether that ended the game.
void Game::onEngineMove(const QString &move) {
    if (m_staleBestMoves > 0) {      // the answer to a search we cancelled: not for this game
        --m_staleBestMoves;
        return;
    }
    m_engineThinking = false;          // the search is over: a cancel now has nothing to stop
    if (m_tableRequested && m_opponentTable.empty()) {
        m_heldMove = move;             // the pick needs the coach's table: wait for setOpponentTable
        return;
    }
    playPicked(move);
}

// The engine's move goes through the pick (if the level makes mistakes), then the courtesy pause, then the board.
void Game::playPicked(const QString &engineMove) {
    m_lastPickMistake = false;
    m_lastEngineMove = engineMove;
    QString move = engineMove;
    const Fallibility fallibility = opponentLevels[std::min(m_level, fullStrength)].fallibility;
    if (fallibility.lapse + fallibility.oversight > 0.0) {
        const Pick pick = pickMove(engineMove.toStdString(), m_opponentTable, m_opponentHope, m_opponentLook, fallibility,
                                   QRandomGenerator::global()->generateDouble());
        move = QString::fromStdString(pick.move);
        m_lastPickMistake = pick.mistake;
    }
    m_opponentTable.clear();           // used: the next turn needs a new one
    m_opponentHope.clear();
    m_opponentLook.clear();
    const long long wait = courtesyMs - (m_time.elapsed() - m_thinkStart);
    if (wait <= 0) {
        playEngineMove(move);          // it thought long enough
        return;
    }
    const int token = m_moveToken;
    QTimer::singleShot(wait, this, [this, move, token]() {
        if (token == m_moveToken)      // nothing cancelled it meanwhile (takeback, new game, flag)
            playEngineMove(move);
    });
}

// The engine's move, played (after the courtesy pause).
void Game::playEngineMove(const QString &move) {
    if (!applyUci(move)) return;
    m_moves << move;
    clockAfterMove();
    emit movePlayed(move, m_sanMoves.last(), false);
    if (m_viewPly != -1)
        m_movedWhileViewing = true;
    const Position::GameState state = gameState();
    if (m_viewPly == -1)
        m_board->setGameState(state);      // only when the board shows the live position
    if (state != Position::GameState::Ongoing)
        emit gameOver(resultText(state), resultCode(state));
    m_board->setInputEnabled(state == Position::GameState::Ongoing);
    if (state == Position::GameState::Ongoing) {
        emit humanTurnStarted(m_startFen, m_moves, m_position);
        setStatus("Your move");
    }
}

// Play a UCI move on the game's position and show the result on the board.
bool Game::applyUci(const QString &uci) {
    std::optional<Position::Move> move = m_model.UciToMove(m_position, uci.toStdString());
    if (!move)
        return false; // not legal here (e.g. "(none)"): change nothing
    Moves mover;
    Position::Move m = *move;
    if (const int taken = m_model.CapturedPiece(m_position, m))
        m_captured << taken;  // 0 = nothing taken: not added
    m_sanMoves << QString::fromStdString(m_model.MoveToSan(m_position, m));   // needs the position BEFORE the move
    m_position = mover.make(m_position, m);
    m_positions << m_position; // the position after this ply
    m_moveFrom << m.from_square;
    m_moveTo << m.to_square;
    m_history << positionKey();
    if (m_viewPly == -1) {  // live: show the move; viewing: leave the board alone
        m_board->setPosition(m_position);
        m_board->setLastMove(m.from_square, m.to_square);
    }

    emit capturesChanged(m_captured);
    emit sanMovesChanged(m_sanMoves, m_firstNumber, m_blackFirst);
    return true;
}

// =================================================================================================
// 4. TURNS AND THE ENGINE: who begins, results, asking for a move
// =================================================================================================
// The engine has finished its handshake: start playing.
void Game::onEngineReady() {
    m_engineReady = true;
    if (m_waitingToPlay) {                 // the first start, or a resume that came before the engine was ready
        m_waitingToPlay = false;
        startPlay();
    }
}

void Game::engineRestarted() {
    m_engineReady = false;
    m_engineAvailable = true;              // a new process: whatever happened to the old one
    m_engineThinking = false;              // the old process's search died with it ...
    m_staleBestMoves = 0;                  // ... so no bestmove from it will come to be skipped
    ++m_moveToken;                         // and a move of its waiting out the courtesy pause is dropped
}

// Whoever is to move begins: the human gets input, or the engine is asked to move.
void Game::startPlay() {
    if (!m_engineAvailable) { // no engine, no game
        m_board->setInputEnabled(false);
        return;
    }
    m_board->setInputEnabled(humanToMove());
    if (!humanToMove())
        requestEngineMove();
    else {
        emit humanTurnStarted(m_startFen, m_moves, m_position);
        setStatus("Your move");
    }
}

// pit == true means Black to move; it's the human's turn when that matches the human's side.
bool Game::humanToMove() const {
    return m_position.pit != m_humanIsWhite;
}

// The text for a finished game. The side to move in m_position is the side that was mated.
QString Game::resultText(Position::GameState state) const {
    switch (state) {
        case Position::GameState::Checkmate:
            if (m_moves.isEmpty()) // a pasted position: nobody in this game delivered the mate
                return "Checkmate.";
            return humanToMove() ? "Checkmate: " + m_engine->shortName() + " wins." : QString("Checkmate: you win!");
        case Position::GameState::Stalemate:
            return "Stalemate: draw.";
        case Position::GameState::FiftyMoves:
            return "Draw by the 50-move rule.";
        case Position::GameState::Repetition:
            return "Draw by threefold repetition.";
        case Position::GameState::InsufficientMaterial:
            return "Draw: insufficient material.";
        case Position::GameState::Ongoing:
            break;
    }
    return "";
}

// PGN's result for a finished position. Checkmate: the side to move is mated, so the other side won.
QString Game::resultCode(Position::GameState state) const {
    switch (state) {
        case Position::GameState::Checkmate:
            return m_position.pit ? "1-0" : "0-1";   // pit: Black to move (and mated)
        case Position::GameState::Ongoing:
            return "*";
        default:
            return "1/2-1/2";                        // stalemate, 50 moves, repetition, insufficient material
    }
}

// The game's time control in PGN's form: seconds + increment ("300+3"); "-" for an untimed game.
QString Game::timeControlPgn() const {
    return m_timed ? QString("%1+%2").arg(m_baseMs / 1000).arg(m_incMs / 1000) : QString("-");
}

// Tell the engine the game so far and let it think.
void Game::requestEngineMove() {
    m_lastPickMistake = false;         // a book or practice move skips the pick: never a mistake on purpose
    // Opening practice, still on the line: its next move at once (the line isn't thought about). Through the event
    // loop all the same (a 0 ms timer), so the move that led here has finished being handled first.
    if (const QString line = practiceMove(); !line.isEmpty()) {
        const int token = m_moveToken;
        QTimer::singleShot(0, this, [this, line, token]() {
            if (token == m_moveToken)
                playEngineMove(line);
        });
        return;
    }
    // In the book: a move people play, after the courtesy pause, without the engine. Not through onEngineMove: that
    // would count it as a stale answer if a cancelled search's bestmove is still on its way.
    if (const QString book = bookMove(); !book.isEmpty()) {
        setStatus(m_engine->shortName() + " plays from the book…");
        const int token = m_moveToken;
        QTimer::singleShot(courtesyMs, this, [this, book, token]() {
            if (token == m_moveToken)   // nothing cancelled it meanwhile (takeback, new game, pause, flag)
                playEngineMove(book);
        });
        return;
    }

    QString command = "position fen " + m_startFen;
    m_opponentTable.clear();                        // a new turn: an old table belongs to another position
    m_opponentHope.clear();
    m_opponentLook.clear();
    const OpponentLevel &level = opponentLevels[std::min(m_level, fullStrength)];
    m_tableRequested = level.fallibility.lapse + level.fallibility.oversight > 0.0;
    if (m_tableRequested)                           // the engine thinks: the coach scores its moves for the pick
        emit opponentTurnStarted(m_startFen, m_moves, m_position, level.fallibility.lapse > 0.0,
                                 level.fallibility.oversight > 0.0 ? level.horizon : 0);
    if (!m_moves.isEmpty())
        command += " moves " + m_moves.join(' ');
    m_engine->send(command);
    m_thinkStart = m_time.elapsed();   // the courtesy pause counts from here
    const int search = ++m_searchCount;   // this search's number (the cap below checks it)
    // Below full strength: a node limit (the same strength on any computer). Full strength: none.
    const QString nodes = (m_level < fullStrength) ? QString(" nodes %1").arg(levelNodes[m_level]) : QString();
    if (m_timed) {   // the engine manages its own time from the real clocks (ms); the node limit on top
        const long long now = m_time.elapsed();
        m_engine->send(QString("go wtime %1 btime %2 winc %3 binc %3")
                           .arg(m_clock.remaining(true, now)).arg(m_clock.remaining(false, now)).arg(m_incMs)
                       + nodes);
    } else {         // untimed: the node limit alone, or at full strength a virtual clock
        // Full strength: a virtual 5-minute clock, fresh every move (release_design.md 14): the engine's own time
        // management spends it where it matters (Buster: ~10 s on average, never more than 45 s).
        m_engine->send(m_level < fullStrength ? "go" + nodes : QString("go wtime %1 btime %1").arg(virtualClockMs));
        if (m_level >= fullStrength)                 // the cap (release_design.md 14): stopped at 60 s, the best move so far played
            QTimer::singleShot(maxThinkMs, this, [this, search]() {
                if (search == m_searchCount)
                    playNow();             // still this search (playNow does nothing if it has already answered)
            });
    }
    m_engineThinking = true;
    setStatus(m_engine->shortName() + " is thinking…");
}

// A book move for the position now: drawn at random, each move as likely as its share of the games. "" = not in the
// book (or no book). A move that isn't legal here (a hash collision, however unlikely) is never returned.
QString Game::bookMove() {
    if (!m_book)
        return "";
    const int ply = 2 * (m_position.fullmove_number - 1) + (m_position.pit ? 1 : 0);   // 0 = White's first move
    const int last = int(std::size(bookPlies)) - 1;   // a level beyond the table uses its last value
    if (m_practiceName.isEmpty() && ply >= bookPlies[std::min(m_level, last)])   // practice: the opening decides
        return "";                     // deeper than this level knows: the engine decides
    std::vector<BookMove> moves = m_book->moves(m_model.Key(m_position));
    if (!m_practiceName.isEmpty() && m_names) {   // practice: only the moves that stay inside the opening, if any do
        std::vector<BookMove> inside;
        for (const BookMove &m : moves)
            if (staysInside(m.uci))
                inside.push_back(m);
        if (!inside.empty())
            moves = inside;
    }
    int total = 0;
    for (const BookMove &m : moves)
        total += m.games;
    if (total == 0)
        return "";
    int roll = QRandomGenerator::global()->bounded(total);   // 0 .. total-1
    for (const BookMove &m : moves) {
        roll -= m.games;
        if (roll < 0)
            return m_model.UciToMove(m_position, m.uci) ? QString::fromStdString(m.uci) : "";
    }
    return "";
}

// The line's next move, as long as every move so far is the line's (and the line isn't over). "" = left or done.
QString Game::practiceMove() const {
    if (m_moves.size() >= m_practiceLine.size() || m_practiceLine.mid(0, m_moves.size()) != m_moves)
        return "";
    return m_practiceLine[m_moves.size()];
}

// Practice: the game's name after this move (the last named position on the path, the new one included) is the chosen
// opening or one of its branches ("…, English Attack", or for a family "Sicilian Defense: …").
bool Game::staysInside(const std::string &uci) {
    if (QString::fromStdString(uci) == practiceMove())
        return true;                                   // the line's own next move: on the way into the opening
    // Known paths only: a book move, or a move to a named position. Off them (an unknown sideline) is out of the
    // opening, even if the game keeps its last name (an unnamed position inherits it).
    bool known = false;
    if (m_book)
        for (const BookMove &m : m_book->moves(m_model.Key(m_position)))
            known = known || m.uci == uci;
    if (!known && m_names) {
        Moves mover;
        std::optional<Position::Move> move = m_model.UciToMove(m_position, uci);
        known = move && m_names->find(m_model.Key(mover.make(m_position, *move))).has_value();
    }
    const QString name = nameAfter(uci);
    return known && !name.isEmpty() && (name == m_practiceName || name.startsWith(m_practiceName + ":")
                                        || name.startsWith(m_practiceName + ","));
}

QString Game::nameAfter(const std::string &uci) {
    std::optional<Position::Move> move = m_model.UciToMove(m_position, uci);
    if (!move || !m_names)
        return "";
    Moves mover;
    QList<Position::Chess> path = m_positions;
    path << mover.make(m_position, *move);             // the game as it would be after the move
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (const auto named = m_names->find(m_model.Key(*it)))
            return QString::fromStdString(named->name);
    }
    return "";                                         // no name on the way at all
}

// ⏩: the rest of the line, both sides, at once, as moves of this game (the move list, the record and the strip follow
// as for any move); then whoever is to move goes on: you, or the opponent from the book inside the opening.
void Game::skipLine() {
    viewEnd();                         // as a takeback: in the live view
    if (!canSkipLine() || m_ended)
        return;
    cancelSearch();                    // the opponent's line move waiting on its timer is dropped (the token)
    emit humanTurnEnded();
    for (QString move = practiceMove(); !move.isEmpty(); move = practiceMove()) {
        const bool human = humanToMove();
        applyUci(move);                // the line's moves are legal: they were played from the start (the tool)
        m_moves << move;
        clockAfterMove();
        emit movePlayed(move, m_sanMoves.last(), human);
    }
    startPlay();
}


// =================================================================================================
// 5. NEW GAMES, RESETS, TAKEBACK
// =================================================================================================
// New game from the standard start position.
void Game::newGame(bool humanIsWhite) {
    m_practiceLine.clear();            // a normal game
    m_practiceName.clear();
    m_startFen = Position::StartFen;
    restart(humanIsWhite);
}

// Opening practice: a new game from the start, whose opponent follows the chosen line (openings_design.md).
void Game::newPracticeGame(const QString &line, const QString &name, bool humanIsWhite) {
    m_startFen = Position::StartFen;
    m_practiceLine = line.split(' ', Qt::SkipEmptyParts);
    m_practiceName = name;
    restart(humanIsWhite);             // AFTER the line is set: as Black, the opponent's first move comes from it
}

// New game from any FEN (e.g. pasted). Nothing changes if the FEN isn't valid.
bool Game::newGameFromFen(const QString &fen, bool humanIsWhite) {
    m_practiceLine.clear();            // a normal game
    m_practiceName.clear();
    std::optional<Position::Chess> start = m_model.FenInput(fen.trimmed().toStdString());
    if (!start || !m_model.IsValid(*start))  // must parse AND be a possible position
        return false;
    m_startFen = QString::fromStdString(m_model.FenOutput(*start));   // normalised (e.g. counters added)
    restart(humanIsWhite);
    return true;
}

bool Game::loadGame(const QString &startFen, const QStringList &moves, bool humanIsWhite) {
    const std::optional<Position::Chess> start = m_model.FenInput(startFen.toStdString());
    if (!start)
        return false;
    m_practiceLine.clear();
    m_practiceName.clear();
    viewEnd();
    cancelSearch();                    // the opponent may be thinking in the game being left
    emit humanTurnEnded();             // the coaches stop
    emit newGameStarted();             // listeners reset; the game being left is saved if unfinished (as with New)
    m_humanIsWhite = humanIsWhite;
    m_board->setFlipped(!humanIsWhite);
    emit sidesChanged(humanIsWhite);
    m_startFen = QString::fromStdString(m_model.FenOutput(*start));
    m_moves.clear();
    resetToStart();
    for (const QString &move : moves) {   // replayed silently: no movePlayed (the record comes from the file)
        if (!applyUci(move))
            break;
        m_moves << move;
    }
    m_ended = true;                    // browsing: nobody moves
    applyTimeControl();                // the clocks as at a game's start, nothing running
    m_board->setPosition(m_position);
    m_board->setGameState(gameState());   // a mate or a draw at the end: its marks
    m_board->setInputEnabled(false);
    setStatus("A game from a file: browse it with the move list and the arrows.");
    return true;
}

// Play from here (pgn_design.md step 6): the game goes on from the position after `ply` moves, against the opponent.
// The moves up to there stay; the rest is dropped. No newGameStarted: the record (kept by MainWindow) stays.
void Game::continueFrom(int ply, bool humanIsWhite) {
    const QStringList kept = m_moves.mid(0, ply);
    viewEnd();
    cancelSearch();
    m_ended = false;                   // running again
    m_humanIsWhite = humanIsWhite;
    m_board->setFlipped(!humanIsWhite);
    emit sidesChanged(humanIsWhite);
    m_moves = kept;
    resetToStart();
    for (const QString &move : m_moves)
        applyUci(move);                // these were legal: they were played
    m_board->setPosition(m_position);
    m_engine->send("ucinewgame");
    applyTimeControl();                // fresh clocks, nothing running yet
    const Position::GameState state = gameState();
    m_board->setGameState(state);
    if (state != Position::GameState::Ongoing) {   // the viewed position is already over (a mate): nothing to play
        m_board->setInputEnabled(false);
        emit gameOver("This position is already finished. " + resultText(state), resultCode(state));
        return;
    }
    startPlay();                       // you, or the opponent, to move
}

// If the engine is searching: tell it to stop, and skip the bestmove that still comes.
void Game::cancelSearch() {
    ++m_moveToken;   // a move waiting out its courtesy pause is dropped too ...
    m_heldMove.clear();   // ... and one waiting for the coach's table
    if (m_engineThinking) {
        m_engine->send("stop");
        ++m_staleBestMoves;
        m_engineThinking = false;
    }
}

// The position as at the start of the game, with the history to match.
void Game::resetToStart() {
    m_position = m_model.FenInput(m_startFen.toStdString()).value();
    m_positions = {m_position};                // only the start position
    m_moveFrom.clear();
    m_moveTo.clear();
    m_firstNumber = m_position.fullmove_number;   // m_position IS the start position here
    m_blackFirst = m_position.pit;
    m_history.clear();
    m_history << positionKey();
    m_board->setLastMove(-1, -1);   // no move made yet
    m_board->setGameState(Position::GameState::Ongoing);
    m_captured.clear();
    emit capturesChanged(m_captured);
    m_sanMoves.clear();
    emit sanMovesChanged(m_sanMoves, m_firstNumber, m_blackFirst);
}

// Reset everything to m_startFen and begin. (This is the old body of newGame.)
void Game::restart(bool humanIsWhite) {
    m_ended = false;                    // a new game: nothing ended yet
    viewEnd(); // a new game or a takeback always happens in the live view
    cancelSearch();
    emit humanTurnEnded();
    m_humanIsWhite = humanIsWhite;
    m_board->setFlipped(!m_humanIsWhite);   // the human's pieces at the bottom
    emit sidesChanged(m_humanIsWhite);
    m_moves.clear();
    resetToStart();
    m_board->setPosition(m_position);
    m_engine->send("ucinewgame");
    emit newGameStarted();   // the coaches' engines get their ucinewgame too (main)
    // A position can be over before anyone moves (e.g. a pasted FEN): say so instead of playing.
    const Position::GameState state = gameState();
    if (state != Position::GameState::Ongoing) {
        m_board->setGameState(state);
        m_board->setInputEnabled(false);
        emit gameOver("This position is already finished. " + resultText(state), resultCode(state));
        return;
    }
    applyTimeControl();   // a new game: the chosen time control, full time, nothing runs yet
    startPlay();
}

// The current position as FEN (Copy FEN; also the repetition key below).
QString Game::currentFen() {
    return QString::fromStdString(m_model.FenOutput(m_position));
}

// FEN fields 0–3 (board, side to move, castling, e.p.): equal keys = the same position for repetition.
QString Game::positionKey() {
    return QString::fromStdString(m_model.RepetitionKey(m_position));   // e.p. only when possible (FIDE)
}

// The model knows mate, stalemate and the 50-move rule; only the game knows the history.
Position::GameState Game::gameState() {
    const Position::GameState state = m_model.State(m_position);
    if (state == Position::GameState::Ongoing && m_history.count(m_history.last()) >= 3)
        return Position::GameState::Repetition;
    return state;
}

// The engine failed to start or crashed: stop the game cleanly (main shows the message).
void Game::onEngineError() {
    m_engineAvailable = false;
    m_engineThinking = false;
    m_staleBestMoves = 0;
    m_board->setInputEnabled(false);
}

// Remove the last move and rebuild the position by replaying the rest from m_startFen.
void Game::takeback() {
    viewEnd(); // a new game or a takeback always happens in the live view
    if (m_moves.isEmpty()) return; // nothing to take back
    m_ended = false;                    // taking back after a resignation: the game goes on
    cancelSearch();  // Buster may be thinking about the move we remove
    emit humanTurnEnded();
    const int before = m_moves.size();
    do {  // usually twice: Buster's move, then yours
        m_moves.removeLast();
        resetToStart();
        for (const QString &move : m_moves)
            applyUci(move);  // these were legal before, so they are again
    } while (!humanToMove() && !m_moves.isEmpty());
    emit movesTakenBack(before - m_moves.size());   // 1 or 2 plies
    // The clocks keep running (no time given back), now on the side to move; no increment: nobody moved.
    const long long now = m_time.elapsed();
    m_clock.stop(now);                          // book the time used so far
    if (m_moves.size() >= 2)
        m_clock.start(!m_position.pit, now);    // before both sides' first moves: no clock runs
    m_board->setPosition(m_position); // also covers "no moves left" (the loop didn't run)
    startPlay();  // whoever is to move now begins (usually you)
}

void Game::pause() {
    viewEnd();                          // the live position: the editor starts from it
    cancelSearch();                     // Buster stops; a move waiting out its courtesy pause is dropped
    emit humanTurnEnded();              // the coaches stop; colours, threat, hint and coach line go
    m_clock.stop(m_time.elapsed());     // book the time used: no clock runs while the game rests
    m_board->setInputEnabled(false);
}

void Game::resume() {
    m_board->setFlipped(!m_humanIsWhite);   // your pieces at the bottom again (the editor may have flipped it)
    showView();                         // the live position again, with its last move and end marks
    if (m_ended || gameState() != Position::GameState::Ongoing)
        return;                         // a finished game stays finished: nobody moves, no clock
    if (m_moves.size() >= 2)
        m_clock.start(!m_position.pit, m_time.elapsed());   // as after a takeback: before both first moves, none
    if (!m_engineReady) {
        m_waitingToPlay = true;         // a restarted engine: play begins at its "ready" (onEngineReady)
        return;
    }
    startPlay();                        // whoever is to move begins again (Buster: a fresh search)
}

void Game::resign() {
    lose("You resigned: " + m_engine->shortName() + " wins.");
}

// The game ends as your loss: resigning, or (Tournament) a second illegal move.
void Game::lose(const QString &text) {
    cancelSearch();                    // the opponent may be thinking: its move won't come
    emit humanTurnEnded();             // the coaches stop
    m_ended = true;
    m_board->setInputEnabled(false);
    emit gameOver(text, m_humanIsWhite ? "0-1" : "1-0");
}

void Game::addOpponentTime(long long ms) {
    m_clock.addTime(!m_humanIsWhite, ms);   // the clocks redraw on their 100 ms tick
}

void Game::setOpponentTable(const std::map<std::string, UciInfo> &table, const std::map<std::string, UciInfo> &hope,
                            const std::map<std::string, UciInfo> &look) {
    m_opponentTable = table;
    m_opponentHope = hope;
    m_opponentLook = look;
    if (!m_heldMove.isEmpty()) {       // the engine was faster: its move waited for this
        const QString move = m_heldMove;
        m_heldMove.clear();
        playPicked(move);
    }
}

// "stop" and nothing more: the bestmove that answers is played as usual (onEngineMove), its time charged to the
// opponent's clock. cancelSearch() is the other way: "stop" and the answer thrown away (counted as stale).
void Game::playNow() {
    if (m_engineThinking)
        m_engine->send("stop");
}

void Game::agreeDraw() {
    emit humanTurnEnded();             // the coaches stop (the offer is made on your turn)
    m_ended = true;
    m_board->setInputEnabled(false);
    emit gameOver("Draw agreed.", "1/2-1/2");
}


// =================================================================================================
// 6. VIEWING EARLIER POSITIONS, AND THE STATUS TEXT
// =================================================================================================
// The board shows either the live position (input as Game decided) or an earlier one (read-only).
void Game::showView() {
    const int total = m_positions.size() - 1;   // plies played
    const int ply = (m_viewPly == -1) ? total : m_viewPly;
    m_board->setViewing(m_viewPly != -1);
    m_board->setGameState(m_viewPly == -1 ? gameState() // live: the real state
                                          : Position::GameState::Ongoing);  // an earlier position: no marks
    m_board->setPosition(m_positions[ply]);
    if (ply > 0)
        m_board->setLastMove(m_moveFrom[ply - 1], m_moveTo[ply - 1]);
    else
        m_board->setLastMove(-1, -1);   // the start position: no last move
    emit viewChanged(m_viewPly, total);
    if (m_viewPly == -1)
        m_movedWhileViewing = false;   // back to live: you've seen it now
    publishStatus();
}

// One ply back (from live: the position before the last move).
void Game::viewBack() {
    const int current = (m_viewPly == -1) ? m_positions.size() - 1 : m_viewPly;
    if (current == 0)
        return;   // already at the start
    m_viewPly = current - 1;
    showView();
}

// One ply forward; past the newest position: live again.
void Game::viewForward() {
    if (m_viewPly == -1)
        return;                                             // already live
    ++m_viewPly;
    if (m_viewPly >= m_positions.size() - 1)
        m_viewPly = -1;                                     // reached the newest position: live again
    showView();
}

// The start position.
void Game::viewStart() {
    m_viewPly = (m_positions.size() > 1) ? 0 : -1;          // no moves yet: the start IS live
    showView();
}

// Back to the live game.
void Game::viewEnd() {
    m_viewPly = -1;
    showView();
}

// The live game's status: remembered, then shown (unless an earlier position is being viewed).
void Game::setStatus(const QString &text) {
    m_liveStatus = text;
    publishStatus();
}

// Live: the live status. Viewing: which move is shown, e.g. "Viewing 12... Nf6".
void Game::publishStatus() {
    if (m_viewPly == -1) {
        emit statusChanged(m_liveStatus);
        return;
    }
    QString text = "Viewing the start position";
    if (m_viewPly > 0) {
        const int i = m_viewPly - 1;                            // index into m_sanMoves
        const bool black = (i + (m_blackFirst ? 1 : 0)) % 2 == 1;
        const int number = m_firstNumber + (i + (m_blackFirst ? 1 : 0)) / 2;
        text = QString("Viewing %1%2 %3").arg(number).arg(black ? "..." : ".").arg(m_sanMoves[i]);
    }
    if (m_movedWhileViewing)
        text += " · " + m_engine->shortName() + " has moved";
    emit statusChanged(text);
}

// Show the position after this ply (a click in the move list); the newest = live.
void Game::viewPly(int ply) {
    const int total = m_positions.size() - 1;
    m_viewPly = (ply >= total) ? -1 : qMax(ply, 0);   // the newest position IS live
    showView();
}

// =================================================================================================
// 7. CLOCKS: display, starting/switching, flag fall, time control
// =================================================================================================
// Both clocks for the display. The clock itself is exact; this only refreshes what is shown.
void Game::publishClocks() {
    const long long now = m_time.elapsed();
    for (bool white : {true, false})
        if (m_timed && m_clock.isRunning(white) && m_clock.remaining(white, now) <= 0)
            flagFall(white);                    // seen here, at most one refresh (0.1 s) late
    for (bool white : {true, false})
        emit clockChanged(white, m_timed, m_clock.remaining(white, now), m_clock.isRunning(white), m_baseMs);
}

// A real move was played (not a replay): the clocks follow. Each side's first move is free.
void Game::clockAfterMove() {
    const long long now = m_time.elapsed();
    if (m_moves.size() < 2)
        return;                                 // a first move: no clock yet
    if (m_moves.size() == 2)
        m_clock.start(!m_position.pit, now);    // both have moved: the side to move starts (pit = Black)
    else
        m_clock.moveMade(now);                  // the mover gets the increment; the other side runs
}

// A clock ran out. A loss, unless the opponent can't possibly mate: then a draw (the flag rule).
void Game::flagFall(bool white) {
    cancelSearch();                             // Buster may be thinking (its own flag): too late now
    emit humanTurnEnded();                      // the coaches stop
    m_board->setInputEnabled(false);
    const bool humanFlagged = (white == m_humanIsWhite);
    QString text;
    if (!m_model.CanMate(m_position, !white))   // the side with time left
        text = humanFlagged ? "Draw: your time ran out, but " + m_engine->shortName() + " can't mate."
                            : "Draw: " + m_engine->shortName() + "'s time ran out, but you can't mate.";
    else
        text = humanFlagged ? "Time out: " + m_engine->shortName() + " wins." : QString("Time out: you win!");
    const bool draw = !m_model.CanMate(m_position, !white);
    emit gameOver(text, draw ? "1/2-1/2" : white ? "0-1" : "1-0");   // the flagged side loses (White flagged: 0-1)
}

// From the Time menu: the next game's control; this game's too if nothing has been played yet.
void Game::setTimeControl(bool timed, long long baseMs, long long incMs) {
    m_nextTimed = timed;
    m_nextBaseMs = baseMs;
    m_nextIncMs = incMs;
    if (m_moves.isEmpty())
        applyTimeControl();   // nothing played yet: this game takes it at once
}

// The chosen control becomes this game's: fresh clocks, full time.
void Game::applyTimeControl() {
    m_timed = m_nextTimed;
    m_baseMs = m_nextBaseMs;
    m_incMs = m_nextIncMs;
    m_clock = ChessClock(m_baseMs, m_baseMs, m_incMs, m_incMs);
    publishClocks();          // show it at once (or hide it: untimed)
    emit timeControlChanged();
}

// This game's control as text, for the mode status.
QString Game::timeControlText() const {
    if (!m_timed)
        return "Untimed";
    return QString("%1+%2").arg(m_baseMs / 60000).arg(m_incMs / 1000);   // "5+3"
}

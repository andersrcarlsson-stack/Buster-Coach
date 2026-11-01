// saving.cpp — games as PGN (pgn_design.md): autosave into a folder per month, Save game…, Forget last game, Open game….

#include "mainwindow.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QApplication>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QDate>
#include <QDateTime>
#include "game.h"
#include "pgn.h"
#include "evalbar.h"
#include "playerstrip.h"
#include "gamespanel.h"
#include "boardwidget.h"
#include "move.h"
#include "engine.h"
#include "theme.h"



// The autosave folder: ~/Documents/Buster Coach (the month folders are made inside it as needed).
QString MainWindow::gamesFolder() {
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Buster Coach";
}

// The record as PGN into a file. False: it couldn't be written (said in the engine log).
bool MainWindow::saveGame(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_log->appendPlainText("SAVE: can't write " + path);
        return false;
    }
    file.write(QByteArray::fromStdString(writePgn(m_record)));
    return true;
}

// Autosave: this game's file, named at its first save (later saves overwrite it): the month's folder,
// "2026-10-25 11.10 You vs Buster 7.0.2.pgn". Games under 10 plies aren't kept (quick restarts).
void MainWindow::autosave() {
    if (m_loaded)
        return;                                          // it came from a file: it is one already
    if (!m_autosaveAction->isChecked() || m_record.liveCount() < 10)
        return;                                          // switched off (a test session), or a quick restart
    if (m_gameFile.isEmpty()) {
        const GameInfo &info = m_record.info();
        const QString date = QString::fromStdString(info.date).replace('.', '-');      // "2026-10-25"
        QString name = date + " " + QString::fromStdString(info.time).replace(':', '.') + " "
                       + QString::fromStdString(info.white + " vs " + info.black);
        name.replace(QRegularExpression(R"([/\\:*?"<>|])"), "_");   // characters some file systems refuse
        const QString folder = gamesFolder() + "/" + date.left(7);   // "2026-10"
        QDir().mkpath(folder);                                        // made if missing, with its parents
        m_gameFile = folder + "/" + name + ".pgn";
        for (int n = 2; QFile::exists(m_gameFile); ++n)               // the same minute twice: "… (2).pgn"
            m_gameFile = folder + "/" + name + QString(" (%1).pgn").arg(n);
    }
    if (saveGame(m_gameFile)) {
        m_lastSaved = m_gameFile;
        m_forgetAction->setEnabled(true);
    }
}

// A note changed (movelist_design.md 5): during the game it's saved with the game at its end; after the game the
// game's file is written again (autosave's own rules); a loaded game: its file, when notes may go back into it.
void MainWindow::saveNotes() {
    if (m_loaded) {
        if (!m_loadedFile.isEmpty())
            saveGame(m_loadedFile);
    } else if (m_gameOver) {
        autosave();
    }
}

// A game being left unfinished (a new game, quitting): kept too, with PGN's "*", if it's long enough (autosave).
void MainWindow::saveAbandoned() {
    if (m_loaded)
        return;                                          // it came from a file: it is one already
    if (m_record.info().result != "*")
        return;                                      // finished: saved when it ended
    m_record.info().ending = "Not finished.";
    autosave();
}

// A PGN file's first game onto the board, for browsing: the moves into Game, the read record (values, marks, notes)
// in place of the empty one. Your side: "You" in our files, else White.
void MainWindow::openGame(const QString &path, int index) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const std::vector<PgnGame> games = readPgn(file.readAll().toStdString());
    if (index < 0 || index >= int(games.size())) {
        QMessageBox::warning(this, "Open game", "No such game in " + path);
        return;
    }
    const PgnGame &game = games[index];
    const GameInfo &info = game.record.info();
    QStringList moves;
    for (const MoveEntry *m : game.record.liveMoves())
        moves << QString::fromStdString(m->uci);
    const QString start = info.startFen.empty() ? Position::StartFen
                                                : QString::fromStdString(info.startFen);
    if (!m_game->loadGame(start, moves, info.black != "You"))
        return;
    m_record = game.record;                           // after loadGame: its newGameStarted emptied the old one
    m_loaded = true;
    // Your notes go back into the file only when it's one of ours with just this game (movelist_design.md 5): a file
    // from elsewhere, or with many games, isn't rewritten (its other games and tags would be lost).
    const bool ours = QFileInfo(path).absoluteFilePath().startsWith(QDir(gamesFolder()).absolutePath() + "/");
    m_loadedFile = ours && games.size() == 1 ? path : QString();
    m_playFromWhite->setEnabled(true);
    m_playFromBlack->setEnabled(true);
    m_gameOver = true;                                // the game row: nothing to resign or offer
    updateGameRow();
    updateGraph();
    // The players as the file names them (our files: "You" and the engine), and the end's value and comment.
    m_playerStrip->setPlayer(QString::fromStdString(info.black == "You" ? info.black : info.white), info.black != "You");
    m_opponentStrip->setPlayer(QString::fromStdString(info.black == "You" ? info.white : info.black), info.black == "You");
    showLoadedMove(-1);
    if (!game.problem.empty())
        QMessageBox::information(this, "Open game", "The game stops early: " + QString::fromStdString(game.problem));
}

// Browsing a loaded game: the viewed move's value on the eval bar and in words, and its comment ("Best was …", the
// notes) on the note card. ply: as Game's viewChanged (-1 = the end).
void MainWindow::showLoadedMove(int ply) {
    if (!m_loaded)
        return;
    const std::vector<const MoveEntry *> live = m_record.liveMoves();
    const int index = (ply == -1 ? int(live.size()) : ply) - 1;   // the move that led to the viewed position
    m_coachNote.clear();
    if (index < 0 || index >= int(live.size())) {                 // the start: nothing played yet
        updateCoachLine();
        return;
    }
    const MoveEntry &move = *live[index];
    if (move.after >= 0) {
        m_evalBar->setWinChance(move.after);
        m_evalWords->setText(QString::fromStdString(chanceWords(move.after)));
    }
    Position model;
    const Position::Chess before = m_game->positions()[index];
    const bool bad = move.scored && move.quality == MoveQuality::Bad;
    QStringList parts;
    if (!move.bestMove.empty() && move.bestMove != move.uci)
        if (const auto best = model.UciToMove(before, move.bestMove))
            parts << "Best was " + QString::fromStdString(model.MoveToSan(before, *best)) + ".";
    for (const std::string &note : move.notes)
        parts << QString::fromStdString(note);
    if (bad && parts.isEmpty())
        parts << "A mistake.";
    if (!parts.isEmpty()) {
        const QString number = QString::number(before.fullmove_number) + (before.pit ? "... " : ". ");
        m_coachNote = number + QString::fromStdString(move.san) + (bad ? "?" : "") + ": " + parts.join(" ");
    }
    updateCoachLine();
}

// A game's line in the list, from its tags: "25 Oct 12:16 · You – Buster 7.0.2 · 0-1 · Danish Gambit".
static QString gameLabel(const GameInfo &info) {
    const QDate date = QDate::fromString(QString::fromStdString(info.date), "yyyy.MM.dd");
    QString label = date.isValid() ? date.toString("d MMM") : QString::fromStdString(info.date);
    if (!info.time.empty())
        label += " " + QString::fromStdString(info.time);
    label += " · " + QString::fromStdString(info.white + " – " + info.black + " · " + info.result);
    if (!info.opening.empty())
        label += " · " + QString::fromStdString(info.opening);
    return label;
}

// Every game in one file as list lines (a lichess export can hold many).
QList<GameListEntry> MainWindow::fileGames(const QString &path) {
    QList<GameListEntry> games;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return games;
    const std::vector<PgnGame> read = readPgn(file.readAll().toStdString());
    Position model;
    Moves mover;
    for (std::size_t i = 0; i < read.size(); ++i) {
        // The final position: the game's moves replayed (the reader played them all once already: they're legal).
        const GameInfo &info = read[i].record.info();
        const std::optional<Position::Chess> start = model.FenInput(
            info.startFen.empty() ? Position::StartFen : info.startFen);
        if (!start)
            continue;                                 // a FEN that can't be read: not listed
        Position::Chess pos = *start;
        for (const MoveEntry *m : read[i].record.liveMoves())
            if (std::optional<Position::Move> move = model.UciToMove(pos, m->uci))
                pos = mover.make(pos, *move);
        games << GameListEntry{gameLabel(info), path, int(i), QString::fromStdString(model.FenOutput(pos)),
                               info.black == "You"};
    }
    return games;
}

// A month folder's games, newest first: the file names begin with the date and time, so the names sorted backwards.
QList<GameListEntry> MainWindow::monthGames(const QString &month) {
    const QDir folder(gamesFolder() + "/" + month);
    QList<GameListEntry> games;
    QApplication::setOverrideCursor(Qt::WaitCursor);   // a big month takes a moment: the mouse says so
    for (const QString &name : folder.entryList({"*.pgn"}, QDir::Files, QDir::Name | QDir::Reversed))
        games << fileGames(folder.filePath(name));
    QApplication::restoreOverrideCursor();             // every set needs its restore: they stack
    return games;
}

// Play from here: the loaded game goes on from the viewed move as your game. The record first (the opponent may move
// at once, and its move goes into the record): the moves up to there, with their values and notes; the game's data
// becomes yours (the start position and the opening stay). Then Game plays on.
void MainWindow::playFromHere(bool humanIsWhite) {
    if (!m_loaded)
        return;
    const int ply = m_game->viewedPly();
    GameRecord kept;
    kept.start();
    GameInfo &info = kept.info();
    info = m_record.info();                           // the start position and the opening
    const std::string opponent = m_engine->name().toStdString();
    info.white = humanIsWhite ? "You" : opponent;
    info.black = humanIsWhite ? opponent : "You";
    const QDateTime now = QDateTime::currentDateTime();
    info.date = now.toString("yyyy.MM.dd").toStdString();
    info.time = now.toString("HH:mm").toStdString();
    info.timeControl = m_game->timeControlPgn().toStdString();
    info.level = m_game->level();
    info.result = "*";
    info.ending.clear();
    const std::vector<const MoveEntry *> live = m_record.liveMoves();
    for (int i = 0; i < ply && i < int(live.size()); ++i)
        kept.addMove(*live[i]);
    m_record = kept;
    m_loaded = false;                                 // your game now: saved like any other, into a new file
    m_gameFile.clear();
    m_gameOver = false;
    m_playFromWhite->setEnabled(false);
    m_playFromBlack->setEnabled(false);
    m_coachNote.clear();
    m_game->continueFrom(ply, humanIsWhite);
    updateGameRow();
    updateGraph();
    updateCoachLine();
}

void MainWindow::setupSaving() {
    // A game ends: it's saved by itself (setupRecord's listener has put the result into the record already).
    connect(m_game, &Game::gameOver, this, &MainWindow::autosave);
    connect(m_game, &Game::newGameStarted, this, [this]() {   // a new game: a new file, and not a loaded one (yet)
        m_gameFile.clear();
        m_loaded = false;
        m_playFromWhite->setEnabled(false);           // nothing loaded to play on from
        m_playFromBlack->setEnabled(false);
    });
    // Game → Open game…: the page with your newest month's games; another month, or any file, from there.
    connect(m_openAction, &QAction::triggered, this, [this]() {
        const QStringList months = QDir(gamesFolder()).entryList({"[0-9][0-9][0-9][0-9]-[0-9][0-9]"},
                                                                 QDir::Dirs | QDir::NoDotAndDotDot,
                                                                 QDir::Name | QDir::Reversed);   // months only, newest first
        showPage(m_gamesPanel);                       // first: pausing puts the live position back on the board
        m_gamesPanel->setMonths(months);
        m_gamesPanel->setGames(months.isEmpty() ? QList<GameListEntry>() : monthGames(months.first()));
    });
    connect(m_gamesPanel, &GamesPanel::monthChosen, this, [this](const QString &month) {
        m_gamesPanel->setGames(monthGames(month));
    });
    // A row chosen on the page: its final position on the board (Cancel's resume puts the live game back).
    connect(m_gamesPanel, &GamesPanel::previewRequested, this, [this](const QString &fen, bool flipped) {
        Position model;
        if (const std::optional<Position::Chess> pos = model.FenInput(fen.toStdString())) {
            m_board->setPosition(*pos);
            m_board->setFlipped(flipped);
            m_board->setLastMove(-1, -1);                 // the live game's marks don't belong to it
            m_board->setGameState(Position::GameState::Ongoing);
        }
    });
    connect(m_gamesPanel, &GamesPanel::otherFileRequested, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "Open game", QDir::homePath(),
                                                          "PGN files (*.pgn);;All files (*)", nullptr,
                                                          fileDialogOptions());   // dark: Qt's own dialog
        if (!path.isEmpty())
            m_gamesPanel->setGames(fileGames(path));
    });
    connect(m_gamesPanel, &GamesPanel::openRequested, this, [this](const QString &path, int index) {
        showPlay();
        openGame(path, index);
    });
    connect(m_gamesPanel, &GamesPanel::cancelled, this, [this]() {
        showPlay();
        m_game->resume();                             // Cancel: the game goes on
    });

    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::saveAbandoned);   // quitting mid-game

    // Game → Save game…: the game as it stands, anywhere (the dialog starts in the games folder).
    connect(m_saveAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, "Save game", gamesFolder(), "PGN files (*.pgn)",
                                                          nullptr, fileDialogOptions());   // dark: Qt's own dialog
        if (!path.isEmpty())
            saveGame(path.endsWith(".pgn") ? path : path + ".pgn");
    });
    
    // Game → Forget last game: the newest autosaved file to the trash (a test run, noticed afterwards).
    m_forgetAction->setEnabled(false);                  // nothing saved yet
    connect(m_forgetAction, &QAction::triggered, this, [this]() {
        QFile::moveToTrash(m_lastSaved);
        if (m_lastSaved == m_gameFile)
            m_gameFile.clear();                         // this game: if saved again, it gets a new file
        m_lastSaved.clear();
        m_forgetAction->setEnabled(false);
    });
    connect(m_game, &Game::viewChanged, this, &MainWindow::showLoadedMove);   // browsing: each move speaks
    
    m_playFromWhite->setEnabled(false);               // until a game is loaded
    m_playFromBlack->setEnabled(false);
    connect(m_playFromWhite, &QAction::triggered, this, [this]() { playFromHere(true); });
    connect(m_playFromBlack, &QAction::triggered, this, [this]() { playFromHere(false); });
}

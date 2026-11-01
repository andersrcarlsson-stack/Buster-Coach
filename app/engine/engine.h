// engine.h — a UCI engine (Buster) running as a child process.
//
// Wraps a QProcess and turns the engine's text output into signals, so the rest of the
// app never deals with pipes, chunks or partial lines. It does NOT know about the board
// or the game: whoever is interested connects to its signals.

#pragma once
#include <QObject>
#include <QProcess>
#include "uci_info.h"

class Engine : public QObject
{
    Q_OBJECT    // needed for signals (moc)

signals:
    // Every complete line the engine prints (without the trailing newline).
    void lineReceived(const QString &line);

    // The uci/uciok + isready/readyok handshake has finished: the engine accepts commands.
    void ready();

    // Every command we send (for the log). Emitted by send().
    void commandSent(const QString &cmd);

    // The engine couldn't start, or stopped unexpectedly. Text for the user.
    void error(const QString &message);

    void bestMove(const QString &move);   // the move from a "bestmove ..." line, e.g. "e7e5"

    // A search-progress line with a score (depth, cp or mate), already parsed.
    void infoReceived(const UciInfo &info);

public:
    // Standard Qt constructor: the parent (if any) will own and delete this object.
    // `explicit`: no silent conversion from a QObject* to an Engine.
    explicit Engine(QObject *parent = nullptr);

    // Sends "quit" and waits for the engine to exit, so it isn't killed abruptly.
    ~Engine() override;

    // Start the engine program at path (stopping a running one first) and begin the UCI handshake.
    void start(const QString &path);

    // Ends the engine process: "quit", and if it doesn't go within a second (stuck in a search), kill it. Blocks for
    // at most ~2 s. start() calls it first, so start() is also a restart.
    void stop();

    // The engine's name from its "id name" line, e.g. "Stockfish 16" (known from its handshake on), and the short form
    // for sentences: the first word ("Stockfish"). Before the handshake: "Engine".
    QString name() const { return m_name.isEmpty() ? QString("Engine") : m_name; }
    QString shortName() const { return name().section(' ', 0, 0); }

    // Send one command line (the newline is added here) and emit commandSent.
    void send(const QString &cmd);

private:
    // Where we are in the start-up handshake. Lets us react to "uciok"/"readyok" only
    // when we are actually waiting for them (not when the user types "isready" later).
    enum class State { Starting, WaitingUciOk, WaitingReadyOk, Ready };
    State m_state = State::Starting;

    // The Hash to set during the handshake (engine_design.md: at least 128 MB); nullopt = leave the engine's own.
    std::optional<int> m_hash;
    QString m_name;   // from "id name" ("" until the engine has said it)


    // A plain member (not new-ed): it lives and dies with the Engine (RAII).
    QProcess m_process;
};

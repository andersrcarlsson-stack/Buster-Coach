// engine.cpp — talking UCI to the engine process.
//
// Sections:
//   1. Construction: reading the engine's output line by line; process errors
//   2. Shutting down politely
//   3. Starting the engine, sending commands

#include "engine.h"

// =================================================================================================
// 1. CONSTRUCTION: reading the engine's output line by line; process errors
// =================================================================================================
Engine::Engine(QObject *parent)
    : QObject(parent)   // hand the parent to the base class: that registers the ownership
{
    // Output arrives in chunks whenever the engine writes; Qt emits readyReadStandardOutput
    // for each. `this` as the context object: if the Engine dies, the connection is removed,
    // so the lambda can never run with a dangling `this`.
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        // Chunks are not lines: one chunk may hold many lines, or half a line. QProcess
        // buffers everything; canReadLine() = at least one COMPLETE line is waiting, and an
        // incomplete tail stays buffered until the rest arrives. `while`, not `if`.
        while (m_process.canReadLine()) {
            QString line = QString::fromUtf8(m_process.readLine()).trimmed();  // bytes -> text, drop "\n"
            emit lineReceived(line);

            // "info depth 8 score cp -35 ...": parsed by plain C++ (uci_info), tested by uci_test.
            if (const auto info = parseInfo(line.toStdString()))
                emit infoReceived(*info);

            // Start-up handshake: uci -> uciok, then isready -> readyok. Only react when the
            // state says we're waiting for that answer.
            if (m_state == State::WaitingUciOk && line.startsWith("id name "))
                m_name = line.mid(8);                       // "id name Stockfish 16" -> "Stockfish 16"
            if (m_state == State::WaitingUciOk && line.startsWith("option name Hash "))
                m_hash = hashSetting(line.toStdString());   // the engine introduces its options before "uciok"
            if (m_state == State::WaitingUciOk && line == "uciok") {
                m_state = State::WaitingReadyOk;
                if (m_hash)                                 // options go between "uciok" and "isready"
                    send(QString("setoption name Hash value %1").arg(*m_hash));
                send("isready");
            } else if (m_state == State::WaitingReadyOk && line == "readyok") {
                m_state = State::Ready;
                emit ready();
            }

            // "bestmove e7e5 ponder g1f3": listeners only want the move, the second word.
            // (Can be "(none)" at game end; Game::applyUci ignores moves that aren't legal.)
            if (line.startsWith("bestmove "))
                emit bestMove(line.split(' ').at(1));
        }
    });

    // Process problems (wrong path, not executable, crash) become a readable error signal.
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit error("Could not start the engine:\n" + m_process.program()
                       + "\n\n(" + m_process.errorString() + ")");
        else if (e == QProcess::Crashed)
            emit error("The engine stopped unexpectedly.");
    });
}

// =================================================================================================
// 2. SHUTTING DOWN POLITELY
// =================================================================================================
// Runs when the owner (the main window) deletes us. The body runs BEFORE the members are
// destroyed, so m_process is still usable here. Ask the engine to exit politely; otherwise
// QProcess's destructor would kill it and warn "Destroyed while process is still running".
Engine::~Engine() {
    stop();
}

void Engine::stop(){
    if (m_process.state() == QProcess::NotRunning)
        return;                                   // nothing to stop (never started, or already gone)
    // A raw write, not send(): no signals (from the destructor, the listeners may already be gone).
    m_process.write("quit\n");
    // A blocking wait: normally wrong in a GUI (it freezes), acceptable for at most a second or two.
    if (!m_process.waitForFinished(1000)) {       // stuck in a search, deaf to "quit" (the engine tests meet these)
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    m_state = State::Starting;                    // a later start() begins the handshake afresh
}


// =================================================================================================
// 3. STARTING THE ENGINE, SENDING COMMANDS
// =================================================================================================
void Engine::start(const QString &path) {
    stop();                                 // a restart: the old process goes first
    m_hash.reset();
    m_name.clear();                         // a (re)start: this engine's options are read again
    m_process.start(path);
    send("uci");
    // Safe to set the state after send(): the answer can only be read later, from the
    // event loop, never during this function.
    m_state = State::WaitingUciOk;
}

// The single place where commands are written: logging and the "\n" can't be forgotten.
void Engine::send(const QString &cmd) {
    emit commandSent(cmd);
    m_process.write((cmd + "\n").toUtf8());   // text -> bytes; UCI engines read whole lines
}

// practice.cpp — opening practice's wiring (openings_design.md "Opening practice"): in and out of its page.

#include "mainwindow.h"
#include <QAction>
#include "game.h"
#include "practicepanel.h"

// "Be3", "Bg5", "Be2" → "Be3, Bg5 or Be2".
static QString joinOr(const QStringList &items) {
    if (items.size() < 2)
        return items.join("");
    return items.mid(0, items.size() - 1).join(", ") + " or " + items.last();
}

void MainWindow::setupPractice() {
    // The tree from the names, once: each named opening with its share of all the book's games.
    Position model;
    const auto start = model.FenInput(Position::StartFen).value();
    const double all = m_book.games(model.Key(start));
    QList<PracticeOpening> openings;
    for (const auto &[key, named] : m_openings.all()) {
        const int games = m_book.games(key);
        openings.append({QString::fromStdString(named.name), QString::fromStdString(key), games > 0 ? games / all : -1.0});
    }
    m_practice->setOpenings(openings);
    connect(m_practiceAction, &QAction::triggered, this, [this]() { showPage(m_practice); });   // the game rests
    connect(m_practice, &PracticePanel::cancelled, this, [this]() {
        showPlay();
        m_game->resume();                            // Cancel: the game goes on
    });
    connect(m_practice, &PracticePanel::startRequested, this, [this](const QString &key, bool humanIsWhite) {
        showPlay();
        const auto named = m_openings.find(key.toStdString());
        if (named)
            m_game->newPracticeGame(QString::fromStdString(named->moves), QString::fromStdString(named->name),
                                    humanIsWhite);
    });

    // Your turn in practice: which legal moves keep the game inside the opening, and the book's most played among them.
    // Empty when the game is already out of it (or you left it): then practice says nothing more.
    connect(m_game, &Game::humanTurnStarted, this, [this]() {
        m_practiceInside.clear();
        m_practiceAlternatives.clear();
        m_practiceBest.clear();
        if (!m_game->isPractice() || m_practiceLeft) {
            m_practiceOn = false;
            updateMode();
            return;
        }
        const Position::Chess &now = m_game->positions().last();
        Position model;
        for (const Position::Move &move : model.LegalMoves(now).moves) {
            const std::string uci = model.MoveToUci(move);
            if (m_game->staysInside(uci))
                m_practiceInside.insert(QString::fromStdString(uci));
        }
        for (const BookMove &m : m_book.moves(model.Key(now))) {   // the most played first
            if (m_practiceAlternatives.size() == 3)
                break;
            if (!m_practiceInside.contains(QString::fromStdString(m.uci)))
                continue;
            if (const auto move = model.UciToMove(now, m.uci))
                m_practiceAlternatives << QString::fromStdString(model.MoveToSan(now, *move));
            if (m_practiceBest.isEmpty())
                m_practiceBest = QString::fromStdString(m.uci);
        }
        m_practiceOn = !m_practiceInside.isEmpty();   // still inside the opening (the opponent may have left it)
        updateMode();
        updateHint();                                // H can follow the opening at once (not only after the master)
    });
    // Your move left the opening: the card says so, once, with what the book plays inside it.
    connect(m_game, &Game::movePlayed, this, [this](const QString &uci, const QString &, bool human) {
        if (!human || m_practiceInside.isEmpty())
            return;
        const bool left = !m_practiceInside.contains(uci);
        m_practiceInside.clear();                    // used up: it was this position's (⏩ plays several moves at once)
        if (!left)
            return;
        m_practiceLeft = true;
        m_practiceOn = false;                        // the status's "Opening practice" goes with the card's arrival
        updateMode();
        QString text = "You left the " + shortOpening(m_game->practiceName());
        if (!m_practiceAlternatives.isEmpty())
            text += ": the book plays " + joinOr(m_practiceAlternatives);
        m_coachNote = text + ". Take back to try again.";
        updateCoachLine();
    });
    // A takeback or a new game: inside again (as far as practice knows), so leaving is told again.
    connect(m_game, &Game::movesTakenBack, this, [this]() { m_practiceLeft = false; });
    connect(m_game, &Game::newGameStarted, this, [this]() {
        m_practiceLeft = false;
        m_practiceOn = m_game->isPractice();         // from the first move on (as Black, before your first turn)
        updateMode();
    });
}

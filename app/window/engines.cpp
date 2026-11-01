// engines.cpp — the engine setup's wiring (engine_design.md): in and out of the engine page.

#include "mainwindow.h"
#include <QAction>
#include "enginepanel.h"
#include "game.h"
#include "engine.h"
#include "mastercoach.h"
#include "playerstrip.h"

void MainWindow::setupEngines() {
    m_engines->setBuiltIn(bundledEngine());     // the Buster beside the app: always in the page's library
    connect(m_enginesAction, &QAction::triggered, this, [this]() {
        m_engines->open();                           // the library and the roles as last saved
        showPage(m_engines);
    });
    // OK: the page has saved the choices; a role whose file changed restarts with it, then the game goes on (the new
    // engine gets the whole position with its first move request). Cancel: just the game.
    connect(m_engines, &EnginePanel::accepted, this, [this]() {
        const QString opponent = savedEngine("engines/opponent");
        const QString coach = savedEngine("engines/coach");
        if (opponent != m_opponentPath) {
            m_opponentPath = opponent;
            m_opponentStrip->setRatingsShown(m_opponentPath == bundledEngine());   // measured with it only
            m_game->engineRestarted();                // first: forget the old process's search ...
            m_engine->start(opponent);                // ... then the old one goes and the new one starts
        }
        if (coach != m_coachPath) {
            m_coachPath = coach;
            m_coach->engineRestarted();
            m_master->engineRestarted();
            m_coachEngine->start(coach);
            m_masterEngine->start(coach);
        }
        showPlay();
        m_game->resume();                             // waits for a restarted opponent's "ready" (5b)
    });
    connect(m_engines, &EnginePanel::cancelled, this, [this]() {
        showPlay();
        m_game->resume();
    });
}

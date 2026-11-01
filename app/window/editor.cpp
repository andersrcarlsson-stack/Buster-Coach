// editor.cpp — the board editor's wiring (editor_design.md): in and out of the editor page.

#include "mainwindow.h"
#include <QAction>
#include <QStackedWidget>
#include "editorpanel.h"
#include "game.h"
#include "boardwidget.h"

void MainWindow::setupEditor() {
    // In: the game rests, everything that could touch it is switched off, the bar's place stays empty.
    connect(m_setupAction, &QAction::triggered, this, [this]() {
        showPage(m_editor);                          // the game rests (mainwindow.cpp)
        m_board->setEditMode(true);
        m_board->setLastMove(-1, -1);                // the game's marks don't belong to an edited board
        m_board->setGameState(Position::GameState::Ongoing);
        m_editor->start(m_game->currentFen());
    });
    // Out: everything back as it was. Cancel: the game goes on. Play: a new game from the editor's position.
    auto leave = [this]() {
        m_board->setEditMode(false);
        showPlay();
    };
    connect(m_editor, &EditorPanel::cancelled, this, [this, leave]() {
        leave();
        m_game->resume();
    });
    connect(m_editor, &EditorPanel::playRequested, this, [this, leave](const QString &fen, bool humanIsWhite) {
        leave();
        m_game->newGameFromFen(fen, humanIsWhite);   // valid: the button was greyed otherwise
    });
    // While editing: the board's clicks go to the editor, and the editor's position goes to the board.
    connect(m_board, &BoardWidget::editClicked, m_editor, &EditorPanel::squareClicked);
    connect(m_editor, &EditorPanel::positionChanged, m_board, &BoardWidget::setPosition);
    connect(m_editor, &EditorPanel::flipRequested, this, [this]() { m_board->setFlipped(!m_board->isFlipped()); });

}

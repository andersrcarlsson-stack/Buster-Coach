// editorpanel.h — the board editor's side panel page (editor_design.md): "Set up a position". It grows step by step;
// so far a title and Cancel.
#pragma once
#include <QFrame>
#include "position.h"

class QButtonGroup;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;


class QRadioButton;

class EditorPanel : public QFrame
{
    Q_OBJECT

signals:
    void cancelled();                                       // Cancel: back to the game, unchanged
    void positionChanged(const Position::Chess &position);  // the position being edited changed: show it
    void playRequested(const QString &fen, bool humanIsWhite);   // Play as White / Black: a new game from here
    void flipRequested();                                   // Flip: turn the board (the board is MainWindow's)


public:
    explicit EditorPanel(QWidget *parent = nullptr);
    void start(const QString &fen);                         // editing begins from this position
    void squareClicked(int square, bool right);             // from the board: place the tool, or clear (right)

protected:
    void changeEvent(QEvent *event) override;              // the palette changed (light / dark): redraw the icons

private:
    Position m_model;
    Position::Chess m_position;                             // the position being edited
    int m_tool = 10;                                        // the piece to place (1-12), 0 = eraser. The palette sets it
    QButtonGroup *m_tools = nullptr;                        // the palette: button id = the piece code (0 = eraser)
    void updateToolIcons();                                 // the pieces drawn as the board draws them
    QRadioButton *m_whiteToMove = nullptr;                  // side to move: two radio buttons
    QRadioButton *m_blackToMove = nullptr;
    void changed();                                         // the position changed: the controls follow, the board is told
    QCheckBox *m_castling[4] = {};                          // White O-O, White O-O-O, Black O-O, Black O-O-O
    QLineEdit *m_ep = nullptr;                              // the en passant square, typed by hand ("" = none)
    QLineEdit *m_fen = nullptr;                             // the position as FEN: follows every change; Enter loads one
    QLabel *m_validity = nullptr;                           // the first problem (Validate), or "A legal position"
    QPushButton *m_playWhite = nullptr;                     // greyed while the position is impossible
    QPushButton *m_playBlack = nullptr;

};


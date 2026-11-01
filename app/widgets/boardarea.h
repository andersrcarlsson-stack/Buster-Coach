// boardarea.h — the board and its eval bar, kept together: the board is always square, the bar
// sits right beside it (as tall as the board), and the pair is centred in the space given.
// A layout can't do "square", so this widget places its two children by hand on every resize.

#pragma once
#include <QWidget>

class BoardWidget;
class EvalBar;

class BoardArea : public QWidget
{
    Q_OBJECT

public:
    BoardArea(BoardWidget *board, EvalBar *bar, QWidget *parent = nullptr);

public slots:
    // Show or hide the eval bar; its strip stays reserved (the board never moves)
    void setBarVisible(bool visible);

protected:
    void resizeEvent(QResizeEvent *event) override;   // "your size changed": place the children

private:
    BoardWidget *m_board;   // children of this widget: owned through the parent-child tree
    EvalBar *m_bar;
    void placeChildren();   // the geometry: used on resize and when the bar is switched
};

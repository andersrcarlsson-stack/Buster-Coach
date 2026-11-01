// boardarea.cpp — placing the board and the bar.

#include "boardarea.h"
#include "boardwidget.h"
#include "evalbar.h"

BoardArea::BoardArea(BoardWidget *board, EvalBar *bar, QWidget *parent)
    : QWidget(parent), m_board(board), m_bar(bar)
{
    m_board->setParent(this);   // both become children: drawn inside this area, deleted with it
    m_bar->setParent(this);
}

void BoardArea::resizeEvent(QResizeEvent *) {
    placeChildren();
}

void BoardArea::setBarVisible(bool visible) {
    m_bar->setVisible(visible);
    placeChildren();   // hiding a child doesn't resize this widget, so recompute here
}

// The largest square board, with the bar's strip beside it (always reserved); the group centred.
void BoardArea::placeChildren() {
    // The bar's strip is always reserved, shown or not: switching the bar never moves the board (eval bar v3).
    const int gap  = 6;
    const int barW = m_bar->minimumWidth();
    const int side = qMin(height(), width() - barW - gap);
    if (side <= 0)
        return;
    const int x0 = (width() - (barW + gap + side)) / 2;
    const int y0 = (height() - side) / 2;
    m_bar->setGeometry(x0, y0, barW, side);
    m_board->setGeometry(x0 + barW + gap, y0, side, side);
}

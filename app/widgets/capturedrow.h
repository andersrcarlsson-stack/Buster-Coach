// capturedrow.h — the pieces one player has captured (Looks, 2026-10-21): grouped by type (P N B R Q), overlapping
// within a group, drawn like the board's pieces. Their size follows the width, up to 28 high, so that the full
// set plus two promoted queens always fits.
#pragma once
#include <QList>
#include <QWidget>

class CapturedRow : public QWidget
{
public:
    explicit CapturedRow(QWidget *parent = nullptr);
    void setPieces(const QList<int> &codes);   // the captured pieces' codes (1-12, one colour), any order

    // The piece size for a width (the widest row just fits), at most maxPiece. The row's own height is fixed
    // (maxPiece): no height-for-width for the layout, which Qt's minimum sizes don't count properly.
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QList<int> m_pieces;   // sorted by code: pawns, knights, bishops, rooks, queens
};

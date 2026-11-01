// capturedrow.cpp — the captured pieces' row (see capturedrow.h).

#include "capturedrow.h"
#include "piecepainter.h"
#include <QPainter>
#include <algorithm>

// The widest row we plan for, in piece widths: the full set (8 P, 2 N, 2 B, 2 R, 1 Q) plus two promoted queens:
// 12 steps within groups × 0.45 + 4 new groups × 0.85 + the last piece = 9.8, rounded up.
static constexpr double rowUnits = 10.0;
static constexpr double sameStep = 0.45;   // the next piece of the same type: 45 % of a piece further (overlapping)
static constexpr double nextStep = 0.85;   // a new type: 85 %

CapturedRow::CapturedRow(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // the width comes from the strip, never from us
}

void CapturedRow::setPieces(const QList<int> &codes) {
    m_pieces = codes;
    std::sort(m_pieces.begin(), m_pieces.end());   // one colour: by code = by type, pawns first
    update();
}

int CapturedRow::heightForWidth(int width) const {
    return int(width / rowUnits);                  // the piece size: the widest row just fits
}

QSize CapturedRow::sizeHint() const {
    return QSize(200, heightForWidth(200));
}

void CapturedRow::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int size = heightForWidth(width());
    double x = 0;
    int previousType = -1;
    for (int code : m_pieces) {
        const int type = (code + 1) / 2;
        if (previousType != -1)
            x += size * (type == previousType ? sameStep : nextStep);
        drawPiece(painter, QRect(int(x), 0, size, size), code, QColor(), panelRim(palette(), code));
        previousType = type;
    }
}

// evalbar.cpp — drawing the bar.

#include "evalbar.h"
#include <QPainter>
#include <algorithm>
#include "piecepainter.h"
#include <QFontMetrics>

EvalBar::EvalBar(QWidget *parent) : QWidget(parent) {
    setFixedWidth(48);   // room for the frame, the number ("62%") and the kings at the ends (eval bar v3)
}

// Two rectangles: the opponent's share at the top, the player's share at the bottom.
void EvalBar::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (!isEnabled())
        painter.setOpacity(0.3);   // greyed out: it describes the live game, not the position shown
    const int h = height();
    const int filled = int(h * m_chance);                                     // the player's share in pixels
    painter.fillRect(0, 0, width(), h - filled, QColor(205, 205, 205));       // opponent (top): neutral light grey
    painter.fillRect(0, h - filled, width(), filled, QColor(70, 130, 180));   // player (bottom): steel blue

    // The preview frame: from the bottom up to "if you play this", green (best) .. red (a big drop).
    if (m_previewChance >= 0.0) {
        const double t = std::min(m_previewDrop / 0.2, 1.0);                    // 0 = best move, 1 = drop of 20+ points
        const QColor colour = QColor::fromHsvF((1.0 - t) * 120.0 / 360.0, 0.9, 0.9);   // hue: 120° green .. 0° red
        const int pen = 6;                                                       // the coloured frame's width
        const int edge = pen + 2;                                                // the dark rim: 1 px more on each side
        const int radius = 0;                                                    // 0 = sharp corners; 6..10 = rounded (test)
        const int top = h - int(h * m_previewChance);
        const QRect frame(edge / 2, top + edge / 2, width() - edge, h - top - edge);   // inside the bar: a pen is centred on its line
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(30, 30, 30), edge, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));   // first the dark rim ...
        painter.drawRoundedRect(frame, radius, radius);
        painter.setPen(QPen(colour, pen, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));   // ... then the colour on top of it
        painter.drawRoundedRect(frame, radius, radius);
    }

    // The scale, on top of the fill and the frame (eval bar v3; drawn last, so the frame can't hide it):
    // ticks at the eval words' thresholds (chanceWords, lib/uci), short and faint at both edges ...
    const int w = width();
    painter.setPen(QPen(QColor(0, 0, 0, 170), 2));   // tune by eye: opacity 0..255, width in pixels
    for (double t : {0.10, 0.30, 0.43, 0.57, 0.70, 0.90}) {
        const int y = h - int(h * t);
        painter.drawLine(0, y, 8, y);
        painter.drawLine(w - 9, y, w - 1, y);
    }
    // ... and the midline across: above it you're better, below it worse.
    painter.setPen(QPen(QColor(0, 0, 0, 220), 3));
    painter.drawLine(0, h / 2, w, h / 2);

    // The number at the fill's edge: white on the blue, or dark just above the fill when the fill is short. Kept
    // between the ends (where the kings will sit), so it never covers them.
    QFont number = font();
    number.setPixelSize(13);
    number.setBold(true);
    painter.setFont(number);
    const QString text = m_text.isEmpty() ? QString::number(int(m_chance * 100 + 0.5)) + "%" : m_text;
    const bool inside = filled > 40;
    const int y = std::clamp(inside ? h - filled + 4 : h - filled - 20, w, h - w - 16);
    painter.setPen(inside ? QColor(Qt::white) : QColor(40, 40, 40));
    painter.drawText(QRect(0, y, w, 16), Qt::AlignCenter, text);

    // The kings at the ends, in their sides' colours: Buster at the top, you at the bottom (eval bar v3).
    const int k = w - 6;                                                     // a square inside the bar's edges
    drawPiece(painter, QRect(3, 3, k, k), m_playerWhite ? 11 : 12);          // Buster's king
    drawPiece(painter, QRect(3, h - k - 3, k, k), m_playerWhite ? 12 : 11);  // yours

    // The title, vertical and quiet, just below Buster's king (eval bar v3).
    painter.save();
    QFont titleFont = font();
    titleFont.setPixelSize(11);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    painter.setFont(titleFont);
    painter.setPen(QColor(110, 110, 110));
    const QString title = "WIN CHANCE";
    const QFontMetrics tm(titleFont);
    // Move the origin to where the text starts (its bottom end), then turn the coordinate system: the text runs upwards.
    painter.translate(w / 2.0 + tm.ascent() / 2.0 - 1, w + 22 + tm.horizontalAdvance(title));   // below the label's top slot
    painter.rotate(-90);
    painter.drawText(0, 0, title);
    painter.restore();
}

void EvalBar::setWinChance(double chance, const QString &text) {
    m_chance = chance;
    m_text = text;
    update();
}

void EvalBar::setPreview(double chance, double drop) {
    m_previewChance = chance;
    m_previewDrop = drop;
    update();
}

void EvalBar::clearPreview() {
    m_previewChance = -1.0;
    update();
}

void EvalBar::setPlayerWhite(bool white) {
    m_playerWhite = white;
    update();
}

// piecepainter.cpp — the two-layer piece (see piecepainter.h).

#include "piecepainter.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>


void drawPiece(QPainter &painter, const QRect &cell, int code, const QColor &tint, const QColor &rim) {
    static const QString glyphs = " ♟♙♞♘♝♗♜♖♛♕♚♔";      // index = piece code; odd = filled, even = outline
    const bool white = (code % 2 == 0);
    const QString outline(glyphs[white ? code : code + 1]);   // the outline twin of either colour
    QColor body = white ? QColor(250, 248, 240) : QColor(60, 60, 60);
    QColor line = Qt::black;
    if (tint.isValid()) {                                 // a state: White a light body + darker lines, Black the
        body = white ? tint.lighter(150) : tint;          // colour + much darker lines
        line = white ? tint.darker(160) : tint.darker(200);
    }
    painter.save();
    QFont font("Buster Pieces");                          // FreeSerif's chess glyphs, built in (main.cpp, resources.qrc)
    font.setPixelSize(int(cell.height() * 1.1));          // 10 % larger than the square (corner marks still clear)
    painter.setFont(font);
    // The body: the outline glyph's shape as a path, every contour filled solid (the filled glyphs have their
    // details cut out as holes). Placed where drawText(AlignCenter) puts the glyph: advance and height centred.
    const QFontMetricsF fm(font);
    const QPointF origin(cell.x() + (cell.width() - fm.horizontalAdvance(outline)) / 2,
                         cell.y() + (cell.height() - (fm.ascent() + fm.descent())) / 2 + fm.ascent());
    QPainterPath shape;
    shape.addText(origin, font, outline);
    if (rim.isValid()) {                                  // the rim: the contours stroked first; the body then covers
        painter.setPen(QPen(rim, std::max(1.5, cell.height() / 22.0)));   // the pen's inner half, the outer shows
        painter.setBrush(Qt::NoBrush);
        for (const QPolygonF &contour : shape.toSubpathPolygons())
            painter.drawPolygon(contour, Qt::WindingFill);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(body);
    for (const QPolygonF &contour : shape.toSubpathPolygons())
        painter.drawPolygon(contour, Qt::WindingFill);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(line);
    painter.drawText(cell, Qt::AlignCenter, outline);     // the lines on top
    painter.restore();
}

QPixmap piecePixmap(int code, int size, const QColor &rim) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);              // only the piece is opaque
    QPainter painter(&pixmap);                 // a painter can draw on an image just as on a widget
    painter.setRenderHint(QPainter::Antialiasing);
    drawPiece(painter, QRect(0, 0, size, size), code, QColor(), rim);
    return pixmap;                             // the painter ends here (its destructor), before the copy is used
}

QColor panelRim(const QPalette &palette, int code) {
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    return (dark && code % 2 == 1) ? QColor(170, 170, 170) : QColor();   // odd codes are Black
}

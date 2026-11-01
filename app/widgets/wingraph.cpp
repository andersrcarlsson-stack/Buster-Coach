// wingraph.cpp — the win-chance graph (see wingraph.h).

#include "wingraph.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

WinGraph::WinGraph(QWidget *parent) : QWidget(parent) {
    setFixedHeight(100);                                   // the width comes from the side panel
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);   // never widens the panel (as the coach line)
}

void WinGraph::setCurve(const QList<double> &values, const QList<bool> &red) {
    m_values = values;
    m_red = red;
    update();
}

void WinGraph::setShownPly(int ply) {
    m_shownPly = ply;
    update();
}

void WinGraph::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    painter.fillRect(r, palette().color(QPalette::Base));          // the list's background: light or dark
    const double mid = r.top() + r.height() / 2;
    const int span = std::max(80, int(m_values.size()));             // plies across the width: max(40, moves) moves
    auto x = [&](int ply) { return r.left() + r.width() * ply / span; };   // ply 1 = after the first move
    auto y = [&](double chance) { return r.bottom() - r.height() * chance; };

    // Ticks every 10 moves (20 plies), faint.
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    for (int ply = 20; ply < span; ply += 20)
        painter.drawLine(QPointF(x(ply), r.top()), QPointF(x(ply), r.bottom()));

    // The curve through the known values (an unknown one is bridged: the path just goes on to the next).
    QPainterPath line;
    int first = -1, last = -1;
    for (int i = 0; i < m_values.size(); ++i) {
        if (m_values[i] < 0)
            continue;
        const QPointF p(x(i + 1), y(m_values[i]));
        if (first < 0) {
            line.moveTo(p);
            first = i;
        } else {
            line.lineTo(p);
        }
        last = i;
    }
    if (first >= 0) {
        // The area between the curve and the midline: blue where you're better, grey where worse (clipped halves).
        QPainterPath area = line;
        area.lineTo(x(last + 1), mid);
        area.lineTo(x(first + 1), mid);
        area.closeSubpath();
        painter.save();
        painter.setClipRect(QRectF(r.left(), r.top(), r.width(), mid - r.top()));
        painter.fillPath(area, QColor(70, 130, 180, 150));         // the bar's steel blue
        painter.setClipRect(QRectF(r.left(), mid, r.width(), r.bottom() - mid));
        painter.fillPath(area, QColor(150, 150, 150, 150));
        painter.restore();
        painter.setPen(QPen(palette().color(QPalette::Text), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);
    }

    // The midline: 50 %, equal.
    painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
    painter.drawLine(QPointF(r.left(), mid), QPointF(r.right(), mid));

    // Your red moves: a red dot on the curve.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(200, 40, 40));
    for (int i = 0; i < m_values.size() && i < m_red.size(); ++i)
        if (m_red[i] && m_values[i] >= 0)
            painter.drawEllipse(QPointF(x(i + 1), y(m_values[i])), 3.5, 3.5);

    // The move shown: an amber line (the newest while playing).
    const int shown = m_shownPly < 0 ? int(m_values.size()) : m_shownPly;
    if (shown > 0) {
        painter.setPen(QPen(QColor(230, 160, 30), 2));
        painter.drawLine(QPointF(x(shown), r.top()), QPointF(x(shown), r.bottom()));
    }

    // The title, small and quiet.
    QFont small = font();
    small.setPixelSize(10);
    painter.setFont(small);
    painter.setPen(palette().color(QPalette::PlaceholderText));
    painter.drawText(r.adjusted(4, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop, "WIN CHANCE · you");
}

void WinGraph::mousePressEvent(QMouseEvent *event) {
    // The click's x back to a ply, on the paint's scale (paintEvent: the same rect and span).
    const QRectF r = QRectF(rect()).adjusted(2, 2, -2, -2);
    const int span = std::max(80, int(m_values.size()));
    const double ply = (event->position().x() - r.left()) / r.width() * span;
    emit plyClicked(std::clamp(int(std::lround(ply)), 0, int(m_values.size())));   // the nearest, within the game
}

// confidencedot.cpp — the dot (see confidencedot.h).

#include "confidencedot.h"
#include <QPainter>
#include <algorithm>

ConfidenceDot::ConfidenceDot(QWidget *parent) : QWidget(parent) {
    setFixedWidth(22);      // the dot and its glow ...
    setMinimumHeight(22);   // ... and as tall as the status bar row gives: the dot is drawn at the centre, so it
                            // sits in the row's middle, beside the words
    setToolTip("How settled the coach's verdicts are: yellow = still changing, green = settled");
}

void ConfidenceDot::setStable(int stable) {
    m_stable = std::clamp(stable, 0, 3);
    update();
}

void ConfidenceDot::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled())
        painter.setOpacity(0.3);                            // greyed while browsing, like the eval words
    // The colour: yellow mixed towards deep green by the stable passes (0..3).
    const QColor yellow(240, 200, 40), green(20, 130, 50);
    const double t = m_stable / 3.0;
    const QColor colour(int(yellow.red() + (green.red() - yellow.red()) * t),
                        int(yellow.green() + (green.green() - yellow.green()) * t),
                        int(yellow.blue() + (green.blue() - yellow.blue()) * t));
    // Level with the words' visual middle: a label centres its line box (ascent to descent), but the eye reads the
    // middle of the lowercase letters (half the x-height above the baseline), which is lower. The dot inherits the
    // status bar's font, so the same metrics apply.
    const QFontMetricsF fm(font());
    const double down = (fm.ascent() - fm.descent()) / 2 - fm.xHeight() / 2;
    const QPointF centre = QRectF(rect()).center() + QPointF(0, down);
    // A soft glow behind, then the dot with a slightly darker rim.
    QRadialGradient glow(centre, 10);
    QColor halo = colour;
    halo.setAlpha(110);
    glow.setColorAt(0.0, halo);
    halo.setAlpha(0);
    glow.setColorAt(1.0, halo);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(centre, 10, 10);
    painter.setBrush(colour);
    painter.setPen(QPen(colour.darker(150), 1));
    painter.drawEllipse(centre, 5, 5);
}

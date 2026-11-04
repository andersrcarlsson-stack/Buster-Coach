// playerstrip.cpp — building the strip, and what it shows.
//
// Sections:
//   1. The level names
//   2. Building the strip: the grid (king | name + second line | clock), the slider, the geek line, captures
//   3. What it shows: player, captures, clock, strength, geek line

#include "playerstrip.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFontDatabase>
#include <QSlider>
#include <QFontMetrics>
#include <QEvent>
#include "piecepainter.h"
#include "capturedrow.h"

// =================================================================================================
// 1. THE LEVEL NAMES
// =================================================================================================
// The name shown under the opponent's name (release_design.md 14, the calibration). The rating, the lichess rapid band
// whose mistakes the level matches, only with the built-in Buster: the node limits were measured with it.
static QString levelName(int level, bool withRating) {
    static const char *names[] = {"", "Beginner", "Novice", "Casual", "Improving", "Club player",
                                  "Strong club", "Expert", "Strong expert", "Candidate master", "Master",
                                  "Full strength"};
    static const int ratings[] = {0, 1000, 1150, 1300, 1450, 1600, 1750, 1900, 2050, 2200, 2400, 0};
    QString text = QString("%1 · %2").arg(level).arg(names[level]);   // "5 · Club player"
    if (withRating && ratings[level] > 0)
        text += QString(" (≈ %1)").arg(ratings[level]);              // "5 · Club player (≈ 1600)"
    return text;
}

// =================================================================================================
// 2. BUILDING THE STRIP: grid (king | name + second line | clock), slider, geek line, captures
// =================================================================================================
PlayerStrip::PlayerStrip(QWidget *parent) : QWidget(parent) {
    auto *column = new QVBoxLayout(this);   // the strip, top to bottom: the grid, the slider, the captures

    // The grid: the king | name and level, stacked | the clock. King and clock span both lines.
    auto *grid = new QGridLayout;
    column->addLayout(grid);

    m_name = new QLabel;
    QFont nameFont = m_name->font();
    nameFont.setPointSize(nameFont.pointSize() + 4);   // larger than the normal text: tune to taste
    nameFont.setBold(true);
    m_name->setFont(nameFont);

    m_king = new QLabel;                                // the king as an image, drawn like the board's (setPlayer)
    m_kingSize = 2 * QFontMetrics(nameFont).height();   // about two text lines high, as before
    grid->addWidget(m_king, 0, 0, 2, 1, Qt::AlignVCenter);   // row 0, column 0, 2 rows, 1 column

    m_advantage = new QLabel;                           // "+N", right after the name
    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(m_name);
    nameRow->addWidget(m_advantage);
    nameRow->addStretch();
    grid->addLayout(nameRow, 0, 1);

    m_levelName = new QLabel;                           // "Level 5", under the name
    // The line is reserved in BOTH strips (the same look; room for a second line of your own later),
    // so it is never hidden: empty, it keeps one line of height.
    m_levelName->setMinimumHeight(m_levelName->fontMetrics().height());
    grid->addWidget(m_levelName, 1, 1);

    m_clock = new QLabel;
    QFont clockFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);   // equal-width digits: no jitter
    clockFont.setPointSize(nameFont.pointSize() + 4);                         // large: tune to taste
    m_clock->setFont(clockFont);
    m_clock->setContentsMargins(6, 0, 6, 0);           // room for the highlight
    m_clock->hide();                                    // untimed (the default): no clock
    grid->addWidget(m_clock, 0, 2, 2, 1, Qt::AlignVCenter);
    grid->setColumnStretch(1, 1);                       // the middle takes the spare width: clock to the right

    // The strength slider (the opponent only): the full width.
    m_level = new QSlider(Qt::Horizontal);
    m_level->setToolTip("The opponent's strength, from 1 (beginner) to 11 (full strength). The weaker levels think "
                        "less and make human mistakes on purpose. A change counts from the opponent's next move; "
                        "locked during a Tournament game.");
    m_level->setRange(1, 11);                           // 1 = easiest .. 11 = full strength (10 · Master since 2026-11-04)
    m_level->setPageStep(1);                            // a click beside the handle: one level
    m_level->setTickPosition(QSlider::TicksBelow);
    m_level->setTickInterval(1);
    m_level->hide();                                    // shown by showStrength()
    column->addWidget(m_level);
    connect(m_level, &QSlider::valueChanged, this, [this](int level) {
        m_levelName->setText(levelName(level, m_ratingsShown));
        emit levelChanged(level);
    });

    // The engine's search statistics (the geek line, opponent only): small and grey.
    m_stats = new QLabel;
    m_stats->setToolTip("The opponent's last search: how deep, how many positions, how fast, how long.");
    QFont statsFont = m_stats->font();
    statsFont.setPointSize(statsFont.pointSize() - 2);
    m_stats->setFont(statsFont);
    m_stats->setStyleSheet("color: #808080;");
    m_stats->setMinimumHeight(m_stats->fontMetrics().height());   // keeps its line while still empty
    m_stats->hide();                                               // shown by showStrength()
    column->addWidget(m_stats);

    // The captured pieces: the full width, at the bottom. Grouped and overlapping, sized by the width
    // (height-for-width: the full set plus two promoted queens always fits); never widens the panel.
    m_captured = new CapturedRow;
    m_captured->setToolTip("The pieces taken so far, and the material balance (+2 = two pawns ahead).");
    column->addWidget(m_captured);

}

// =================================================================================================
// 3. WHAT IT SHOWS: player, captures, clock, strength, geek line
// =================================================================================================
void PlayerStrip::setPlayer(const QString &name, bool white) {
    m_white = white;
    updateKing();
    m_name->setText(name);
    if (m_level->isHidden())               // no strength slider here (your strip): the second line says the colour
        m_levelName->setText(white ? "Playing White" : "Playing Black");
}

void PlayerStrip::updateKing() {
    const int code = m_white ? 12 : 11;   // a king in the player's colour
    m_king->setPixmap(piecePixmap(code, m_kingSize, panelRim(palette(), code)));
}

void PlayerStrip::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);                    // the base class first: it has its own work for these events
    if (event->type() == QEvent::PaletteChange)     // the application's palette reached this widget
        updateKing();
}

void PlayerStrip::setCaptured(const QList<int> &pieces, int advantage) {
    m_captured->setPieces(pieces);
    m_advantage->setText(advantage > 0 ? QString("+%1").arg(advantage) : QString());
}

void PlayerStrip::setClock(const QString &text, bool running, ClockAlert alert) {
    m_clock->setText(text);
    m_clock->setVisible(!text.isEmpty());   // untimed: no clock
    // Running: bold on pale yellow (dark theme: dark amber with light digits); stopped: dimmed. Low time colours the
    // digits (both states); critical in the dark theme: white on red (red digits vanish on the amber).
    const bool dark = palette().color(QPalette::Window).lightness() < 128;   // the theme, read from the palette
    QString style = !running ? "color: #808080;"
                  : dark     ? "background-color: #6b5a1e; color: #fff3b0; font-weight: bold;"
                             : "background-color: #fff3b0; font-weight: bold;";
    switch (alert) {
    case ClockAlert::Low:      style += dark ? "color: #ffb347;" : "color: #c87800;"; break;   // amber
    case ClockAlert::Critical: style += dark ? "background-color: #8e2020; color: white;" : "color: #c62828;"; break;
    case ClockAlert::None:     break;
    }
    m_clock->setStyleSheet(style);
}

void PlayerStrip::showStrength(int level) {
    m_level->setValue(level);
    m_levelName->setText(levelName(level, m_ratingsShown));   // also if the value didn't change (no valueChanged then)
    m_level->show();
    m_levelName->show();
    m_stats->show();
}

void PlayerStrip::setRatingsShown(bool on) {
    m_ratingsShown = on;
    if (!m_level->isHidden())                         // the opponent's strip (yours says "Playing White")
        m_levelName->setText(levelName(m_level->value(), on));
}

void PlayerStrip::setStrengthEnabled(bool enabled) {
    m_level->setEnabled(enabled);
    m_levelName->setEnabled(enabled);   // the level's name greys with it
}

void PlayerStrip::setStats(const QString &text) {
    m_stats->setText(text);
}

// playerstrip.h — one player's strip in the side panel: the king glyph, name and a second line (the
// strength level, or "Playing White"), the clock, the strength slider and geek line (the opponent
// only), and the captured pieces. A composite widget: it arranges standard widgets, paints nothing.

#pragma once
#include <QWidget>
#include "chessclock.h"

class QLabel;
class CapturedRow;

class QSlider;

class PlayerStrip : public QWidget
{
    Q_OBJECT

signals:
    // The strength slider was moved: 1 (easiest) .. 10 (full strength).
    void levelChanged(int level);

public:
    // --- Construction ----------------------------------------------------------------------------
    explicit PlayerStrip(QWidget *parent = nullptr);

    // --- What every strip shows: player, captures, clock -----------------------------------------
    // The name and colour shown, e.g. ("Buster", false) -> "♚ Buster".
    void setPlayer(const QString &name, bool white);

    // The pieces this player has captured (as glyphs) and their material advantage (shown if > 0).
    void setCaptured(const QList<int> &pieces, int advantage);   // the captured pieces' codes, and the advantage

    // The clock reading ("" = no clock: an untimed game), whether it is running (highlighted), and how
    // urgent it looks (low time: amber, critical: red).
    void setClock(const QString &text, bool running, ClockAlert alert);

    // --- The opponent's strip only: strength slider, geek line -----------------------------------
    // Show the strength slider (the opponent's strip only), set to this level.
    void showStrength(int level);

    // Tournament: the strength can't change during a game (the slider greys out).
    void setStrengthEnabled(bool enabled);

    // The level's approximate rating ("≈ 1600") beside its name: only for the built-in Buster (release_design.md 14).
    void setRatingsShown(bool on);

    // A small grey line under the slider: the engine's search (depth, nodes, speed, time). Opponent only.
    void setStats(const QString &text);

    // --- Query -----------------------------------------------------------------------------------
    bool isWhite() const { return m_white; }

protected:
    // The palette changed (light / dark): the king's picture is drawn again with the right rim.
    void changeEvent(QEvent *event) override;

private:
    void updateKing();       // draws the king's picture: the player's colour, the palette's rim
    QLabel *m_king;          // the king glyph in the player's colour (spans the name and level lines)
    int m_kingSize = 40;     // the king's image size: about the name and second line together (set in the constructor)
    QLabel *m_name;   // a child: owned through the parent-child tree
    CapturedRow *m_captured; // the captured pieces: grouped, overlapping, drawn like the board's
    bool m_white = true;     // the colour set by setPlayer
    bool m_ratingsShown = false;   // the level names carry their rating (the opponent is the built-in Buster)
    QLabel *m_advantage;     // "+N" (its own label: its own font size)
    QLabel *m_clock;         // the clock (right, spanning both lines), hidden in untimed games
    QSlider *m_level;        // the strength slider (under the grid), hidden unless showStrength() is called
    QLabel *m_levelName;     // the second line: "5 · Strong club" (opponent) or "Playing White" (you)
    QLabel *m_stats;         // the engine's geek line (shown with the slider)
};
